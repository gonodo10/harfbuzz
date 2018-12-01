/*
 * Copyright © 2018 Adobe Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Adobe Author(s): Michiharu Ariza
 */
#ifndef HB_CFF_INTERP_CS_COMMON_HH
#define HB_CFF_INTERP_CS_COMMON_HH

#include "hb.hh"
#include "hb-cff-interp-common.hh"

namespace CFF {

using namespace OT;

enum CSType {
  CSType_CharString,
  CSType_GlobalSubr,
  CSType_LocalSubr
};

struct CallContext
{
  inline void init (const SubByteStr substr_=SubByteStr (), CSType type_=CSType_CharString, unsigned int subr_num_=0)
  {
    substr = substr_;
    type = type_;
    subr_num = subr_num_;
  }

  inline void fini (void) {}

  SubByteStr      substr;
  CSType	  type;
  unsigned int    subr_num;
};

/* call stack */
const unsigned int kMaxCallLimit = 10;
struct CallStack : Stack<CallContext, kMaxCallLimit> {};

template <typename SUBRS>
struct BiasedSubrs
{
  inline void init (const SUBRS &subrs_)
  {
    subrs = &subrs_;
    unsigned int  nSubrs = subrs_.count;
    if (nSubrs < 1240)
      bias = 107;
    else if (nSubrs < 33900)
      bias = 1131;
    else
      bias = 32768;
  }

  inline void fini (void) {}

  const SUBRS   *subrs;
  unsigned int  bias;
};

struct Point
{
  inline void init (void)
  {
    x.init ();
    y.init ();
  }

  inline void set_int (int _x, int _y)
  {
    x.set_int (_x);
    y.set_int (_y);
  }

  inline void move_x (const Number &dx) { x += dx; }
  inline void move_y (const Number &dy) { y += dy; }
  inline void move (const Number &dx, const Number &dy) { move_x (dx); move_y (dy); }
  inline void move (const Point &d) { move_x (d.x); move_y (d.y); }

  Number  x;
  Number  y;
};

template <typename ARG, typename SUBRS>
struct CSInterpEnv : InterpEnv<ARG>
{
  inline void init (const ByteStr &str, const SUBRS &globalSubrs_, const SUBRS &localSubrs_)
  {
    InterpEnv<ARG>::init (str);

    context.init (str, CSType_CharString);
    seen_moveto = true;
    seen_hintmask = false;
    hstem_count = 0;
    vstem_count = 0;
    pt.init ();
    callStack.init ();
    globalSubrs.init (globalSubrs_);
    localSubrs.init (localSubrs_);
  }
  inline void fini (void)
  {
    InterpEnv<ARG>::fini ();

    callStack.fini ();
    globalSubrs.fini ();
    localSubrs.fini ();
  }

  inline bool in_error (void) const
  {
    return callStack.in_error () || SUPER::in_error ();
  }

  inline bool popSubrNum (const BiasedSubrs<SUBRS>& biasedSubrs, unsigned int &subr_num)
  {
    int n = SUPER::argStack.pop_int ();
    n += biasedSubrs.bias;
    if (unlikely ((n < 0) || ((unsigned int)n >= biasedSubrs.subrs->count)))
      return false;

    subr_num = (unsigned int)n;
    return true;
  }

  inline void callSubr (const BiasedSubrs<SUBRS>& biasedSubrs, CSType type)
  {
    unsigned int subr_num;

    if (unlikely (!popSubrNum (biasedSubrs, subr_num)
		 || callStack.get_count () >= kMaxCallLimit))
    {
      SUPER::set_error ();
      return;
    }
    context.substr = SUPER::substr;
    callStack.push (context);

    context.init ( (*biasedSubrs.subrs)[subr_num], type, subr_num);
    SUPER::substr = context.substr;
  }

  inline void returnFromSubr (void)
  {
    if (unlikely (SUPER::substr.in_error ()))
      SUPER::set_error ();
    context = callStack.pop ();
    SUPER::substr = context.substr;
  }

  inline void determine_hintmask_size (void)
  {
    if (!seen_hintmask)
    {
      vstem_count += SUPER::argStack.get_count() / 2;
      hintmask_size = (hstem_count + vstem_count + 7) >> 3;
      seen_hintmask = true;
    }
  }

