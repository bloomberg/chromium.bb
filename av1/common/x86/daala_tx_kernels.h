/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/* This header does not use an include guard.
   It is intentionally designed to be included multiple times.
   The file that includes it should define the following macros:

   OD_KERNEL  A label for the width of the kernel, e.g., kernel8
   OD_WORD    A label for the size of the SIMD word, e.g., epi16
   OD_REG     The type of a SIMD register, e.g., __m128i
   OD_ADD     The intrinsic function for addition
   OD_SUB     The intrinsic function for subtraction
   OD_RSHIFT1 The function that implements an unbiased right shift by 1
   OD_AVG     The function that implements a signed PAVG[WD]
              I.e., (a + b + 1) >> 1, without overflow
   OD_HRSUB   The function that implements a VHRSUB.S<16|32>
              I.e., (a - b + 1) >> 1, without overflow
   OD_MULHRS  The function that implements a PMULHRS[WD]
              I.e., (a * b + 16384) >> 15, without overflow
   OD_SWAP    The function that swaps two SIMD registers

   See daala_inv_txfm_avx2.c for examples. */

#define OD_KERNEL_FUNC_IMPL(name, kernel, word) name##_##kernel##_##word
#define OD_KERNEL_FUNC_WRAPPER(name, kernel, word) \
  OD_KERNEL_FUNC_IMPL(name, kernel, word)
#define OD_KERNEL_FUNC(name) OD_KERNEL_FUNC_WRAPPER(name, OD_KERNEL, OD_WORD)

static INLINE void OD_KERNEL_FUNC(od_idct2)(OD_REG *p0, OD_REG *p1) {
  OD_REG t_;
  t_ = OD_ADD(*p0, *p1);
  /* 11585/8192 ~= 2*Sin[Pi/4] = 1.4142135623730951 */
  *p1 = OD_ADD(*p0, OD_MULHRS(*p0, (11585 - 8192) << 2));
  /* 11585/16384 ~= Cos[Pi/4] = 0.7071067811865475 */
  *p0 = OD_MULHRS(t_, 11585 << 1);
  *p1 = OD_SUB(*p1, *p0);
}

static INLINE void OD_KERNEL_FUNC(od_idst2)(OD_REG *p0, OD_REG *p1) {
  OD_REG t_;
  t_ = OD_AVG(*p0, *p1);
  /* 8867/16384 ~= Cos[3*Pi/8]*Sqrt[2] = 0.541196100146197 */
  *p0 = OD_MULHRS(*p0, 8867 << 1);
  /* 21407/16384 ~= Sin[3*Pi/8]*Sqrt[2] = 1.3065629648763766 */
  *p1 = OD_ADD(*p1, OD_MULHRS(*p1, (21407 - 16384) << 1));
  /* 15137/8192 ~= 2*Cos[Pi/8] = 1.8477590650225735 */
  t_ = OD_ADD(t_, OD_MULHRS(t_, (15137 - 8192) << 2));
  *p0 = OD_SUB(t_, *p0);
  *p1 = OD_SUB(t_, *p1);
}

static INLINE void OD_KERNEL_FUNC(od_idct2_asym)(OD_REG *p0, OD_REG *p1,
                                                 OD_REG *p1h) {
  *p1 = OD_SUB(*p0, *p1);
  *p1h = OD_RSHIFT1(*p1);
  *p0 = OD_SUB(*p0, *p1h);
}

static INLINE void OD_KERNEL_FUNC(od_idst2_asym)(OD_REG *p0, OD_REG *p1) {
  OD_REG t_;
  OD_REG u_;
  t_ = OD_AVG(*p0, *p1);
  /* 3135/4096 ~= (Cos[Pi/8] - Sin[Pi/8])*Sqrt[2] = 0.7653668647301795 */
  u_ = OD_MULHRS(*p1, 3135 << 3);
  /* 15137/16384 ~= (Cos[Pi/8] + Sin[Pi/8])/Sqrt[2] = 0.9238795325112867 */
  *p1 = OD_MULHRS(*p0, 15137 << 1);
  /* 8867/8192 ~= Cos[3*Pi/8]*2*Sqrt[2] = 1.082392200292394 */
  t_ = OD_ADD(t_, OD_MULHRS(t_, (8867 - 8192) << 2));
  *p0 = OD_ADD(u_, t_);
  *p1 = OD_SUB(*p1, OD_RSHIFT1(t_));
}

