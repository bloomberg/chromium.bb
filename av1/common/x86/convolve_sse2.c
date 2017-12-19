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
#include "aom_dsp/aom_convolve.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "av1/common/convolve.h"

void av1_convolve_y_sse2(const uint8_t *src, int src_stride,
                         const uint8_t *dst0, int dst_stride0, int w, int h,
                         InterpFilterParams *filter_params_x,
                         InterpFilterParams *filter_params_y,
                         const int subpel_x_q4, const int subpel_y_q4,
                         ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int do_average = conv_params->do_average;
  const uint8_t *const src_ptr = src - fo_vert * src_stride;
  const int bits = FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const __m128i left_shift = _mm_cvtsi32_si128(bits);
  const __m128i zero = _mm_setzero_si128();

  (void)filter_params_x;
  (void)subpel_x_q4;
  (void)dst0;
  (void)dst_stride0;

  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
  const __m128i coeffs_y = _mm_loadu_si128((__m128i *)y_filter);

  // coeffs 0 1 0 1 2 3 2 3
  const __m128i tmp_0 = _mm_unpacklo_epi32(coeffs_y, coeffs_y);
  // coeffs 4 5 4 5 6 7 6 7
  const __m128i tmp_1 = _mm_unpackhi_epi32(coeffs_y, coeffs_y);

  // coeffs 0 1 0 1 0 1 0 1
  const __m128i coeff_01 = _mm_unpacklo_epi64(tmp_0, tmp_0);
  // coeffs 2 3 2 3 2 3 2 3
  const __m128i coeff_23 = _mm_unpackhi_epi64(tmp_0, tmp_0);
  // coeffs 4 5 4 5 4 5 4 5
  const __m128i coeff_45 = _mm_unpacklo_epi64(tmp_1, tmp_1);
  // coeffs 6 7 6 7 6 7 6 7
  const __m128i coeff_67 = _mm_unpackhi_epi64(tmp_1, tmp_1);

  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; j += 8) {
      // Filter low index pixels
      const uint8_t *data = &src_ptr[i * src_stride + j];
      const __m128i src_01 = _mm_unpacklo_epi8(
          _mm_loadl_epi64((__m128i *)(data + 0 * src_stride)),
          _mm_loadl_epi64((__m128i *)(data + 1 * src_stride)));
      const __m128i src_23 = _mm_unpacklo_epi8(
          _mm_loadl_epi64((__m128i *)(data + 2 * src_stride)),
          _mm_loadl_epi64((__m128i *)(data + 3 * src_stride)));
      const __m128i src_45 = _mm_unpacklo_epi8(
          _mm_loadl_epi64((__m128i *)(data + 4 * src_stride)),
          _mm_loadl_epi64((__m128i *)(data + 5 * src_stride)));
      const __m128i src_67 = _mm_unpacklo_epi8(
          _mm_loadl_epi64((__m128i *)(data + 6 * src_stride)),
          _mm_loadl_epi64((__m128i *)(data + 7 * src_stride)));

      const __m128i src_01_lo = _mm_unpacklo_epi8(src_01, zero);
      const __m128i src_23_lo = _mm_unpacklo_epi8(src_23, zero);
      const __m128i src_45_lo = _mm_unpacklo_epi8(src_45, zero);
      const __m128i src_67_lo = _mm_unpacklo_epi8(src_67, zero);

      const __m128i res_01_lo = _mm_madd_epi16(src_01_lo, coeff_01);
      const __m128i res_23_lo = _mm_madd_epi16(src_23_lo, coeff_23);
      const __m128i res_45_lo = _mm_madd_epi16(src_45_lo, coeff_45);
      const __m128i res_67_lo = _mm_madd_epi16(src_67_lo, coeff_67);

      const __m128i res_lo = _mm_add_epi32(_mm_add_epi32(res_01_lo, res_23_lo),
                                           _mm_add_epi32(res_45_lo, res_67_lo));

      // Filter high index pixels
      const __m128i src_01_hi = _mm_unpackhi_epi8(src_01, zero);
      const __m128i src_23_hi = _mm_unpackhi_epi8(src_23, zero);
      const __m128i src_45_hi = _mm_unpackhi_epi8(src_45, zero);
      const __m128i src_67_hi = _mm_unpackhi_epi8(src_67, zero);

      const __m128i res_01_hi = _mm_madd_epi16(src_01_hi, coeff_01);
      const __m128i res_23_hi = _mm_madd_epi16(src_23_hi, coeff_23);
      const __m128i res_45_hi = _mm_madd_epi16(src_45_hi, coeff_45);
      const __m128i res_67_hi = _mm_madd_epi16(src_67_hi, coeff_67);

      const __m128i res_hi = _mm_add_epi32(_mm_add_epi32(res_01_hi, res_23_hi),
                                           _mm_add_epi32(res_45_hi, res_67_hi));

      const __m128i res_lo_shift = _mm_sll_epi32(res_lo, left_shift);
      const __m128i res_hi_shift = _mm_sll_epi32(res_hi, left_shift);

      // Accumulate values into the destination buffer
      __m128i *const p = (__m128i *)&dst[i * dst_stride + j];
      if (do_average) {
        _mm_storeu_si128(p + 0,
                         _mm_add_epi32(_mm_loadu_si128(p + 0), res_lo_shift));
        _mm_storeu_si128(p + 1,
                         _mm_add_epi32(_mm_loadu_si128(p + 1), res_hi_shift));
      } else {
        _mm_storeu_si128(p + 0, res_lo_shift);
        _mm_storeu_si128(p + 1, res_hi_shift);
      }
    }
  }
}