  inline void set_endchar (bool endchar_flag_) { endchar_flag = endchar_flag_; }
  inline bool is_endchar (void) const { return endchar_flag; }

  inline const Number &get_x (void) const { return pt.x; }
  inline const Number &get_y (void) const { return pt.y; }
  inline const Point &get_pt (void) const { return pt; }

  inline void moveto (const Point &pt_ ) { pt = pt_; }

  public:
  CallContext   context;
  bool	  endchar_flag;
  bool	  seen_moveto;
  bool	  seen_hintmask;

  unsigned int  hstem_count;
  unsigned int  vstem_count;
  unsigned int  hintmask_size;
  CallStack	    callStack;
  BiasedSubrs<SUBRS>   globalSubrs;
  BiasedSubrs<SUBRS>   localSubrs;

  private:
  Point	 pt;

  typedef InterpEnv<ARG> SUPER;
};

template <typename ENV, typename PARAM>
struct PathProcsNull
{
  static inline void rmoveto (ENV &env, PARAM& param) {}
  static inline void hmoveto (ENV &env, PARAM& param) {}
  static inline void vmoveto (ENV &env, PARAM& param) {}
  static inline void rlineto (ENV &env, PARAM& param) {}
  static inline void hlineto (ENV &env, PARAM& param) {}
  static inline void vlineto (ENV &env, PARAM& param) {}
  static inline void rrcurveto (ENV &env, PARAM& param) {}
  static inline void rcurveline (ENV &env, PARAM& param) {}
  static inline void rlinecurve (ENV &env, PARAM& param) {}
  static inline void vvcurveto (ENV &env, PARAM& param) {}
  static inline void hhcurveto (ENV &env, PARAM& param) {}
  static inline void vhcurveto (ENV &env, PARAM& param) {}
  static inline void hvcurveto (ENV &env, PARAM& param) {}
  static inline void moveto (ENV &env, PARAM& param, const Point &pt) {}
  static inline void line (ENV &env, PARAM& param, const Point &pt1) {}
  static inline void curve (ENV &env, PARAM& param, const Point &pt1, const Point &pt2, const Point &pt3) {}
  static inline void hflex (ENV &env, PARAM& param) {}
  static inline void flex (ENV &env, PARAM& param) {}
  static inline void hflex1 (ENV &env, PARAM& param) {}
  static inline void flex1 (ENV &env, PARAM& param) {}
};

template <typename ARG, typename OPSET, typename ENV, typename PARAM, typename PATH=PathProcsNull<ENV, PARAM> >
struct CSOpSet : OpSet<ARG>
{
  static inline void process_op (OpCode op, ENV &env, PARAM& param)
  {
    switch (op) {

      case OpCode_return:
	env.returnFromSubr ();
	break;
      case OpCode_endchar:
	OPSET::check_width (op, env, param);
	env.set_endchar (true);
	OPSET::flush_args_and_op (op, env, param);
	break;

      case OpCode_fixedcs:
	env.argStack.push_fixed_from_substr (env.substr);
	break;

      case OpCode_callsubr:
	env.callSubr (env.localSubrs, CSType_LocalSubr);
	break;

      case OpCode_callgsubr:
	env.callSubr (env.globalSubrs, CSType_GlobalSubr);
	break;

      case OpCode_hstem:
      case OpCode_hstemhm:
	OPSET::check_width (op, env, param);
	OPSET::process_hstem (op, env, param);
	break;
      case OpCode_vstem:
      case OpCode_vstemhm:
	OPSET::check_width (op, env, param);
	OPSET::process_vstem (op, env, param);
	break;
      case OpCode_hintmask:
      case OpCode_cntrmask:
	OPSET::check_width (op, env, param);
	OPSET::process_hintmask (op, env, param);
	break;
      case OpCode_rmoveto:
	OPSET::check_width (op, env, param);
	PATH::rmoveto (env, param);
	OPSET::process_post_move (op, env, param);
	break;
      case OpCode_hmoveto:
	OPSET::check_width (op, env, param);
	PATH::hmoveto (env, param);
	OPSET::process_post_move (op, env, param);
	break;
      case OpCode_vmoveto:
	OPSET::check_width (op, env, param);
	PATH::vmoveto (env, param);
	OPSET::process_post_move (op, env, param);
	break;
      case OpCode_rlineto:
	PATH::rlineto (env, param);
	process_post_path (op, env, param);
	break;
      case OpCode_hlineto:
	PATH::hlineto (env, param);
	process_post_path (op, env, param);
	break;
      case OpCode_vlineto:
	PATH::vlineto (env, param);
	process_post_path (op, env, param);
	break;
      case OpCode_rrcurveto:
	PATH::rrcurveto (env, param);
	process_post_path (op, env, param);
	break;
      case OpCode_rcurveline:
	PATH::rcurveline (env, param);
	process_post_path (op, env, param);
	break;
      case OpCode_rlinecurve:
	PATH::rlinecurve (env, param);
	process_post_path (op, env, param);
	break;
      case OpCode_vvcurveto:
	PATH::vvcurveto (env, param);
	process_post_path (op, env, param);
	break;
      case OpCode_hhcurveto:
	PATH::hhcurveto (env, param);
	process_post_path (op, env, param);
	break;
      case OpCode_vhcurveto:
	PATH::vhcurveto (env, param);
	process_post_path (op, env, param);
	break;
      case OpCode_hvcurveto:
	PATH::hvcurveto (env, param);
	process_post_path (op, env, param);
	break;

      case OpCode_hflex:
	PATH::hflex (env, param);
	OPSET::process_post_flex (op, env, param);
	break;

      case OpCode_flex:
	PATH::flex (env, param);
	OPSET::process_post_flex (op, env, param);
	break;

      case OpCode_hflex1:
	PATH::hflex1 (env, param);
	OPSET::process_post_flex (op, env, param);
	break;

      case OpCode_flex1:
	PATH::flex1 (env, param);
	OPSET::process_post_flex (op, env, param);
	break;

      default:
	SUPER::process_op (op, env);
	break;
    }
  }

