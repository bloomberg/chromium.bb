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
#ifndef DAALA_TX_KERNELS_H_
#define DAALA_TX_KERNELS_H_

static INLINE void od_idct2_kernel8_epi16(__m128i *p0, __m128i *p1) {
  __m128i t_;
  t_ = _mm_add_epi16(*p0, *p1);
  /* 11585/8192 ~= 2*Sin[Pi/4] = 1.4142135623730951 */
  *p1 = _mm_add_epi16(*p0, od_mulhrs_epi16(*p0, (11585 - 8192) << 2));
  /* 11585/16384 ~= Cos[Pi/4] = 0.7071067811865475 */
  *p0 = od_mulhrs_epi16(t_, 11585 << 1);
  *p1 = _mm_sub_epi16(*p1, *p0);
}

static INLINE void od_idst2_kernel8_epi16(__m128i *p0, __m128i *p1) {
  __m128i t_;
  t_ = od_avg_epi16(*p0, *p1);
  /* 8867/16384 ~= Cos[3*Pi/8]*Sqrt[2] = 0.541196100146197 */
  *p0 = od_mulhrs_epi16(*p0, 8867 << 1);
  /* 21407/16384 ~= Sin[3*Pi/8]*Sqrt[2] = 1.3065629648763766 */
  *p1 = _mm_add_epi16(*p1, od_mulhrs_epi16(*p1, (21407 - 16384) << 1));
  /* 15137/8192 ~= 2*Cos[Pi/8] = 1.8477590650225735 */
  t_ = _mm_add_epi16(t_, od_mulhrs_epi16(t_, (15137 - 8192) << 2));
  *p0 = _mm_sub_epi16(t_, *p0);
  *p1 = _mm_sub_epi16(t_, *p1);
}

static INLINE void od_idct2_asym_kernel8_epi16(__m128i *p0, __m128i *p1,
                                               __m128i *p1h) {
  *p1 = _mm_sub_epi16(*p0, *p1);
  *p1h = od_unbiased_rshift1_epi16(*p1);
  *p0 = _mm_sub_epi16(*p0, *p1h);
}

static INLINE void od_idst2_asym_kernel8_epi16(__m128i *p0, __m128i *p1) {
  __m128i t_;
  __m128i u_;
  t_ = od_avg_epi16(*p0, *p1);
  /* 3135/4096 ~= (Cos[Pi/8] - Sin[Pi/8])*Sqrt[2] = 0.7653668647301795 */
  u_ = od_mulhrs_epi16(*p1, 3135 << 3);
  /* 15137/16384 ~= (Cos[Pi/8] + Sin[Pi/8])/Sqrt[2] = 0.9238795325112867 */
  *p1 = od_mulhrs_epi16(*p0, 15137 << 1);
  /* 8867/8192 ~= Cos[3*Pi/8]*2*Sqrt[2] = 1.082392200292394 */
  t_ = _mm_add_epi16(t_, od_mulhrs_epi16(t_, (8867 - 8192) << 2));
  *p0 = _mm_add_epi16(u_, t_);
  *p1 = _mm_sub_epi16(*p1, od_unbiased_rshift1_epi16(t_));
}

static INLINE void od_idct4_kernel8_epi16(__m128i *q0, __m128i *q2, __m128i *q1,
                                          __m128i *q3) {
  __m128i q1h;
  od_idst2_asym_kernel8_epi16(q3, q2);
  od_idct2_asym_kernel8_epi16(q0, q1, &q1h);
  *q2 = _mm_add_epi16(*q2, q1h);
  *q1 = _mm_sub_epi16(*q1, *q2);
  *q0 = _mm_add_epi16(*q0, od_unbiased_rshift1_epi16(*q3));
  *q3 = _mm_sub_epi16(*q0, *q3);
}

static INLINE void od_idct4_asym_kernel8_epi16(__m128i *q0, __m128i *q2,
                                               __m128i *q1, __m128i *q1h,
                                               __m128i *q3, __m128i *q3h) {
  od_idst2_kernel8_epi16(q3, q2);
  od_idct2_kernel8_epi16(q0, q1);
  *q1 = _mm_sub_epi16(*q1, *q2);
  *q1h = od_unbiased_rshift1_epi16(*q1);
  *q2 = _mm_add_epi16(*q2, *q1h);
  *q3 = _mm_sub_epi16(*q0, *q3);
  *q3h = od_unbiased_rshift1_epi16(*q3);
  *q0 = _mm_sub_epi16(*q0, *q3h);
}