static INLINE void OD_KERNEL_FUNC(od_idct4)(OD_REG *q0, OD_REG *q2, OD_REG *q1,
                                            OD_REG *q3) {
  OD_REG q1h;
  OD_KERNEL_FUNC(od_idst2_asym)(q3, q2);
  OD_KERNEL_FUNC(od_idct2_asym)(q0, q1, &q1h);
  *q2 = OD_ADD(*q2, q1h);
  *q1 = OD_SUB(*q1, *q2);
  *q0 = OD_ADD(*q0, OD_RSHIFT1(*q3));
  *q3 = OD_SUB(*q0, *q3);
}

static INLINE void OD_KERNEL_FUNC(od_idct4_asym)(OD_REG *q0, OD_REG *q2,
                                                 OD_REG *q1, OD_REG *q1h,
                                                 OD_REG *q3, OD_REG *q3h) {
  OD_KERNEL_FUNC(od_idst2)(q3, q2);
  OD_KERNEL_FUNC(od_idct2)(q0, q1);
  *q1 = OD_SUB(*q1, *q2);
  *q1h = OD_RSHIFT1(*q1);
  *q2 = OD_ADD(*q2, *q1h);
  *q3 = OD_SUB(*q0, *q3);
  *q3h = OD_RSHIFT1(*q3);
  *q0 = OD_SUB(*q0, *q3h);
}

static INLINE void OD_KERNEL_FUNC(od_idst_vii4)(OD_REG *q0, OD_REG *q1,
                                                OD_REG *q2, OD_REG *q3) {
  OD_REG t0;
  OD_REG t1;
  OD_REG t2;
  OD_REG t3;
  OD_REG t3h;
  OD_REG t4;
  OD_REG u4;
  t0 = OD_SUB(*q0, *q3);
  t1 = OD_ADD(*q0, *q2);
  t2 = OD_ADD(*q3, OD_HRSUB(t0, *q2));
  t3 = *q1;
  t4 = OD_ADD(*q2, *q3);
  /* 467/2048 ~= 2*Sin[1*Pi/9]/3 ~= 0.228013428883779 */
  t0 = OD_MULHRS(t0, 467 << 4);
  /* 7021/16384 ~= 2*Sin[2*Pi/9]/3 ~= 0.428525073124360 */
  t1 = OD_MULHRS(t1, 7021 << 1);
  /* 37837/32768 ~= 4*Sin[3*Pi/9]/3 ~= 1.154700538379252 */
  t2 = OD_ADD(t2, OD_MULHRS(t2, 37837 - 32768));
  /* 37837/32768 ~= 4*Sin[3*Pi/9]/3 ~= 1.154700538379252 */
  t3 = OD_ADD(t3, OD_MULHRS(t3, 37837 - 32768));
  /* 21513/32768 ~= 2*Sin[4*Pi/9]/3 ~= 0.656538502008139 */
  t4 = OD_MULHRS(t4, 21513);
  t3h = OD_RSHIFT1(t3);
  u4 = OD_ADD(t4, t3h);
  *q0 = OD_ADD(t0, u4);
  /* We swap q1 and q2 to correct for the bitreverse reordering that
     od_row_tx4_avx2() does. */
  *q2 = OD_ADD(t1, OD_SUB(t3, u4));
  *q1 = t2;
  *q3 = OD_ADD(t0, OD_SUB(t1, t3h));
}

static INLINE void OD_KERNEL_FUNC(od_flip_idst_vii4)(OD_REG *q0, OD_REG *q1,
                                                     OD_REG *q2, OD_REG *q3) {
  OD_KERNEL_FUNC(od_idst_vii4)(q0, q1, q2, q3);
  OD_SWAP(q0, q3);
  OD_SWAP(q1, q2);
}

