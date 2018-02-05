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

#ifndef AOM_DSP_X86_CONVOLVE_AVX2_H_
#define AOM_DSP_X86_CONVOLVE_AVX2_H_

// filters for 16
DECLARE_ALIGNED(32, static const uint8_t, filt1_global_avx2[32]) = {
  0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8,
  0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8
};

DECLARE_ALIGNED(32, static const uint8_t, filt2_global_avx2[32]) = {
  2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10,
  2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10
};

DECLARE_ALIGNED(32, static const uint8_t, filt3_global_avx2[32]) = {
  4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12,
  4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12
};

DECLARE_ALIGNED(32, static const uint8_t, filt4_global_avx2[32]) = {
  6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14,
  6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14
};

static INLINE void prepare_coeffs(const InterpFilterParams *const filter_params,
                                  const int subpel_q4,
                                  __m256i *const coeffs /* [4] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      *filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);
  const __m256i filter_coeffs = _mm256_broadcastsi128_si256(coeffs_8);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16(0xffff)));

  const __m256i coeffs_1 = _mm256_srai_epi16(filter_coeffs, 1);

  // coeffs 0 1 0 1 0 1 0 1
  coeffs[0] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0200u));
  // coeffs 2 3 2 3 2 3 2 3
  coeffs[1] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0604u));
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[2] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0a08u));
  // coeffs 6 7 6 7 6 7 6 7
  coeffs[3] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0e0cu));
}

static INLINE void prepare_coeffs_y_2d(
    const InterpFilterParams *const filter_params_y, const int subpel_y_q4,
    __m256i *const coeffs /* [4] */) {
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);

  const __m128i coeffs_y8 = _mm_loadu_si128((__m128i *)y_filter);
  const __m256i coeffs_y = _mm256_broadcastsi128_si256(coeffs_y8);

  // coeffs 0 1 0 1 0 1 0 1
  coeffs[0] = _mm256_shuffle_epi32(coeffs_y, 0x00);
  // coeffs 2 3 2 3 2 3 2 3
  coeffs[1] = _mm256_shuffle_epi32(coeffs_y, 0x55);
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[2] = _mm256_shuffle_epi32(coeffs_y, 0xaa);
  // coeffs 6 7 6 7 6 7 6 7
  coeffs[3] = _mm256_shuffle_epi32(coeffs_y, 0xff);
}

static INLINE __m256i convolve(const __m256i *const s,
                               const __m256i *const coeffs) {
  const __m256i res_01 = _mm256_maddubs_epi16(s[0], coeffs[0]);
  const __m256i res_23 = _mm256_maddubs_epi16(s[1], coeffs[1]);
  const __m256i res_45 = _mm256_maddubs_epi16(s[2], coeffs[2]);
  const __m256i res_67 = _mm256_maddubs_epi16(s[3], coeffs[3]);

  // order: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
  const __m256i res = _mm256_add_epi16(_mm256_add_epi16(res_01, res_45),
                                       _mm256_add_epi16(res_23, res_67));

  return res;
}

static INLINE __m256i convolve_y_2d(const __m256i *const s,
                                    const __m256i *const coeffs) {
  const __m256i res_0 = _mm256_madd_epi16(s[0], coeffs[0]);
  const __m256i res_1 = _mm256_madd_epi16(s[1], coeffs[1]);
  const __m256i res_2 = _mm256_madd_epi16(s[2], coeffs[2]);
  const __m256i res_3 = _mm256_madd_epi16(s[3], coeffs[3]);

  const __m256i res = _mm256_add_epi32(_mm256_add_epi32(res_0, res_1),
                                       _mm256_add_epi32(res_2, res_3));

  return res;
}

static INLINE __m256i convolve_x(const __m256i data,
                                 const __m256i *const coeffs,
                                 const __m256i *const filt) {
  __m256i s[4];

  s[0] = _mm256_shuffle_epi8(data, filt[0]);
  s[1] = _mm256_shuffle_epi8(data, filt[1]);
  s[2] = _mm256_shuffle_epi8(data, filt[2]);
  s[3] = _mm256_shuffle_epi8(data, filt[3]);

  return convolve(s, coeffs);
}

static INLINE void add_store_aligned(CONV_BUF_TYPE *const dst,
                                     const __m256i *const res,
                                     const __m256i *const avg_mask) {
  __m256i d;
  d = _mm256_load_si256((__m256i *)dst);
  d = _mm256_and_si256(d, *avg_mask);
  d = _mm256_add_epi32(d, *res);
  _mm256_store_si256((__m256i *)dst, d);
}

#endif