static INLINE void od_idst_vii4_kernel8_epi16(__m128i *q0, __m128i *q1,
                                              __m128i *q2, __m128i *q3) {
  __m128i t0;
  __m128i t1;
  __m128i t2;
  __m128i t3;
  __m128i t3h;
  __m128i t4;
  __m128i u4;
  t0 = _mm_sub_epi16(*q0, *q3);
  t1 = _mm_add_epi16(*q0, *q2);
  t2 = _mm_add_epi16(*q3, od_hrsub_epi16(t0, *q2));
  t3 = *q1;
  t4 = _mm_add_epi16(*q2, *q3);
  /* 467/2048 ~= 2*Sin[1*Pi/9]/3 ~= 0.228013428883779 */
  t0 = od_mulhrs_epi16(t0, 467 << 4);
  /* 7021/16384 ~= 2*Sin[2*Pi/9]/3 ~= 0.428525073124360 */
  t1 = od_mulhrs_epi16(t1, 7021 << 1);
  /* 37837/32768 ~= 4*Sin[3*Pi/9]/3 ~= 1.154700538379252 */
  t2 = _mm_add_epi16(t2, od_mulhrs_epi16(t2, 37837 - 32768));
  /* 37837/32768 ~= 4*Sin[3*Pi/9]/3 ~= 1.154700538379252 */
  t3 = _mm_add_epi16(t3, od_mulhrs_epi16(t3, 37837 - 32768));
  /* 21513/32768 ~= 2*Sin[4*Pi/9]/3 ~= 0.656538502008139 */
  t4 = od_mulhrs_epi16(t4, 21513);
  t3h = od_unbiased_rshift1_epi16(t3);
  u4 = _mm_add_epi16(t4, t3h);
  *q0 = _mm_add_epi16(t0, u4);
  /* We swap q1 and q2 to correct for the bitreverse reordering that
     od_row_tx4_avx2() does. */
  *q2 = _mm_add_epi16(t1, _mm_sub_epi16(t3, u4));
  *q1 = t2;
  *q3 = _mm_add_epi16(t0, _mm_sub_epi16(t1, t3h));
}

static INLINE void od_flip_idst_vii4_kernel8_epi16(__m128i *q0, __m128i *q1,
                                                   __m128i *q2, __m128i *q3) {
  od_idst_vii4_kernel8_epi16(q0, q1, q2, q3);
  od_swap_epi16(q0, q3);
  od_swap_epi16(q1, q2);
}

static INLINE void od_idst4_asym_kernel8_epi16(__m128i *q0, __m128i *q2,
                                               __m128i *q1, __m128i *q3) {
  __m128i t_;
  __m128i u_;
  __m128i q1h;
  __m128i q3h;
  t_ = od_avg_epi16(*q1, *q2);
  /* 11585/8192 2*Sin[Pi/4] = 1.4142135623730951 */
  *q1 = _mm_add_epi16(*q2, od_mulhrs_epi16(*q2, (11585 - 8192) << 2));
  /* -46341/32768 = -2*Cos[Pi/4] = -1.4142135623730951 */
  *q2 = _mm_sub_epi16(od_mulhrs_epi16(t_, 32768 - 46341), t_);
  *q1 = _mm_add_epi16(*q1, *q2);
  *q1 = _mm_add_epi16(*q1, *q0);
  q1h = od_unbiased_rshift1_epi16(*q1);
  *q0 = _mm_sub_epi16(*q0, q1h);
  *q3 = _mm_sub_epi16(*q3, *q2);
  q3h = od_unbiased_rshift1_epi16(*q3);
  *q2 = _mm_add_epi16(*q2, q3h);
  t_ = _mm_add_epi16(q1h, *q2);
  /* 45451/32768 = Sin[5*Pi/16] + Cos[5*Pi/16] = 1.3870398453221475 */
  u_ = _mm_add_epi16(*q2, od_mulhrs_epi16(*q2, 45451 - 32768));
  /* 9041/32768 = Sin[5*Pi/16] - Cos[5*Pi/16] = 0.27589937928294306 */
  *q2 = od_mulhrs_epi16(*q1, 9041);
  /* 18205/16384 = 2*Cos[5*Pi/16] = 1.1111404660392044 */
  t_ = _mm_add_epi16(t_, od_mulhrs_epi16(t_, (18205 - 16384) << 1));
  *q1 = _mm_sub_epi16(od_unbiased_rshift1_epi16(t_), u_);
  *q2 = _mm_add_epi16(*q2, t_);
  t_ = _mm_add_epi16(*q0, q3h);
  /* 38531/32768 = Sin[7*Pi/16] + Cos[7*Pi/16] = 1.1758756024193586 */
  u_ = _mm_add_epi16(*q0, od_mulhrs_epi16(*q0, 38531 - 32768));
  /* 12873/16384 = Sin[7*Pi/16] - Cos[7*Pi/16] = 0.7856949583871022 */
  *q0 = od_mulhrs_epi16(*q3, 12873 << 1);
  /* 12785/32768 = 2*Cos[7*Pi/16] = 0.3901806440322565 */
  t_ = od_mulhrs_epi16(t_, 12785);
  *q3 = _mm_sub_epi16(u_, od_unbiased_rshift1_epi16(t_));
  *q0 = _mm_add_epi16(*q0, t_);
}