  static inline void process_hstem (OpCode op, ENV &env, PARAM& param)
  {
    env.hstem_count += env.argStack.get_count () / 2;
    OPSET::flush_args_and_op (op, env, param);
  }

  static inline void process_vstem (OpCode op, ENV &env, PARAM& param)
  {
    env.vstem_count += env.argStack.get_count () / 2;
    OPSET::flush_args_and_op (op, env, param);
  }

  static inline void process_hintmask (OpCode op, ENV &env, PARAM& param)
  {
    env.determine_hintmask_size ();
    if (likely (env.substr.avail (env.hintmask_size)))
    {
      OPSET::flush_hintmask (op, env, param);
      env.substr.inc (env.hintmask_size);
    }
  }

  static inline void process_post_flex (OpCode op, ENV &env, PARAM& param)
  {
    OPSET::flush_args_and_op (op, env, param);
  }

  static inline void check_width (OpCode op, ENV &env, PARAM& param)
  {}

  static inline void process_post_move (OpCode op, ENV &env, PARAM& param)
  {
    if (!env.seen_moveto)
    {
      env.determine_hintmask_size ();
      env.seen_moveto = true;
    }
    OPSET::flush_args_and_op (op, env, param);
  }

  static inline void process_post_path (OpCode op, ENV &env, PARAM& param)
  {
    OPSET::flush_args_and_op (op, env, param);
  }

  static inline void flush_args_and_op (OpCode op, ENV &env, PARAM& param)
  {
    OPSET::flush_args (env, param);
    OPSET::flush_op (op, env, param);
  }

  static inline void flush_args (ENV &env, PARAM& param)
  {
    env.pop_n_args (env.argStack.get_count ());
  }

  static inline void flush_op (OpCode op, ENV &env, PARAM& param)
  {
  }

  static inline void flush_hintmask (OpCode op, ENV &env, PARAM& param)
  {
    OPSET::flush_args_and_op (op, env, param);
  }

