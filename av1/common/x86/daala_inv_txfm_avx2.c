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

#include <tmmintrin.h>
#include "./av1_rtcd.h"
#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "av1/common/daala_tx.h"
#include "av1/common/daala_inv_txfm.h"
#include "av1/common/idct.h"

#if CONFIG_DAALA_TX

static INLINE __m128i od_unbiased_rshift1_epi16(__m128i a) {
  return _mm_srai_epi16(_mm_add_epi16(_mm_srli_epi16(a, 15), a), 1);
}

static INLINE __m128i od_avg_epi16(__m128i a, __m128i b) {
  __m128i sign_bit;
  /*x86 only provides an unsigned PAVGW with a bias (ARM is better here).
    We emulate a signed one by adding an offset to convert to unsigned and
    back. We use XOR instead of addition/subtraction because it dispatches
    better on older processors.*/
  sign_bit = _mm_set1_epi16(0x8000);
  return _mm_xor_si128(
      _mm_avg_epu16(_mm_xor_si128(a, sign_bit), _mm_xor_si128(b, sign_bit)),
      sign_bit);
}

/*Like the above, but does (a - b + 1) >> 1 instead.*/
static INLINE __m128i od_hrsub_epi16(__m128i a, __m128i b) {
  __m128i sign_bit;
  sign_bit = _mm_set1_epi16(0x8000);
  return _mm_xor_si128(
      _mm_avg_epu16(_mm_xor_si128(a, sign_bit), _mm_sub_epi16(sign_bit, b)),
      sign_bit);
}

static INLINE void od_swap_epi16(__m128i *q0, __m128i *q1) {
  __m128i t;
  t = *q0;
  *q0 = *q1;
  *q1 = t;
}

static INLINE __m128i od_mulhrs_epi16(__m128i a, int16_t b) {
  return _mm_mulhrs_epi16(a, _mm_set1_epi16(b));
}

static INLINE __m128i od_hbd_max_epi16(int bd) {
  return _mm_set1_epi16((1 << bd) - 1);
}

static INLINE __m128i od_hbd_clamp_epi16(__m128i a, __m128i max) {
  return _mm_max_epi16(_mm_setzero_si128(), _mm_min_epi16(a, max));
}

/* Loads a 4x4 buffer of 32-bit values into four SSE registers. */
static INLINE void od_load_buffer_4x4_epi32(__m128i *q0, __m128i *q1,
                                            __m128i *q2, __m128i *q3,
                                            const tran_low_t *in) {
  *q0 = _mm_loadu_si128((const __m128i *)in + 0);
  *q1 = _mm_loadu_si128((const __m128i *)in + 1);
  *q2 = _mm_loadu_si128((const __m128i *)in + 2);
  *q3 = _mm_loadu_si128((const __m128i *)in + 3);
}

/* Loads a 4x4 buffer of 16-bit values into four SSE registers. */
static INLINE void od_load_buffer_4x4_epi16(__m128i *q0, __m128i *q1,
                                            __m128i *q2, __m128i *q3,
                                            const int16_t *in) {
  *q0 = _mm_loadu_si128((const __m128i *)in + 0);
  *q1 = _mm_unpackhi_epi64(*q0, *q0);
  *q2 = _mm_loadu_si128((const __m128i *)in + 1);
  *q3 = _mm_unpackhi_epi64(*q2, *q2);
}

/* Loads an 8x4 buffer of 16-bit values into four SSE registers. */
static INLINE void od_load_buffer_8x4_epi16(__m128i *q0, __m128i *q1,
                                            __m128i *q2, __m128i *q3,
                                            const int16_t *in, int in_stride) {
  *q0 = _mm_loadu_si128((const __m128i *)(in + 0 * in_stride));
  *q1 = _mm_loadu_si128((const __m128i *)(in + 1 * in_stride));
  *q2 = _mm_loadu_si128((const __m128i *)(in + 2 * in_stride));
  *q3 = _mm_loadu_si128((const __m128i *)(in + 3 * in_stride));
}

/* Loads an 8x4 buffer of 32-bit values and packs them into 16-bit values in
   four SSE registers. */
static INLINE void od_load_pack_buffer_8x4_epi32(__m128i *r0, __m128i *r1,
                                                 __m128i *r2, __m128i *r3,
                                                 const tran_low_t *in) {
  __m128i r4;
  __m128i r5;
  __m128i r6;
  __m128i r7;
  *r0 = _mm_loadu_si128((const __m128i *)in + 0);
  r4 = _mm_loadu_si128((const __m128i *)in + 1);
  *r1 = _mm_loadu_si128((const __m128i *)in + 2);
  r5 = _mm_loadu_si128((const __m128i *)in + 3);
  *r2 = _mm_loadu_si128((const __m128i *)in + 4);
  r6 = _mm_loadu_si128((const __m128i *)in + 5);
  *r3 = _mm_loadu_si128((const __m128i *)in + 6);
  r7 = _mm_loadu_si128((const __m128i *)in + 7);
  *r0 = _mm_packs_epi32(*r0, r4);
  *r1 = _mm_packs_epi32(*r1, r5);
  *r2 = _mm_packs_epi32(*r2, r6);
  *r3 = _mm_packs_epi32(*r3, r7);
}

/* Stores a 4x4 buffer of 16-bit values from two SSE registers.
   Each register holds two rows of values. */
static INLINE void od_store_buffer_4x4_epi16(int16_t *out, __m128i q0,
                                             __m128i q1) {
  _mm_storeu_si128((__m128i *)out + 0, q0);
  _mm_storeu_si128((__m128i *)out + 1, q1);
}

/* Stores a 4x8 buffer of 16-bit values from four SSE registers.
   Each register holds two rows of values. */
