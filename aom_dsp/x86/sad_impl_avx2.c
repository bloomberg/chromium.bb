/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
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

static unsigned int sad32x32(const uint8_t *src_ptr, int src_stride,
                             const uint8_t *ref_ptr, int ref_stride) {
  __m256i s1, s2, r1, r2;
  __m256i sum = _mm256_setzero_si256();
  __m128i sum_i128;
  int i;

  for (i = 0; i < 16; ++i) {
    r1 = _mm256_loadu_si256((__m256i const *)ref_ptr);
    r2 = _mm256_loadu_si256((__m256i const *)(ref_ptr + ref_stride));
    s1 = _mm256_sad_epu8(r1, _mm256_loadu_si256((__m256i const *)src_ptr));
    s2 = _mm256_sad_epu8(
        r2, _mm256_loadu_si256((__m256i const *)(src_ptr + src_stride)));
    sum = _mm256_add_epi32(sum, _mm256_add_epi32(s1, s2));
    ref_ptr += ref_stride << 1;
    src_ptr += src_stride << 1;
  }

  sum = _mm256_add_epi32(sum, _mm256_srli_si256(sum, 8));
  sum_i128 = _mm_add_epi32(_mm256_extracti128_si256(sum, 1),
                           _mm256_castsi256_si128(sum));
  return _mm_cvtsi128_si32(sum_i128);
}

static unsigned int sad64x32(const uint8_t *src_ptr, int src_stride,
                             const uint8_t *ref_ptr, int ref_stride) {
  unsigned int half_width = 32;
  uint32_t sum = sad32x32(src_ptr, src_stride, ref_ptr, ref_stride);
  src_ptr += half_width;
  ref_ptr += half_width;
  sum += sad32x32(src_ptr, src_stride, ref_ptr, ref_stride);
  return sum;
}

static unsigned int sad64x64(const uint8_t *src_ptr, int src_stride,
                             const uint8_t *ref_ptr, int ref_stride) {
  uint32_t sum = sad64x32(src_ptr, src_stride, ref_ptr, ref_stride);
  src_ptr += src_stride << 5;
  ref_ptr += ref_stride << 5;
  sum += sad64x32(src_ptr, src_stride, ref_ptr, ref_stride);
  return sum;
}

unsigned int aom_sad128x64_avx2(const uint8_t *src_ptr, int src_stride,
                                const uint8_t *ref_ptr, int ref_stride) {
  unsigned int half_width = 64;
  uint32_t sum = sad64x64(src_ptr, src_stride, ref_ptr, ref_stride);
  src_ptr += half_width;
  ref_ptr += half_width;
  sum += sad64x64(src_ptr, src_stride, ref_ptr, ref_stride);
  return sum;
}

unsigned int aom_sad64x128_avx2(const uint8_t *src_ptr, int src_stride,
                                const uint8_t *ref_ptr, int ref_stride) {
  uint32_t sum = sad64x64(src_ptr, src_stride, ref_ptr, ref_stride);
  src_ptr += src_stride << 6;
  ref_ptr += ref_stride << 6;
  sum += sad64x64(src_ptr, src_stride, ref_ptr, ref_stride);
  return sum;
}

unsigned int aom_sad128x128_avx2(const uint8_t *src_ptr, int src_stride,
                                 const uint8_t *ref_ptr, int ref_stride) {
  uint32_t sum = aom_sad128x64_avx2(src_ptr, src_stride, ref_ptr, ref_stride);
  src_ptr += src_stride << 6;
  ref_ptr += ref_stride << 6;
  sum += aom_sad128x64_avx2(src_ptr, src_stride, ref_ptr, ref_stride);
  return sum;
}
