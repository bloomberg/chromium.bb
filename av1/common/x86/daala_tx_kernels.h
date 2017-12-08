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
   OD_MUL     The function that implements the multiplies
              I.e., (a * b + ((1 << r) >> 1)) >> r, without overflow
   OD_SWAP    The function that swaps two SIMD registers

   See daala_inv_txfm_avx2.c for examples. */

#define OD_KERNEL_FUNC_IMPL(name, kernel, word) name##_##kernel##_##word
#define OD_KERNEL_FUNC_WRAPPER(name, kernel, word) \
  OD_KERNEL_FUNC_IMPL(name, kernel, word)
#define OD_KERNEL_FUNC(name) OD_KERNEL_FUNC_WRAPPER(name, OD_KERNEL, OD_WORD)

static INLINE void OD_KERNEL_FUNC(od_rotate_add)(OD_REG *q0, OD_REG *q1, int c0,
                                                 int r0, int c1, int r1, int c2,
                                                 int r2, int s, int avg) {
  OD_REG t_;
  OD_REG u_;

  if (avg)
    t_ = OD_AVG(*q0, *q1);
  else
    t_ = OD_ADD(*q0, *q1);
  u_ = OD_MUL(*q1, c0, r0);
  *q1 = OD_MUL(*q0, c1, r1);
  t_ = OD_MUL(t_, c2, r2);
  if (s)
    *q0 = OD_SUB(u_, OD_RSHIFT1(t_));
  else
    *q0 = OD_SUB(u_, t_);
  *q1 = OD_ADD(*q1, t_);
}

static INLINE void OD_KERNEL_FUNC(od_rotate_addh)(OD_REG *q0, OD_REG *q1,
                                                  OD_REG *q1h, int c0, int r0,
                                                  int c1, int r1, int c2,
                                                  int r2, int s) {
  OD_REG t_;
  OD_REG u_;

  t_ = OD_ADD(*q0, *q1h);
  u_ = OD_MUL(*q1, c0, r0);
  *q1 = OD_MUL(*q0, c1, r1);
  t_ = OD_MUL(t_, c2, r2);
  *q0 = OD_SUB(u_, t_);
  if (s)
    *q1 = OD_ADD(*q1, OD_RSHIFT1(t_));
  else
    *q1 = OD_ADD(*q1, t_);
}

static INLINE void OD_KERNEL_FUNC(od_rotate_sub)(OD_REG *q0, OD_REG *q1, int c0,
                                                 int r0, int c1, int r1, int c2,
                                                 int r2, int s) {
  OD_REG t_;
  OD_REG u_;

  t_ = OD_SUB(*q0, *q1);
  u_ = OD_MUL(*q1, c0, r0);
  *q1 = OD_MUL(*q0, c1, r1);
  t_ = OD_MUL(t_, c2, r2);
  if (s)
    *q0 = OD_ADD(u_, OD_RSHIFT1(t_));
  else
    *q0 = OD_ADD(u_, t_);
  *q1 = OD_ADD(*q1, t_);
}

static INLINE void OD_KERNEL_FUNC(od_rotate_sub2)(OD_REG *q0, OD_REG *q1,
                                                  int c0, int r0, int c1,
                                                  int r1, int c2, int r2,
                                                  int avg) {
  OD_REG t_;
  OD_REG u_;

  if (avg)
    t_ = OD_HRSUB(*q1, *q0);
  else
    t_ = OD_SUB(*q1, *q0);
  u_ = OD_MUL(*q1, c0, r0);
  *q1 = OD_MUL(*q0, c1, r1);
  t_ = OD_MUL(t_, c2, r2);
  *q0 = OD_SUB(t_, u_);
  *q1 = OD_SUB(*q1, t_);
}

static INLINE void OD_KERNEL_FUNC(od_rotate_subh)(OD_REG *q0, OD_REG *q1,
                                                  OD_REG *q1h, int c0, int r0,
                                                  int c1, int r1, int c2,
                                                  int r2, int s) {
  OD_REG t_;
  OD_REG u_;

  t_ = OD_SUB(*q0, *q1h);
  u_ = OD_MUL(*q1, c0, r0);
  *q1 = OD_MUL(*q0, c1, r1);
  t_ = OD_MUL(t_, c2, r2);
  *q0 = OD_ADD(u_, t_);
  if (s)
    *q1 = OD_ADD(*q1, OD_RSHIFT1(t_));
  else
    *q1 = OD_ADD(*q1, t_);
}

