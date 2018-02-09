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

#include <emmintrin.h>

#include "./aom_dsp_rtcd.h"
#include "aom_dsp/aom_convolve.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "av1/common/convolve.h"

void av1_convolve_2d_sse2(const uint8_t *src, int src_stride, uint8_t *dst0,
                          int dst_stride0, int w, int h,
                          InterpFilterParams *filter_params_x,
                          InterpFilterParams *filter_params_y,
                          const int subpel_x_q4, const int subpel_y_q4,
                          ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int bd = 8;
  (void)dst0;
  (void)dst_stride0;

  DECLARE_ALIGNED(16, int16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = MAX_SB_SIZE;
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int do_average = conv_params->do_average;
  const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

  const __m128i zero = _mm_setzero_si128();

  /* Horizontal filter */
  {
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

    const __m128i round_const = _mm_set1_epi32(
        ((1 << conv_params->round_0) >> 1) + (1 << (bd + FILTER_BITS - 1)));
    const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_0);

    for (i = 0; i < im_h; ++i) {
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

        __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_4),
                                         _mm_add_epi32(res_2, res_6));
        res_even =
            _mm_sra_epi32(_mm_add_epi32(res_even, round_const), round_shift);

        // Filter odd-index pixels
        const __m128i src_1 = _mm_unpacklo_epi8(_mm_srli_si128(data, 1), zero);
        const __m128i res_1 = _mm_madd_epi16(src_1, coeff_01);
        const __m128i src_3 = _mm_unpacklo_epi8(_mm_srli_si128(data, 3), zero);
        const __m128i res_3 = _mm_madd_epi16(src_3, coeff_23);
        const __m128i src_5 = _mm_unpacklo_epi8(_mm_srli_si128(data, 5), zero);
        const __m128i res_5 = _mm_madd_epi16(src_5, coeff_45);
        const __m128i src_7 = _mm_unpacklo_epi8(_mm_srli_si128(data, 7), zero);
        const __m128i res_7 = _mm_madd_epi16(src_7, coeff_67);

        __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_1, res_5),
                                        _mm_add_epi32(res_3, res_7));
        res_odd =
            _mm_sra_epi32(_mm_add_epi32(res_odd, round_const), round_shift);

        // Pack in the column order 0, 2, 4, 6, 1, 3, 5, 7
        __m128i res = _mm_packs_epi32(res_even, res_odd);
        _mm_storeu_si128((__m128i *)&im_block[i * im_stride + j], res);
      }
    }
  }

  /* Vertical filter */
  {
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

    const __m128i round_const = _mm_set1_epi32(
        ((1 << conv_params->round_1) >> 1) -
        (1 << (bd + 2 * FILTER_BITS - conv_params->round_0 - 1)));
    const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_1);

    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 8) {
        // Filter even-index pixels
        const int16_t *data = &im_block[i * im_stride + j];
        const __m128i src_0 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 0 * im_stride),
                               *(__m128i *)(data + 1 * im_stride));
        const __m128i src_2 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 2 * im_stride),
                               *(__m128i *)(data + 3 * im_stride));
        const __m128i src_4 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 4 * im_stride),
                               *(__m128i *)(data + 5 * im_stride));
        const __m128i src_6 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 6 * im_stride),
                               *(__m128i *)(data + 7 * im_stride));

        const __m128i res_0 = _mm_madd_epi16(src_0, coeff_01);
        const __m128i res_2 = _mm_madd_epi16(src_2, coeff_23);
        const __m128i res_4 = _mm_madd_epi16(src_4, coeff_45);
        const __m128i res_6 = _mm_madd_epi16(src_6, coeff_67);

        const __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_2),
                                               _mm_add_epi32(res_4, res_6));

        // Filter odd-index pixels
        const __m128i src_1 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 0 * im_stride),
                               *(__m128i *)(data + 1 * im_stride));
        const __m128i src_3 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 2 * im_stride),
                               *(__m128i *)(data + 3 * im_stride));
        const __m128i src_5 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 4 * im_stride),
                               *(__m128i *)(data + 5 * im_stride));
        const __m128i src_7 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 6 * im_stride),
                               *(__m128i *)(data + 7 * im_stride));

        const __m128i res_1 = _mm_madd_epi16(src_1, coeff_01);
        const __m128i res_3 = _mm_madd_epi16(src_3, coeff_23);
        const __m128i res_5 = _mm_madd_epi16(src_5, coeff_45);
        const __m128i res_7 = _mm_madd_epi16(src_7, coeff_67);

        const __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_1, res_3),
                                              _mm_add_epi32(res_5, res_7));

        // Rearrange pixels back into the order 0 ... 7
        const __m128i res_lo = _mm_unpacklo_epi32(res_even, res_odd);
        const __m128i res_hi = _mm_unpackhi_epi32(res_even, res_odd);

        const __m128i res_lo_round =
            _mm_sra_epi32(_mm_add_epi32(res_lo, round_const), round_shift);
        const __m128i res_hi_round =
            _mm_sra_epi32(_mm_add_epi32(res_hi, round_const), round_shift);

        // Accumulate values into the destination buffer
        __m128i *const p = (__m128i *)&dst[i * dst_stride + j];
        if (do_average) {
          _mm_storeu_si128(p + 0,
                           _mm_add_epi32(_mm_loadu_si128(p + 0), res_lo_round));
          _mm_storeu_si128(p + 1,
                           _mm_add_epi32(_mm_loadu_si128(p + 1), res_hi_round));
        } else {
          _mm_storeu_si128(p + 0, res_lo_round);
          _mm_storeu_si128(p + 1, res_hi_round);
        }
      }
    }
  }
}