static INLINE void od_store_buffer_4x8_epi16(int16_t *out, __m128i q0,
                                             __m128i q1, __m128i q2,
                                             __m128i q3) {
  _mm_storeu_si128((__m128i *)out + 0, q0);
  _mm_storeu_si128((__m128i *)out + 1, q1);
  _mm_storeu_si128((__m128i *)out + 2, q2);
  _mm_storeu_si128((__m128i *)out + 3, q3);
}

/* Loads a 4x4 buffer of 16-bit values, adds a 4x4 block of 16-bit values to
   them, clamps to high bit depth, and stores the sum back. */
static INLINE void od_add_store_buffer_hbd_4x4_epi16(void *output_pixels,
                                                     int output_stride,
                                                     __m128i q0, __m128i q1,
                                                     __m128i q2, __m128i q3,
                                                     int bd) {
  uint16_t *output_pixels16;
  __m128i p0;
  __m128i p1;
  __m128i p2;
  __m128i p3;
  __m128i max;
  __m128i round;
  int downshift;
  output_pixels16 = CONVERT_TO_SHORTPTR(output_pixels);
  max = od_hbd_max_epi16(bd);
  downshift = TX_COEFF_DEPTH - bd;
  round = _mm_set1_epi16((1 << downshift) >> 1);
  p0 = _mm_loadl_epi64((const __m128i *)(output_pixels16 + 0 * output_stride));
  p1 = _mm_loadl_epi64((const __m128i *)(output_pixels16 + 1 * output_stride));
  p2 = _mm_loadl_epi64((const __m128i *)(output_pixels16 + 2 * output_stride));
  p3 = _mm_loadl_epi64((const __m128i *)(output_pixels16 + 3 * output_stride));
  q0 = _mm_srai_epi16(_mm_add_epi16(q0, round), downshift);
  q1 = _mm_srai_epi16(_mm_add_epi16(q1, round), downshift);
  q2 = _mm_srai_epi16(_mm_add_epi16(q2, round), downshift);
  q3 = _mm_srai_epi16(_mm_add_epi16(q3, round), downshift);
  p0 = od_hbd_clamp_epi16(_mm_add_epi16(p0, q0), max);
  p1 = od_hbd_clamp_epi16(_mm_add_epi16(p1, q1), max);
  p2 = od_hbd_clamp_epi16(_mm_add_epi16(p2, q2), max);
  p3 = od_hbd_clamp_epi16(_mm_add_epi16(p3, q3), max);
  _mm_storel_epi64((__m128i *)(output_pixels16 + 0 * output_stride), p0);
  _mm_storel_epi64((__m128i *)(output_pixels16 + 1 * output_stride), p1);
  _mm_storel_epi64((__m128i *)(output_pixels16 + 2 * output_stride), p2);
  _mm_storel_epi64((__m128i *)(output_pixels16 + 3 * output_stride), p3);
}

/* Loads an 8x4 buffer of 16-bit values, adds a 8x4 block of 16-bit values to
   them, clamps to the high bit depth max, and stores the sum back. */
static INLINE void od_add_store_buffer_hbd_8x4_epi16(void *output_pixels,
                                                     int output_stride,
                                                     __m128i q0, __m128i q1,
                                                     __m128i q2, __m128i q3,
                                                     int bd) {
  uint16_t *output_pixels16;
  __m128i p0;
  __m128i p1;
  __m128i p2;
  __m128i p3;
  __m128i max;
  __m128i round;
  int downshift;
  output_pixels16 = CONVERT_TO_SHORTPTR(output_pixels);
  max = od_hbd_max_epi16(bd);
  downshift = TX_COEFF_DEPTH - bd;
  round = _mm_set1_epi16((1 << downshift) >> 1);
  p0 = _mm_loadu_si128((const __m128i *)(output_pixels16 + 0 * output_stride));
  p1 = _mm_loadu_si128((const __m128i *)(output_pixels16 + 1 * output_stride));
  p2 = _mm_loadu_si128((const __m128i *)(output_pixels16 + 2 * output_stride));
  p3 = _mm_loadu_si128((const __m128i *)(output_pixels16 + 3 * output_stride));
  q0 = _mm_srai_epi16(_mm_add_epi16(q0, round), downshift);
  q1 = _mm_srai_epi16(_mm_add_epi16(q1, round), downshift);
  q2 = _mm_srai_epi16(_mm_add_epi16(q2, round), downshift);
  q3 = _mm_srai_epi16(_mm_add_epi16(q3, round), downshift);
  p0 = od_hbd_clamp_epi16(_mm_add_epi16(p0, q0), max);
  p1 = od_hbd_clamp_epi16(_mm_add_epi16(p1, q1), max);
  p2 = od_hbd_clamp_epi16(_mm_add_epi16(p2, q2), max);
  p3 = od_hbd_clamp_epi16(_mm_add_epi16(p3, q3), max);
  _mm_storeu_si128((__m128i *)(output_pixels16 + 0 * output_stride), p0);
  _mm_storeu_si128((__m128i *)(output_pixels16 + 1 * output_stride), p1);
  _mm_storeu_si128((__m128i *)(output_pixels16 + 2 * output_stride), p2);
  _mm_storeu_si128((__m128i *)(output_pixels16 + 3 * output_stride), p3);
}

