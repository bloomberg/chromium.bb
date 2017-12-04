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
#include <immintrin.h>
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

static INLINE __m256i od_mm256_unbiased_rshift1_epi16(__m256i a) {
  return _mm256_srai_epi16(_mm256_add_epi16(_mm256_srli_epi16(a, 15), a), 1);
}

static INLINE __m256i od_mm256_unbiased_rshift1_epi32(__m256i a) {
  return _mm256_srai_epi32(_mm256_add_epi32(_mm256_srli_epi32(a, 31), a), 1);
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

static INLINE __m256i od_mm256_avg_epi16(__m256i a, __m256i b) {
  __m256i sign_bit;
  sign_bit = _mm256_set1_epi16(0x8000);
  return _mm256_xor_si256(_mm256_avg_epu16(_mm256_xor_si256(a, sign_bit),
                                           _mm256_xor_si256(b, sign_bit)),
                          sign_bit);
}

static INLINE __m256i od_mm256_avg_epi32(__m256i a, __m256i b) {
  __m256i neg1;
  /* It's cheaper to generate -1's than 1's. */
  neg1 = _mm256_set1_epi64x(-1);
  /* There is no corresponding PAVGD, but we are not in danger of overflowing
     a 32-bit register. */
  return _mm256_srai_epi32(_mm256_add_epi32(a, _mm256_sub_epi32(b, neg1)), 1);
}

/*Like the above, but does (a - b + 1) >> 1 instead.*/
static INLINE __m128i od_hrsub_epi16(__m128i a, __m128i b) {
  __m128i sign_bit;
  sign_bit = _mm_set1_epi16(0x8000);
  return _mm_xor_si128(
      _mm_avg_epu16(_mm_xor_si128(a, sign_bit), _mm_sub_epi16(sign_bit, b)),
      sign_bit);
}

static INLINE __m256i od_mm256_hrsub_epi16(__m256i a, __m256i b) {
  __m256i sign_bit;
  sign_bit = _mm256_set1_epi16(0x8000);
  return _mm256_xor_si256(_mm256_avg_epu16(_mm256_xor_si256(a, sign_bit),
                                           _mm256_sub_epi16(sign_bit, b)),
                          sign_bit);
}

static INLINE __m256i od_mm256_hrsub_epi32(__m256i a, __m256i b) {
  __m256i neg1;
  /* It's cheaper to generate -1's than 1's. */
  neg1 = _mm256_set1_epi64x(-1);
  /* There is no corresponding PAVGD, but we are not in danger of overflowing
     a 32-bit register. */
  return _mm256_srai_epi32(_mm256_sub_epi32(a, _mm256_add_epi32(b, neg1)), 1);
}

static INLINE void od_swap_si128(__m128i *q0, __m128i *q1) {
  __m128i t;
  t = *q0;
  *q0 = *q1;
  *q1 = t;
}

static INLINE void od_mm256_swap_si256(__m256i *q0, __m256i *q1) {
  __m256i t;
  t = *q0;
  *q0 = *q1;
  *q1 = t;
}

static INLINE __m128i od_mulhrs_epi16(__m128i a, int16_t b) {
  return _mm_mulhrs_epi16(a, _mm_set1_epi16(b));
}

static INLINE __m128i od_mul_epi16(__m128i a, int32_t b, int r) {
  int32_t b_q15;
  b_q15 = b << (15 - r);
  /* b and r are in all cases compile-time constants, so these branches
     disappear when this function gets inlined. */
  if (b_q15 > 32767) {
    return _mm_add_epi16(a, od_mulhrs_epi16(a, (int16_t)(b_q15 - 32768)));
  } else if (b_q15 < -32767) {
    return _mm_sub_epi16(od_mulhrs_epi16(a, (int16_t)(32768 + b_q15)), a);
  } else {
    return od_mulhrs_epi16(a, b_q15);
  }
}

static INLINE __m256i od_mm256_mulhrs_epi16(__m256i a, int16_t b) {
  return _mm256_mulhrs_epi16(a, _mm256_set1_epi16(b));
}

static INLINE __m256i od_mm256_mul_epi16(__m256i a, int32_t b, int r) {
  int32_t b_q15;
  b_q15 = b << (15 - r);
  /* b and r are in all cases compile-time constants, so these branches
     disappear when this function gets inlined. */
  if (b_q15 > 32767) {
    return _mm256_add_epi16(a,
                            od_mm256_mulhrs_epi16(a, (int16_t)(b_q15 - 32768)));
  } else if (b_q15 < -32767) {
    return _mm256_sub_epi16(od_mm256_mulhrs_epi16(a, (int16_t)(32768 + b_q15)),
                            a);
  } else {
    return od_mm256_mulhrs_epi16(a, b_q15);
  }
}

static INLINE __m256i od_mm256_mul_epi32(__m256i a, int32_t b, int r) {
  __m256i neg1;
  /* It's cheaper to generate -1's than 1's. */
  neg1 = _mm256_set1_epi64x(-1);
  /* There's no 32-bit version of PMULHRSW on x86 like there is on ARM .*/
  a = _mm256_mullo_epi32(a, _mm256_set1_epi32(b));
  a = _mm256_srai_epi32(a, r - 1);
  a = _mm256_sub_epi32(a, neg1);
  return _mm256_srai_epi32(a, 1);
}

static INLINE __m128i od_hbd_max_epi16(int bd) {
  return _mm_set1_epi16((1 << bd) - 1);
}

static INLINE __m256i od_mm256_hbd_max_epi16(int bd) {
  return _mm256_set1_epi16((1 << bd) - 1);
}

static INLINE __m128i od_hbd_clamp_epi16(__m128i a, __m128i max) {
  return _mm_max_epi16(_mm_setzero_si128(), _mm_min_epi16(a, max));
}

static INLINE __m256i od_mm256_hbd_clamp_epi16(__m256i a, __m256i max) {
  return _mm256_max_epi16(_mm256_setzero_si256(), _mm256_min_epi16(a, max));
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

/* Loads an 8x4 buffer of 32-bit values into four AVX registers. */
static INLINE void od_load_buffer_8x4_epi32(__m256i *r0, __m256i *r1,
                                            __m256i *r2, __m256i *r3,
                                            const tran_low_t *in) {
  *r0 = _mm256_loadu_si256((const __m256i *)in + 0);
  *r1 = _mm256_loadu_si256((const __m256i *)in + 1);
  *r2 = _mm256_loadu_si256((const __m256i *)in + 2);
  *r3 = _mm256_loadu_si256((const __m256i *)in + 3);
}

/* Loads a 16x4 buffer of 16-bit values into four AVX registers. */
static INLINE void od_load_buffer_16x4_epi16(__m256i *r0, __m256i *r1,
                                             __m256i *r2, __m256i *r3,
                                             const int16_t *in, int in_stride) {
  *r0 = _mm256_loadu_si256((const __m256i *)(in + 0 * in_stride));
  *r1 = _mm256_loadu_si256((const __m256i *)(in + 1 * in_stride));
  *r2 = _mm256_loadu_si256((const __m256i *)(in + 2 * in_stride));
  *r3 = _mm256_loadu_si256((const __m256i *)(in + 3 * in_stride));
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

static INLINE void od_store_buffer_2x16_epi16(int16_t *out, __m256i r0,
                                              __m256i r1) {
  _mm256_storeu_si256((__m256i *)out + 0, r0);
  _mm256_storeu_si256((__m256i *)out + 1, r1);
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

static INLINE void od_add_store_buffer_hbd_16x4_epi16(void *output_pixels,
                                                      int output_stride,
                                                      __m256i r0, __m256i r1,
                                                      __m256i r2, __m256i r3,
                                                      int bd) {
  uint16_t *output_pixels16;
  __m256i p0;
  __m256i p1;
  __m256i p2;
  __m256i p3;
  __m256i max;
  __m256i round;
  int downshift;
  output_pixels16 = CONVERT_TO_SHORTPTR(output_pixels);
  max = od_mm256_hbd_max_epi16(bd);
  downshift = TX_COEFF_DEPTH - bd;
  round = _mm256_set1_epi16((1 << downshift) >> 1);
  p0 = _mm256_loadu_si256(
      (const __m256i *)(output_pixels16 + 0 * output_stride));
  p1 = _mm256_loadu_si256(
      (const __m256i *)(output_pixels16 + 1 * output_stride));
  p2 = _mm256_loadu_si256(
      (const __m256i *)(output_pixels16 + 2 * output_stride));
  p3 = _mm256_loadu_si256(
      (const __m256i *)(output_pixels16 + 3 * output_stride));
  r0 = _mm256_srai_epi16(_mm256_add_epi16(r0, round), downshift);
  r1 = _mm256_srai_epi16(_mm256_add_epi16(r1, round), downshift);
  r2 = _mm256_srai_epi16(_mm256_add_epi16(r2, round), downshift);
  r3 = _mm256_srai_epi16(_mm256_add_epi16(r3, round), downshift);
  p0 = od_mm256_hbd_clamp_epi16(_mm256_add_epi16(p0, r0), max);
  p1 = od_mm256_hbd_clamp_epi16(_mm256_add_epi16(p1, r1), max);
  p2 = od_mm256_hbd_clamp_epi16(_mm256_add_epi16(p2, r2), max);
  p3 = od_mm256_hbd_clamp_epi16(_mm256_add_epi16(p3, r3), max);
  _mm256_storeu_si256((__m256i *)(output_pixels16 + 0 * output_stride), p0);
  _mm256_storeu_si256((__m256i *)(output_pixels16 + 1 * output_stride), p1);
  _mm256_storeu_si256((__m256i *)(output_pixels16 + 2 * output_stride), p2);
  _mm256_storeu_si256((__m256i *)(output_pixels16 + 3 * output_stride), p3);
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

static INLINE void od_transpose8x8_epi16(__m128i *r0, __m128i *r1, __m128i *r2,
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

static INLINE void od_transpose8x8_epi32(__m256i *r0, __m256i *r1, __m256i *r2,
                                         __m256i *r3, __m256i *r4, __m256i *r5,
                                         __m256i *r6, __m256i *r7) {
  __m256i a;
  __m256i b;
  __m256i c;
  __m256i d;
  __m256i e;
  __m256i f;
  __m256i g;
  __m256i h;
  __m256i x;
  __m256i y;
  a = _mm256_unpacklo_epi32(*r0, *r1);
  b = _mm256_unpacklo_epi32(*r2, *r3);
  c = _mm256_unpackhi_epi32(*r0, *r1);
  d = _mm256_unpackhi_epi32(*r2, *r3);
  e = _mm256_unpacklo_epi32(*r4, *r5);
  f = _mm256_unpacklo_epi32(*r6, *r7);
  g = _mm256_unpackhi_epi32(*r4, *r5);
  h = _mm256_unpackhi_epi32(*r6, *r7);
  x = _mm256_unpacklo_epi64(a, b);
  y = _mm256_unpacklo_epi64(e, f);
  *r0 = _mm256_permute2x128_si256(x, y, 0 | (2 << 4));
  *r4 = _mm256_permute2x128_si256(x, y, 1 | (3 << 4));
  x = _mm256_unpackhi_epi64(a, b);
  y = _mm256_unpackhi_epi64(e, f);
  *r1 = _mm256_permute2x128_si256(x, y, 0 | (2 << 4));
  *r5 = _mm256_permute2x128_si256(x, y, 1 | (3 << 4));
  x = _mm256_unpacklo_epi64(c, d);
  y = _mm256_unpacklo_epi64(g, h);
  *r2 = _mm256_permute2x128_si256(x, y, 0 | (2 << 4));
  *r6 = _mm256_permute2x128_si256(x, y, 1 | (3 << 4));
  x = _mm256_unpackhi_epi64(c, d);
  y = _mm256_unpackhi_epi64(g, h);
  *r3 = _mm256_permute2x128_si256(x, y, 0 | (2 << 4));
  *r7 = _mm256_permute2x128_si256(x, y, 1 | (3 << 4));
}

static INLINE void od_transpose_pack8x8_epi32(__m256i *rr0, __m256i *rr1,
                                              __m256i *rr2, __m256i *rr3,
                                              __m256i rr4, __m256i rr5,
                                              __m256i rr6, __m256i rr7) {
  __m256i a;
  __m256i b;
  __m256i c;
  __m256i d;
  __m256i w;
  __m256i x;
  __m256i y;
  __m256i z;
  /* rr0: r47 r46 r45 r44 r07 r06 r05 r04 | r43 r42 r41 r40 r03 r02 r01 r00 */
  a = _mm256_packs_epi32(*rr0, rr4);
  /* rr1: r57 r56 r55 r54 r17 r16 r15 r14 | r53 r52 r51 r50 r13 r12 r11 r10 */
  b = _mm256_packs_epi32(*rr1, rr5);
  /* rr2: r67 r66 r65 r64 r27 r26 r25 r24 | r63 r62 r61 r60 r23 r22 r21 r20 */
  c = _mm256_packs_epi32(*rr2, rr6);
  /* rr3: r77 r76 r75 r74 r37 r36 r35 r34 | r73 r72 r71 r70 r33 r32 r31 r30 */
  d = _mm256_packs_epi32(*rr3, rr7);
  /* w: r17 r07 r16 r06 r15 r05 r14 r04 | r13 r03 r12 r02 r11 r01 r10 r00 */
  w = _mm256_unpacklo_epi16(a, b);
  /* x: r57 r47 r56 r46 r55 r45 r54 r44 | r53 r43 r52 r42 r51 r41 r50 r40 */
  x = _mm256_unpackhi_epi16(a, b);
  /* y: r37 r27 r36 r26 r35 r25 r34 r24 | r33 r23 r32 r22 r31 r21 r30 r20 */
  y = _mm256_unpacklo_epi16(c, d);
  /* z: r77 r67 r76 r66 r75 r65 r74 r64 | r73 r63 r72 r62 r71 r61 r70 r60 */
  z = _mm256_unpackhi_epi16(c, d);
  /* a: r35 r25 r15 r05 r34 r24 r14 r04 | r31 r21 r11 r01 r30 r20 r10 r00 */
  a = _mm256_unpacklo_epi32(w, y);
  /* b: r77 r67 r57 r47 r76 r66 r56 r46 | r33 r23 r13 r03 r32 r22 r12 r02 */
  b = _mm256_unpackhi_epi32(w, y);
  /* c: r75 r65 r55 r45 r74 r64 r54 r44 | r71 r61 r51 r41 r70 r60 r50 r40 */
  c = _mm256_unpacklo_epi32(x, z);
  /* d: r77 r67 r57 r47 r76 r66 r56 r46 | r73 r63 r53 r43 r72 r62 r52 r42 */
  d = _mm256_unpackhi_epi32(x, z);
  /* w: r74 r64 r54 r44 r34 r24 r14 r04 | r70 r60 r50 r40 r30 r20 r10 r00 */
  w = _mm256_unpacklo_epi64(a, c);
  /* x: r75 r65 r55 r45 r35 r25 r15 r05 | r71 r61 r51 r41 r31 r21 r11 r01 */
  x = _mm256_unpackhi_epi64(a, c);
  /* y: r76 r66 r56 r46 r36 r26 r16 r06 | r72 r62 r52 r42 r32 r22 r12 r02 */
  y = _mm256_unpacklo_epi64(b, d);
  /* z: r77 r67 r57 r47 r37 r27 r17 r07 | r73 r63 r53 r43 r33 r23 r13 r03 */
  z = _mm256_unpackhi_epi64(b, d);
  /* rr0: r71 r61 r51 r41 r31 r21 r11 r01 | r70 r60 r50 r40 r30 r20 r10 r00 */
  *rr0 = _mm256_permute2x128_si256(w, x, 0 | (2 << 4));
  /* rr1: r73 r63 r53 r43 r33 r23 r13 r03 | r72 r62 r52 r42 r32 r22 r12 r02 */
  *rr1 = _mm256_permute2x128_si256(y, z, 0 | (2 << 4));
  /* rr2: r75 r65 r55 r45 r35 r25 r15 r05 r74 r64 r54 r44 r34 r24 r14 r04 */
  *rr2 = _mm256_permute2x128_si256(w, x, 1 | (3 << 4));
  /* rr3: r77 r67 r57 r47 r37 r27 r17 r07 r76 r66 r56 r46 r36 r26 r16 r06 */
  *rr3 = _mm256_permute2x128_si256(y, z, 1 | (3 << 4));
}

#undef OD_KERNEL
#undef OD_WORD
#undef OD_REG
#undef OD_ADD
#undef OD_SUB
#undef OD_RSHIFT1
#undef OD_AVG
#undef OD_HRSUB
#undef OD_MUL
#undef OD_SWAP

/* Define 8-wide 16-bit SSSE3 kernels. */

#define OD_KERNEL kernel8
#define OD_WORD epi16
#define OD_REG __m128i
#define OD_ADD _mm_add_epi16
#define OD_SUB _mm_sub_epi16
#define OD_RSHIFT1 od_unbiased_rshift1_epi16
#define OD_AVG od_avg_epi16
#define OD_HRSUB od_hrsub_epi16
#define OD_MUL od_mul_epi16
#define OD_SWAP od_swap_si128

#include "av1/common/x86/daala_tx_kernels.h"

#undef OD_KERNEL
#undef OD_REG
#undef OD_ADD
#undef OD_SUB
#undef OD_RSHIFT1
#undef OD_AVG
#undef OD_HRSUB
#undef OD_MUL
#undef OD_SWAP

/* Define 16-wide 16-bit AVX2 kernels. */

#define OD_KERNEL kernel16
#define OD_REG __m256i
#define OD_ADD _mm256_add_epi16
#define OD_SUB _mm256_sub_epi16
#define OD_RSHIFT1 od_mm256_unbiased_rshift1_epi16
#define OD_AVG od_mm256_avg_epi16
#define OD_HRSUB od_mm256_hrsub_epi16
#define OD_MUL od_mm256_mul_epi16
#define OD_SWAP od_mm256_swap_si256

#include "av1/common/x86/daala_tx_kernels.h"  // NOLINT

/* Define 8-wide 32-bit AVX2 kernels. */

#undef OD_KERNEL
#undef OD_WORD
#undef OD_ADD
#undef OD_SUB
#undef OD_RSHIFT1
#undef OD_AVG
#undef OD_HRSUB
#undef OD_MUL

#define OD_KERNEL kernel8
#define OD_WORD epi32
#define OD_ADD _mm256_add_epi32
#define OD_SUB _mm256_sub_epi32
#define OD_RSHIFT1 od_mm256_unbiased_rshift1_epi32
#define OD_AVG od_mm256_avg_epi32
#define OD_HRSUB od_mm256_hrsub_epi32
#define OD_MUL od_mm256_mul_epi32

#include "av1/common/x86/daala_tx_kernels.h"  // NOLINT

static void od_row_iidtx_avx2(int16_t *out, int coeffs, const tran_low_t *in) {
  int c;
  /* The number of rows and number of columns are both multiples of 4, so the
     total number of coefficients should be a multiple of 16. */
  assert(!(coeffs & 0xF));
  /* TODO(any): Use AVX2 for larger block sizes. */
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
      /* TODO(any): Use AVX2 for larger column counts. */
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
  od_row_tx4_avx2(out, rows, in, od_idst_vii4_kernel8_epi16);
}

static void od_col_idst4_add_hbd_avx2(unsigned char *output_pixels,
                                      int output_stride, int cols,
                                      const int16_t *in, int bd) {
  od_col_tx4_add_hbd_avx2(output_pixels, output_stride, cols, in, bd,
                          od_idst_vii4_kernel8_epi16);
}

static void od_row_flip_idst4_avx2(int16_t *out, int rows,
                                   const tran_low_t *in) {
  od_row_tx4_avx2(out, rows, in, od_flip_idst_vii4_kernel8_epi16);
}

static void od_col_flip_idst4_add_hbd_avx2(unsigned char *output_pixels,
                                           int output_stride, int cols,
                                           const int16_t *in, int bd) {
  od_col_tx4_add_hbd_avx2(output_pixels, output_stride, cols, in, bd,
                          od_flip_idst_vii4_kernel8_epi16);
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

typedef void (*od_tx8_mm256_kernel)(__m256i *r0, __m256i *r4, __m256i *r2,
                                    __m256i *r6, __m256i *r1, __m256i *r5,
                                    __m256i *r3, __m256i *r7);

static void od_row_tx8_avx2(int16_t *out, int rows, const tran_low_t *in,
                            od_tx8_kernel8_epi16 kernel8_epi16,
                            od_tx8_mm256_kernel kernel8_epi32) {
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
    kernel8_epi16(&r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7);
    od_transpose4x8(&r0, r4, &r2, r6, &r1, r5, &r3, r7);
    od_store_buffer_4x4_epi16(out, r0, r2);
    od_store_buffer_4x4_epi16(out + 16, r1, r3);
  } else if (rows <= 8) {
    od_load_pack_buffer_8x4_epi32(&r0, &r1, &r2, &r3, in);
    od_load_pack_buffer_8x4_epi32(&r4, &r5, &r6, &r7, in + 32);
    /*TODO(any): Merge this transpose with coefficient scanning.*/
    od_transpose8x8_epi16(&r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7);
    kernel8_epi16(&r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7);
    od_transpose8x8_epi16(&r0, &r4, &r2, &r6, &r1, &r5, &r3, &r7);
    od_store_buffer_4x8_epi16(out, r0, r4, r2, r6);
    od_store_buffer_4x8_epi16(out + 32, r1, r5, r3, r7);
  } else {
    int r;
    /* 16 or more rows requires 32-bit precision.
       TODO(any): If the column TX is IDTX, then we can still use 16 bits. */
    for (r = 0; r < rows; r += 8) {
      __m256i rr0;
      __m256i rr1;
      __m256i rr2;
      __m256i rr3;
      __m256i rr4;
      __m256i rr5;
      __m256i rr6;
      __m256i rr7;
      od_load_buffer_8x4_epi32(&rr0, &rr1, &rr2, &rr3, in + r * 8);
      od_load_buffer_8x4_epi32(&rr4, &rr5, &rr6, &rr7, in + r * 8 + 32);
      od_transpose8x8_epi32(&rr0, &rr1, &rr2, &rr3, &rr4, &rr5, &rr6, &rr7);
      kernel8_epi32(&rr0, &rr1, &rr2, &rr3, &rr4, &rr5, &rr6, &rr7);
      od_transpose_pack8x8_epi32(&rr0, &rr4, &rr2, &rr6, rr1, rr5, rr3, rr7);
      od_store_buffer_2x16_epi16(out + r * 8, rr0, rr4);
      od_store_buffer_2x16_epi16(out + r * 8 + 32, rr2, rr6);
    }
  }
}

static void od_col_tx8_add_hbd_avx2(unsigned char *output_pixels,
                                    int output_stride, int cols,
                                    const int16_t *in, int bd,
                                    od_tx8_kernel8_epi16 kernel8_epi16,
                                    od_tx8_mm256_kernel kernel16_epi16) {
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
    kernel8_epi16(&r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7);
    od_add_store_buffer_hbd_4x4_epi16(output_pixels, output_stride, r0, r4, r2,
                                      r6, bd);
    od_add_store_buffer_hbd_4x4_epi16(output_pixels + 4 * output_stride,
                                      output_stride, r1, r5, r3, r7, bd);
  } else if (cols <= 8) {
    od_load_buffer_8x4_epi16(&r0, &r1, &r2, &r3, in, cols);
    od_load_buffer_8x4_epi16(&r4, &r5, &r6, &r7, in + 32, cols);
    kernel8_epi16(&r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7);
    od_add_store_buffer_hbd_8x4_epi16(output_pixels, output_stride, r0, r4, r2,
                                      r6, bd);
    od_add_store_buffer_hbd_8x4_epi16(output_pixels + 4 * output_stride,
                                      output_stride, r1, r5, r3, r7, bd);
  } else {
    __m256i rr0;
    __m256i rr1;
    __m256i rr2;
    __m256i rr3;
    __m256i rr4;
    __m256i rr5;
    __m256i rr6;
    __m256i rr7;
    int c;
    for (c = 0; c < cols; c += 16) {
      od_load_buffer_16x4_epi16(&rr0, &rr1, &rr2, &rr3, in + c, cols);
      od_load_buffer_16x4_epi16(&rr4, &rr5, &rr6, &rr7, in + 4 * cols + c,
                                cols);
      kernel16_epi16(&rr0, &rr1, &rr2, &rr3, &rr4, &rr5, &rr6, &rr7);
      od_add_store_buffer_hbd_16x4_epi16(output_pixels, output_stride, rr0, rr4,
                                         rr2, rr6, bd);
      od_add_store_buffer_hbd_16x4_epi16(output_pixels + 4 * output_stride,
                                         output_stride, rr1, rr5, rr3, rr7, bd);
    }
  }
}

static void od_row_idct8_avx2(int16_t *out, int rows, const tran_low_t *in) {
  od_row_tx8_avx2(out, rows, in, od_idct8_kernel8_epi16,
                  od_idct8_kernel8_epi32);
}

static void od_col_idct8_add_hbd_avx2(unsigned char *output_pixels,
                                      int output_stride, int cols,
                                      const int16_t *in, int bd) {
  od_col_tx8_add_hbd_avx2(output_pixels, output_stride, cols, in, bd,
                          od_idct8_kernel8_epi16, od_idct8_kernel16_epi16);
}

static void od_row_idst8_avx2(int16_t *out, int rows, const tran_low_t *in) {
  od_row_tx8_avx2(out, rows, in, od_idst8_kernel8_epi16,
                  od_idst8_kernel8_epi32);
}

static void od_col_idst8_add_hbd_avx2(unsigned char *output_pixels,
                                      int output_stride, int cols,
                                      const int16_t *in, int bd) {
  od_col_tx8_add_hbd_avx2(output_pixels, output_stride, cols, in, bd,
                          od_idst8_kernel8_epi16, od_idst8_kernel16_epi16);
}

static void od_row_flip_idst8_avx2(int16_t *out, int rows,
                                   const tran_low_t *in) {
  od_row_tx8_avx2(out, rows, in, od_flip_idst8_kernel8_epi16,
                  od_flip_idst8_kernel8_epi32);
}

static void od_col_flip_idst8_add_hbd_avx2(unsigned char *output_pixels,
                                           int output_stride, int cols,
                                           const int16_t *in, int bd) {
  od_col_tx8_add_hbd_avx2(output_pixels, output_stride, cols, in, bd,
                          od_flip_idst8_kernel8_epi16,
                          od_flip_idst8_kernel16_epi16);
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
