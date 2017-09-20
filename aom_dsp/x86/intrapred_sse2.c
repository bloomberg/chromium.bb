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

#include <emmintrin.h>

#include "./aom_dsp_rtcd.h"

static INLINE void dc_store_4x8(uint32_t dc, uint8_t *dst, ptrdiff_t stride) {
  int i;
  for (i = 0; i < 4; ++i) {
    *(uint32_t *)dst = dc;
    dst += stride;
    *(uint32_t *)dst = dc;
    dst += stride;
  }
}

static INLINE void dc_store_8xh(const __m128i *row, int height, uint8_t *dst,
                                ptrdiff_t stride) {
  int i;
  for (i = 0; i < height; ++i) {
    _mm_storel_epi64((__m128i *)dst, *row);
    dst += stride;
  }
}

static INLINE void dc_store_16xh(const __m128i *row, int height, uint8_t *dst,
                                 ptrdiff_t stride) {
  int i;
  for (i = 0; i < height; ++i) {
    _mm_store_si128((__m128i *)dst, *row);
    dst += stride;
  }
}

static INLINE void dc_store_32xh(const __m128i *row, int height, uint8_t *dst,
                                 ptrdiff_t stride) {
  int i;
  for (i = 0; i < height; ++i) {
    _mm_store_si128((__m128i *)dst, *row);
    _mm_store_si128((__m128i *)(dst + 16), *row);
    dst += stride;
  }
}

static INLINE __m128i dc_sum_4(const uint8_t *ref) {
  __m128i x = _mm_loadl_epi64((__m128i const *)ref);
  const __m128i zero = _mm_setzero_si128();
  x = _mm_unpacklo_epi8(x, zero);
  return _mm_sad_epu8(x, zero);
}

static INLINE __m128i dc_sum_8(const uint8_t *ref) {
  __m128i x = _mm_loadl_epi64((__m128i const *)ref);
  const __m128i zero = _mm_setzero_si128();
  return _mm_sad_epu8(x, zero);
}

static INLINE __m128i dc_sum_16(const uint8_t *ref) {
  __m128i x = _mm_load_si128((__m128i const *)ref);
  const __m128i zero = _mm_setzero_si128();
  x = _mm_sad_epu8(x, zero);
  const __m128i high = _mm_unpackhi_epi64(x, x);
  return _mm_add_epi16(x, high);
}

static INLINE __m128i dc_sum_32(const uint8_t *ref) {
  __m128i x0 = _mm_load_si128((__m128i const *)ref);
  __m128i x1 = _mm_load_si128((__m128i const *)(ref + 16));
  const __m128i zero = _mm_setzero_si128();
  x0 = _mm_sad_epu8(x0, zero);
  x1 = _mm_sad_epu8(x1, zero);
  x0 = _mm_add_epi16(x0, x1);
  const __m128i high = _mm_unpackhi_epi64(x0, x0);
  return _mm_add_epi16(x0, high);
}

// -----------------------------------------------------------------------------
// DC_PRED

void aom_dc_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_8(left);
  __m128i sum_above = dc_sum_4(above);
  sum_above = _mm_add_epi16(sum_left, sum_above);

  uint32_t sum = _mm_cvtsi128_si32(sum_above);
  sum += 6;
  sum /= 12;

  const __m128i row = _mm_set1_epi8((uint8_t)sum);
  const uint32_t pred = _mm_cvtsi128_si32(row);
  dc_store_4x8(pred, dst, stride);
}

void aom_dc_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_4(left);
  __m128i sum_above = dc_sum_8(above);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = _mm_cvtsi128_si32(sum_above);
  sum += 6;
  sum /= 12;

  const __m128i row = _mm_set1_epi8((uint8_t)sum);
  dc_store_8xh(&row, 4, dst, stride);
}

void aom_dc_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_16(left);
  __m128i sum_above = dc_sum_8(above);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = _mm_cvtsi128_si32(sum_above);
  sum += 12;
  sum /= 24;
  const __m128i row = _mm_set1_epi8((uint8_t)sum);
  dc_store_8xh(&row, 16, dst, stride);
}

void aom_dc_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_8(left);
  __m128i sum_above = dc_sum_16(above);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = _mm_cvtsi128_si32(sum_above);
  sum += 12;
  sum /= 24;
  const __m128i row = _mm_set1_epi8((uint8_t)sum);
  dc_store_16xh(&row, 8, dst, stride);
}

void aom_dc_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_32(left);
  __m128i sum_above = dc_sum_16(above);
  sum_above = _mm_add_epi16(sum_left, sum_above);

  uint32_t sum = _mm_cvtsi128_si32(sum_above);
  sum += 24;
  sum /= 48;
  const __m128i row = _mm_set1_epi8((uint8_t)sum);
  dc_store_16xh(&row, 32, dst, stride);
}

void aom_dc_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  __m128i sum_above = dc_sum_32(above);
  const __m128i sum_left = dc_sum_16(left);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = _mm_cvtsi128_si32(sum_above);
  sum += 24;
  sum /= 48;
  const __m128i row = _mm_set1_epi8((uint8_t)sum);
  dc_store_32xh(&row, 16, dst, stride);
}