static INLINE void od_transpose_pack4x4(__m128i *q0, __m128i *q1, __m128i *q2,
                                        __m128i *q3) {
  __m128i a;
  __m128i b;
  __m128i c;
  __m128i d;
  /* Input:
     q0: q30 q20 q10 q00
     q1: q31 q21 q11 q01
     q2: q32 q22 q12 q02
     q3: q33 q23 q13 q03
  */
  /* a: q32 q22 q12 q02 q30 q20 q10 q00 */
  a = _mm_packs_epi32(*q0, *q2);
  /* b: q33 q23 q13 q03 q31 q21 q11 q01 */
  b = _mm_packs_epi32(*q1, *q3);
  /* c: q31 q30 q21 q20 q11 q10 q01 q00 */
  c = _mm_unpacklo_epi16(a, b);
  /* d: q33 q32 q23 q22 q13 q12 q03 q02 */
  d = _mm_unpackhi_epi16(a, b);
  /* We don't care about the contents of the high half of each register. */
  /* q0: q13 q12 q11 q10 [q03 q02 q01 q00] */
  *q0 = _mm_unpacklo_epi32(c, d);
  /* q1: q13 q12 q11 q10 [q13 q12 q11 q10] */
  *q1 = _mm_unpackhi_epi64(*q0, *q0);
  /* q2: q33 q32 q31 q30 [q23 q22 q21 q20] */
  *q2 = _mm_unpackhi_epi32(c, d);
  /* q3: q33 q32 q31 q30 [q33 q32 q31 q30] */
  *q3 = _mm_unpackhi_epi64(*q2, *q2);
}

static INLINE void od_transpose4x4(__m128i *q0, __m128i q1, __m128i *q2,
                                   __m128i q3) {
  __m128i a;
  __m128i b;
  /* Input:
     q0: ... ... ... ... q30 q20 q10 q00
     q1: ... ... ... ... q31 q21 q11 q01
     q2: ... ... ... ... q32 q22 q12 q02
     q3: ... ... ... ... q33 q23 q13 q03
  */
  /* a: q31 q30 q21 q20 q11 q10 q01 q00 */
  a = _mm_unpacklo_epi16(*q0, q1);
  /* b: q33 q32 q23 q22 q13 q12 q03 q02 */
  b = _mm_unpacklo_epi16(*q2, q3);
  /* q0: q13 q12 q11 q10 | q03 q02 q01 q00 */
  *q0 = _mm_unpacklo_epi32(a, b);
  /* q2: q33 q32 q31 q30 | q23 q22 q21 q20 */
  *q2 = _mm_unpackhi_epi32(a, b);
}

static inline void od_transpose4x8(__m128i *r0, __m128i r1, __m128i *r2,
                                   __m128i r3, __m128i *r4, __m128i r5,
                                   __m128i *r6, __m128i r7) {
  __m128i a;
  __m128i b;
  /* Input:
     q0: ... ... ... ... q30 q20 q10 q00
     q1: ... ... ... ... q31 q21 q11 q01
     q2: ... ... ... ... q32 q22 q12 q02
     q3: ... ... ... ... q33 q23 q13 q03
     q4: ... ... ... ... q34 q24 q14 q04
     q5: ... ... ... ... q35 q25 q15 q05
     q6: ... ... ... ... q36 q26 q16 q06
     q7: ... ... ... ... q37 q27 q17 q07
  */
  /* r0: r13 r12 11 r10 r03 r02 r01 r00
     r2: r33 r32 31 r30 r23 r22 r21 r20 */
  od_transpose4x4(r0, r1, r2, r3);
  /* r4: r17 r16 15 r14 r07 r06 r05 r04
     r6: r37 r36 35 r34 r27 r26 r25 r24 */
  od_transpose4x4(r4, r5, r6, r7);
  a = *r0;
  b = *r2;
  /* r0: r07 r06 r05 r04 r04 r02 r01 r00 */
  *r0 = _mm_unpacklo_epi64(a, *r4);
  /* r2: r17 r16 r15 r14 r14 r12 r11 r10 */
  *r2 = _mm_unpackhi_epi64(a, *r4);
  /* r4: r27 r26 r25 r24 r24 r22 r21 r20 */
  *r4 = _mm_unpacklo_epi64(b, *r6);
  /* r6: r37 r36 r35 r34 r34 r32 r31 r30 */
  *r6 = _mm_unpackhi_epi64(b, *r6);
}

static INLINE void od_transpose8x4(__m128i *q0, __m128i *q1, __m128i *q2,
                                   __m128i *q3) {
  __m128i a;
  __m128i b;
  __m128i c;
  __m128i d;
  /* Input:
     q0: q07 q06 q05 q04 q03 q02 q01 q00
     q1: q17 q16 q15 q14 q13 q12 q11 q10
     q2: q27 q26 q25 q24 q23 q22 q21 q20
     q3: q37 q36 q35 q34 q33 q32 q31 q30
  */
  /* a: q13 q03 q12 q02 q11 q01 q10 q00 */
  a = _mm_unpacklo_epi16(*q0, *q1);
  /* b: q17 q07 q16 q06 q15 q05 q14 q04 */
  b = _mm_unpackhi_epi16(*q0, *q1);
  /* c: q33 q23 q32 q22 q31 q21 q30 q20 */
  c = _mm_unpacklo_epi16(*q2, *q3);
  /* d: q37 q27 q36 q26 q35 q25 q34 q24 */
  d = _mm_unpackhi_epi16(*q2, *q3);
  /* q0: q31 q21 q11 q01 | q30 q20 q10 q00 */
  *q0 = _mm_unpacklo_epi32(a, c);
  /* q1: q33 q23 q13 q03 | q32 q22 q12 q02 */
  *q1 = _mm_unpackhi_epi32(a, c);
  /* q2: q35 q25 q15 q05 | q34 q24 q14 q04 */
  *q2 = _mm_unpacklo_epi32(b, d);
  /* q3: q37 q27 q17 q07 | q36 q26 q16 q06 */
  *q3 = _mm_unpackhi_epi32(b, d);
}