void av1_convolve_x_sse2(const uint8_t *src, int src_stride,
                         const uint8_t *dst0, int dst_stride0, int w, int h,
                         InterpFilterParams *filter_params_x,
                         InterpFilterParams *filter_params_y,
                         const int subpel_x_q4, const int subpel_y_q4,
                         ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  int i, j;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int do_average = conv_params->do_average;
  const uint8_t *const src_ptr = src - fo_horiz;
  const int bits = FILTER_BITS - conv_params->round_1;
  const __m128i left_shift = _mm_cvtsi32_si128(bits);
  const __m128i zero = _mm_setzero_si128();

  (void)filter_params_y;
  (void)subpel_y_q4;
  (void)dst0;
  (void)dst_stride0;

  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  const __m128i coeffs_x = _mm_loadu_si128((__m128i *)x_filter);

  // coeffs 0 1 0 1 2 3 2 3
  const __m128i tmp_0 = _mm_unpacklo_epi32(coeffs_x, coeffs_x);
  // coeffs 4 5 4 5 6 7 6 7
  const __m128i tmp_1 = _mm_unpackhi_epi32(coeffs_x, coeffs_x);

  // coeffs 0 1 0 1 0 1 0 1
  const __m128i coeff_01 = _mm_unpacklo_epi64(tmp_0, tmp_0);
  // coeffs 2 3 2 3 2 3 2 3
  const __m128i coeff_23 = _mm_unpackhi_epi64(tmp_0, tmp_0);
  // coeffs 4 5 4 5 4 5 4 5
  const __m128i coeff_45 = _mm_unpacklo_epi64(tmp_1, tmp_1);
  // coeffs 6 7 6 7 6 7 6 7
  const __m128i coeff_67 = _mm_unpackhi_epi64(tmp_1, tmp_1);

  const __m128i round_const = _mm_set1_epi32((1 << conv_params->round_0) >> 1);
  const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_0);

  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; j += 8) {
      const __m128i data =
          _mm_loadu_si128((__m128i *)&src_ptr[i * src_stride + j]);

      // Filter even-index pixels
      const __m128i src_0 = _mm_unpacklo_epi8(data, zero);
      const __m128i res_0 = _mm_madd_epi16(src_0, coeff_01);
      const __m128i src_2 = _mm_unpacklo_epi8(_mm_srli_si128(data, 2), zero);
      const __m128i res_2 = _mm_madd_epi16(src_2, coeff_23);
      const __m128i src_4 = _mm_unpacklo_epi8(_mm_srli_si128(data, 4), zero);
      const __m128i res_4 = _mm_madd_epi16(src_4, coeff_45);
      const __m128i src_6 = _mm_unpacklo_epi8(_mm_srli_si128(data, 6), zero);
      const __m128i res_6 = _mm_madd_epi16(src_6, coeff_67);
      const __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_4),
                                             _mm_add_epi32(res_2, res_6));

      // Filter odd-index pixels
      const __m128i src_1 = _mm_unpacklo_epi8(_mm_srli_si128(data, 1), zero);
      const __m128i res_1 = _mm_madd_epi16(src_1, coeff_01);
      const __m128i src_3 = _mm_unpacklo_epi8(_mm_srli_si128(data, 3), zero);
      const __m128i res_3 = _mm_madd_epi16(src_3, coeff_23);
      const __m128i src_5 = _mm_unpacklo_epi8(_mm_srli_si128(data, 5), zero);
      const __m128i res_5 = _mm_madd_epi16(src_5, coeff_45);
      const __m128i src_7 = _mm_unpacklo_epi8(_mm_srli_si128(data, 7), zero);
      const __m128i res_7 = _mm_madd_epi16(src_7, coeff_67);
      const __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_1, res_5),
                                            _mm_add_epi32(res_3, res_7));

      // Rearrange pixels back into the order 0 ... 7
      const __m128i res_lo = _mm_unpacklo_epi32(res_even, res_odd);
      const __m128i res_hi = _mm_unpackhi_epi32(res_even, res_odd);

      const __m128i res_lo_round =
          _mm_sra_epi32(_mm_add_epi32(res_lo, round_const), round_shift);
      const __m128i res_hi_round =
          _mm_sra_epi32(_mm_add_epi32(res_hi, round_const), round_shift);
      const __m128i res_lo_shift = _mm_sll_epi32(res_lo_round, left_shift);
      const __m128i res_hi_shift = _mm_sll_epi32(res_hi_round, left_shift);

      // Accumulate values into the destination buffer
      __m128i *const p = (__m128i *)&dst[i * dst_stride + j];
      if (do_average) {
        _mm_storeu_si128(p + 0,
                         _mm_add_epi32(_mm_loadu_si128(p + 0), res_lo_shift));
        _mm_storeu_si128(p + 1,
                         _mm_add_epi32(_mm_loadu_si128(p + 1), res_hi_shift));
      } else {
        _mm_storeu_si128(p + 0, res_lo_shift);
        _mm_storeu_si128(p + 1, res_hi_shift);
      }
    }
  }
}