  static inline bool is_number_op (OpCode op)
  {
    switch (op)
    {
      case OpCode_shortint:
      case OpCode_fixedcs:
      case OpCode_TwoBytePosInt0: case OpCode_TwoBytePosInt1:
      case OpCode_TwoBytePosInt2: case OpCode_TwoBytePosInt3:
      case OpCode_TwoByteNegInt0: case OpCode_TwoByteNegInt1:
      case OpCode_TwoByteNegInt2: case OpCode_TwoByteNegInt3:
	return true;

      default:
	/* 1-byte integer */
	return (OpCode_OneByteIntFirst <= op) && (op <= OpCode_OneByteIntLast);
    }
  }

  protected:
  typedef OpSet<ARG>  SUPER;
};

template <typename PATH, typename ENV, typename PARAM>
struct PathProcs
{
  static inline void rmoveto (ENV &env, PARAM& param)
  {
    Point pt1 = env.get_pt ();
    const Number &dy = env.pop_arg ();
    const Number &dx = env.pop_arg ();
    pt1.move (dx, dy);
    PATH::moveto (env, param, pt1);
  }

  static inline void hmoveto (ENV &env, PARAM& param)
  {
    Point pt1 = env.get_pt ();
    pt1.move_x (env.pop_arg ());
    PATH::moveto (env, param, pt1);
  }

  static inline void vmoveto (ENV &env, PARAM& param)
  {
    Point pt1 = env.get_pt ();
    pt1.move_y (env.pop_arg ());
    PATH::moveto (env, param, pt1);
  }

  static inline void rlineto (ENV &env, PARAM& param)
  {
    for (unsigned int i = 0; i + 2 <= env.argStack.get_count (); i += 2)
    {
      Point pt1 = env.get_pt ();
      pt1.move (env.eval_arg (i), env.eval_arg (i+1));
      PATH::line (env, param, pt1);
    }
  }

  static inline void hlineto (ENV &env, PARAM& param)
  {
    Point pt1;
    unsigned int i = 0;
    for (; i + 2 <= env.argStack.get_count (); i += 2)
    {
      pt1 = env.get_pt ();
      pt1.move_x (env.eval_arg (i));
      PATH::line (env, param, pt1);
      pt1.move_y (env.eval_arg (i+1));
      PATH::line (env, param, pt1);
    }
    if (i < env.argStack.get_count ())
    {
      pt1 = env.get_pt ();
      pt1.move_x (env.eval_arg (i));
      PATH::line (env, param, pt1);
    }
  }

  static inline void vlineto (ENV &env, PARAM& param)
  {
    Point pt1;
    unsigned int i = 0;
    for (; i + 2 <= env.argStack.get_count (); i += 2)
    {
      pt1 = env.get_pt ();
      pt1.move_y (env.eval_arg (i));
      PATH::line (env, param, pt1);
      pt1.move_x (env.eval_arg (i+1));
      PATH::line (env, param, pt1);
    }
    if (i < env.argStack.get_count ())
    {
      pt1 = env.get_pt ();
      pt1.move_y (env.eval_arg (i));
      PATH::line (env, param, pt1);
    }
  }

  static inline void rrcurveto (ENV &env, PARAM& param)
  {
    for (unsigned int i = 0; i + 6 <= env.argStack.get_count (); i += 6)
    {
      Point pt1 = env.get_pt ();
      pt1.move (env.eval_arg (i), env.eval_arg (i+1));
      Point pt2 = pt1;
      pt2.move (env.eval_arg (i+2), env.eval_arg (i+3));
      Point pt3 = pt2;
      pt3.move (env.eval_arg (i+4), env.eval_arg (i+5));
      PATH::curve (env, param, pt1, pt2, pt3);
    }
  }

  static inline void rcurveline (ENV &env, PARAM& param)
  {
    unsigned int i = 0;
    for (; i + 6 <= env.argStack.get_count (); i += 6)
    {
      Point pt1 = env.get_pt ();
      pt1.move (env.eval_arg (i), env.eval_arg (i+1));
      Point pt2 = pt1;
      pt2.move (env.eval_arg (i+2), env.eval_arg (i+3));
      Point pt3 = pt2;
      pt3.move (env.eval_arg (i+4), env.eval_arg (i+5));
      PATH::curve (env, param, pt1, pt2, pt3);
    }
    for (; i + 2 <= env.argStack.get_count (); i += 2)
    {
      Point pt1 = env.get_pt ();
      pt1.move (env.eval_arg (i), env.eval_arg (i+1));
      PATH::line (env, param, pt1);
    }
  }