static INLINE void od_transpose_pack4x8(__m128i *q0, __m128i *q1, __m128i *q2,
                                        __m128i *q3, __m128i q4, __m128i q5,
                                        __m128i q6, __m128i q7) {
  __m128i a;
  __m128i b;
  __m128i c;
  __m128i d;
  /* Input:
     q0: q30 q20 q10 q00
     q1: q31 q21 q11 q01
     q2: q32 q22 q12 q02
     q3: q33 q23 q13 q03
     q4: q34 q24 q14 q04
     q5: q35 q25 q15 q05
     q6: q36 q26 q16 q06
     q7: q37 q27 q17 q07
  */
  /* a: q34 q24 q14 q04 q30 q20 q10 q00 */
  a = _mm_packs_epi32(*q0, q4);
  /* b: q35 q25 q15 q05 q31 q21 q11 q01 */
  b = _mm_packs_epi32(*q1, q5);
  /* c: q36 q26 q16 q06 q32 q22 q12 q02 */
  c = _mm_packs_epi32(*q2, q6);
  /* d: q37 q27 q17 q07 q33 q23 q13 q03 */
  d = _mm_packs_epi32(*q3, q7);
  /* a: q13 q12 q11 q10 q03 q02 q01 q00
     b: q33 q32 q31 q30 q33 q22 q21 q20
     c: q53 q52 q51 q50 q43 q42 q41 q40
     d: q73 q72 q71 q70 q63 q62 q61 q60 */
  od_transpose8x4(&a, &b, &c, &d);
  /* q0: q07 q06 q05 q04 q03 q02 q01 q00 */
  *q0 = _mm_unpacklo_epi64(a, c);
  /* q1: q17 q16 q15 q14 q13 q12 q11 q10 */
  *q1 = _mm_unpackhi_epi64(a, c);
  /* q2: q27 q26 q25 q24 q23 q22 q21 q20 */
  *q2 = _mm_unpacklo_epi64(b, d);
  /* q3: q37 q36 q35 q34 q33 q32 q31 q30 */
  *q3 = _mm_unpackhi_epi64(b, d);
}

static INLINE void od_transpose_pack8x4(__m128i *r0, __m128i *r1, __m128i *r2,
                                        __m128i *r3, __m128i *r4, __m128i *r5,
                                        __m128i *r6, __m128i *r7) {
  /* Input:
     r1: r07 r06 r05 r04  r0: r03 r02 r01 r00
     r3: r17 r16 r15 r14  r2: r13 r12 r11 r10
     r5: r27 r26 r25 r24  r4: r23 r22 r21 r20
     r7: r37 r36 r35 r34  r6: r33 r32 r31 r30
  */
  /* r0: r07 r06 r05 r04 r03 r02 r01 r00 */
  *r0 = _mm_packs_epi32(*r0, *r1);
  /* r2: r17 r16 r15 r14 r13 r12 r11 r10 */
  *r2 = _mm_packs_epi32(*r2, *r3);
  /* r4: r27 r26 r25 r24 r23 r22 r21 r20 */
  *r4 = _mm_packs_epi32(*r4, *r5);
  /* r6: r37 r36 r35 r34 r33 r32 r31 r30 */
  *r6 = _mm_packs_epi32(*r6, *r7);
  /* r0: r31 r21 r11 r01 [r30 r20 r10 r00]
     r2: r33 r23 r13 r03 [r32 r22 r12 r02]
     r4: r35 r25 r15 r05 [r34 r24 r14 r04]
     r6: r37 r27 r17 r07 [r36 r26 r16 r06] */
  od_transpose8x4(r0, r2, r4, r6);
  /* We don't care about the contents of the high half of each register. */
  /* r1: r31 r21 r11 r01 [r31 r21 r11 r01] */
  *r1 = _mm_unpackhi_epi64(*r0, *r0);
  /* r3: r33 r23 r13 r03 [r33 r23 r13 r03] */
  *r3 = _mm_unpackhi_epi64(*r2, *r2);
  /* r5: r35 r25 r15 r05 [r35 r25 r15 r05] */
  *r5 = _mm_unpackhi_epi64(*r4, *r4);
  /* r7: r37 r27 r17 r07 [r37 r27 r17 r07] */
  *r7 = _mm_unpackhi_epi64(*r6, *r6);
}

static INLINE void od_transpose8x8(__m128i *r0, __m128i *r1, __m128i *r2,
                                   __m128i *r3, __m128i *r4, __m128i *r5,
                                   __m128i *r6, __m128i *r7) {
  __m128i r8;
  /*8x8 transpose with only 1 temporary register that takes the rows in order
    and returns the columns in order. The compiler's own register allocator
    will probably screw this up, but that's no reason not to pretend we might
    be able to have nice things. This only matters when we port to pre-AVX
    instruction sets without 3-operand instructions.*/
  r8 = *r4;
  *r4 = _mm_unpacklo_epi16(*r4, *r5);
  r8 = _mm_unpackhi_epi16(r8, *r5);
  *r5 = *r0;
  *r0 = _mm_unpacklo_epi16(*r0, *r1);
  *r5 = _mm_unpackhi_epi16(*r5, *r1);
  *r1 = *r6;
  *r6 = _mm_unpacklo_epi16(*r6, *r7);
  *r1 = _mm_unpackhi_epi16(*r1, *r7);
  *r7 = *r2;
  *r2 = _mm_unpackhi_epi16(*r2, *r3);
  *r7 = _mm_unpacklo_epi16(*r7, *r3);
  *r3 = *r0;
  *r0 = _mm_unpacklo_epi32(*r0, *r7);
  *r3 = _mm_unpackhi_epi32(*r3, *r7);
  *r7 = *r5;
  *r5 = _mm_unpacklo_epi32(*r5, *r2);
  *r7 = _mm_unpackhi_epi32(*r7, *r2);
  *r2 = *r4;
  *r4 = _mm_unpackhi_epi32(*r4, *r6);
  *r2 = _mm_unpacklo_epi32(*r2, *r6);
  *r6 = r8;
  r8 = _mm_unpackhi_epi32(r8, *r1);
  *r6 = _mm_unpacklo_epi32(*r6, *r1);
  *r1 = *r0;
  *r0 = _mm_unpacklo_epi64(*r0, *r2);
  *r1 = _mm_unpackhi_epi64(*r1, *r2);
  *r2 = *r3;
  *r3 = _mm_unpackhi_epi64(*r3, *r4);
  *r2 = _mm_unpacklo_epi64(*r2, *r4);
  *r4 = *r5;
  *r5 = _mm_unpackhi_epi64(*r5, *r6);
  *r4 = _mm_unpacklo_epi64(*r4, *r6);
  *r6 = *r7;
  *r7 = _mm_unpackhi_epi64(*r7, r8);
  *r6 = _mm_unpacklo_epi64(*r6, r8);
}

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

