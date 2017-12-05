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
#include <assert.h>

#include "./aom_dsp_rtcd.h"
#include "aom_dsp/aom_convolve.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "av1/common/convolve.h"

#if CONFIG_COMPOUND_ROUND
void av1_highbd_convolve_2d_avx2(const uint16_t *src, int src_stride,
                                 CONV_BUF_TYPE *dst, int dst_stride, int w,
                                 int h, InterpFilterParams *filter_params_x,
                                 InterpFilterParams *filter_params_y,
                                 const int subpel_x_q4, const int subpel_y_q4,
                                 ConvolveParams *conv_params, int bd) {
  DECLARE_ALIGNED(32, int16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = MAX_SB_SIZE;
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int do_average = conv_params->do_average;
  const uint16_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

  /* Horizontal filter */
  {
    const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
        *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
    const __m128i coeffs_x8 = _mm_loadu_si128((__m128i *)x_filter);
    // since not all compilers yet support _mm256_set_m128i()
    const __m256i coeffs_x = _mm256_insertf128_si256(
        _mm256_castsi128_si256(coeffs_x8), coeffs_x8, 1);

    // coeffs 0 1 0 1 2 3 2 3
    const __m256i tmp_0 = _mm256_unpacklo_epi32(coeffs_x, coeffs_x);
    // coeffs 4 5 4 5 6 7 6 7
    const __m256i tmp_1 = _mm256_unpackhi_epi32(coeffs_x, coeffs_x);

    // coeffs 0 1 0 1 0 1 0 1
    const __m256i coeff_01 = _mm256_unpacklo_epi64(tmp_0, tmp_0);
    // coeffs 2 3 2 3 2 3 2 3
    const __m256i coeff_23 = _mm256_unpackhi_epi64(tmp_0, tmp_0);
    // coeffs 4 5 4 5 4 5 4 5
    const __m256i coeff_45 = _mm256_unpacklo_epi64(tmp_1, tmp_1);
    // coeffs 6 7 6 7 6 7 6 7
    const __m256i coeff_67 = _mm256_unpackhi_epi64(tmp_1, tmp_1);

    const __m256i round_const =
        _mm256_set1_epi32((1 << conv_params->round_0) >> 1);
    const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_0);

    for (i = 0; i < im_h; ++i) {
      for (j = 0; j < w; j += 16) {
        const __m256i data =
            _mm256_loadu_si256((__m256i *)&src_ptr[i * src_stride + j]);
        const __m128i data2_1 =
            _mm_loadu_si128((__m128i *)&src_ptr[i * src_stride + j + 16]);
        const __m256i data2 = _mm256_insertf128_si256(
            _mm256_castsi128_si256(data2_1), data2_1, 1);

        // Filter even-index pixels
        const __m256i res_0 = _mm256_madd_epi16(data, coeff_01);
        const __m256i res_2 = _mm256_madd_epi16(
            _mm256_alignr_epi8(_mm256_permute2x128_si256(data2, data, 0x13),
                               data, 4),
            coeff_23);
        const __m256i res_4 = _mm256_madd_epi16(
            _mm256_alignr_epi8(_mm256_permute2x128_si256(data2, data, 0x13),
                               data, 8),
            coeff_45);
        const __m256i res_6 = _mm256_madd_epi16(
            _mm256_alignr_epi8(_mm256_permute2x128_si256(data2, data, 0x13),
                               data, 12),
            coeff_67);

        __m256i res_even = _mm256_add_epi32(_mm256_add_epi32(res_0, res_4),
                                            _mm256_add_epi32(res_2, res_6));
        res_even = _mm256_sra_epi32(_mm256_add_epi32(res_even, round_const),
                                    round_shift);

        // Filter odd-index pixels
        const __m256i res_1 = _mm256_madd_epi16(
            _mm256_alignr_epi8(_mm256_permute2x128_si256(data2, data, 0x13),
                               data, 2),
            coeff_01);
        const __m256i res_3 = _mm256_madd_epi16(
            _mm256_alignr_epi8(_mm256_permute2x128_si256(data2, data, 0x13),
                               data, 6),
            coeff_23);
        const __m256i res_5 = _mm256_madd_epi16(
            _mm256_alignr_epi8(_mm256_permute2x128_si256(data2, data, 0x13),
                               data, 10),
            coeff_45);
        const __m256i res_7 = _mm256_madd_epi16(
            _mm256_alignr_epi8(_mm256_permute2x128_si256(data2, data, 0x13),
                               data, 14),
            coeff_67);

        __m256i res_odd = _mm256_add_epi32(_mm256_add_epi32(res_1, res_5),
                                           _mm256_add_epi32(res_3, res_7));
        res_odd = _mm256_sra_epi32(_mm256_add_epi32(res_odd, round_const),
                                   round_shift);

        // Pack in the column order 0, 2, 4, 6, 1, 3, 5, 7
        const __m256i maxval = _mm256_set1_epi16((1 << bd) - 1);
        __m256i res = _mm256_packs_epi32(res_even, res_odd);
        res = _mm256_max_epi16(_mm256_min_epi16(res, maxval),
                               _mm256_setzero_si256());
        _mm_storeu_si128((__m128i *)&im_block[i * im_stride + j],
                         _mm256_extractf128_si256(res, 0));
        _mm_storeu_si128((__m128i *)&im_block[i * im_stride + j + 8],
                         _mm256_extractf128_si256(res, 1));
      }
    }
  }

  /* Vertical filter */
  {
    const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
        *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
    const __m128i coeffs_y8 = _mm_loadu_si128((__m128i *)y_filter);
    const __m256i coeffs_y = _mm256_insertf128_si256(
        _mm256_castsi128_si256(coeffs_y8), coeffs_y8, 1);

    // coeffs 0 1 0 1 2 3 2 3
    const __m256i tmp_0 = _mm256_unpacklo_epi32(coeffs_y, coeffs_y);
    // coeffs 4 5 4 5 6 7 6 7
    const __m256i tmp_1 = _mm256_unpackhi_epi32(coeffs_y, coeffs_y);

    // coeffs 0 1 0 1 0 1 0 1
    const __m256i coeff_01 = _mm256_unpacklo_epi64(tmp_0, tmp_0);
    // coeffs 2 3 2 3 2 3 2 3
    const __m256i coeff_23 = _mm256_unpackhi_epi64(tmp_0, tmp_0);
    // coeffs 4 5 4 5 4 5 4 5
    const __m256i coeff_45 = _mm256_unpacklo_epi64(tmp_1, tmp_1);
    // coeffs 6 7 6 7 6 7 6 7
    const __m256i coeff_67 = _mm256_unpackhi_epi64(tmp_1, tmp_1);

    const __m256i round_const =
        _mm256_set1_epi32((1 << conv_params->round_1) >> 1);
    const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_1);

    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 16) {
        // Filter even-index pixels
        const int16_t *data = &im_block[i * im_stride + j];
        const __m256i src_0 =
            _mm256_unpacklo_epi16(*(__m256i *)(data + 0 * im_stride),
                                  *(__m256i *)(data + 1 * im_stride));
        const __m256i src_2 =
            _mm256_unpacklo_epi16(*(__m256i *)(data + 2 * im_stride),
                                  *(__m256i *)(data + 3 * im_stride));
        const __m256i src_4 =
            _mm256_unpacklo_epi16(*(__m256i *)(data + 4 * im_stride),
                                  *(__m256i *)(data + 5 * im_stride));
        const __m256i src_6 =
            _mm256_unpacklo_epi16(*(__m256i *)(data + 6 * im_stride),
                                  *(__m256i *)(data + 7 * im_stride));

        const __m256i res_0 = _mm256_madd_epi16(src_0, coeff_01);
        const __m256i res_2 = _mm256_madd_epi16(src_2, coeff_23);
        const __m256i res_4 = _mm256_madd_epi16(src_4, coeff_45);
        const __m256i res_6 = _mm256_madd_epi16(src_6, coeff_67);

        const __m256i res_even = _mm256_add_epi32(
            _mm256_add_epi32(res_0, res_2), _mm256_add_epi32(res_4, res_6));

        // Filter odd-index pixels
        const __m256i src_1 =
            _mm256_unpackhi_epi16(*(__m256i *)(data + 0 * im_stride),
                                  *(__m256i *)(data + 1 * im_stride));
        const __m256i src_3 =
            _mm256_unpackhi_epi16(*(__m256i *)(data + 2 * im_stride),
                                  *(__m256i *)(data + 3 * im_stride));
        const __m256i src_5 =
            _mm256_unpackhi_epi16(*(__m256i *)(data + 4 * im_stride),
                                  *(__m256i *)(data + 5 * im_stride));
        const __m256i src_7 =
            _mm256_unpackhi_epi16(*(__m256i *)(data + 6 * im_stride),
                                  *(__m256i *)(data + 7 * im_stride));

        const __m256i res_1 = _mm256_madd_epi16(src_1, coeff_01);
        const __m256i res_3 = _mm256_madd_epi16(src_3, coeff_23);
        const __m256i res_5 = _mm256_madd_epi16(src_5, coeff_45);
        const __m256i res_7 = _mm256_madd_epi16(src_7, coeff_67);

        const __m256i res_odd = _mm256_add_epi32(
            _mm256_add_epi32(res_1, res_3), _mm256_add_epi32(res_5, res_7));

        // Rearrange pixels back into the order 0 ... 7
        const __m256i res_lo = _mm256_unpacklo_epi32(res_even, res_odd);
        const __m256i res_hi = _mm256_unpackhi_epi32(res_even, res_odd);

        const __m256i res_lo_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_lo, round_const), round_shift);
        const __m256i res_hi_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_hi, round_const), round_shift);

        // Accumulate values into the destination buffer
        __m128i *const p = (__m128i *)&dst[i * dst_stride + j];
        if (do_average) {
          _mm_storeu_si128(
              p + 0, _mm_add_epi32(_mm_loadu_si128(p + 0),
                                   _mm256_extractf128_si256(res_lo_round, 0)));
          _mm_storeu_si128(
              p + 1, _mm_add_epi32(_mm_loadu_si128(p + 1),
                                   _mm256_extractf128_si256(res_hi_round, 0)));
          if (w - j > 8) {
            _mm_storeu_si128(p + 2, _mm_add_epi32(_mm_loadu_si128(p + 2),
                                                  _mm256_extractf128_si256(
                                                      res_lo_round, 1)));
            _mm_storeu_si128(p + 3, _mm_add_epi32(_mm_loadu_si128(p + 3),
                                                  _mm256_extractf128_si256(
                                                      res_hi_round, 1)));
          }
        } else {
          _mm_storeu_si128(p + 0, _mm256_extractf128_si256(res_lo_round, 0));
          _mm_storeu_si128(p + 1, _mm256_extractf128_si256(res_hi_round, 0));
          if (w - j > 8) {
            _mm_storeu_si128(p + 2, _mm256_extractf128_si256(res_lo_round, 1));
            _mm_storeu_si128(p + 3, _mm256_extractf128_si256(res_hi_round, 1));
          }
        }
      }
    }
  }
}
#else
void av1_highbd_convolve_2d_avx2(const uint16_t *src, int src_stride,
                                 CONV_BUF_TYPE *dst, int dst_stride, int w,
                                 int h, InterpFilterParams *filter_params_x,
                                 InterpFilterParams *filter_params_y,
                                 const int subpel_x_q4, const int subpel_y_q4,
                                 ConvolveParams *conv_params, int bd) {
  DECLARE_ALIGNED(32, int16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = MAX_SB_SIZE;
  int i, j;
  const int do_average = conv_params->do_average;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint16_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

  // Check that, even with 12-bit input, the intermediate values will fit
  // into an unsigned 15-bit intermediate array.
  assert(conv_params->round_0 >= 5);

  /* Horizontal filter */
  {
    const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
        *filter_params_x, subpel_x_q4 & SUBPEL_MASK);

    const __m128i coeffs_x8 = _mm_loadu_si128((__m128i *)x_filter);
    // since not all compilers yet support _mm256_set_m128i()
    const __m256i coeffs_x = _mm256_insertf128_si256(
        _mm256_castsi128_si256(coeffs_x8), coeffs_x8, 1);

    // coeffs 0 1 0 1 2 3 2 3
    const __m256i tmp_0 = _mm256_unpacklo_epi32(coeffs_x, coeffs_x);
    // coeffs 4 5 4 5 6 7 6 7
    const __m256i tmp_1 = _mm256_unpackhi_epi32(coeffs_x, coeffs_x);

    // coeffs 0 1 0 1 0 1 0 1
    const __m256i coeff_01 = _mm256_unpacklo_epi64(tmp_0, tmp_0);
    // coeffs 2 3 2 3 2 3 2 3
    const __m256i coeff_23 = _mm256_unpackhi_epi64(tmp_0, tmp_0);
    // coeffs 4 5 4 5 4 5 4 5
    const __m256i coeff_45 = _mm256_unpacklo_epi64(tmp_1, tmp_1);
    // coeffs 6 7 6 7 6 7 6 7
    const __m256i coeff_67 = _mm256_unpackhi_epi64(tmp_1, tmp_1);

    const __m256i round_const = _mm256_set1_epi32(
        ((1 << conv_params->round_0) >> 1) + (1 << (bd + FILTER_BITS - 1)));
    const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_0);

    for (i = 0; i < im_h; ++i) {
      for (j = 0; j < w; j += 16) {
        const __m256i data =
            _mm256_loadu_si256((__m256i *)&src_ptr[i * src_stride + j]);
        const __m128i data2_1 =
            _mm_loadu_si128((__m128i *)&src_ptr[i * src_stride + j + 16]);
        const __m256i data2 = _mm256_insertf128_si256(
            _mm256_castsi128_si256(data2_1), data2_1, 1);

        // Filter even-index pixels
        const __m256i res_0 = _mm256_madd_epi16(data, coeff_01);
        const __m256i res_2 = _mm256_madd_epi16(
            _mm256_alignr_epi8(_mm256_permute2x128_si256(data2, data, 0x13),
                               data, 4),
            coeff_23);
        const __m256i res_4 = _mm256_madd_epi16(
            _mm256_alignr_epi8(_mm256_permute2x128_si256(data2, data, 0x13),
                               data, 8),
            coeff_45);
        const __m256i res_6 = _mm256_madd_epi16(
            _mm256_alignr_epi8(_mm256_permute2x128_si256(data2, data, 0x13),
                               data, 12),
            coeff_67);

        __m256i res_even = _mm256_add_epi32(_mm256_add_epi32(res_0, res_4),
                                            _mm256_add_epi32(res_2, res_6));
        res_even = _mm256_sra_epi32(_mm256_add_epi32(res_even, round_const),
                                    round_shift);

        // Filter odd-index pixels
        const __m256i res_1 = _mm256_madd_epi16(
            _mm256_alignr_epi8(_mm256_permute2x128_si256(data2, data, 0x13),
                               data, 2),
            coeff_01);
        const __m256i res_3 = _mm256_madd_epi16(
            _mm256_alignr_epi8(_mm256_permute2x128_si256(data2, data, 0x13),
                               data, 6),
            coeff_23);
        const __m256i res_5 = _mm256_madd_epi16(
            _mm256_alignr_epi8(_mm256_permute2x128_si256(data2, data, 0x13),
                               data, 10),
            coeff_45);
        const __m256i res_7 = _mm256_madd_epi16(
            _mm256_alignr_epi8(_mm256_permute2x128_si256(data2, data, 0x13),
                               data, 14),
            coeff_67);

        __m256i res_odd = _mm256_add_epi32(_mm256_add_epi32(res_1, res_5),
                                           _mm256_add_epi32(res_3, res_7));
        res_odd = _mm256_sra_epi32(_mm256_add_epi32(res_odd, round_const),
                                   round_shift);

        __m256i res = _mm256_packs_epi32(res_even, res_odd);
        _mm256_storeu_si256((__m256i *)&im_block[i * im_stride + j], res);
      }
    }
  }

  /* Vertical filter */
  {
    const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
        *filter_params_y, subpel_y_q4 & SUBPEL_MASK);

    const __m128i coeffs_y8 = _mm_loadu_si128((__m128i *)y_filter);
    const __m256i coeffs_y = _mm256_insertf128_si256(
        _mm256_castsi128_si256(coeffs_y8), coeffs_y8, 1);

    // coeffs 0 1 0 1 2 3 2 3
    const __m256i tmp_0 = _mm256_unpacklo_epi32(coeffs_y, coeffs_y);
    // coeffs 4 5 4 5 6 7 6 7
    const __m256i tmp_1 = _mm256_unpackhi_epi32(coeffs_y, coeffs_y);

    // coeffs 0 1 0 1 0 1 0 1
    const __m256i coeff_01 = _mm256_unpacklo_epi64(tmp_0, tmp_0);
    // coeffs 2 3 2 3 2 3 2 3
    const __m256i coeff_23 = _mm256_unpackhi_epi64(tmp_0, tmp_0);
    // coeffs 4 5 4 5 4 5 4 5
    const __m256i coeff_45 = _mm256_unpacklo_epi64(tmp_1, tmp_1);
    // coeffs 6 7 6 7 6 7 6 7
    const __m256i coeff_67 = _mm256_unpackhi_epi64(tmp_1, tmp_1);

    const __m256i round_const = _mm256_set1_epi32(
        ((1 << conv_params->round_1) >> 1) -
        (1 << (bd + 2 * FILTER_BITS - conv_params->round_0 - 1)));
    const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_1);

    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 16) {
        // Filter even-index pixels
        const int16_t *data = &im_block[i * im_stride + j];
        const __m256i src_0 =
            _mm256_unpacklo_epi16(*(__m256i *)(data + 0 * im_stride),
                                  *(__m256i *)(data + 1 * im_stride));
        const __m256i src_2 =
            _mm256_unpacklo_epi16(*(__m256i *)(data + 2 * im_stride),
                                  *(__m256i *)(data + 3 * im_stride));
        const __m256i src_4 =
            _mm256_unpacklo_epi16(*(__m256i *)(data + 4 * im_stride),
                                  *(__m256i *)(data + 5 * im_stride));
        const __m256i src_6 =
            _mm256_unpacklo_epi16(*(__m256i *)(data + 6 * im_stride),
                                  *(__m256i *)(data + 7 * im_stride));

        const __m256i res_0 = _mm256_madd_epi16(src_0, coeff_01);
        const __m256i res_2 = _mm256_madd_epi16(src_2, coeff_23);
        const __m256i res_4 = _mm256_madd_epi16(src_4, coeff_45);
        const __m256i res_6 = _mm256_madd_epi16(src_6, coeff_67);

        const __m256i res_even = _mm256_add_epi32(
            _mm256_add_epi32(res_0, res_2), _mm256_add_epi32(res_4, res_6));

        // Filter odd-index pixels
        const __m256i src_1 =
            _mm256_unpackhi_epi16(*(__m256i *)(data + 0 * im_stride),
                                  *(__m256i *)(data + 1 * im_stride));
        const __m256i src_3 =
            _mm256_unpackhi_epi16(*(__m256i *)(data + 2 * im_stride),
                                  *(__m256i *)(data + 3 * im_stride));
        const __m256i src_5 =
            _mm256_unpackhi_epi16(*(__m256i *)(data + 4 * im_stride),
                                  *(__m256i *)(data + 5 * im_stride));
        const __m256i src_7 =
            _mm256_unpackhi_epi16(*(__m256i *)(data + 6 * im_stride),
                                  *(__m256i *)(data + 7 * im_stride));

        const __m256i res_1 = _mm256_madd_epi16(src_1, coeff_01);
        const __m256i res_3 = _mm256_madd_epi16(src_3, coeff_23);
        const __m256i res_5 = _mm256_madd_epi16(src_5, coeff_45);
        const __m256i res_7 = _mm256_madd_epi16(src_7, coeff_67);

        const __m256i res_odd = _mm256_add_epi32(
            _mm256_add_epi32(res_1, res_3), _mm256_add_epi32(res_5, res_7));

        // Rearrange pixels back into the order 0 ... 7
        const __m256i res_lo = _mm256_unpacklo_epi32(res_even, res_odd);
        const __m256i res_hi = _mm256_unpackhi_epi32(res_even, res_odd);

        const __m256i res_lo_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_lo, round_const), round_shift);
        const __m256i res_hi_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_hi, round_const), round_shift);

        // Accumulate values into the destination buffer
        __m128i *const p = (__m128i *)&dst[i * dst_stride + j];
        if (do_average) {
          _mm_storeu_si128(
              p + 0, _mm_add_epi32(_mm_loadu_si128(p + 0),
                                   _mm256_extractf128_si256(res_lo_round, 0)));
          _mm_storeu_si128(
              p + 1, _mm_add_epi32(_mm_loadu_si128(p + 1),
                                   _mm256_extractf128_si256(res_hi_round, 0)));
          if (w - j > 8) {
            _mm_storeu_si128(p + 2, _mm_add_epi32(_mm_loadu_si128(p + 2),
                                                  _mm256_extractf128_si256(
                                                      res_lo_round, 1)));
            _mm_storeu_si128(p + 3, _mm_add_epi32(_mm_loadu_si128(p + 3),
                                                  _mm256_extractf128_si256(
                                                      res_hi_round, 1)));
          }
        } else {
          _mm_storeu_si128(p + 0, _mm256_extractf128_si256(res_lo_round, 0));
          _mm_storeu_si128(p + 1, _mm256_extractf128_si256(res_hi_round, 0));
          if (w - j > 8) {
            _mm_storeu_si128(p + 2, _mm256_extractf128_si256(res_lo_round, 1));
            _mm_storeu_si128(p + 3, _mm256_extractf128_si256(res_hi_round, 1));
          }
        }
      }
    }
  }
}
#endif