void av1_convolve_2d_sr_sse2(const uint8_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h,
                             InterpFilterParams *filter_params_x,
                             InterpFilterParams *filter_params_y,
                             const int subpel_x_q4, const int subpel_y_q4,
                             ConvolveParams *conv_params) {
  const int bd = 8;

  DECLARE_ALIGNED(16, int16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = MAX_SB_SIZE;
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

  const __m128i zero = _mm_setzero_si128();
  const int bits =
      FILTER_BITS * 2 - conv_params->round_0 - conv_params->round_1;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;

  /* Horizontal filter */
  {
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

    const __m128i round_const = _mm_set1_epi32(
        ((1 << conv_params->round_0) >> 1) + (1 << (bd + FILTER_BITS - 1)));
    const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_0);

    for (i = 0; i < im_h; ++i) {
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

        __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_4),
                                         _mm_add_epi32(res_2, res_6));
        res_even =
            _mm_sra_epi32(_mm_add_epi32(res_even, round_const), round_shift);

        // Filter odd-index pixels
        const __m128i src_1 = _mm_unpacklo_epi8(_mm_srli_si128(data, 1), zero);
        const __m128i res_1 = _mm_madd_epi16(src_1, coeff_01);
        const __m128i src_3 = _mm_unpacklo_epi8(_mm_srli_si128(data, 3), zero);
        const __m128i res_3 = _mm_madd_epi16(src_3, coeff_23);
        const __m128i src_5 = _mm_unpacklo_epi8(_mm_srli_si128(data, 5), zero);
        const __m128i res_5 = _mm_madd_epi16(src_5, coeff_45);
        const __m128i src_7 = _mm_unpacklo_epi8(_mm_srli_si128(data, 7), zero);
        const __m128i res_7 = _mm_madd_epi16(src_7, coeff_67);

        __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_1, res_5),
                                        _mm_add_epi32(res_3, res_7));
        res_odd =
            _mm_sra_epi32(_mm_add_epi32(res_odd, round_const), round_shift);

        // Pack in the column order 0, 2, 4, 6, 1, 3, 5, 7
        __m128i res = _mm_packs_epi32(res_even, res_odd);
        _mm_storeu_si128((__m128i *)&im_block[i * im_stride + j], res);
      }
    }
  }

  /* Vertical filter */
  {
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

    const __m128i round_const = _mm_set1_epi32(
        ((((1 << bits) + 1) << conv_params->round_1) - (1 << offset_bits)) >>
        1);
    const __m128i round_shift = _mm_cvtsi32_si128(bits + conv_params->round_1);

    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 8) {
        // Filter even-index pixels
        const int16_t *data = &im_block[i * im_stride + j];
        const __m128i src_0 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 0 * im_stride),
                               *(__m128i *)(data + 1 * im_stride));
        const __m128i src_2 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 2 * im_stride),
                               *(__m128i *)(data + 3 * im_stride));
        const __m128i src_4 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 4 * im_stride),
                               *(__m128i *)(data + 5 * im_stride));
        const __m128i src_6 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 6 * im_stride),
                               *(__m128i *)(data + 7 * im_stride));

        const __m128i res_0 = _mm_madd_epi16(src_0, coeff_01);
        const __m128i res_2 = _mm_madd_epi16(src_2, coeff_23);
        const __m128i res_4 = _mm_madd_epi16(src_4, coeff_45);
        const __m128i res_6 = _mm_madd_epi16(src_6, coeff_67);

        const __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_2),
                                               _mm_add_epi32(res_4, res_6));

        // Filter odd-index pixels
        const __m128i src_1 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 0 * im_stride),
                               *(__m128i *)(data + 1 * im_stride));
        const __m128i src_3 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 2 * im_stride),
                               *(__m128i *)(data + 3 * im_stride));
        const __m128i src_5 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 4 * im_stride),
                               *(__m128i *)(data + 5 * im_stride));
        const __m128i src_7 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 6 * im_stride),
                               *(__m128i *)(data + 7 * im_stride));

        const __m128i res_1 = _mm_madd_epi16(src_1, coeff_01);
        const __m128i res_3 = _mm_madd_epi16(src_3, coeff_23);
        const __m128i res_5 = _mm_madd_epi16(src_5, coeff_45);
        const __m128i res_7 = _mm_madd_epi16(src_7, coeff_67);

        const __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_1, res_3),
                                              _mm_add_epi32(res_5, res_7));

        // Rearrange pixels back into the order 0 ... 7
        const __m128i res_lo = _mm_unpacklo_epi32(res_even, res_odd);
        const __m128i res_hi = _mm_unpackhi_epi32(res_even, res_odd);

        const __m128i res_lo_round =
            _mm_sra_epi32(_mm_add_epi32(res_lo, round_const), round_shift);
        const __m128i res_hi_round =
            _mm_sra_epi32(_mm_add_epi32(res_hi, round_const), round_shift);

        const __m128i res16 = _mm_packs_epi32(res_lo_round, res_hi_round);
        const __m128i res = _mm_packus_epi16(res16, res16);

        // Accumulate values into the destination buffer
        __m128i *const p = (__m128i *)&dst[i * dst_stride + j];

        if (w == 2) {
          *(uint16_t *)p = _mm_cvtsi128_si32(res);
        } else if (w == 4) {
          *(uint32_t *)p = _mm_cvtsi128_si32(res);
        } else {
          _mm_storel_epi64(p, res);
        }
      }
    }
  }
}