static INLINE void od_idst4_kernel8_epi16(__m128i *q0, __m128i *q1, __m128i *q2,
                                          __m128i *q3) {
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

static INLINE void od_flip_idst4_kernel8_epi16(__m128i *q0, __m128i *q1,
                                               __m128i *q2, __m128i *q3) {
  od_idst4_kernel8_epi16(q0, q1, q2, q3);
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

static void od_row_iidtx_avx2(int16_t *out, int coeffs, const tran_low_t *in) {
  int c;
  /* The number of rows and number of columns are both multiples of 4, so the
     total number of coefficients should be a multiple of 16. */
  assert(!(coeffs & 0xF));
  for (c = 0; c < coeffs; c += 16) {
    __m128i q0;
    __m128i q1;
    __m128i q2;
    __m128i q3;
    od_load_buffer_4x4_epi32(&q0, &q1, &q2, &q3, in + c);
    q0 = _mm_packs_epi32(q0, q1);
    q2 = _mm_packs_epi32(q2, q3);
    od_store_buffer_4x4_epi16(out + c, q0, q2);
  }
}

static void od_col_iidtx_add_hbd_avx2(unsigned char *output_pixels,
                                      int output_stride, int rows, int cols,
                                      const int16_t *in, int bd) {
  __m128i q0;
  __m128i q1;
  __m128i q2;
  __m128i q3;
  if (cols <= 4) {
    uint16_t *output_pixels16;
    __m128i p0;
    __m128i p1;
    __m128i p2;
    __m128i p3;
    __m128i max;
    __m128i round;
    int downshift;
    int hr;
    output_pixels16 = CONVERT_TO_SHORTPTR(output_pixels);
    max = od_hbd_max_epi16(bd);
    downshift = TX_COEFF_DEPTH - bd;
    round = _mm_set1_epi16((1 << downshift) >> 1);
    /* Here hr counts half the number of rows, to simplify address calculations
       when loading two rows of coefficients at once. */
    for (hr = 0; 2 * hr < rows; hr += 2) {
      q0 = _mm_loadu_si128((const __m128i *)in + hr + 0);
      q2 = _mm_loadu_si128((const __m128i *)in + hr + 1);
      p0 = _mm_loadl_epi64(
          (const __m128i *)(output_pixels16 + (2 * hr + 0) * output_stride));
      p1 = _mm_loadl_epi64(
          (const __m128i *)(output_pixels16 + (2 * hr + 1) * output_stride));
      p2 = _mm_loadl_epi64(
          (const __m128i *)(output_pixels16 + (2 * hr + 2) * output_stride));
      p3 = _mm_loadl_epi64(
          (const __m128i *)(output_pixels16 + (2 * hr + 3) * output_stride));
      q0 = _mm_srai_epi16(_mm_add_epi16(q0, round), downshift);
      q2 = _mm_srai_epi16(_mm_add_epi16(q2, round), downshift);
      q1 = _mm_unpackhi_epi64(q0, q0);
      q3 = _mm_unpackhi_epi64(q2, q2);
      p0 = od_hbd_clamp_epi16(_mm_add_epi16(p0, q0), max);
      p1 = od_hbd_clamp_epi16(_mm_add_epi16(p1, q1), max);
      p2 = od_hbd_clamp_epi16(_mm_add_epi16(p2, q2), max);
      p3 = od_hbd_clamp_epi16(_mm_add_epi16(p3, q3), max);
      _mm_storel_epi64(
          (__m128i *)(output_pixels16 + (2 * hr + 0) * output_stride), p0);
      _mm_storel_epi64(
          (__m128i *)(output_pixels16 + (2 * hr + 1) * output_stride), p1);
      _mm_storel_epi64(
          (__m128i *)(output_pixels16 + (2 * hr + 2) * output_stride), p2);
      _mm_storel_epi64(
          (__m128i *)(output_pixels16 + (2 * hr + 3) * output_stride), p3);
    }
  } else {
    int r;
    for (r = 0; r < rows; r += 4) {
      int c;
      for (c = 0; c < cols; c += 8) {
        od_load_buffer_8x4_epi16(&q0, &q1, &q2, &q3, in + r * cols + c, cols);
        od_add_store_buffer_hbd_8x4_epi16(output_pixels + r * output_stride + c,
                                          output_stride, q0, q1, q2, q3, bd);
      }
    }
  }
}

typedef void (*od_tx4_kernel8_epi16)(__m128i *q0, __m128i *q2, __m128i *q1,
                                     __m128i *q3);

static void od_row_tx4_avx2(int16_t *out, int rows, const tran_low_t *in,
                            od_tx4_kernel8_epi16 kernel8) {
  __m128i q0;
  __m128i q1;
  __m128i q2;
  __m128i q3;
  if (rows <= 4) {
    od_load_buffer_4x4_epi32(&q0, &q1, &q2, &q3, in);
    /*TODO(any): Merge this transpose with coefficient scanning.*/
    od_transpose_pack4x4(&q0, &q1, &q2, &q3);
    kernel8(&q0, &q1, &q2, &q3);
    od_transpose4x4(&q0, q2, &q1, q3);
    od_store_buffer_4x4_epi16(out, q0, q1);
  } else {
    int r;
    /* Higher row counts require 32-bit precision. */
    assert(rows <= 16);
    for (r = 0; r < rows; r += 8) {
      __m128i q4;
      __m128i q5;
      __m128i q6;
      __m128i q7;
      od_load_buffer_4x4_epi32(&q0, &q1, &q2, &q3, in + 4 * r);
      od_load_buffer_4x4_epi32(&q4, &q5, &q6, &q7, in + 4 * r + 16);
      /*TODO(any): Merge this transpose with coefficient scanning.*/
      od_transpose_pack4x8(&q0, &q1, &q2, &q3, q4, q5, q6, q7);
      kernel8(&q0, &q1, &q2, &q3);
      od_transpose8x4(&q0, &q2, &q1, &q3);
      od_store_buffer_4x8_epi16(out + 4 * r, q0, q2, q1, q3);
    }
  }
}

static void od_col_tx4_add_hbd_avx2(unsigned char *output_pixels,
                                    int output_stride, int cols,
                                    const int16_t *in, int bd,
                                    od_tx4_kernel8_epi16 kernel8) {
  __m128i q0;
  __m128i q1;
  __m128i q2;
  __m128i q3;
  if (cols <= 4) {
    od_load_buffer_4x4_epi16(&q0, &q1, &q2, &q3, in);
    kernel8(&q0, &q1, &q2, &q3);
    od_add_store_buffer_hbd_4x4_epi16(output_pixels, output_stride, q0, q2, q1,
                                      q3, bd);
  } else {
    int c;
    for (c = 0; c < cols; c += 8) {
      od_load_buffer_8x4_epi16(&q0, &q1, &q2, &q3, in + c, cols);
      kernel8(&q0, &q1, &q2, &q3);
      od_add_store_buffer_hbd_8x4_epi16(output_pixels + c, output_stride, q0,
                                        q2, q1, q3, bd);
    }
  }
}

static void od_row_idct4_avx2(int16_t *out, int rows, const tran_low_t *in) {
  od_row_tx4_avx2(out, rows, in, od_idct4_kernel8_epi16);
}

static void od_col_idct4_add_hbd_avx2(unsigned char *output_pixels,
                                      int output_stride, int cols,
                                      const int16_t *in, int bd) {
  od_col_tx4_add_hbd_avx2(output_pixels, output_stride, cols, in, bd,
                          od_idct4_kernel8_epi16);
}

static void od_row_idst4_avx2(int16_t *out, int rows, const tran_low_t *in) {
  od_row_tx4_avx2(out, rows, in, od_idst4_kernel8_epi16);
}

static void od_col_idst4_add_hbd_avx2(unsigned char *output_pixels,
                                      int output_stride, int cols,
                                      const int16_t *in, int bd) {
  od_col_tx4_add_hbd_avx2(output_pixels, output_stride, cols, in, bd,
                          od_idst4_kernel8_epi16);
}

static void od_row_flip_idst4_avx2(int16_t *out, int rows,
                                   const tran_low_t *in) {
  od_row_tx4_avx2(out, rows, in, od_flip_idst4_kernel8_epi16);
}

static void od_col_flip_idst4_add_hbd_avx2(unsigned char *output_pixels,
                                           int output_stride, int cols,
                                           const int16_t *in, int bd) {
  od_col_tx4_add_hbd_avx2(output_pixels, output_stride, cols, in, bd,
                          od_flip_idst4_kernel8_epi16);
}

static void od_row_iidtx4_avx2(int16_t *out, int rows, const tran_low_t *in) {
  od_row_iidtx_avx2(out, rows * 4, in);
}

static void od_col_iidtx4_add_hbd_avx2(unsigned char *output_pixels,
                                       int output_stride, int cols,
                                       const int16_t *in, int bd) {
  od_col_iidtx_add_hbd_avx2(output_pixels, output_stride, 4, cols, in, bd);
}

typedef void (*od_tx8_kernel8_epi16)(__m128i *r0, __m128i *r4, __m128i *r2,
                                     __m128i *r6, __m128i *r1, __m128i *r5,
                                     __m128i *r3, __m128i *r7);

static void od_row_tx8_avx2(int16_t *out, int rows, const tran_low_t *in,
                            od_tx8_kernel8_epi16 kernel8) {
  __m128i r0;
  __m128i r1;
  __m128i r2;
  __m128i r3;
  __m128i r4;
  __m128i r5;
  __m128i r6;
  __m128i r7;
  if (rows <= 4) {
    od_load_buffer_4x4_epi32(&r0, &r1, &r2, &r3, in);
    od_load_buffer_4x4_epi32(&r4, &r5, &r6, &r7, in + 16);
    /*TODO(any): Merge this transpose with coefficient scanning.*/
    od_transpose_pack8x4(&r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7);
    kernel8(&r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7);
    od_transpose4x8(&r0, r4, &r2, r6, &r1, r5, &r3, r7);
    od_store_buffer_4x4_epi16(out, r0, r2);
    od_store_buffer_4x4_epi16(out + 16, r1, r3);
  } else if (rows <= 8) {
    od_load_pack_buffer_8x4_epi32(&r0, &r1, &r2, &r3, in);
    od_load_pack_buffer_8x4_epi32(&r4, &r5, &r6, &r7, in + 32);
    /*TODO(any): Merge this transpose with coefficient scanning.*/
    od_transpose8x8(&r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7);
    kernel8(&r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7);
    od_transpose8x8(&r0, &r4, &r2, &r6, &r1, &r5, &r3, &r7);
    od_store_buffer_4x8_epi16(out, r0, r4, r2, r6);
    od_store_buffer_4x8_epi16(out + 32, r1, r5, r3, r7);
  } else {
    /* Higher row counts require 32-bit precision.
       This will be added when we start adding 16-point transform SIMD. */
    assert(0);
  }
}

static void od_col_tx8_add_hbd_avx2(unsigned char *output_pixels,
                                    int output_stride, int cols,
                                    const int16_t *in, int bd,
                                    od_tx8_kernel8_epi16 kernel8) {
  __m128i r0;
  __m128i r1;
  __m128i r2;
  __m128i r3;
  __m128i r4;
  __m128i r5;
  __m128i r6;
  __m128i r7;
  if (cols <= 4) {
    od_load_buffer_4x4_epi16(&r0, &r1, &r2, &r3, in);
    od_load_buffer_4x4_epi16(&r4, &r5, &r6, &r7, in + 16);
    kernel8(&r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7);
    od_add_store_buffer_hbd_4x4_epi16(output_pixels, output_stride, r0, r4, r2,
                                      r6, bd);
    od_add_store_buffer_hbd_4x4_epi16(output_pixels + 4 * output_stride,
                                      output_stride, r1, r5, r3, r7, bd);
  } else if (cols <= 8) {
    od_load_buffer_8x4_epi16(&r0, &r1, &r2, &r3, in, cols);
    od_load_buffer_8x4_epi16(&r4, &r5, &r6, &r7, in + 32, cols);
    kernel8(&r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7);
    od_add_store_buffer_hbd_8x4_epi16(output_pixels, output_stride, r0, r4, r2,
                                      r6, bd);
    od_add_store_buffer_hbd_8x4_epi16(output_pixels + 4 * output_stride,
                                      output_stride, r1, r5, r3, r7, bd);
  } else {
    /* Higher column counts should use a 16-bit AVX version.
       This will be added when we start adding 16-point transform SIMD. */
    assert(0);
  }
}

static void od_row_idct8_avx2(int16_t *out, int rows, const tran_low_t *in) {
  od_row_tx8_avx2(out, rows, in, od_idct8_kernel8_epi16);
}

static void od_col_idct8_add_hbd_avx2(unsigned char *output_pixels,
                                      int output_stride, int cols,
                                      const int16_t *in, int bd) {
  od_col_tx8_add_hbd_avx2(output_pixels, output_stride, cols, in, bd,
                          od_idct8_kernel8_epi16);
}

static void od_row_idst8_avx2(int16_t *out, int rows, const tran_low_t *in) {
  od_row_tx8_avx2(out, rows, in, od_idst8_kernel8_epi16);
}

static void od_col_idst8_add_hbd_avx2(unsigned char *output_pixels,
                                      int output_stride, int cols,
                                      const int16_t *in, int bd) {
  od_col_tx8_add_hbd_avx2(output_pixels, output_stride, cols, in, bd,
                          od_idst8_kernel8_epi16);
}

static void od_row_flip_idst8_avx2(int16_t *out, int rows,
                                   const tran_low_t *in) {
  od_row_tx8_avx2(out, rows, in, od_flip_idst8_kernel8_epi16);
}

static void od_col_flip_idst8_add_hbd_avx2(unsigned char *output_pixels,
                                           int output_stride, int cols,
                                           const int16_t *in, int bd) {
  od_col_tx8_add_hbd_avx2(output_pixels, output_stride, cols, in, bd,
                          od_flip_idst8_kernel8_epi16);
}

static void od_row_iidtx8_avx2(int16_t *out, int rows, const tran_low_t *in) {
  od_row_iidtx_avx2(out, rows * 8, in);
}

static void od_col_iidtx8_add_hbd_avx2(unsigned char *output_pixels,
                                       int output_stride, int cols,
                                       const int16_t *in, int bd) {
  od_col_iidtx_add_hbd_avx2(output_pixels, output_stride, 8, cols, in, bd);
}

typedef void (*daala_row_itx)(int16_t *out, int rows, const tran_low_t *in);
typedef void (*daala_col_itx_add)(unsigned char *output_pixels,
                                  int output_stride, int cols,
                                  const int16_t *in, int bd);

static const daala_row_itx TX_ROW_MAP[TX_SIZES][TX_TYPES] = {
  // 4-point transforms
  { od_row_idct4_avx2, od_row_idst4_avx2, od_row_flip_idst4_avx2,
    od_row_iidtx4_avx2 },
  // 8-point transforms
  { od_row_idct8_avx2, od_row_idst8_avx2, od_row_flip_idst8_avx2,
    od_row_iidtx8_avx2 },
  // 16-point transforms
  { NULL, NULL, NULL, NULL },
  // 32-point transforms
  { NULL, NULL, NULL, NULL },
#if CONFIG_TX64X64
  // 64-point transforms
  { NULL, NULL, NULL, NULL },
#endif
};

static const daala_col_itx_add TX_COL_MAP[2][TX_SIZES][TX_TYPES] = {
  // Low bit depth output
  {
      // 4-point transforms
      { NULL, NULL, NULL, NULL },
      // 8-point transforms
      { NULL, NULL, NULL, NULL },
      // 16-point transforms
      { NULL, NULL, NULL, NULL },
      // 32-point transforms
      { NULL, NULL, NULL, NULL },
#if CONFIG_TX64X64
      // 64-point transforms
      { NULL, NULL, NULL, NULL },
#endif
  },
  // High bit depth output
  {
      // 4-point transforms
      { od_col_idct4_add_hbd_avx2, od_col_idst4_add_hbd_avx2,
        od_col_flip_idst4_add_hbd_avx2, od_col_iidtx4_add_hbd_avx2 },
      // 8-point transforms
      { od_col_idct8_add_hbd_avx2, od_col_idst8_add_hbd_avx2,
        od_col_flip_idst8_add_hbd_avx2, od_col_iidtx8_add_hbd_avx2 },
      // 16-point transforms
      { NULL, NULL, NULL, NULL },
      // 32-point transforms
      { NULL, NULL, NULL, NULL },
#if CONFIG_TX64X64
      // 64-point transforms
      { NULL, NULL, NULL, NULL },
#endif
  }
};

/* Define this to verify the SIMD against the C versions of the transforms.
   This is intended to be replaced by real unit tests in the future. */
#undef DAALA_TX_VERIFY_SIMD

void daala_inv_txfm_add_avx2(const tran_low_t *input_coeffs,
                             void *output_pixels, int output_stride,
                             TxfmParam *txfm_param) {
  const TX_SIZE tx_size = txfm_param->tx_size;
  const TX_TYPE tx_type = txfm_param->tx_type;
  assert(tx_size <= TX_SIZES_ALL);
  assert(tx_type <= TX_TYPES);

  if (txfm_param->lossless) {
    daala_inv_txfm_add_c(input_coeffs, output_pixels, output_stride,
                         txfm_param);
  } else {
    // General TX case
    assert(sizeof(tran_low_t) == sizeof(od_coeff));
    assert(sizeof(tran_low_t) >= 4);

    // Hook into existing map translation infrastructure to select
    // appropriate TX functions
    const TX_SIZE col_idx = txsize_vert_map[tx_size];
    const TX_SIZE row_idx = txsize_horz_map[tx_size];
    assert(col_idx <= TX_SIZES);
    assert(row_idx <= TX_SIZES);
    assert(vtx_tab[tx_type] <= (int)TX_TYPES_1D);
    assert(htx_tab[tx_type] <= (int)TX_TYPES_1D);
    daala_row_itx row_tx = TX_ROW_MAP[row_idx][htx_tab[tx_type]];
    daala_col_itx_add col_tx =
        TX_COL_MAP[txfm_param->is_hbd][col_idx][vtx_tab[tx_type]];
    int16_t tmpsq[MAX_TX_SQUARE];

    if (row_tx == NULL || col_tx == NULL) {
      daala_inv_txfm_add_c(input_coeffs, output_pixels, output_stride,
                           txfm_param);
    } else {
      const int cols = tx_size_wide[tx_size];
      const int rows = tx_size_high[tx_size];
#if defined(DAALA_TX_VERIFY_SIMD)
      unsigned char out_check_buf8[MAX_TX_SQUARE];
      int16_t out_check_buf16[MAX_TX_SQUARE];
      unsigned char *out_check_buf;
      {
        if (txfm_param->is_hbd) {
          uint16_t *output_pixels16;
          int r;
          output_pixels16 = CONVERT_TO_SHORTPTR(output_pixels);
          for (r = 0; r < rows; r++) {
            memcpy(out_check_buf16 + r * cols,
                   output_pixels16 + r * output_stride,
                   cols * sizeof(*out_check_buf16));
          }
          out_check_buf = CONVERT_TO_BYTEPTR(out_check_buf16);
        } else {
          unsigned char *output_pixels8;
          int r;
          output_pixels8 = (unsigned char *)output_pixels;
          for (r = 0; r < rows; r++) {
            memcpy(out_check_buf8 + r * cols,
                   output_pixels8 + r * output_stride,
                   cols * sizeof(*out_check_buf8));
          }
          out_check_buf = out_check_buf8;
        }
      }
      daala_inv_txfm_add_c(input_coeffs, out_check_buf, cols, txfm_param);
#endif
      // Inverse-transform rows
      row_tx(tmpsq, rows, input_coeffs);
      // Inverse-transform columns and sum with destination
      col_tx(output_pixels, output_stride, cols, tmpsq, txfm_param->bd);
#if defined(DAALA_TX_VERIFY_SIMD)
      {
        if (txfm_param->is_hbd) {
          uint16_t *output_pixels16;
          int r;
          output_pixels16 = CONVERT_TO_SHORTPTR(output_pixels);
          for (r = 0; r < rows; r++) {
            if (memcmp(out_check_buf16 + r * cols,
                       output_pixels16 + r * output_stride,
                       cols * sizeof(*out_check_buf16))) {
              fprintf(stderr, "%s(%i): Inverse %ix%i %i_%i TX SIMD mismatch.\n",
                      __FILE__, __LINE__, rows, cols, vtx_tab[tx_type],
                      htx_tab[tx_type]);
              assert(0);
              exit(EXIT_FAILURE);
            }
          }
        } else {
          unsigned char *output_pixels8;
          int r;
          output_pixels8 = (unsigned char *)output_pixels;
          for (r = 0; r < rows; r++) {
            if (memcmp(out_check_buf8 + r * cols,
                       output_pixels8 + r * output_stride,
                       cols * sizeof(*out_check_buf8))) {
              fprintf(stderr, "%s(%i): Inverse %ix%i %i_%i TX SIMD mismatch.\n",
                      __FILE__, __LINE__, rows, cols, vtx_tab[tx_type],
                      htx_tab[tx_type]);
              assert(0);
              exit(EXIT_FAILURE);
            }
          }
        }
      }
#endif
    }
  }
}

#endif
