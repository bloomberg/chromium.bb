/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#ifndef AV1_COMMON_X86_AV1_TXFM_SSE2_H_
#define AV1_COMMON_X86_AV1_TXFM_SSE2_H_

#include <emmintrin.h>  // SSE2

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "aom/aom_integer.h"
#include "aom_dsp/x86/transpose_sse2.h"
#include "av1/common/av1_txfm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define pair_set_epi16(a, b)                                            \
  _mm_set_epi16((int16_t)(b), (int16_t)(a), (int16_t)(b), (int16_t)(a), \
                (int16_t)(b), (int16_t)(a), (int16_t)(b), (int16_t)(a))

#define btf_16_sse2(w0, w1, in0, in1, out0, out1) \
  {                                               \
    __m128i t0 = _mm_unpacklo_epi16(in0, in1);    \
    __m128i t1 = _mm_unpackhi_epi16(in0, in1);    \
    __m128i u0 = _mm_madd_epi16(t0, w0);          \
    __m128i u1 = _mm_madd_epi16(t1, w0);          \
    __m128i v0 = _mm_madd_epi16(t0, w1);          \
    __m128i v1 = _mm_madd_epi16(t1, w1);          \
                                                  \
    __m128i a0 = _mm_add_epi32(u0, __rounding);   \
    __m128i a1 = _mm_add_epi32(u1, __rounding);   \
    __m128i b0 = _mm_add_epi32(v0, __rounding);   \
    __m128i b1 = _mm_add_epi32(v1, __rounding);   \
                                                  \
    __m128i c0 = _mm_srai_epi32(a0, cos_bit);     \
    __m128i c1 = _mm_srai_epi32(a1, cos_bit);     \
    __m128i d0 = _mm_srai_epi32(b0, cos_bit);     \
    __m128i d1 = _mm_srai_epi32(b1, cos_bit);     \
                                                  \
    out0 = _mm_packs_epi32(c0, c1);               \
    out1 = _mm_packs_epi32(d0, d1);               \
  }

static INLINE __m128i load_16bit_to_16bit(const int16_t *a) {
  return _mm_load_si128((const __m128i *)a);
}

static INLINE __m128i load_32bit_to_16bit(const int32_t *a) {
  const __m128i a_low = _mm_load_si128((const __m128i *)a);
  return _mm_packs_epi32(a_low, *(const __m128i *)(a + 4));
}

// Store 8 16 bit values. Sign extend the values.
static INLINE void store_16bit_to_32bit(__m128i a, int32_t *b) {
  const __m128i a_lo = _mm_unpacklo_epi16(a, a);
  const __m128i a_hi = _mm_unpackhi_epi16(a, a);
  const __m128i a_1 = _mm_srai_epi32(a_lo, 16);
  const __m128i a_2 = _mm_srai_epi32(a_hi, 16);
  _mm_store_si128((__m128i *)b, a_1);
  _mm_store_si128((__m128i *)(b + 4), a_2);
}

static INLINE void store_rect_16bit_to_32bit(__m128i a, int32_t *b) {
  const __m128i sqrt2_coef = _mm_set1_epi16(NewSqrt2);
  const __m128i rounding = _mm_set1_epi32(1 << (NewSqrt2Bits - 1));
  __m128i a_lo, a_hi;
  a_lo = _mm_unpacklo_epi16(a, _mm_setzero_si128());
  a_hi = _mm_unpackhi_epi16(a, _mm_setzero_si128());
  a_lo = _mm_madd_epi16(a_lo, sqrt2_coef);
  a_hi = _mm_madd_epi16(a_hi, sqrt2_coef);
  a_lo = _mm_add_epi32(a_lo, rounding);
  a_hi = _mm_add_epi32(a_hi, rounding);
  a_lo = _mm_srai_epi32(a_lo, NewSqrt2Bits);
  a_hi = _mm_srai_epi32(a_hi, NewSqrt2Bits);
  _mm_store_si128((__m128i *)b, a_lo);
  _mm_store_si128((__m128i *)(b + 4), a_hi);
}

static INLINE void load_buffer_16bit_to_16bit(const int16_t *in, int stride,
                                              __m128i *out, int out_size) {
  for (int i = 0; i < out_size; ++i) {
    out[i] = load_16bit_to_16bit(in + i * stride);
  }
}

static INLINE void load_buffer_16bit_to_16bit_flip(const int16_t *in,
                                                   int stride, __m128i *out,
                                                   int out_size) {
  for (int i = 0; i < out_size; ++i) {
    out[out_size - i - 1] = load_16bit_to_16bit(in + i * stride);
  }
}

static INLINE void load_buffer_32bit_to_16bit(const int32_t *in, int stride,
                                              __m128i *out, int out_size) {
  for (int i = 0; i < out_size; ++i) {
    out[i] = load_32bit_to_16bit(in + i * stride);
  }
}

static INLINE void load_buffer_32bit_to_16bit_flip(const int32_t *in,
                                                   int stride, __m128i *out,
                                                   int out_size) {
  for (int i = 0; i < out_size; ++i) {
    out[out_size - i - 1] = load_32bit_to_16bit(in + i * stride);
  }
}