void av1_convolve_2d_copy_sse2(const uint8_t *src, int src_stride,
                               uint8_t *dst0, int dst_stride0, int w, int h,
                               InterpFilterParams *filter_params_x,
                               InterpFilterParams *filter_params_y,
                               const int subpel_x_q4, const int subpel_y_q4,
                               ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  (void)filter_params_x;
  (void)filter_params_y;
  (void)subpel_x_q4;
  (void)subpel_y_q4;
  (void)dst0;
  (void)dst_stride0;

  const int bits =
      FILTER_BITS * 2 - conv_params->round_1 - conv_params->round_0;
  const int do_average = conv_params->do_average;
  const __m128i zero = _mm_setzero_si128();
  const __m128i left_shift = _mm_cvtsi32_si128(bits);
  int i, j;

  if (!(w % 16)) {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 16) {
        const __m128i d8 = _mm_loadu_si128((__m128i *)&src[j]);
        const __m128i d16_0 = _mm_unpacklo_epi8(d8, zero);
        const __m128i d16_1 = _mm_unpackhi_epi8(d8, zero);
        __m128i d32_0 = _mm_unpacklo_epi16(d16_0, zero);
        __m128i d32_1 = _mm_unpackhi_epi16(d16_0, zero);
        __m128i d32_2 = _mm_unpacklo_epi16(d16_1, zero);
        __m128i d32_3 = _mm_unpackhi_epi16(d16_1, zero);

        d32_0 = _mm_sll_epi32(d32_0, left_shift);
        d32_1 = _mm_sll_epi32(d32_1, left_shift);
        d32_2 = _mm_sll_epi32(d32_2, left_shift);
        d32_3 = _mm_sll_epi32(d32_3, left_shift);

        __m128i *const p = (__m128i *)&dst[j];
        if (do_average) {
          _mm_storeu_si128(p + 0, _mm_add_epi32(_mm_loadu_si128(p + 0), d32_0));
          _mm_storeu_si128(p + 1, _mm_add_epi32(_mm_loadu_si128(p + 1), d32_1));
          _mm_storeu_si128(p + 2, _mm_add_epi32(_mm_loadu_si128(p + 2), d32_2));
          _mm_storeu_si128(p + 3, _mm_add_epi32(_mm_loadu_si128(p + 3), d32_3));
        } else {
          _mm_storeu_si128(p + 0, d32_0);
          _mm_storeu_si128(p + 1, d32_1);
          _mm_storeu_si128(p + 2, d32_2);
          _mm_storeu_si128(p + 3, d32_3);
        }
      }
      src += src_stride;
      dst += dst_stride;
    }
  } else if (!(w % 8)) {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 8) {
        const __m128i d8 = _mm_loadl_epi64((__m128i *)&src[j]);
        const __m128i d16_0 = _mm_unpacklo_epi8(d8, zero);
        __m128i d32_0 = _mm_unpacklo_epi16(d16_0, zero);
        __m128i d32_1 = _mm_unpackhi_epi16(d16_0, zero);

        d32_0 = _mm_sll_epi32(d32_0, left_shift);
        d32_1 = _mm_sll_epi32(d32_1, left_shift);

        __m128i *const p = (__m128i *)&dst[j];
        if (do_average) {
          _mm_storeu_si128(p + 0, _mm_add_epi32(_mm_loadu_si128(p + 0), d32_0));
          _mm_storeu_si128(p + 1, _mm_add_epi32(_mm_loadu_si128(p + 1), d32_1));
        } else {
          _mm_storeu_si128(p + 0, d32_0);
          _mm_storeu_si128(p + 1, d32_1);
        }
      }
      src += src_stride;
      dst += dst_stride;
    }
  } else if (!(w % 4)) {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 4) {
        const __m128i d8 = _mm_cvtsi32_si128(*(const int *)&src[j]);
        const __m128i d16_0 = _mm_unpacklo_epi8(d8, zero);
        __m128i d32_0 = _mm_unpacklo_epi16(d16_0, zero);

        d32_0 = _mm_sll_epi32(d32_0, left_shift);
        __m128i *const p = (__m128i *)&dst[j];
        if (do_average) {
          _mm_storeu_si128(p, _mm_add_epi32(_mm_loadu_si128(p), d32_0));
        } else {
          _mm_storeu_si128(p, d32_0);
        }
      }
      src += src_stride;
      dst += dst_stride;
    }
  } else {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 2) {
        const __m128i d8 = _mm_cvtsi32_si128(*(const int *)&src[j]);
        const __m128i d16_0 = _mm_unpacklo_epi8(d8, zero);
        __m128i d32_0 = _mm_unpacklo_epi16(d16_0, zero);

        d32_0 = _mm_sll_epi32(d32_0, left_shift);
        __m128i *const p = (__m128i *)&dst[j];
        if (do_average) {
          _mm_storel_epi64(p, _mm_add_epi32(_mm_loadl_epi64(p), d32_0));
        } else {
          _mm_storel_epi64(p, d32_0);
        }
      }
      src += src_stride;
      dst += dst_stride;
    }
  }
}

