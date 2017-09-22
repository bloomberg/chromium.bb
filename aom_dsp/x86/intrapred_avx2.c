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

#include <immintrin.h>

#include "./aom_dsp_rtcd.h"

static INLINE __m256i dc_sum_32(const uint8_t *ref) {
  const __m256i x = _mm256_loadu_si256((const __m256i *)ref);
  const __m256i zero = _mm256_setzero_si256();
  __m256i y = _mm256_sad_epu8(x, zero);
  __m256i u = _mm256_permute2x128_si256(y, y, 1);
  y = _mm256_add_epi64(u, y);
  u = _mm256_unpackhi_epi64(y, y);
  return _mm256_add_epi16(y, u);
}

static INLINE void row_store_32xh(const __m256i *r, int height, uint8_t *dst,
                                  ptrdiff_t stride) {
  int i;
  for (i = 0; i < height; ++i) {
    _mm256_storeu_si256((__m256i *)dst, *r);
    dst += stride;
  }
}

void aom_dc_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  const __m256i sum_above = dc_sum_32(above);
  __m256i sum_left = dc_sum_32(left);
  sum_left = _mm256_add_epi16(sum_left, sum_above);
  const __m256i thirtytwo = _mm256_set1_epi16(32);
  sum_left = _mm256_add_epi16(sum_left, thirtytwo);
  sum_left = _mm256_srai_epi16(sum_left, 6);
  const __m256i zero = _mm256_setzero_si256();
  __m256i row = _mm256_shuffle_epi8(sum_left, zero);
  row_store_32xh(&row, 32, dst, stride);
}

void aom_dc_top_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m256i sum = dc_sum_32(above);
  (void)left;

  const __m256i sixteen = _mm256_set1_epi16(16);
  sum = _mm256_add_epi16(sum, sixteen);
  sum = _mm256_srai_epi16(sum, 5);
  const __m256i zero = _mm256_setzero_si256();
  __m256i row = _mm256_shuffle_epi8(sum, zero);
  row_store_32xh(&row, 32, dst, stride);
}

void aom_dc_left_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m256i sum = dc_sum_32(left);
  (void)above;

  const __m256i sixteen = _mm256_set1_epi16(16);
  sum = _mm256_add_epi16(sum, sixteen);
  sum = _mm256_srai_epi16(sum, 5);
  const __m256i zero = _mm256_setzero_si256();
  __m256i row = _mm256_shuffle_epi8(sum, zero);
  row_store_32xh(&row, 32, dst, stride);
}

void aom_dc_128_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m256i row = _mm256_set1_epi8((uint8_t)0x80);
  row_store_32xh(&row, 32, dst, stride);
}

void aom_v_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m256i row = _mm256_loadu_si256((const __m256i *)above);
  (void)left;
  row_store_32xh(&row, 32, dst, stride);
}

// There are 32 rows togeter. This function does line:
// 0,1,2,3, and 16,17,18,19. The next call would do
// 4,5,6,7, and 20,21,22,23. So 4 times of calling
// would finish 32 rows.
static INLINE void h_predictor_32x8line(const __m256i *row, uint8_t *dst,
                                        ptrdiff_t stride) {
  __m256i t[4];
  __m256i m = _mm256_setzero_si256();
  const __m256i inc = _mm256_set1_epi8(4);
  int i;

  for (i = 0; i < 4; i++) {
    t[i] = _mm256_shuffle_epi8(*row, m);
    __m256i r0 = _mm256_permute2x128_si256(t[i], t[i], 0);
    __m256i r1 = _mm256_permute2x128_si256(t[i], t[i], 0x11);
    _mm256_storeu_si256((__m256i *)dst, r0);
    _mm256_storeu_si256((__m256i *)(dst + (stride << 4)), r1);
    dst += stride;
    m = _mm256_add_epi8(m, inc);
  }
}

void aom_h_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)above;
  const __m256i left_col = _mm256_loadu_si256((__m256i const *)left);

  __m256i u = _mm256_unpacklo_epi8(left_col, left_col);

  __m256i v = _mm256_unpacklo_epi8(u, u);
  h_predictor_32x8line(&v, dst, stride);
  dst += stride << 2;

  v = _mm256_unpackhi_epi8(u, u);
  h_predictor_32x8line(&v, dst, stride);
  dst += stride << 2;

  u = _mm256_unpackhi_epi8(left_col, left_col);

  v = _mm256_unpacklo_epi8(u, u);
  h_predictor_32x8line(&v, dst, stride);
  dst += stride << 2;

  v = _mm256_unpackhi_epi8(u, u);
  h_predictor_32x8line(&v, dst, stride);
}

// -----------------------------------------------------------------------------
// Rectangle

// TODO(luoyi) The following two functions are shared with intrapred_sse2.c.
// Use a header file, intrapred_common_x86.h
static INLINE __m128i dc_sum_16_sse2(const uint8_t *ref) {
  __m128i x = _mm_load_si128((__m128i const *)ref);
  const __m128i zero = _mm_setzero_si128();
  x = _mm_sad_epu8(x, zero);
  const __m128i high = _mm_unpackhi_epi64(x, x);
  return _mm_add_epi16(x, high);
}

static INLINE __m128i dc_sum_32_sse2(const uint8_t *ref) {
  __m128i x0 = _mm_load_si128((__m128i const *)ref);
  __m128i x1 = _mm_load_si128((__m128i const *)(ref + 16));
  const __m128i zero = _mm_setzero_si128();
  x0 = _mm_sad_epu8(x0, zero);
  x1 = _mm_sad_epu8(x1, zero);
  x0 = _mm_add_epi16(x0, x1);
  const __m128i high = _mm_unpackhi_epi64(x0, x0);
  return _mm_add_epi16(x0, high);
}

void aom_dc_predictor_32x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  const __m128i top_sum = dc_sum_32_sse2(above);
  __m128i left_sum = dc_sum_16_sse2(left);
  left_sum = _mm_add_epi16(top_sum, left_sum);
  uint32_t sum = _mm_cvtsi128_si32(left_sum);
  sum += 24;
  sum /= 48;

  const __m256i row = _mm256_set1_epi8((uint8_t)sum);
  row_store_32xh(&row, 16, dst, stride);
}

void aom_dc_top_predictor_32x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m256i sum = dc_sum_32(above);
  (void)left;

  const __m256i sixteen = _mm256_set1_epi16(16);
  sum = _mm256_add_epi16(sum, sixteen);
  sum = _mm256_srai_epi16(sum, 5);
  const __m256i zero = _mm256_setzero_si256();
  __m256i row = _mm256_shuffle_epi8(sum, zero);
  row_store_32xh(&row, 16, dst, stride);
}

void aom_dc_left_predictor_32x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i sum = dc_sum_16_sse2(left);
  (void)above;

  const __m128i eight = _mm_set1_epi16(8);
  sum = _mm_add_epi16(sum, eight);
  sum = _mm_srai_epi16(sum, 4);
  const __m128i zero = _mm_setzero_si128();
  const __m128i r = _mm_shuffle_epi8(sum, zero);
  const __m256i row = _mm256_inserti128_si256(_mm256_castsi128_si256(r), r, 1);
  row_store_32xh(&row, 16, dst, stride);
}

void aom_dc_128_predictor_32x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m256i row = _mm256_set1_epi8((uint8_t)0x80);
  row_store_32xh(&row, 16, dst, stride);
}

void aom_v_predictor_32x16_avx2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m256i row = _mm256_loadu_si256((const __m256i *)above);
  (void)left;
  row_store_32xh(&row, 16, dst, stride);
}