static INLINE void store_buffer_16bit_to_32bit_8x8(const __m128i *const in,
                                                   int32_t *const out,
                                                   const int stride) {
  for (int i = 0; i < 8; ++i) {
    store_16bit_to_32bit(in[i], out + i * stride);
  }
}

static INLINE void store_rect_buffer_16bit_to_32bit_8x8(const __m128i *const in,
                                                        int32_t *const out,
                                                        const int stride) {
  for (int i = 0; i < 8; ++i) {
    store_rect_16bit_to_32bit(in[i], out + i * stride);
  }
}

static INLINE void store_buffer_16bit_to_16bit_8x8(const __m128i *in,
                                                   uint16_t *out,
                                                   const int stride) {
  for (int i = 0; i < 8; ++i) {
    _mm_store_si128((__m128i *)(out + i * stride), in[i]);
  }
}

static INLINE void round_shift_16bit(__m128i *in, int size, int bit) {
  if (bit < 0) {
    bit = -bit;
    __m128i rounding = _mm_set1_epi16(1 << (bit - 1));
    for (int i = 0; i < size; ++i) {
      in[i] = _mm_adds_epi16(in[i], rounding);
      in[i] = _mm_srai_epi16(in[i], bit);
    }
  } else if (bit > 0) {
    for (int i = 0; i < size; ++i) {
      in[i] = _mm_slli_epi16(in[i], bit);
    }
  }
}

static INLINE void flip_buf_sse2(__m128i *in, __m128i *out, int size) {
  for (int i = 0; i < size; ++i) {
    out[size - i - 1] = in[i];
  }
}

void av1_lowbd_fwd_txfm2d_8x8_sse2(const int16_t *input, int32_t *output,
                                   int stride, TX_TYPE tx_type, int bd);

void av1_lowbd_fwd_txfm2d_8x16_sse2(const int16_t *input, int32_t *output,
                                    int stride, TX_TYPE tx_type, int bd);

void av1_lowbd_fwd_txfm2d_8x32_sse2(const int16_t *input, int32_t *output,
                                    int stride, TX_TYPE tx_type, int bd);

void av1_lowbd_fwd_txfm2d_16x8_sse2(const int16_t *input, int32_t *output,
                                    int stride, TX_TYPE tx_type, int bd);

void av1_lowbd_fwd_txfm2d_16x16_sse2(const int16_t *input, int32_t *output,
                                     int stride, TX_TYPE tx_type, int bd);

void av1_lowbd_fwd_txfm2d_16x32_sse2(const int16_t *input, int32_t *output,
                                     int stride, TX_TYPE tx_type, int bd);

void av1_lowbd_fwd_txfm2d_32x8_sse2(const int16_t *input, int32_t *output,
                                    int stride, TX_TYPE tx_type, int bd);

void av1_lowbd_fwd_txfm2d_32x16_sse2(const int16_t *input, int32_t *output,
                                     int stride, TX_TYPE tx_type, int bd);

void av1_lowbd_fwd_txfm2d_32x32_sse2(const int16_t *input, int32_t *output,
                                     int stride, TX_TYPE tx_type, int bd);

typedef void (*transform_1d_sse2)(const __m128i *input, __m128i *output,
                                  int8_t cos_bit);

typedef struct {
  transform_1d_sse2 col, row;  // vertical and horizontal
} transform_2d_sse2;

void av1_lowbd_inv_txfm2d_add_8x8_sse2(const int32_t *input, uint8_t *output,
                                       int stride, TX_TYPE tx_type, int bd);

void av1_lowbd_inv_txfm2d_add_16x16_sse2(const int32_t *input, uint8_t *output,
                                         int stride, TX_TYPE tx_type, int bd);

void av1_lowbd_inv_txfm2d_add_32x32_sse2(const int32_t *input, uint8_t *output,
                                         int stride, TX_TYPE tx_type, int bd);

void av1_lowbd_inv_txfm2d_add_8x16_sse2(const int32_t *input, uint8_t *output,
                                        int stride, TX_TYPE tx_type, int bd);

void av1_lowbd_inv_txfm2d_add_16x8_sse2(const int32_t *input, uint8_t *output,
                                        int stride, TX_TYPE tx_type, int bd);

void av1_lowbd_inv_txfm2d_add_16x32_sse2(const int32_t *input, uint8_t *output,
                                         int stride, TX_TYPE tx_type, int bd);

void av1_lowbd_inv_txfm2d_add_32x16_sse2(const int32_t *input, uint8_t *output,
                                         int stride, TX_TYPE tx_type, int bd);

void av1_lowbd_inv_txfm2d_add_8x32_sse2(const int32_t *input, uint8_t *output,
                                        int stride, TX_TYPE tx_type, int bd);

void av1_lowbd_inv_txfm2d_add_32x8_sse2(const int32_t *input, uint8_t *output,
                                        int stride, TX_TYPE tx_type, int bd);

#ifdef __cplusplus
}
#endif  // __cplusplus
#endif  // AV1_COMMON_X86_AV1_TXFM_SSE2_H_