#if CONFIG_EXT_PARTITION
static INLINE void copy_128(const uint8_t *src, uint8_t *dst) {
  __m128i s[8];
  s[0] = _mm_loadu_si128((__m128i *)(src + 0 * 16));
  s[1] = _mm_loadu_si128((__m128i *)(src + 1 * 16));
  s[2] = _mm_loadu_si128((__m128i *)(src + 2 * 16));
  s[3] = _mm_loadu_si128((__m128i *)(src + 3 * 16));
  s[4] = _mm_loadu_si128((__m128i *)(src + 4 * 16));
  s[5] = _mm_loadu_si128((__m128i *)(src + 5 * 16));
  s[6] = _mm_loadu_si128((__m128i *)(src + 6 * 16));
  s[7] = _mm_loadu_si128((__m128i *)(src + 7 * 16));
  _mm_store_si128((__m128i *)(dst + 0 * 16), s[0]);
  _mm_store_si128((__m128i *)(dst + 1 * 16), s[1]);
  _mm_store_si128((__m128i *)(dst + 2 * 16), s[2]);
  _mm_store_si128((__m128i *)(dst + 3 * 16), s[3]);
  _mm_store_si128((__m128i *)(dst + 4 * 16), s[4]);
  _mm_store_si128((__m128i *)(dst + 5 * 16), s[5]);
  _mm_store_si128((__m128i *)(dst + 6 * 16), s[6]);
  _mm_store_si128((__m128i *)(dst + 7 * 16), s[7]);
}
#endif

