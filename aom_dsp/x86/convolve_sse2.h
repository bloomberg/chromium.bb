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

#ifndef AOM_DSP_X86_CONVOLVE_SSE2_H_
#define AOM_DSP_X86_CONVOLVE_SSE2_H_

static INLINE void prepare_coeffs(const InterpFilterParams *const filter_params,
                                  const int subpel_q4,
                                  __m128i *const coeffs /* [4] */) {
  const int16_t *filter = av1_get_interp_filter_subpel_kernel(
      *filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeff = _mm_loadu_si128((__m128i *)filter);

  // coeffs 0 1 0 1 0 1 0 1
  coeffs[0] = _mm_shuffle_epi32(coeff, 0x00);
  // coeffs 2 3 2 3 2 3 2 3
  coeffs[1] = _mm_shuffle_epi32(coeff, 0x55);
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[2] = _mm_shuffle_epi32(coeff, 0xaa);
  // coeffs 6 7 6 7 6 7 6 7
  coeffs[3] = _mm_shuffle_epi32(coeff, 0xff);
}

static INLINE __m128i convolve(const __m128i *const s,
                               const __m128i *const coeffs) {
  const __m128i res_0 = _mm_madd_epi16(s[0], coeffs[0]);
  const __m128i res_1 = _mm_madd_epi16(s[1], coeffs[1]);
  const __m128i res_2 = _mm_madd_epi16(s[2], coeffs[2]);
  const __m128i res_3 = _mm_madd_epi16(s[3], coeffs[3]);

  const __m128i res =
      _mm_add_epi32(_mm_add_epi32(res_0, res_1), _mm_add_epi32(res_2, res_3));

  return res;
}

#endif