static INLINE void OD_KERNEL_FUNC(od_rotate45)(OD_REG *p0, OD_REG *p1,
                                               int avg) {
  OD_REG t_;
  if (avg)
    t_ = OD_AVG(*p0, *p1);
  else
    t_ = OD_ADD(*p0, *p1);
  /* 11585/8192 ~= 2*Sin[Pi/4] ~= 1.4142135623730951 */
  *p0 = OD_MUL(*p1, 11585, 13);
  /* 11585/8192 ~= 2*Cos[Pi/4] ~= 1.4142135623730951 */
  if (avg)
    *p1 = OD_MUL(t_, 11585, 13);
  else
    *p1 = OD_MUL(t_, 11585, 14);
  *p0 = OD_SUB(*p0, *p1);
}

static INLINE void OD_KERNEL_FUNC(od_butterfly_add)(OD_REG *q0, OD_REG *q1) {
  *q0 = OD_ADD(*q0, OD_RSHIFT1(*q1));
  *q1 = OD_SUB(*q0, *q1);
}

static INLINE void OD_KERNEL_FUNC(od_butterfly_add2)(OD_REG *q0, OD_REG *q1) {
  *q0 = OD_ADD(*q0, OD_RSHIFT1(*q1));
  *q1 = OD_SUB(*q1, *q0);
}

static INLINE void OD_KERNEL_FUNC(od_butterfly_sub2)(OD_REG *q0, OD_REG *q1) {
  *q0 = OD_SUB(*q0, OD_RSHIFT1(*q1));
  *q1 = OD_ADD(*q1, *q0);
}

static INLINE void OD_KERNEL_FUNC(od_butterfly_addh)(OD_REG *q0, OD_REG *q1,
                                                     OD_REG *q1h) {
  *q0 = OD_ADD(*q0, *q1h);
  *q1 = OD_SUB(*q1, *q0);
}

static INLINE void OD_KERNEL_FUNC(od_butterfly_subh)(OD_REG *q0, OD_REG *q1,
                                                     OD_REG *q1h) {
  *q0 = OD_SUB(*q0, *q1h);
  *q1 = OD_ADD(*q1, *q0);
}

static INLINE void OD_KERNEL_FUNC(od_butterfly_v1)(OD_REG *q0, OD_REG *q1,
                                                   OD_REG *q1h) {
  *q1 = OD_SUB(*q0, *q1);
  *q1h = OD_RSHIFT1(*q1);
  *q0 = OD_SUB(*q0, *q1h);
}

static INLINE void OD_KERNEL_FUNC(od_butterfly_v2)(OD_REG *q0, OD_REG *q1,
                                                   OD_REG *q1h) {
  *q1 = OD_SUB(*q1, *q0);
  *q1h = OD_RSHIFT1(*q1);
  *q0 = OD_ADD(*q0, *q1h);
}

static INLINE void OD_KERNEL_FUNC(od_butterfly_v3)(OD_REG *q0, OD_REG *q1,
                                                   OD_REG *q1h) {
  *q1 = OD_ADD(*q0, *q1);
  *q1h = OD_RSHIFT1(*q1);
  *q0 = OD_SUB(*q0, *q1h);
}

static INLINE void OD_KERNEL_FUNC(od_idct2)(OD_REG *p0, OD_REG *p1) {
  OD_KERNEL_FUNC(od_rotate45)(p1, p0, 0);
}

static INLINE void OD_KERNEL_FUNC(od_idst2)(OD_REG *p0, OD_REG *p1, int neg) {
  // Note: special case of rotation
  OD_REG t_;
  OD_REG u_;
  if (neg)
    t_ = OD_HRSUB(*p0, *p1);
  else
    t_ = OD_AVG(*p0, *p1);
  /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */
  u_ = OD_MUL(*p0, 21407, 14);
  /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.541196100146197 */
  *p0 = OD_MUL(*p1, 8867, 14);
  /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */
  t_ = OD_MUL(t_, 3135, 12);
  if (neg) {
    *p0 = OD_SUB(*p0, t_);
    *p1 = OD_SUB(t_, u_);
  } else {
    *p0 = OD_ADD(*p0, t_);
    *p1 = OD_SUB(u_, t_);
  }
}

static INLINE void OD_KERNEL_FUNC(od_idct2_asym)(OD_REG *p0, OD_REG *p1,
                                                 OD_REG *p1h) {
  OD_KERNEL_FUNC(od_butterfly_v1)(p0, p1, p1h);
}

