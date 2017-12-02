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

static void od_row_idct4_avx2(int16_t *out, int rows, const tran_low_t *in) {
  __m128i q0;
  __m128i q1;
  __m128i q2;
  __m128i q3;
  if (rows <= 4) {
    od_load_buffer_4x4_epi32(&q0, &q1, &q2, &q3, in);
    /*TODO(any): Merge this transpose with coefficient scanning.*/
    od_transpose_pack4x4(&q0, &q1, &q2, &q3);
    od_idct4_kernel8_epi16(&q0, &q1, &q2, &q3);
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
      od_idct4_kernel8_epi16(&q0, &q1, &q2, &q3);
      od_transpose8x4(&q0, &q2, &q1, &q3);
      od_store_buffer_4x8_epi16(out + 4 * r, q0, q2, q1, q3);
    }
  }
}

static void od_col_idct4_add_hbd_avx2(unsigned char *output_pixels,
                                      int output_stride, int cols,
                                      const int16_t *in, int bd) {
  __m128i q0;
  __m128i q1;
  __m128i q2;
  __m128i q3;
  if (cols <= 4) {
    od_load_buffer_4x4_epi16(&q0, &q1, &q2, &q3, in);
    od_idct4_kernel8_epi16(&q0, &q1, &q2, &q3);
    od_add_store_buffer_hbd_4x4_epi16(output_pixels, output_stride, q0, q2, q1,
                                      q3, bd);
  } else {
    int c;
    for (c = 0; c < cols; c += 8) {
      od_load_buffer_8x4_epi16(&q0, &q1, &q2, &q3, in + c, cols);
      od_idct4_kernel8_epi16(&q0, &q1, &q2, &q3);
      od_add_store_buffer_hbd_8x4_epi16(output_pixels + c, output_stride, q0,
                                        q2, q1, q3, bd);
    }
  }
}

typedef void (*daala_row_itx)(int16_t *out, int rows, const tran_low_t *in);
typedef void (*daala_col_itx_add)(unsigned char *output_pixels,
                                  int output_stride, int cols,
                                  const int16_t *in, int bd);

static const daala_row_itx TX_ROW_MAP[TX_SIZES][TX_TYPES] = {
  // 4-point transforms
  { od_row_idct4_avx2, NULL, NULL, NULL },
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
      { od_col_idct4_add_hbd_avx2, NULL, NULL, NULL },
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