void av1_convolve_2d_copy_sr_sse2(const uint8_t *src, int src_stride,
                                  uint8_t *dst, int dst_stride, int w, int h,
                                  InterpFilterParams *filter_params_x,
                                  InterpFilterParams *filter_params_y,
                                  const int subpel_x_q4, const int subpel_y_q4,
                                  ConvolveParams *conv_params) {
  (void)filter_params_x;
  (void)filter_params_y;
  (void)subpel_x_q4;
  (void)subpel_y_q4;
  (void)conv_params;

  if (w >= 16) {
    assert(!((intptr_t)dst % 16));
    assert(!(dst_stride % 16));
  }

  if (w == 2) {
    do {
      *(uint16_t *)dst = *(uint16_t *)src;
      src += src_stride;
      dst += dst_stride;
      *(uint16_t *)dst = *(uint16_t *)src;
      src += src_stride;
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 4) {
    do {
      *(uint32_t *)dst = *(uint32_t *)src;
      src += src_stride;
      dst += dst_stride;
      *(uint32_t *)dst = *(uint32_t *)src;
      src += src_stride;
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 8) {
    do {
      __m128i s[2];
      s[0] = _mm_loadl_epi64((__m128i *)src);
      src += src_stride;
      s[1] = _mm_loadl_epi64((__m128i *)src);
      src += src_stride;
      _mm_storel_epi64((__m128i *)dst, s[0]);
      dst += dst_stride;
      _mm_storel_epi64((__m128i *)dst, s[1]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 16) {
    do {
      __m128i s[2];
      s[0] = _mm_loadu_si128((__m128i *)src);
      src += src_stride;
      s[1] = _mm_loadu_si128((__m128i *)src);
      src += src_stride;
      _mm_store_si128((__m128i *)dst, s[0]);
      dst += dst_stride;
      _mm_store_si128((__m128i *)dst, s[1]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 32) {
    do {
      __m128i s[4];
      s[0] = _mm_loadu_si128((__m128i *)(src + 0 * 16));
      s[1] = _mm_loadu_si128((__m128i *)(src + 1 * 16));
      src += src_stride;
      s[2] = _mm_loadu_si128((__m128i *)(src + 0 * 16));
      s[3] = _mm_loadu_si128((__m128i *)(src + 1 * 16));
      src += src_stride;
      _mm_store_si128((__m128i *)(dst + 0 * 16), s[0]);
      _mm_store_si128((__m128i *)(dst + 1 * 16), s[1]);
      dst += dst_stride;
      _mm_store_si128((__m128i *)(dst + 0 * 16), s[2]);
      _mm_store_si128((__m128i *)(dst + 1 * 16), s[3]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 64) {
    do {
      __m128i s[8];
      s[0] = _mm_loadu_si128((__m128i *)(src + 0 * 16));
      s[1] = _mm_loadu_si128((__m128i *)(src + 1 * 16));
      s[2] = _mm_loadu_si128((__m128i *)(src + 2 * 16));
      s[3] = _mm_loadu_si128((__m128i *)(src + 3 * 16));
      src += src_stride;
      s[4] = _mm_loadu_si128((__m128i *)(src + 0 * 16));
      s[5] = _mm_loadu_si128((__m128i *)(src + 1 * 16));
      s[6] = _mm_loadu_si128((__m128i *)(src + 2 * 16));
      s[7] = _mm_loadu_si128((__m128i *)(src + 3 * 16));
      src += src_stride;
      _mm_store_si128((__m128i *)(dst + 0 * 16), s[0]);
      _mm_store_si128((__m128i *)(dst + 1 * 16), s[1]);
      _mm_store_si128((__m128i *)(dst + 2 * 16), s[2]);
      _mm_store_si128((__m128i *)(dst + 3 * 16), s[3]);
      dst += dst_stride;
      _mm_store_si128((__m128i *)(dst + 0 * 16), s[4]);
      _mm_store_si128((__m128i *)(dst + 1 * 16), s[5]);
      _mm_store_si128((__m128i *)(dst + 2 * 16), s[6]);
      _mm_store_si128((__m128i *)(dst + 3 * 16), s[7]);
      dst += dst_stride;
      h -= 2;
    } while (h);
#if CONFIG_EXT_PARTITION
  } else {
    do {
      copy_128(src, dst);
      src += src_stride;
      dst += dst_stride;
      copy_128(src, dst);
      src += src_stride;
      dst += dst_stride;
      h -= 2;
    } while (h);
#endif  // CONFIG_EXT_PARTITION
  }
}

#if CONFIG_JNT_COMP
void av1_jnt_convolve_2d_copy_sse2(const uint8_t *src, int src_stride,
                                   uint8_t *dst0, int dst_stride0, int w, int h,
                                   InterpFilterParams *filter_params_x,
                                   InterpFilterParams *filter_params_y,
                                   const int subpel_x_q4, const int subpel_y_q4,
                                   ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  (void)filter_params_x;
  (void)filter_params_y;
  (void)subpel_x_q4;
  (void)subpel_y_q4;
  (void)dst0;
  (void)dst_stride0;

  const int bits =
      FILTER_BITS * 2 - conv_params->round_1 - conv_params->round_0;
  const int do_average = conv_params->do_average;
  const __m128i zero = _mm_setzero_si128();
  const __m128i left_shift = _mm_cvtsi32_si128(bits);
  int i, j;

  const int w0 = conv_params->fwd_offset;
  const int w1 = conv_params->bck_offset;
  const __m128i wt0 = _mm_set1_epi32(w0);
  const __m128i wt1 = _mm_set1_epi32(w1);

  if (!(w % 16)) {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 16) {
        const __m128i d8 = _mm_loadu_si128((__m128i *)&src[j]);
        const __m128i d16_0 = _mm_unpacklo_epi8(d8, zero);
        const __m128i d16_1 = _mm_unpackhi_epi8(d8, zero);
        __m128i d32_0 = _mm_unpacklo_epi16(d16_0, zero);
        __m128i d32_1 = _mm_unpackhi_epi16(d16_0, zero);
        __m128i d32_2 = _mm_unpacklo_epi16(d16_1, zero);
        __m128i d32_3 = _mm_unpackhi_epi16(d16_1, zero);

        __m128i *const p = (__m128i *)&dst[j];

        if (conv_params->use_jnt_comp_avg) {
          if (do_average) {
            __m128i mul = _mm_mullo_epi16(d32_0, wt1);
            __m128i weighted_res = _mm_sll_epi32(mul, left_shift);
            __m128i sum = _mm_add_epi32(_mm_loadu_si128(p + 0), weighted_res);
            d32_0 = sum;

            mul = _mm_mullo_epi16(d32_1, wt1);
            weighted_res = _mm_sll_epi32(mul, left_shift);
            sum = _mm_add_epi32(_mm_loadu_si128(p + 1), weighted_res);
            d32_1 = sum;

            mul = _mm_mullo_epi16(d32_2, wt1);
            weighted_res = _mm_sll_epi32(mul, left_shift);
            sum = _mm_add_epi32(_mm_loadu_si128(p + 2), weighted_res);
            d32_2 = sum;

            mul = _mm_mullo_epi16(d32_3, wt1);
            weighted_res = _mm_sll_epi32(mul, left_shift);
            sum = _mm_add_epi32(_mm_loadu_si128(p + 3), weighted_res);
            d32_3 = sum;
          } else {
            d32_0 = _mm_sll_epi32(_mm_mullo_epi16(d32_0, wt0), left_shift);
            d32_1 = _mm_sll_epi32(_mm_mullo_epi16(d32_1, wt0), left_shift);
            d32_2 = _mm_sll_epi32(_mm_mullo_epi16(d32_2, wt0), left_shift);
            d32_3 = _mm_sll_epi32(_mm_mullo_epi16(d32_3, wt0), left_shift);
          }
        } else {
          if (do_average) {
            d32_0 = _mm_add_epi32(_mm_loadu_si128(p + 0),
                                  _mm_sll_epi32(d32_0, left_shift));
            d32_1 = _mm_add_epi32(_mm_loadu_si128(p + 1),
                                  _mm_sll_epi32(d32_1, left_shift));
            d32_2 = _mm_add_epi32(_mm_loadu_si128(p + 2),
                                  _mm_sll_epi32(d32_2, left_shift));
            d32_3 = _mm_add_epi32(_mm_loadu_si128(p + 3),
                                  _mm_sll_epi32(d32_3, left_shift));
          } else {
            d32_0 = _mm_sll_epi32(d32_0, left_shift);
            d32_1 = _mm_sll_epi32(d32_1, left_shift);
            d32_2 = _mm_sll_epi32(d32_2, left_shift);
            d32_3 = _mm_sll_epi32(d32_3, left_shift);
          }
        }

        _mm_storeu_si128(p + 0, d32_0);
        _mm_storeu_si128(p + 1, d32_1);
        _mm_storeu_si128(p + 2, d32_2);
        _mm_storeu_si128(p + 3, d32_3);
      }
      src += src_stride;
      dst += dst_stride;
    }
  } else if (!(w % 8)) {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 8) {
        const __m128i d8 = _mm_loadl_epi64((__m128i *)&src[j]);
        const __m128i d16_0 = _mm_unpacklo_epi8(d8, zero);
        __m128i d32_0 = _mm_unpacklo_epi16(d16_0, zero);
        __m128i d32_1 = _mm_unpackhi_epi16(d16_0, zero);

        __m128i *const p = (__m128i *)&dst[j];
        if (conv_params->use_jnt_comp_avg) {
          if (do_average) {
            __m128i mul = _mm_mullo_epi16(d32_0, wt1);
            __m128i weighted_res = _mm_sll_epi32(mul, left_shift);
            __m128i sum = _mm_add_epi32(_mm_loadu_si128(p + 0), weighted_res);
            d32_0 = sum;

            mul = _mm_mullo_epi16(d32_1, wt1);
            weighted_res = _mm_sll_epi32(mul, left_shift);
            sum = _mm_add_epi32(_mm_loadu_si128(p + 1), weighted_res);
            d32_1 = sum;
          } else {
            d32_0 = _mm_sll_epi32(_mm_mullo_epi16(d32_0, wt0), left_shift);
            d32_1 = _mm_sll_epi32(_mm_mullo_epi16(d32_1, wt0), left_shift);
          }
        } else {
          if (do_average) {
            d32_0 = _mm_add_epi32(_mm_loadu_si128(p + 0),
                                  _mm_sll_epi32(d32_0, left_shift));
            d32_1 = _mm_add_epi32(_mm_loadu_si128(p + 1),
                                  _mm_sll_epi32(d32_1, left_shift));
          } else {
            d32_0 = _mm_sll_epi32(d32_0, left_shift);
            d32_1 = _mm_sll_epi32(d32_1, left_shift);
          }
        }

        _mm_storeu_si128(p + 0, d32_0);
        _mm_storeu_si128(p + 1, d32_1);
      }
      src += src_stride;
      dst += dst_stride;
    }
  } else if (!(w % 4)) {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 4) {
        const __m128i d8 = _mm_loadl_epi64((__m128i *)&src[j]);
        const __m128i d16_0 = _mm_unpacklo_epi8(d8, zero);
        __m128i d32_0 = _mm_unpacklo_epi16(d16_0, zero);

        __m128i *const p = (__m128i *)&dst[j];
        if (conv_params->use_jnt_comp_avg) {
          if (do_average) {
            __m128i mul = _mm_mullo_epi16(d32_0, wt1);
            __m128i weighted_res = _mm_sll_epi32(mul, left_shift);
            __m128i sum = _mm_add_epi32(_mm_loadu_si128(p + 0), weighted_res);
            d32_0 = sum;
          } else {
            d32_0 = _mm_sll_epi32(_mm_mullo_epi16(d32_0, wt0), left_shift);
          }
        } else {
          if (do_average) {
            d32_0 = _mm_add_epi32(_mm_loadu_si128(p + 0),
                                  _mm_sll_epi32(d32_0, left_shift));
          } else {
            d32_0 = _mm_sll_epi32(d32_0, left_shift);
          }
        }

        _mm_storeu_si128(p, d32_0);
      }
      src += src_stride;
      dst += dst_stride;
    }
  } else {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 2) {
        const __m128i d8 = _mm_cvtsi32_si128(*(const int *)&src[j]);
        const __m128i d16_0 = _mm_unpacklo_epi8(d8, zero);
        __m128i d32_0 = _mm_unpacklo_epi16(d16_0, zero);

        __m128i *const p = (__m128i *)&dst[j];
        if (conv_params->use_jnt_comp_avg) {
          if (do_average) {
            __m128i mul = _mm_mullo_epi16(d32_0, wt1);
            __m128i weighted_res = _mm_sll_epi32(mul, left_shift);
            __m128i sum = _mm_add_epi32(_mm_loadl_epi64(p), weighted_res);
            d32_0 = sum;
          } else {
            d32_0 = _mm_sll_epi32(_mm_mullo_epi16(d32_0, wt0), left_shift);
          }
        } else {
          if (do_average) {
            d32_0 = _mm_add_epi32(_mm_loadl_epi64(p),
                                  _mm_sll_epi32(d32_0, left_shift));
          } else {
            d32_0 = _mm_sll_epi32(d32_0, left_shift);
          }
        }

        _mm_storel_epi64(p, d32_0);
      }
      src += src_stride;
      dst += dst_stride;
    }
  }
}
#endif  // CONFIG_JNT_COMP