static INLINE void OD_KERNEL_FUNC(od_idst2_asym)(OD_REG *p0, OD_REG *p1) {
  // Note: special case of rotation
  OD_REG t_;
  OD_REG u_;
  t_ = OD_AVG(*p0, *p1);
  /* 3135/4096 ~= (Cos[Pi/8] - Sin[Pi/8])*Sqrt[2] = 0.7653668647301795 */
  u_ = OD_MUL(*p1, 3135, 12);
  /* 15137/16384 ~= (Cos[Pi/8] + Sin[Pi/8])/Sqrt[2] = 0.9238795325112867 */
  *p1 = OD_MUL(*p0, 15137, 14);
  /* 8867/8192 ~= Cos[3*Pi/8]*2*Sqrt[2] = 1.082392200292394 */
  t_ = OD_MUL(t_, 8867, 13);
  *p0 = OD_ADD(u_, t_);
  *p1 = OD_SUB(*p1, OD_RSHIFT1(t_));
}

static INLINE void OD_KERNEL_FUNC(od_idct4)(OD_REG *q0, OD_REG *q2, OD_REG *q1,
                                            OD_REG *q3) {
  OD_REG q1h;
  OD_KERNEL_FUNC(od_idst2_asym)(q3, q2);
  OD_KERNEL_FUNC(od_idct2_asym)(q0, q1, &q1h);
  OD_KERNEL_FUNC(od_butterfly_addh)(q2, q1, &q1h);
  OD_KERNEL_FUNC(od_butterfly_add)(q0, q3);
}

static INLINE void OD_KERNEL_FUNC(od_idct4_asym)(OD_REG *q0, OD_REG *q2,
                                                 OD_REG *q1, OD_REG *q1h,
                                                 OD_REG *q3, OD_REG *q3h) {
  OD_KERNEL_FUNC(od_idst2)(q3, q2, 0);
  OD_KERNEL_FUNC(od_idct2)(q0, q1);
  OD_KERNEL_FUNC(od_butterfly_v2)(q2, q1, q1h);
  OD_KERNEL_FUNC(od_butterfly_v1)(q0, q3, q3h);
}

