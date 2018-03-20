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

#include <tmmintrin.h>
#include <assert.h>

#include "./aom_dsp_rtcd.h"
#include "aom_dsp/aom_convolve.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/x86/convolve_sse2.h"
#include "av1/common/convolve.h"

#if !CONFIG_LOWPRECISION_BLEND
void av1_highbd_convolve_2d_ssse3(const uint16_t *src, int src_stride,
                                  uint16_t *dst0, int dst_stride0, int w, int h,
                                  InterpFilterParams *filter_params_x,
                                  InterpFilterParams *filter_params_y,
                                  const int subpel_x_q4, const int subpel_y_q4,
                                  ConvolveParams *conv_params, int bd) {
  DECLARE_ALIGNED(16, int16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = MAX_SB_SIZE;
  int i, j;
  const int do_average = conv_params->do_average;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint16_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;
  (void)dst0;
  (void)dst_stride0;
  // Check that, even with 12-bit input, the intermediate values will fit
  // into an unsigned 16-bit intermediate array.
  assert(bd + FILTER_BITS + 2 - conv_params->round_0 <= 16);

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
        const __m128i data2 =
            _mm_loadu_si128((__m128i *)&src_ptr[i * src_stride + j + 8]);

        // Filter even-index pixels
        const __m128i res_0 = _mm_madd_epi16(data, coeff_01);
        const __m128i res_2 =
            _mm_madd_epi16(_mm_alignr_epi8(data2, data, 4), coeff_23);
        const __m128i res_4 =
            _mm_madd_epi16(_mm_alignr_epi8(data2, data, 8), coeff_45);
        const __m128i res_6 =
            _mm_madd_epi16(_mm_alignr_epi8(data2, data, 12), coeff_67);

        __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_4),
                                         _mm_add_epi32(res_2, res_6));
        res_even =
            _mm_sra_epi32(_mm_add_epi32(res_even, round_const), round_shift);

        // Filter odd-index pixels
        const __m128i res_1 =
            _mm_madd_epi16(_mm_alignr_epi8(data2, data, 2), coeff_01);
        const __m128i res_3 =
            _mm_madd_epi16(_mm_alignr_epi8(data2, data, 6), coeff_23);
        const __m128i res_5 =
            _mm_madd_epi16(_mm_alignr_epi8(data2, data, 10), coeff_45);
        const __m128i res_7 =
            _mm_madd_epi16(_mm_alignr_epi8(data2, data, 14), coeff_67);

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
          _mm_storeu_si128(
              p + 0,
              _mm_srai_epi32(
                  _mm_add_epi32(_mm_loadu_si128(p + 0), res_lo_round), 1));
          _mm_storeu_si128(
              p + 1,
              _mm_srai_epi32(
                  _mm_add_epi32(_mm_loadu_si128(p + 1), res_hi_round), 1));
        } else {
          _mm_storeu_si128(p + 0, res_lo_round);
          _mm_storeu_si128(p + 1, res_hi_round);
        }
      }
    }
  }
}
#endif  // CONFIG_LOWPRECISION_BLEND