// -----------------------------------------------------------------------------
// DC_TOP

void aom_dc_top_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_4(above);
  const __m128i two = _mm_set1_epi16((int16_t)2);
  sum_above = _mm_add_epi16(sum_above, two);
  sum_above = _mm_srai_epi16(sum_above, 2);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  sum_above = _mm_packus_epi16(sum_above, sum_above);

  const uint32_t pred = _mm_cvtsi128_si32(sum_above);
  dc_store_4x8(pred, dst, stride);
}

void aom_dc_top_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_8(above);
  const __m128i four = _mm_set1_epi16((uint16_t)4);
  sum_above = _mm_add_epi16(sum_above, four);
  sum_above = _mm_srai_epi16(sum_above, 3);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  const __m128i row = _mm_shufflelo_epi16(sum_above, 0);
  dc_store_8xh(&row, 4, dst, stride);
}

void aom_dc_top_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_8(above);
  const __m128i four = _mm_set1_epi16((uint16_t)4);
  sum_above = _mm_add_epi16(sum_above, four);
  sum_above = _mm_srai_epi16(sum_above, 3);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  const __m128i row = _mm_shufflelo_epi16(sum_above, 0);
  dc_store_8xh(&row, 16, dst, stride);
}

void aom_dc_top_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_16(above);
  const __m128i eight = _mm_set1_epi16((uint16_t)8);
  sum_above = _mm_add_epi16(sum_above, eight);
  sum_above = _mm_srai_epi16(sum_above, 4);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_above, sum_above);
  dc_store_16xh(&row, 8, dst, stride);
}

void aom_dc_top_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_16(above);
  const __m128i eight = _mm_set1_epi16((uint16_t)8);
  sum_above = _mm_add_epi16(sum_above, eight);
  sum_above = _mm_srai_epi16(sum_above, 4);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_above, sum_above);
  dc_store_16xh(&row, 32, dst, stride);
}

void aom_dc_top_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_32(above);
  const __m128i sixteen = _mm_set1_epi16((uint16_t)16);
  sum_above = _mm_add_epi16(sum_above, sixteen);
  sum_above = _mm_srai_epi16(sum_above, 5);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_above, sum_above);
  dc_store_32xh(&row, 16, dst, stride);
}

// -----------------------------------------------------------------------------
// DC_LEFT

void aom_dc_left_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_8(left);
  const __m128i four = _mm_set1_epi16((uint16_t)4);
  sum_left = _mm_add_epi16(sum_left, four);
  sum_left = _mm_srai_epi16(sum_left, 3);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  sum_left = _mm_packus_epi16(sum_left, sum_left);

  const uint32_t pred = _mm_cvtsi128_si32(sum_left);
  dc_store_4x8(pred, dst, stride);
}

void aom_dc_left_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_4(left);
  const __m128i two = _mm_set1_epi16((uint16_t)2);
  sum_left = _mm_add_epi16(sum_left, two);
  sum_left = _mm_srai_epi16(sum_left, 2);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  const __m128i row = _mm_shufflelo_epi16(sum_left, 0);
  dc_store_8xh(&row, 4, dst, stride);
}

void aom_dc_left_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_16(left);
  const __m128i eight = _mm_set1_epi16((uint16_t)8);
  sum_left = _mm_add_epi16(sum_left, eight);
  sum_left = _mm_srai_epi16(sum_left, 4);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  const __m128i row = _mm_shufflelo_epi16(sum_left, 0);
  dc_store_8xh(&row, 16, dst, stride);
}

void aom_dc_left_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_8(left);
  const __m128i four = _mm_set1_epi16((uint16_t)4);
  sum_left = _mm_add_epi16(sum_left, four);
  sum_left = _mm_srai_epi16(sum_left, 3);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_left, sum_left);
  dc_store_16xh(&row, 8, dst, stride);
}

void aom_dc_left_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_32(left);
  const __m128i sixteen = _mm_set1_epi16((uint16_t)16);
  sum_left = _mm_add_epi16(sum_left, sixteen);
  sum_left = _mm_srai_epi16(sum_left, 5);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_left, sum_left);
  dc_store_16xh(&row, 32, dst, stride);
}

void aom_dc_left_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_16(left);
  const __m128i eight = _mm_set1_epi16((uint16_t)8);
  sum_left = _mm_add_epi16(sum_left, eight);
  sum_left = _mm_srai_epi16(sum_left, 4);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_left, sum_left);
  dc_store_32xh(&row, 16, dst, stride);
}

// -----------------------------------------------------------------------------
// DC_128

void aom_dc_128_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  const uint32_t pred = 0x80808080;
  dc_store_4x8(pred, dst, stride);
}

void aom_dc_128_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((uint8_t)128);
  dc_store_8xh(&row, 4, dst, stride);
}

void aom_dc_128_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((uint8_t)128);
  dc_store_8xh(&row, 16, dst, stride);
}

void aom_dc_128_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((uint8_t)128);
  dc_store_16xh(&row, 8, dst, stride);
}

void aom_dc_128_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((uint8_t)128);
  dc_store_16xh(&row, 32, dst, stride);
}

void aom_dc_128_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((uint8_t)128);
  dc_store_32xh(&row, 16, dst, stride);
}