static INLINE void OD_KERNEL_FUNC(od_idst_vii4)(OD_REG *q0, OD_REG *q1,
                                                OD_REG *q2, OD_REG *q3) {
  // Note: special case
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
  t0 = OD_MUL(t0, 467, 11);
  /* 7021/16384 ~= 2*Sin[2*Pi/9]/3 ~= 0.428525073124360 */
  t1 = OD_MUL(t1, 7021, 14);
  /* 37837/32768 ~= 4*Sin[3*Pi/9]/3 ~= 1.154700538379252 */
  t2 = OD_MUL(t2, 37837, 15);
  /* 37837/32768 ~= 4*Sin[3*Pi/9]/3 ~= 1.154700538379252 */
  t3 = OD_MUL(t3, 37837, 15);
  /* 21513/32768 ~= 2*Sin[4*Pi/9]/3 ~= 0.656538502008139 */
  t4 = OD_MUL(t4, 21513, 15);
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

static INLINE void OD_KERNEL_FUNC(od_idst4)(OD_REG *q0, OD_REG *q1, OD_REG *q2,
                                            OD_REG *q3) {
  OD_REG q2h;
  OD_REG q3h;
  OD_KERNEL_FUNC(od_rotate45)(q2, q1, 1);
  OD_KERNEL_FUNC(od_butterfly_v3)(q0, q2, &q2h);
  OD_KERNEL_FUNC(od_butterfly_v3)(q1, q3, &q3h);
  /* 16069/16384 ~= (Sin[5*Pi/16] + Cos[5*Pi/16])/Sqrt[2] ~= 0.9807852804032 */
  /* 12785/32768 ~= (Sin[5*Pi/16] - Cos[5*Pi/16])*Sqrt[2] ~= 0.3901806440323 */
  /* 12873/16384 ~= Cos[5*Pi/16]*Sqrt[2] ~= 0.7856949583871021 */
  OD_KERNEL_FUNC(od_rotate_addh)
  (q1, q2, &q2h, 16069, 14, 12785, 15, 12873, 14, 0);
  /* 13623/16384 ~= (Sin[7*Pi/16] + Cos[7*Pi/16])/Sqrt[2] ~= 0.8314696123025 */
  /* 18205/16384 ~= (Sin[7*Pi/16] - Cos[7*Pi/16])*Sqrt[2] ~= 1.1111404660392 */
  /* 9041/32768 ~= Cos[7*Pi/16]*Sqrt[2] = 0.275899379282943 */
  OD_KERNEL_FUNC(od_rotate_subh)
  (q0, q3, &q3h, 13623, 14, 18205, 14, 9041, 15, 0);
}

static INLINE void OD_KERNEL_FUNC(od_idst4_asym)(OD_REG *q0, OD_REG *q2,
                                                 OD_REG *q1, OD_REG *q3) {
  OD_REG q1h;
  OD_REG q3h;
  OD_KERNEL_FUNC(od_rotate45)(q1, q2, 1);
  OD_KERNEL_FUNC(od_butterfly_v3)(q0, q1, &q1h);
  OD_KERNEL_FUNC(od_butterfly_v3)(q2, q3, &q3h);
  /* 45451/32768 ~= Sin[5*Pi/16] + Cos[5*Pi/16] ~= 1.3870398453221475 */
  /* 9041/32768 ~= Sin[5*Pi/16] - Cos[5*Pi/16] ~= 0.27589937928294306 */
  /* 18205/16384 ~= 2*Cos[5*Pi/16] ~= 1.1111404660392044 */
  OD_KERNEL_FUNC(od_rotate_addh)
  (q2, q1, &q1h, 45451, 15, 9041, 15, 18205, 14, 1);
  /* 38531/32768 ~= Sin[7*Pi/16] + Cos[7*Pi/16] = 1.1758756024193586 */
  /* 12873/16384 ~= Sin[7*Pi/16] - Cos[7*Pi/16] = 0.7856949583871022 */
  /* 12785/32768 ~= 2*Cos[7*Pi/16] = 0.3901806440322565 */
  OD_KERNEL_FUNC(od_rotate_subh)
  (q0, q3, &q3h, 38531, 15, 12873, 14, 12785, 15, 1);
}

static INLINE void OD_KERNEL_FUNC(od_idct8)(OD_REG *r0, OD_REG *r4, OD_REG *r2,
                                            OD_REG *r6, OD_REG *r1, OD_REG *r5,
                                            OD_REG *r3, OD_REG *r7) {
  OD_REG r1h;
  OD_REG r3h;
  OD_KERNEL_FUNC(od_idst4_asym)(r7, r5, r6, r4);
  OD_KERNEL_FUNC(od_idct4_asym)(r0, r2, r1, &r1h, r3, &r3h);
  OD_KERNEL_FUNC(od_butterfly_addh)(r4, r3, &r3h);
  OD_KERNEL_FUNC(od_butterfly_add)(r2, r5);
  OD_KERNEL_FUNC(od_butterfly_addh)(r6, r1, &r1h);
  OD_KERNEL_FUNC(od_butterfly_add)(r0, r7);
}

static INLINE void OD_KERNEL_FUNC(od_idct8_asym)(
    OD_REG *r0, OD_REG *r4, OD_REG *r2, OD_REG *r6, OD_REG *r1, OD_REG *r1h,
    OD_REG *r5, OD_REG *r5h, OD_REG *r3, OD_REG *r3h, OD_REG *r7, OD_REG *r7h) {
  OD_KERNEL_FUNC(od_idst4)(r7, r5, r6, r4);
  OD_KERNEL_FUNC(od_idct4)(r0, r2, r1, r3);
  OD_KERNEL_FUNC(od_butterfly_v1)(r0, r7, r7h);
  OD_KERNEL_FUNC(od_butterfly_v2)(r6, r1, r1h);
  OD_KERNEL_FUNC(od_butterfly_v1)(r2, r5, r5h);
  OD_KERNEL_FUNC(od_butterfly_v2)(r4, r3, r3h);
}

static INLINE void OD_KERNEL_FUNC(od_idst8)(OD_REG *r0, OD_REG *r4, OD_REG *r2,
                                            OD_REG *r6, OD_REG *r1, OD_REG *r5,
                                            OD_REG *r3, OD_REG *r7) {
  OD_REG r0h;
  OD_REG r2h;
  OD_REG r5h;
  OD_REG r7h;
  OD_KERNEL_FUNC(od_rotate45)(r1, r6, 1);
  OD_KERNEL_FUNC(od_idst2)(r5, r2, 1);
  OD_KERNEL_FUNC(od_idst2)(r4, r3, 0);
  OD_KERNEL_FUNC(od_butterfly_v3)(r6, r7, &r7h);
  OD_KERNEL_FUNC(od_butterfly_v3)(r4, r2, &r2h);
  OD_KERNEL_FUNC(od_butterfly_v2)(r1, r0, &r0h);
  OD_KERNEL_FUNC(od_butterfly_v3)(r3, r5, &r5h);
  OD_KERNEL_FUNC(od_butterfly_subh)(r4, r7, &r7h);
  OD_KERNEL_FUNC(od_butterfly_addh)(r6, r5, &r5h);
  OD_KERNEL_FUNC(od_butterfly_addh)(r3, r0, &r0h);
  OD_KERNEL_FUNC(od_butterfly_subh)(r1, r2, &r2h);
  /* 17911/16384 ~= Sin[15*Pi/32] + Cos[15*Pi/32] ~= 1.0932018670017576 */
  /* 14699/16384 ~= Sin[15*Pi/32] - Cos[15*Pi/32] ~= 0.8971675863426363 */
  /* 803/8192 ~= Cos[15*Pi/32] ~= 0.0980171403295606 */
  OD_KERNEL_FUNC(od_rotate_add)(r7, r0, 17911, 14, 14699, 14, 803, 13, 0, 0);
  /* 40869/32768 ~= Sin[13*Pi/32] + Cos[13*Pi/32] ~= 1.247225012986671 */
  /* 21845/32768 ~= Sin[13*Pi/32] - Cos[13*Pi/32] ~= 0.6666556584777465 */
  /* 1189/4096 ~= Cos[13*Pi/32] ~= 0.29028467725446233 */
  OD_KERNEL_FUNC(od_rotate_sub)(r1, r6, 40869, 15, 21845, 15, 1189, 12, 0);
  /* 22173/16384 ~= Sin[11*Pi/32] + Cos[11*Pi/32] ~= 1.3533180011743526 */
  /* 3363/8192 ~= Sin[11*Pi/32] - Cos[11*Pi/32] ~= 0.4105245275223574 */
  /* 15447/32768 ~= Cos[11*Pi/32] ~= 0.47139673682599764 */
  OD_KERNEL_FUNC(od_rotate_add)(r5, r2, 22173, 14, 3363, 13, 15447, 15, 0, 0);
  /* 23059/16384 ~= Sin[9*Pi/32] + Cos[9*Pi/32] ~= 1.4074037375263826 */
  /* 2271/16384 ~= Sin[9*Pi/32] - Cos[9*Pi/32] ~= 0.1386171691990915 */
  /* 5197/8192 ~= Cos[9*Pi/32] ~= 0.6343932841636455 */
  OD_KERNEL_FUNC(od_rotate_sub)(r3, r4, 23059, 14, 2271, 14, 5197, 13, 0);
}

static INLINE void OD_KERNEL_FUNC(od_idst8_asym)(OD_REG *r0, OD_REG *r4,
                                                 OD_REG *r2, OD_REG *r6,
                                                 OD_REG *r1, OD_REG *r5,
                                                 OD_REG *r3, OD_REG *r7) {
  OD_REG r0h;
  OD_REG r2h;
  OD_REG r5h;
  OD_REG r7h;
  OD_KERNEL_FUNC(od_rotate45)(r1, r6, 1);
  OD_KERNEL_FUNC(od_idst2)(r5, r2, 1);
  OD_KERNEL_FUNC(od_idst2)(r4, r3, 0);
  OD_KERNEL_FUNC(od_butterfly_v3)(r6, r7, &r7h);
  OD_KERNEL_FUNC(od_butterfly_v3)(r4, r2, &r2h);
  OD_KERNEL_FUNC(od_butterfly_v2)(r1, r0, &r0h);
  OD_KERNEL_FUNC(od_butterfly_v3)(r3, r5, &r5h);
  OD_KERNEL_FUNC(od_butterfly_subh)(r4, r7, &r7h);
  OD_KERNEL_FUNC(od_butterfly_addh)(r6, r5, &r5h);
  OD_KERNEL_FUNC(od_butterfly_addh)(r3, r0, &r0h);
  OD_KERNEL_FUNC(od_butterfly_subh)(r1, r2, &r2h);
  /* 12665/16384 ~= (Sin[15*Pi/32] + Cos[15*Pi/32])/Sqrt[2] ~= 0.77301045336 */
  /* 5197/4096 ~= (Sin[15*Pi/32] - Cos[15*Pi/32])*Sqrt[2] ~= 1.2687865683273 */
  /* 2271/16384 ~= Cos[15*Pi/32]*Sqrt[2] ~= 0.13861716919909148 */
  OD_KERNEL_FUNC(od_rotate_add)(r7, r0, 12665, 14, 5197, 12, 2271, 14, 1, 0);
  /* 28899/32768 ~= (Sin[13*Pi/32] + Cos[13*Pi/32])/Sqrt[2] ~= 0.88192126435 */
  /* 30893/32768 ~= (Sin[13*Pi/32] - Cos[13*Pi/32])*Sqrt[2] ~= 0.94279347365 */
  /* 3363/8192 ~= Cos[13*Pi/32]*Sqrt[2] ~= 0.41052452752235735 */
  OD_KERNEL_FUNC(od_rotate_sub)(r1, r6, 28899, 15, 30893, 15, 3363, 13, 1);
  /* 31357/32768 ~= (Sin[11*Pi/32] + Cos[11*Pi/32])/Sqrt[2] ~= 0.95694033573 */
  /* 1189/2048 ~= (Sin[11*Pi/32] - Cos[11*Pi/32])*Sqrt[2] ~= 0.5805693545089 */
  /* 21845/32768 ~= Cos[11*Pi/32] ~= 0.6666556584777465 */
  OD_KERNEL_FUNC(od_rotate_add)(r5, r2, 31357, 15, 1189, 11, 21845, 15, 1, 0);
  /* 16305/16384 ~= (Sin[9*Pi/32] + Cos[9*Pi/32])/Sqrt[2] ~= 0.9951847266722 */
  /* 803/4096 ~= (Sin[9*Pi/32] - Cos[9*Pi/32])*Sqrt[2] ~= 0.1960342806591213 */
  /* 14699/16384 ~= Cos[9*Pi/32]*Sqrt[2] ~= 0.8971675863426364 */
  OD_KERNEL_FUNC(od_rotate_sub)(r3, r4, 16305, 14, 803, 12, 14699, 14, 1);
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

static INLINE void OD_KERNEL_FUNC(od_idct16)(OD_REG *s0, OD_REG *s8, OD_REG *s4,
                                             OD_REG *sc, OD_REG *s2, OD_REG *sa,
                                             OD_REG *s6, OD_REG *se, OD_REG *s1,
                                             OD_REG *s9, OD_REG *s5, OD_REG *sd,
                                             OD_REG *s3, OD_REG *sb, OD_REG *s7,
                                             OD_REG *sf) {
  OD_REG s1h;
  OD_REG s3h;
  OD_REG s5h;
  OD_REG s7h;
  OD_KERNEL_FUNC(od_idst8_asym)(sf, sb, sd, s9, se, sa, sc, s8);
  OD_KERNEL_FUNC(od_idct8_asym)
  (s0, s4, s2, s6, s1, &s1h, s5, &s5h, s3, &s3h, s7, &s7h);
  OD_KERNEL_FUNC(od_butterfly_addh)(s8, s7, &s7h);
  OD_KERNEL_FUNC(od_butterfly_add)(s6, s9);
  OD_KERNEL_FUNC(od_butterfly_addh)(sa, s5, &s5h);
  OD_KERNEL_FUNC(od_butterfly_add)(s4, sb);
  OD_KERNEL_FUNC(od_butterfly_addh)(sc, s3, &s3h);
  OD_KERNEL_FUNC(od_butterfly_add)(s2, sd);
  OD_KERNEL_FUNC(od_butterfly_addh)(se, s1, &s1h);
  OD_KERNEL_FUNC(od_butterfly_add)(s0, sf);
}

static INLINE void OD_KERNEL_FUNC(od_idst16)(OD_REG *s0, OD_REG *s1, OD_REG *s2,
                                             OD_REG *s3, OD_REG *s4, OD_REG *s5,
                                             OD_REG *s6, OD_REG *s7, OD_REG *s8,
                                             OD_REG *s9, OD_REG *sa, OD_REG *sb,
                                             OD_REG *sc, OD_REG *sd, OD_REG *se,
                                             OD_REG *sf) {
  OD_REG s0h;
  OD_REG s1h;
  OD_REG s2h;
  OD_REG s3h;
  OD_REG s4h;
  OD_REG s5h;
  OD_REG s6h;
  OD_REG s7h;
  OD_REG sbh;
  OD_REG sfh;
  OD_REG h;
  OD_KERNEL_FUNC(od_rotate45)(s9, s6, 1);
  OD_KERNEL_FUNC(od_rotate45)(sa, s5, 1);
  OD_KERNEL_FUNC(od_rotate45)(s8, s7, 1);
  OD_KERNEL_FUNC(od_idst2)(s3, sc, 0);
  OD_KERNEL_FUNC(od_idst2)(sb, s4, 1);
  OD_KERNEL_FUNC(od_butterfly_v3)(s2, sa, &h);
  OD_KERNEL_FUNC(od_butterfly_v2)(sd, s5, &h);
  OD_KERNEL_FUNC(od_butterfly_v2)(s9, s1, &h);
  OD_KERNEL_FUNC(od_butterfly_v3)(s6, se, &h);
  OD_KERNEL_FUNC(od_butterfly_v3)(sc, sb, &sbh);
  OD_KERNEL_FUNC(od_butterfly_v3)(s7, sf, &sfh);
  OD_KERNEL_FUNC(od_butterfly_v2)(s8, s0, &s0h);
  OD_KERNEL_FUNC(od_butterfly_v3)(s3, s4, &s4h);
  /* 38531/32768 ~= Sin[7*Pi/16] + Cos[7*Pi/16] = 1.1758756024193586 */
  /* 12873/16384 ~= Sin[7*Pi/16] - Cos[7*Pi/16] = 0.7856949583871022 */
  /* 6393/32768 ~= Cos[7*Pi/16] = 0.19509032201612825 */
  OD_KERNEL_FUNC(od_rotate_sub2)
  (s2, sd, 38531, 15, 12873, 14, 6393, 15, 0);
  /* 22725/16384 ~= Sin[5*Pi/16] + Cos[5*Pi/16] ~= 1.3870398453221475 */
  /* 9041/32768 ~= Sin[5*Pi/16] - Cos[5*Pi/16] ~= 0.27589937928294306 */
  /* 18205/16384 ~= 2*Cos[5*Pi/16] ~= 1.1111404660392044 */
  OD_KERNEL_FUNC(od_rotate_sub2)
  (sa, s5, 22725, 14, 9041, 15, 18205, 14, 1);
  /* 45451/32768 ~= Sin[5*Pi/16] + Cos[5*Pi/16] ~= 1.3870398453221475 */
  /* 9041/32768 ~= Sin[5*Pi/16] - Cos[5*Pi/16] ~= 0.27589937928294306 */
  /* 18205/32768 ~= Cos[5*Pi/16] ~= 0.5555702330196022 */
  OD_KERNEL_FUNC(od_rotate_add)
  (s6, s9, 45451, 15, 9041, 15, 18205, 15, 0, 0);
  /* 9633/8192 ~= Sin[7*Pi/16] + Cos[7*Pi/16] ~= 1.1758756024193586 */
  /* 12873/16384 ~= Sin[7*Pi/16] - Cos[7*Pi/16] ~= 0.7856949583871022 */
  /* 12785/32768 ~= 2*Cos[7*Pi/16] ~= 0.3901806440322565 */
  OD_KERNEL_FUNC(od_rotate_add)
  (se, s1, 9633, 13, 12873, 14, 12785, 15, 0, 1);
  OD_KERNEL_FUNC(od_butterfly_subh)(s8, s4, &s4h);
  OD_KERNEL_FUNC(od_butterfly_addh)(s7, sb, &sbh);
  OD_KERNEL_FUNC(od_butterfly_subh)(s3, sf, &sfh);
  OD_KERNEL_FUNC(od_butterfly_addh)(sc, s0, &s0h);
  OD_KERNEL_FUNC(od_butterfly_add2)(sd, se);
  OD_KERNEL_FUNC(od_butterfly_add2)(s2, s1);
  OD_KERNEL_FUNC(od_butterfly_sub2)(s6, s5);
  OD_KERNEL_FUNC(od_butterfly_sub2)(s9, sa);
  OD_KERNEL_FUNC(od_butterfly_v2)(se, s0, &s0h);
  OD_KERNEL_FUNC(od_butterfly_v2)(sf, s1, &s1h);
  OD_KERNEL_FUNC(od_butterfly_v3)(sc, s2, &s2h);
  OD_KERNEL_FUNC(od_butterfly_v3)(sd, s3, &s3h);
  OD_KERNEL_FUNC(od_butterfly_v2)(sa, s4, &s4h);
  OD_KERNEL_FUNC(od_butterfly_v2)(sb, s5, &s5h);
  OD_KERNEL_FUNC(od_butterfly_v3)(s8, s6, &s6h);
  OD_KERNEL_FUNC(od_butterfly_v3)(s9, s7, &s7h);
  /* 32729/32768 ~= (Sin[17*Pi/64] + Cos[17*Pi/64])/Sqrt[2] ~= 0.99879545620 */
  /* 201/2048 ~= (Sin[17*Pi/64] - Cos[17*Pi/64])*Sqrt[2] ~= 0.09813534865484 */
  /* 31121/32768 ~= Cos[17*Pi/64]*Sqrt[2] ~= 0.9497277818777543 */
  OD_KERNEL_FUNC(od_rotate_subh)
  (se, s1, &s1h, 32729, 15, 201, 11, 31121, 15, 0);
  /* 32413/32768 ~= (Sin[19*Pi/64] + Cos[19*Pi/64])/Sqrt[2] ~= 0.98917650996 */
  /* 601/2048 ~= (Sin[19*Pi/64] - Cos[19*Pi/64])*Sqrt[2] ~= 0.29346094891072 */
  /* 27605/32768 ~= Cos[19*Pi/64]*Sqrt[2] ~= 0.8424460355094193 */
  OD_KERNEL_FUNC(od_rotate_addh)
  (s9, s6, &s6h, 32413, 15, 601, 11, 27605, 15, 0);
  /* 15893/16384 ~= (Sin[21*Pi/64] + Cos[21*Pi/64])/Sqrt[2] ~= 0.97003125319 */
  /* 3981/8192 ~= (Sin[21*Pi/64] - Cos[21*Pi/64])*Sqrt[2] ~= 0.4859603598065 */
  /* 1489/2048 ~= Cos[21*Pi/64]*Sqrt[2] ~= 0.72705107329128 */
  OD_KERNEL_FUNC(od_rotate_subh)
  (sa, s5, &s5h, 15893, 14, 3981, 13, 1489, 11, 0);
  /* 30853/32768 ~= (Sin[23*Pi/64] + Cos[23*Pi/64])/Sqrt[2] ~= 0.94154406518 */
  /* 11039/16384 ~= (Sin[23*Pi/64] - Cos[23*Pi/64])*Sqrt[2] ~= 0.67377970678 */
  /* 19813/32768 ~= Cos[23*Pi/64]*Sqrt[2] ~= 0.6046542117908008 */
  OD_KERNEL_FUNC(od_rotate_addh)
  (sd, s2, &s2h, 30853, 15, 11039, 14, 19813, 15, 0);
  /* 14811/16384 ~= (Sin[25*Pi/64] + Cos[25*Pi/64])/Sqrt[2] ~= 0.90398929312 */
  /* 7005/8192 ~= (Sin[25*Pi/64] - Cos[25*Pi/64])*Sqrt[2] ~= 0.8551101868606 */
  /* 3903/8192 ~= Cos[25*Pi/64]*Sqrt[2] ~= 0.47643419969316125 */
  OD_KERNEL_FUNC(od_rotate_subh)
  (sc, s3, &s3h, 14811, 14, 7005, 13, 3903, 13, 0);
  /* 14053/16384 ~= (Sin[27*Pi/64] + Cos[27*Pi/64])/Sqrt[2] ~= 0.85772861000 */
  /* 8423/8192 ~= (Sin[27*Pi/64] - Cos[27*Pi/64])*Sqrt[2] ~= 1.0282054883864 */
  /* 2815/8192 ~= Cos[27*Pi/64]*Sqrt[2] ~= 0.34362586580705035 */
  OD_KERNEL_FUNC(od_rotate_addh)
  (sb, s4, &s4h, 14053, 14, 8423, 13, 2815, 13, 0);
  /* 1645/2048 ~= (Sin[29*Pi/64] + Cos[29*Pi/64])/Sqrt[2] ~= 0.8032075314806 */
  /* 305/256 ~= (Sin[29*Pi/64] - Cos[29*Pi/64])*Sqrt[2] ~= 1.191398608984867 */
  /* 425/2048 ~= Cos[29*Pi/64]*Sqrt[2] ~= 0.20750822698821159 */
  OD_KERNEL_FUNC(od_rotate_subh)
  (s8, s7, &s7h, 1645, 11, 305, 8, 425, 11, 0);
  /* 24279/32768 ~= (Sin[31*Pi/64] + Cos[31*Pi/64])/Sqrt[2] ~= 0.74095112535 */
  /* 44011/32768 ~= (Sin[31*Pi/64] - Cos[31*Pi/64])*Sqrt[2] ~= 1.34311790969 */
  /* 1137/16384 ~= Cos[31*Pi/64]*Sqrt[2] ~= 0.06939217050794069 */
  OD_KERNEL_FUNC(od_rotate_addh)
  (sf, s0, &s0h, 24279, 15, 44011, 15, 1137, 14, 0);
}

static INLINE void OD_KERNEL_FUNC(od_flip_idst16)(
    OD_REG *s0, OD_REG *s1, OD_REG *s2, OD_REG *s3, OD_REG *s4, OD_REG *s5,
    OD_REG *s6, OD_REG *s7, OD_REG *s8, OD_REG *s9, OD_REG *sa, OD_REG *sb,
    OD_REG *sc, OD_REG *sd, OD_REG *se, OD_REG *sf) {
  OD_KERNEL_FUNC(od_idst16)
  (s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, sa, sb, sc, sd, se, sf);
  OD_SWAP(s0, sf);
  OD_SWAP(s1, se);
  OD_SWAP(s2, sd);
  OD_SWAP(s3, sc);
  OD_SWAP(s4, sb);
  OD_SWAP(s5, sa);
  OD_SWAP(s6, s9);
  OD_SWAP(s7, s8);
}