  static inline void rlinecurve (ENV &env, PARAM& param)
  {
    unsigned int i = 0;
    unsigned int line_limit = (env.argStack.get_count () % 6);
    for (; i + 2 <= line_limit; i += 2)
    {
      Point pt1 = env.get_pt ();
      pt1.move (env.eval_arg (i), env.eval_arg (i+1));
      PATH::line (env, param, pt1);
    }
    for (; i + 6 <= env.argStack.get_count (); i += 6)
    {
      Point pt1 = env.get_pt ();
      pt1.move (env.eval_arg (i), env.eval_arg (i+1));
      Point pt2 = pt1;
      pt2.move (env.eval_arg (i+2), env.eval_arg (i+3));
      Point pt3 = pt2;
      pt3.move (env.eval_arg (i+4), env.eval_arg (i+5));
      PATH::curve (env, param, pt1, pt2, pt3);
    }
  }

  static inline void vvcurveto (ENV &env, PARAM& param)
  {
    unsigned int i = 0;
    Point pt1 = env.get_pt ();
    if ((env.argStack.get_count () & 1) != 0)
      pt1.move_x (env.eval_arg (i++));
    for (; i + 4 <= env.argStack.get_count (); i += 4)
    {
      pt1.move_y (env.eval_arg (i));
      Point pt2 = pt1;
      pt2.move (env.eval_arg (i+1), env.eval_arg (i+2));
      Point pt3 = pt2;
      pt3.move_y (env.eval_arg (i+3));
      PATH::curve (env, param, pt1, pt2, pt3);
      pt1 = env.get_pt ();
    }
  }

  static inline void hhcurveto (ENV &env, PARAM& param)
  {
    unsigned int i = 0;
    Point pt1 = env.get_pt ();
    if ((env.argStack.get_count () & 1) != 0)
      pt1.move_y (env.eval_arg (i++));
    for (; i + 4 <= env.argStack.get_count (); i += 4)
    {
      pt1.move_x (env.eval_arg (i));
      Point pt2 = pt1;
      pt2.move (env.eval_arg (i+1), env.eval_arg (i+2));
      Point pt3 = pt2;
      pt3.move_x (env.eval_arg (i+3));
      PATH::curve (env, param, pt1, pt2, pt3);
      pt1 = env.get_pt ();
    }
  }

  static inline void vhcurveto (ENV &env, PARAM& param)
  {
    Point pt1, pt2, pt3;
    unsigned int i = 0;
    if ((env.argStack.get_count () % 8) >= 4)
    {
      Point pt1 = env.get_pt ();
      pt1.move_y (env.eval_arg (i));
      Point pt2 = pt1;
      pt2.move (env.eval_arg (i+1), env.eval_arg (i+2));
      Point pt3 = pt2;
      pt3.move_x (env.eval_arg (i+3));
      i += 4;

      for (; i + 8 <= env.argStack.get_count (); i += 8)
      {
	PATH::curve (env, param, pt1, pt2, pt3);
	pt1 = env.get_pt ();
	pt1.move_x (env.eval_arg (i));
	pt2 = pt1;
	pt2.move (env.eval_arg (i+1), env.eval_arg (i+2));
	pt3 = pt2;
	pt3.move_y (env.eval_arg (i+3));
	PATH::curve (env, param, pt1, pt2, pt3);

	pt1 = pt3;
	pt1.move_y (env.eval_arg (i+4));
	pt2 = pt1;
	pt2.move (env.eval_arg (i+5), env.eval_arg (i+6));
	pt3 = pt2;
	pt3.move_x (env.eval_arg (i+7));
      }
      if (i < env.argStack.get_count ())
	pt3.move_y (env.eval_arg (i));
      PATH::curve (env, param, pt1, pt2, pt3);
    }
    else
    {
      for (; i + 8 <= env.argStack.get_count (); i += 8)
      {
	pt1 = env.get_pt ();
	pt1.move_y (env.eval_arg (i));
	pt2 = pt1;
	pt2.move (env.eval_arg (i+1), env.eval_arg (i+2));
	pt3 = pt2;
	pt3.move_x (env.eval_arg (i+3));
	PATH::curve (env, param, pt1, pt2, pt3);

	pt1 = pt3;
	pt1.move_x (env.eval_arg (i+4));
	pt2 = pt1;
	pt2.move (env.eval_arg (i+5), env.eval_arg (i+6));
	pt3 = pt2;
	pt3.move_y (env.eval_arg (i+7));
	if ((env.argStack.get_count () - i < 16) && ((env.argStack.get_count () & 1) != 0))
	  pt3.move_x (env.eval_arg (i+8));
	PATH::curve (env, param, pt1, pt2, pt3);
      }
    }
  }