static INLINE void OD_KERNEL_FUNC(od_idst4_asym)(OD_REG *q0, OD_REG *q2,
                                                 OD_REG *q1, OD_REG *q3) {
  OD_REG t_;
  OD_REG u_;
  OD_REG q1h;
  OD_REG q3h;
  t_ = OD_AVG(*q1, *q2);
  /* 11585/8192 2*Sin[Pi/4] = 1.4142135623730951 */
  *q1 = OD_ADD(*q2, OD_MULHRS(*q2, (11585 - 8192) << 2));
  /* -46341/32768 = -2*Cos[Pi/4] = -1.4142135623730951 */
  *q2 = OD_SUB(OD_MULHRS(t_, 32768 - 46341), t_);
  *q1 = OD_ADD(*q1, *q2);
  *q1 = OD_ADD(*q1, *q0);
  q1h = OD_RSHIFT1(*q1);
  *q0 = OD_SUB(*q0, q1h);
  *q3 = OD_SUB(*q3, *q2);
  q3h = OD_RSHIFT1(*q3);
  *q2 = OD_ADD(*q2, q3h);
  t_ = OD_ADD(q1h, *q2);
  /* 45451/32768 = Sin[5*Pi/16] + Cos[5*Pi/16] = 1.3870398453221475 */
  u_ = OD_ADD(*q2, OD_MULHRS(*q2, 45451 - 32768));
  /* 9041/32768 = Sin[5*Pi/16] - Cos[5*Pi/16] = 0.27589937928294306 */
  *q2 = OD_MULHRS(*q1, 9041);
  /* 18205/16384 = 2*Cos[5*Pi/16] = 1.1111404660392044 */
  t_ = OD_ADD(t_, OD_MULHRS(t_, (18205 - 16384) << 1));
  *q1 = OD_SUB(OD_RSHIFT1(t_), u_);
  *q2 = OD_ADD(*q2, t_);
  t_ = OD_ADD(*q0, q3h);
  /* 38531/32768 = Sin[7*Pi/16] + Cos[7*Pi/16] = 1.1758756024193586 */
  u_ = OD_ADD(*q0, OD_MULHRS(*q0, 38531 - 32768));
  /* 12873/16384 = Sin[7*Pi/16] - Cos[7*Pi/16] = 0.7856949583871022 */
  *q0 = OD_MULHRS(*q3, 12873 << 1);
  /* 12785/32768 = 2*Cos[7*Pi/16] = 0.3901806440322565 */
  t_ = OD_MULHRS(t_, 12785);
  *q3 = OD_SUB(u_, OD_RSHIFT1(t_));
  *q0 = OD_ADD(*q0, t_);
}

static INLINE void OD_KERNEL_FUNC(od_idct8)(OD_REG *r0, OD_REG *r4, OD_REG *r2,
                                            OD_REG *r6, OD_REG *r1, OD_REG *r5,
                                            OD_REG *r3, OD_REG *r7) {
  OD_REG r1h;
  OD_REG r3h;
  OD_KERNEL_FUNC(od_idst4_asym)(r7, r5, r6, r4);
  OD_KERNEL_FUNC(od_idct4_asym)(r0, r2, r1, &r1h, r3, &r3h);
  *r4 = OD_ADD(*r4, r3h);
  *r3 = OD_SUB(*r3, *r4);
  *r2 = OD_ADD(*r2, OD_RSHIFT1(*r5));
  *r5 = OD_SUB(*r2, *r5);
  *r6 = OD_ADD(*r6, r1h);
  *r1 = OD_SUB(*r1, *r6);
  *r0 = OD_ADD(*r0, OD_RSHIFT1(*r7));
  *r7 = OD_SUB(*r0, *r7);
}