void av1_highbd_convolve_2d_sr_ssse3(const uint16_t *src, int src_stride,
                                     uint16_t *dst, int dst_stride, int w,
                                     int h, InterpFilterParams *filter_params_x,
                                     InterpFilterParams *filter_params_y,
                                     const int subpel_x_q4,
                                     const int subpel_y_q4,
                                     ConvolveParams *conv_params, int bd) {
  DECLARE_ALIGNED(32, int16_t, im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * 8]);
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = 8;
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint16_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

  // Check that, even with 12-bit input, the intermediate values will fit
  // into an unsigned 16-bit intermediate array.
  assert(bd + FILTER_BITS + 2 - conv_params->round_0 <= 16);
  __m128i coeffs_x[4], coeffs_y[4], s[16];

  const __m128i round_const_x = _mm_set1_epi32(
      ((1 << conv_params->round_0) >> 1) + (1 << (bd + FILTER_BITS - 1)));
  const __m128i round_shift_x = _mm_cvtsi32_si128(conv_params->round_0);

  const __m128i round_const_y =
      _mm_set1_epi32(((1 << conv_params->round_1) >> 1) -
                     (1 << (bd + 2 * FILTER_BITS - conv_params->round_0 - 1)));
  const __m128i round_shift_y = _mm_cvtsi32_si128(conv_params->round_1);

  const int bits =
      FILTER_BITS * 2 - conv_params->round_0 - conv_params->round_1;
  const __m128i round_shift_bits = _mm_cvtsi32_si128(bits);
  const __m128i round_const_bits = _mm_set1_epi32((1 << bits) >> 1);
  const __m128i clip_pixel =
      _mm_set1_epi16(bd == 10 ? 1023 : (bd == 12 ? 4095 : 255));
  const __m128i zero = _mm_setzero_si128();

  prepare_coeffs(filter_params_x, subpel_x_q4, coeffs_x);
  prepare_coeffs(filter_params_y, subpel_y_q4, coeffs_y);

  for (j = 0; j < w; j += 8) {
    /* Horizontal filter */
    {
      for (i = 0; i < im_h; i += 1) {
        const __m128i row00 =
            _mm_loadu_si128((__m128i *)&src_ptr[i * src_stride + j]);
        const __m128i row01 =
            _mm_loadu_si128((__m128i *)&src_ptr[i * src_stride + (j + 8)]);

        // even pixels
        s[0] = _mm_alignr_epi8(row01, row00, 0);
        s[1] = _mm_alignr_epi8(row01, row00, 4);
        s[2] = _mm_alignr_epi8(row01, row00, 8);
        s[3] = _mm_alignr_epi8(row01, row00, 12);

        __m128i res_even = convolve(s, coeffs_x);
        res_even = _mm_sra_epi32(_mm_add_epi32(res_even, round_const_x),
                                 round_shift_x);

        // odd pixels
        s[0] = _mm_alignr_epi8(row01, row00, 2);
        s[1] = _mm_alignr_epi8(row01, row00, 6);
        s[2] = _mm_alignr_epi8(row01, row00, 10);
        s[3] = _mm_alignr_epi8(row01, row00, 14);

        __m128i res_odd = convolve(s, coeffs_x);
        res_odd =
            _mm_sra_epi32(_mm_add_epi32(res_odd, round_const_x), round_shift_x);

        __m128i res_even1 = _mm_packs_epi32(res_even, res_even);
        __m128i res_odd1 = _mm_packs_epi32(res_odd, res_odd);
        __m128i res = _mm_unpacklo_epi16(res_even1, res_odd1);

        _mm_store_si128((__m128i *)&im_block[i * im_stride], res);
      }
    }
    /* Vertical filter */
    {
      __m128i s0 = _mm_loadu_si128((__m128i *)(im_block + 0 * im_stride));
      __m128i s1 = _mm_loadu_si128((__m128i *)(im_block + 1 * im_stride));
      __m128i s2 = _mm_loadu_si128((__m128i *)(im_block + 2 * im_stride));
      __m128i s3 = _mm_loadu_si128((__m128i *)(im_block + 3 * im_stride));
      __m128i s4 = _mm_loadu_si128((__m128i *)(im_block + 4 * im_stride));
      __m128i s5 = _mm_loadu_si128((__m128i *)(im_block + 5 * im_stride));
      __m128i s6 = _mm_loadu_si128((__m128i *)(im_block + 6 * im_stride));

      s[0] = _mm_unpacklo_epi16(s0, s1);
      s[1] = _mm_unpacklo_epi16(s2, s3);
      s[2] = _mm_unpacklo_epi16(s4, s5);

      s[4] = _mm_unpackhi_epi16(s0, s1);
      s[5] = _mm_unpackhi_epi16(s2, s3);
      s[6] = _mm_unpackhi_epi16(s4, s5);

      s[0 + 8] = _mm_unpacklo_epi16(s1, s2);
      s[1 + 8] = _mm_unpacklo_epi16(s3, s4);
      s[2 + 8] = _mm_unpacklo_epi16(s5, s6);

      s[4 + 8] = _mm_unpackhi_epi16(s1, s2);
      s[5 + 8] = _mm_unpackhi_epi16(s3, s4);
      s[6 + 8] = _mm_unpackhi_epi16(s5, s6);

      for (i = 0; i < h; i += 2) {
        const int16_t *data = &im_block[i * im_stride];

        __m128i s7 = _mm_loadu_si128((__m128i *)(data + 7 * im_stride));
        __m128i s8 = _mm_loadu_si128((__m128i *)(data + 8 * im_stride));

        s[3] = _mm_unpacklo_epi16(s6, s7);
        s[7] = _mm_unpackhi_epi16(s6, s7);

        s[3 + 8] = _mm_unpacklo_epi16(s7, s8);
        s[7 + 8] = _mm_unpackhi_epi16(s7, s8);

        const __m128i res_a0 = convolve(s, coeffs_y);
        __m128i res_a_round0 =
            _mm_sra_epi32(_mm_add_epi32(res_a0, round_const_y), round_shift_y);
        res_a_round0 = _mm_sra_epi32(
            _mm_add_epi32(res_a_round0, round_const_bits), round_shift_bits);

        const __m128i res_a1 = convolve(s + 8, coeffs_y);
        __m128i res_a_round1 =
            _mm_sra_epi32(_mm_add_epi32(res_a1, round_const_y), round_shift_y);
        res_a_round1 = _mm_sra_epi32(
            _mm_add_epi32(res_a_round1, round_const_bits), round_shift_bits);

        if (w - j > 4) {
          const __m128i res_b0 = convolve(s + 4, coeffs_y);
          __m128i res_b_round0 = _mm_sra_epi32(
              _mm_add_epi32(res_b0, round_const_y), round_shift_y);
          res_b_round0 = _mm_sra_epi32(
              _mm_add_epi32(res_b_round0, round_const_bits), round_shift_bits);

          const __m128i res_b1 = convolve(s + 4 + 8, coeffs_y);
          __m128i res_b_round1 = _mm_sra_epi32(
              _mm_add_epi32(res_b1, round_const_y), round_shift_y);
          res_b_round1 = _mm_sra_epi32(
              _mm_add_epi32(res_b_round1, round_const_bits), round_shift_bits);

          __m128i res_16bit0 = _mm_packs_epi32(res_a_round0, res_b_round0);
          res_16bit0 = _mm_min_epi16(res_16bit0, clip_pixel);
          res_16bit0 = _mm_max_epi16(res_16bit0, zero);

          __m128i res_16bit1 = _mm_packs_epi32(res_a_round1, res_b_round1);
          res_16bit1 = _mm_min_epi16(res_16bit1, clip_pixel);
          res_16bit1 = _mm_max_epi16(res_16bit1, zero);

          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j], res_16bit0);
          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j + dst_stride],
                           res_16bit1);
        } else if (w == 4) {
          res_a_round0 = _mm_packs_epi32(res_a_round0, res_a_round0);
          res_a_round0 = _mm_min_epi16(res_a_round0, clip_pixel);
          res_a_round0 = _mm_max_epi16(res_a_round0, zero);

          res_a_round1 = _mm_packs_epi32(res_a_round1, res_a_round1);
          res_a_round1 = _mm_min_epi16(res_a_round1, clip_pixel);
          res_a_round1 = _mm_max_epi16(res_a_round1, zero);

          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j], res_a_round0);
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j + dst_stride],
                           res_a_round1);
        } else {
          res_a_round0 = _mm_packs_epi32(res_a_round0, res_a_round0);
          res_a_round0 = _mm_min_epi16(res_a_round0, clip_pixel);
          res_a_round0 = _mm_max_epi16(res_a_round0, zero);

          res_a_round1 = _mm_packs_epi32(res_a_round1, res_a_round1);
          res_a_round1 = _mm_min_epi16(res_a_round1, clip_pixel);
          res_a_round1 = _mm_max_epi16(res_a_round1, zero);

          *((uint32_t *)(&dst[i * dst_stride + j])) =
              _mm_cvtsi128_si32(res_a_round0);

          *((uint32_t *)(&dst[i * dst_stride + j + dst_stride])) =
              _mm_cvtsi128_si32(res_a_round1);
        }
        s[0] = s[1];
        s[1] = s[2];
        s[2] = s[3];

        s[4] = s[5];
        s[5] = s[6];
        s[6] = s[7];

        s[0 + 8] = s[1 + 8];
        s[1 + 8] = s[2 + 8];
        s[2 + 8] = s[3 + 8];

        s[4 + 8] = s[5 + 8];
        s[5 + 8] = s[6 + 8];
        s[6 + 8] = s[7 + 8];

        s6 = s8;
      }
    }
  }
}