  static inline void hvcurveto (ENV &env, PARAM& param)
  {
    Point pt1, pt2, pt3;
    unsigned int i = 0;
    if ((env.argStack.get_count () % 8) >= 4)
    {
      Point pt1 = env.get_pt ();
      pt1.move_x (env.eval_arg (i));
      Point pt2 = pt1;
      pt2.move (env.eval_arg (i+1), env.eval_arg (i+2));
      Point pt3 = pt2;
      pt3.move_y (env.eval_arg (i+3));
      i += 4;

      for (; i + 8 <= env.argStack.get_count (); i += 8)
      {
	PATH::curve (env, param, pt1, pt2, pt3);
	pt1 = env.get_pt ();
	pt1.move_y (env.eval_arg (i));
	pt2 = pt1;
	pt2.move (env.eval_arg (i+1), env.eval_arg (i+2));
	pt3 = pt2;
	pt3.move_x (env.eval_arg (i+3));
	PATH::curve (env, param, pt1, pt2, pt3);

	pt1 = pt3;
	pt1.move_x (env.eval_arg (i+4));
	pt2 = pt1;
	pt2.move (env.eval_arg (i+5), env.eval_arg (i+6));
	pt3 = pt2;
	pt3.move_y (env.eval_arg (i+7));
      }
      if (i < env.argStack.get_count ())
	pt3.move_x (env.eval_arg (i));
      PATH::curve (env, param, pt1, pt2, pt3);
    }
    else
    {
      for (; i + 8 <= env.argStack.get_count (); i += 8)
      {
	pt1 = env.get_pt ();
	pt1.move_x (env.eval_arg (i));
	pt2 = pt1;
	pt2.move (env.eval_arg (i+1), env.eval_arg (i+2));
	pt3 = pt2;
	pt3.move_y (env.eval_arg (i+3));
	PATH::curve (env, param, pt1, pt2, pt3);

	pt1 = pt3;
	pt1.move_y (env.eval_arg (i+4));
	pt2 = pt1;
	pt2.move (env.eval_arg (i+5), env.eval_arg (i+6));
	pt3 = pt2;
	pt3.move_x (env.eval_arg (i+7));
	if ((env.argStack.get_count () - i < 16) && ((env.argStack.get_count () & 1) != 0))
	  pt3.move_y (env.eval_arg (i+8));
	PATH::curve (env, param, pt1, pt2, pt3);
      }
    }
  }

  /* default actions to be overridden */
  static inline void moveto (ENV &env, PARAM& param, const Point &pt)
  { env.moveto (pt); }

  static inline void line (ENV &env, PARAM& param, const Point &pt1)
  { PATH::moveto (env, param, pt1); }

  static inline void curve (ENV &env, PARAM& param, const Point &pt1, const Point &pt2, const Point &pt3)
  { PATH::moveto (env, param, pt3); }

  static inline void hflex (ENV &env, PARAM& param)
  {
    if (likely (env.argStack.get_count () == 7))
    {
      Point pt1 = env.get_pt ();
      pt1.move_x (env.eval_arg (0));
      Point pt2 = pt1;
      pt2.move (env.eval_arg (1), env.eval_arg (2));
      Point pt3 = pt2;
      pt3.move_x (env.eval_arg (3));
      Point pt4 = pt3;
      pt4.move_x (env.eval_arg (4));
      Point pt5 = pt4;
      pt5.move_x (env.eval_arg (5));
      pt5.y = pt1.y;
      Point pt6 = pt5;
      pt6.move_x (env.eval_arg (6));

      curve2 (env, param, pt1, pt2, pt3, pt4, pt5, pt6);
    }
    else
      env.set_error ();
  }