static INLINE void od_idct8_kernel8_epi16(__m128i *r0, __m128i *r4, __m128i *r2,
                                          __m128i *r6, __m128i *r1, __m128i *r5,
                                          __m128i *r3, __m128i *r7) {
  __m128i r1h;
  __m128i r3h;
  od_idst4_asym_kernel8_epi16(r7, r5, r6, r4);
  od_idct4_asym_kernel8_epi16(r0, r2, r1, &r1h, r3, &r3h);
  *r4 = _mm_add_epi16(*r4, r3h);
  *r3 = _mm_sub_epi16(*r3, *r4);
  *r2 = _mm_add_epi16(*r2, od_unbiased_rshift1_epi16(*r5));
  *r5 = _mm_sub_epi16(*r2, *r5);
  *r6 = _mm_add_epi16(*r6, r1h);
  *r1 = _mm_sub_epi16(*r1, *r6);
  *r0 = _mm_add_epi16(*r0, od_unbiased_rshift1_epi16(*r7));
  *r7 = _mm_sub_epi16(*r0, *r7);
}

static INLINE void od_idst8_kernel8_epi16(__m128i *r0, __m128i *r4, __m128i *r2,
                                          __m128i *r6, __m128i *r1, __m128i *r5,
                                          __m128i *r3, __m128i *r7) {
  __m128i t_;
  __m128i u_;
  __m128i r0h;
  __m128i r2h;
  __m128i r5h;
  __m128i r7h;
  t_ = od_avg_epi16(*r1, *r6);
  /* 11585/8192 ~= 2*Sin[Pi/4] ~= 1.4142135623730951 */
  *r1 = _mm_add_epi16(*r6, od_mulhrs_epi16(*r6, (11585 - 8192) << 2));
  /* 11585/8192 ~= 2*Cos[Pi/4] ~= 1.4142135623730951 */
  *r6 = _mm_add_epi16(t_, od_mulhrs_epi16(t_, (11585 - 8192) << 2));
  *r1 = _mm_sub_epi16(*r1, *r6);
  t_ = od_hrsub_epi16(*r5, *r2);
  /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */
  u_ = _mm_add_epi16(*r5, od_mulhrs_epi16(*r5, (21407 - 16384) << 1));
  /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */
  *r5 = od_mulhrs_epi16(*r2, 8867 << 1);
  /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */
  t_ = od_mulhrs_epi16(t_, 3135 << 3);
  *r5 = _mm_sub_epi16(*r5, t_);
  *r2 = _mm_sub_epi16(t_, u_);
  t_ = od_avg_epi16(*r3, *r4);
  /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */
  u_ = _mm_add_epi16(*r4, od_mulhrs_epi16(*r4, (21407 - 16384) << 1));
  /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */
  *r4 = od_mulhrs_epi16(*r3, 8867 << 1);
  /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */
  t_ = od_mulhrs_epi16(t_, 3135 << 3);
  *r3 = _mm_sub_epi16(u_, t_);
  *r4 = _mm_add_epi16(*r4, t_);
  *r7 = _mm_add_epi16(*r7, *r6);
  r7h = od_unbiased_rshift1_epi16(*r7);
  *r6 = _mm_sub_epi16(*r6, r7h);
  *r2 = _mm_add_epi16(*r2, *r4);
  r2h = od_unbiased_rshift1_epi16(*r2);
  *r4 = _mm_sub_epi16(*r4, r2h);
  *r0 = _mm_sub_epi16(*r0, *r1);
  r0h = od_unbiased_rshift1_epi16(*r0);
  *r1 = _mm_add_epi16(*r1, r0h);
  *r5 = _mm_add_epi16(*r5, *r3);
  r5h = od_unbiased_rshift1_epi16(*r5);
  *r3 = _mm_sub_epi16(*r3, r5h);
  *r4 = _mm_sub_epi16(*r4, r7h);
  *r7 = _mm_add_epi16(*r7, *r4);
  *r6 = _mm_add_epi16(*r6, r5h);
  *r5 = _mm_sub_epi16(*r5, *r6);
  *r3 = _mm_add_epi16(*r3, r0h);
  *r0 = _mm_sub_epi16(*r0, *r3);
  *r1 = _mm_sub_epi16(*r1, r2h);
  *r2 = _mm_add_epi16(*r2, *r1);
  t_ = _mm_add_epi16(*r0, *r7);
  /* 17911/16384 ~= Sin[15*Pi/32] + Cos[15*Pi/32] ~= 1.0932018670017576 */
  u_ = _mm_add_epi16(*r0, od_mulhrs_epi16(*r0, (17911 - 16384) << 1));
  /* 14699/16384 ~= Sin[15*Pi/32] - Cos[15*Pi/32] ~= 0.8971675863426363 */
  *r0 = od_mulhrs_epi16(*r7, 14699 << 1);
  /* 803/8192 ~= Cos[15*Pi/32] ~= 0.0980171403295606 */
  t_ = od_mulhrs_epi16(t_, 803 << 2);
  *r7 = _mm_sub_epi16(u_, t_);
  *r0 = _mm_add_epi16(*r0, t_);
  t_ = _mm_sub_epi16(*r1, *r6);
  /* 40869/32768 ~= Sin[13*Pi/32] + Cos[13*Pi/32] ~= 1.247225012986671 */
  u_ = _mm_add_epi16(*r6, od_mulhrs_epi16(*r6, 40869 - 32768));
  /* 21845/32768 ~= Sin[13*Pi/32] - Cos[13*Pi/32] ~= 0.6666556584777465 */
  *r6 = od_mulhrs_epi16(*r1, 21845);
  /* 1189/4096 ~= Cos[13*Pi/32] ~= 0.29028467725446233 */
  t_ = od_mulhrs_epi16(t_, 1189 << 3);
  *r1 = _mm_add_epi16(u_, t_);
  *r6 = _mm_add_epi16(*r6, t_);
  t_ = _mm_add_epi16(*r2, *r5);
  /* 22173/16384 ~= Sin[11*Pi/32] + Cos[11*Pi/32] ~= 1.3533180011743526 */
  u_ = _mm_add_epi16(*r2, od_mulhrs_epi16(*r2, (22173 - 16384) << 1));
  /* 3363/8192 ~= Sin[11*Pi/32] - Cos[11*Pi/32] ~= 0.4105245275223574 */
  *r2 = od_mulhrs_epi16(*r5, 3363 << 2);
  /* 15447/32768 ~= Cos[11*Pi/32] ~= 0.47139673682599764 */
  t_ = od_mulhrs_epi16(t_, 15447);
  *r5 = _mm_sub_epi16(u_, t_);
  *r2 = _mm_add_epi16(*r2, t_);
  t_ = _mm_sub_epi16(*r3, *r4);
  /* 23059/16384 ~= Sin[9*Pi/32] + Cos[9*Pi/32] ~= 1.4074037375263826 */
  u_ = _mm_add_epi16(*r4, od_mulhrs_epi16(*r4, (23059 - 16384) << 1));
  /* 2271/16384 ~= Sin[9*Pi/32] - Cos[9*Pi/32] ~= 0.1386171691990915 */
  *r4 = od_mulhrs_epi16(*r3, 2271 << 1);
  /* 5197/8192 ~= Cos[9*Pi/32] ~= 0.6343932841636455 */
  t_ = od_mulhrs_epi16(t_, 5197 << 2);
  *r3 = _mm_add_epi16(u_, t_);
  *r4 = _mm_add_epi16(*r4, t_);
}

static INLINE void od_flip_idst8_kernel8_epi16(__m128i *r0, __m128i *r4,
                                               __m128i *r2, __m128i *r6,
                                               __m128i *r1, __m128i *r5,
                                               __m128i *r3, __m128i *r7) {
  od_idst8_kernel8_epi16(r0, r4, r2, r6, r1, r5, r3, r7);
  od_swap_epi16(r0, r7);
  od_swap_epi16(r4, r3);
  od_swap_epi16(r2, r5);
  od_swap_epi16(r6, r1);
}

#endif