static INLINE void OD_KERNEL_FUNC(od_idst8)(OD_REG *r0, OD_REG *r4, OD_REG *r2,
                                            OD_REG *r6, OD_REG *r1, OD_REG *r5,
                                            OD_REG *r3, OD_REG *r7) {
  OD_REG t_;
  OD_REG u_;
  OD_REG r0h;
  OD_REG r2h;
  OD_REG r5h;
  OD_REG r7h;
  t_ = OD_AVG(*r1, *r6);
  /* 11585/8192 ~= 2*Sin[Pi/4] ~= 1.4142135623730951 */
  *r1 = OD_ADD(*r6, OD_MULHRS(*r6, (11585 - 8192) << 2));
  /* 11585/8192 ~= 2*Cos[Pi/4] ~= 1.4142135623730951 */
  *r6 = OD_ADD(t_, OD_MULHRS(t_, (11585 - 8192) << 2));
  *r1 = OD_SUB(*r1, *r6);
  t_ = OD_HRSUB(*r5, *r2);
  /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */
  u_ = OD_ADD(*r5, OD_MULHRS(*r5, (21407 - 16384) << 1));
  /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */
  *r5 = OD_MULHRS(*r2, 8867 << 1);
  /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */
  t_ = OD_MULHRS(t_, 3135 << 3);
  *r5 = OD_SUB(*r5, t_);
  *r2 = OD_SUB(t_, u_);
  t_ = OD_AVG(*r3, *r4);
  /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */
  u_ = OD_ADD(*r4, OD_MULHRS(*r4, (21407 - 16384) << 1));
  /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */
  *r4 = OD_MULHRS(*r3, 8867 << 1);
  /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */
  t_ = OD_MULHRS(t_, 3135 << 3);
  *r3 = OD_SUB(u_, t_);
  *r4 = OD_ADD(*r4, t_);
  *r7 = OD_ADD(*r7, *r6);
  r7h = OD_RSHIFT1(*r7);
  *r6 = OD_SUB(*r6, r7h);
  *r2 = OD_ADD(*r2, *r4);
  r2h = OD_RSHIFT1(*r2);
  *r4 = OD_SUB(*r4, r2h);
  *r0 = OD_SUB(*r0, *r1);
  r0h = OD_RSHIFT1(*r0);
  *r1 = OD_ADD(*r1, r0h);
  *r5 = OD_ADD(*r5, *r3);
  r5h = OD_RSHIFT1(*r5);
  *r3 = OD_SUB(*r3, r5h);
  *r4 = OD_SUB(*r4, r7h);
  *r7 = OD_ADD(*r7, *r4);
  *r6 = OD_ADD(*r6, r5h);
  *r5 = OD_SUB(*r5, *r6);
  *r3 = OD_ADD(*r3, r0h);
  *r0 = OD_SUB(*r0, *r3);
  *r1 = OD_SUB(*r1, r2h);
  *r2 = OD_ADD(*r2, *r1);
  t_ = OD_ADD(*r0, *r7);
  /* 17911/16384 ~= Sin[15*Pi/32] + Cos[15*Pi/32] ~= 1.0932018670017576 */
  u_ = OD_ADD(*r0, OD_MULHRS(*r0, (17911 - 16384) << 1));
  /* 14699/16384 ~= Sin[15*Pi/32] - Cos[15*Pi/32] ~= 0.8971675863426363 */
  *r0 = OD_MULHRS(*r7, 14699 << 1);
  /* 803/8192 ~= Cos[15*Pi/32] ~= 0.0980171403295606 */
  t_ = OD_MULHRS(t_, 803 << 2);
  *r7 = OD_SUB(u_, t_);
  *r0 = OD_ADD(*r0, t_);
  t_ = OD_SUB(*r1, *r6);
  /* 40869/32768 ~= Sin[13*Pi/32] + Cos[13*Pi/32] ~= 1.247225012986671 */
  u_ = OD_ADD(*r6, OD_MULHRS(*r6, 40869 - 32768));
  /* 21845/32768 ~= Sin[13*Pi/32] - Cos[13*Pi/32] ~= 0.6666556584777465 */
  *r6 = OD_MULHRS(*r1, 21845);
  /* 1189/4096 ~= Cos[13*Pi/32] ~= 0.29028467725446233 */
  t_ = OD_MULHRS(t_, 1189 << 3);
  *r1 = OD_ADD(u_, t_);
  *r6 = OD_ADD(*r6, t_);
  t_ = OD_ADD(*r2, *r5);
  /* 22173/16384 ~= Sin[11*Pi/32] + Cos[11*Pi/32] ~= 1.3533180011743526 */
  u_ = OD_ADD(*r2, OD_MULHRS(*r2, (22173 - 16384) << 1));
  /* 3363/8192 ~= Sin[11*Pi/32] - Cos[11*Pi/32] ~= 0.4105245275223574 */
  *r2 = OD_MULHRS(*r5, 3363 << 2);
  /* 15447/32768 ~= Cos[11*Pi/32] ~= 0.47139673682599764 */
  t_ = OD_MULHRS(t_, 15447);
  *r5 = OD_SUB(u_, t_);
  *r2 = OD_ADD(*r2, t_);
  t_ = OD_SUB(*r3, *r4);
  /* 23059/16384 ~= Sin[9*Pi/32] + Cos[9*Pi/32] ~= 1.4074037375263826 */
  u_ = OD_ADD(*r4, OD_MULHRS(*r4, (23059 - 16384) << 1));
  /* 2271/16384 ~= Sin[9*Pi/32] - Cos[9*Pi/32] ~= 0.1386171691990915 */
  *r4 = OD_MULHRS(*r3, 2271 << 1);
  /* 5197/8192 ~= Cos[9*Pi/32] ~= 0.6343932841636455 */
  t_ = OD_MULHRS(t_, 5197 << 2);
  *r3 = OD_ADD(u_, t_);
  *r4 = OD_ADD(*r4, t_);
}

static INLINE void OD_KERNEL_FUNC(od_flip_idst8)(OD_REG *r0, OD_REG *r4,
                                                 OD_REG *r2, OD_REG *r6,
                                                 OD_REG *r1, OD_REG *r5,
                                                 OD_REG *r3, OD_REG *r7) {
  OD_KERNEL_FUNC(od_idst8)(r0, r4, r2, r6, r1, r5, r3, r7);
  OD_SWAP(r0, r7);
  OD_SWAP(r4, r3);
  OD_SWAP(r2, r5);
  OD_SWAP(r6, r1);
}