  static inline void flex (ENV &env, PARAM& param)
  {
    if (likely (env.argStack.get_count () == 13))
    {
      Point pt1 = env.get_pt ();
      pt1.move (env.eval_arg (0), env.eval_arg (1));
      Point pt2 = pt1;
      pt2.move (env.eval_arg (2), env.eval_arg (3));
      Point pt3 = pt2;
      pt3.move (env.eval_arg (4), env.eval_arg (5));
      Point pt4 = pt3;
      pt4.move (env.eval_arg (6), env.eval_arg (7));
      Point pt5 = pt4;
      pt5.move (env.eval_arg (8), env.eval_arg (9));
      Point pt6 = pt5;
      pt6.move (env.eval_arg (10), env.eval_arg (11));

      curve2 (env, param, pt1, pt2, pt3, pt4, pt5, pt6);
    }
    else
      env.set_error ();
  }

  static inline void hflex1 (ENV &env, PARAM& param)
  {
    if (likely (env.argStack.get_count () == 9))
    {
      Point pt1 = env.get_pt ();
      pt1.move (env.eval_arg (0), env.eval_arg (1));
      Point pt2 = pt1;
      pt2.move (env.eval_arg (2), env.eval_arg (3));
      Point pt3 = pt2;
      pt3.move_x (env.eval_arg (4));
      Point pt4 = pt3;
      pt4.move_x (env.eval_arg (5));
      Point pt5 = pt4;
      pt5.move (env.eval_arg (6), env.eval_arg (7));
      Point pt6 = pt5;
      pt6.move_x (env.eval_arg (8));
      pt6.y = env.get_pt ().y;

      curve2 (env, param, pt1, pt2, pt3, pt4, pt5, pt6);
    }
    else
      env.set_error ();
  }

  static inline void flex1 (ENV &env, PARAM& param)
  {
    if (likely (env.argStack.get_count () == 11))
    {
      Point d;
      d.init ();
      for (unsigned int i = 0; i < 10; i += 2)
	d.move (env.eval_arg (i), env.eval_arg (i+1));

      Point pt1 = env.get_pt ();
      pt1.move (env.eval_arg (0), env.eval_arg (1));
      Point pt2 = pt1;
      pt2.move (env.eval_arg (2), env.eval_arg (3));
      Point pt3 = pt2;
      pt3.move (env.eval_arg (4), env.eval_arg (5));
      Point pt4 = pt3;
      pt4.move (env.eval_arg (6), env.eval_arg (7));
      Point pt5 = pt4;
      pt5.move (env.eval_arg (8), env.eval_arg (9));
      Point pt6 = pt5;

      if (fabs (d.x.to_real ()) > fabs (d.y.to_real ()))
      {
	pt6.move_x (env.eval_arg (10));
	pt6.y = env.get_pt ().y;
      }
      else
      {
	pt6.x = env.get_pt ().x;
	pt6.move_y (env.eval_arg (10));
      }

      curve2 (env, param, pt1, pt2, pt3, pt4, pt5, pt6);
    }
    else
      env.set_error ();
  }

  protected:
  static inline void curve2 (ENV &env, PARAM& param,
			     const Point &pt1, const Point &pt2, const Point &pt3,
			     const Point &pt4, const Point &pt5, const Point &pt6)
  {
    PATH::curve (env, param, pt1, pt2, pt3);
    PATH::curve (env, param, pt4, pt5, pt6);
  }
};

template <typename ENV, typename OPSET, typename PARAM>
struct CSInterpreter : Interpreter<ENV>
{
  inline bool interpret (PARAM& param)
  {
    SUPER::env.set_endchar (false);

    for (;;) {
      OPSET::process_op (SUPER::env.fetch_op (), SUPER::env, param);
      if (unlikely (SUPER::env.in_error ()))
	return false;
      if (SUPER::env.is_endchar ())
	break;
    }

    return true;
  }

  private:
  typedef Interpreter<ENV> SUPER;
};

} /* namespace CFF */

#endif /* HB_CFF_INTERP_CS_COMMON_HH */
