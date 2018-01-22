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
#include "./av1_rtcd.h"
#include "aom_dsp/aom_convolve.h"
#include "aom_dsp/x86/convolve_avx2.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/x86/synonyms.h"
#include "av1/common/convolve.h"

void av1_convolve_2d_avx2(const uint8_t *src, int src_stride, uint8_t *dst0,
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

  DECLARE_ALIGNED(32, int16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = MAX_SB_SIZE;
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const int do_average = conv_params->do_average;
  const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

  __m256i filt[4], s[4];

  filt[0] = _mm256_load_si256((__m256i const *)filt1_global_avx2);
  filt[1] = _mm256_load_si256((__m256i const *)filt2_global_avx2);
  filt[2] = _mm256_load_si256((__m256i const *)filt3_global_avx2);
  filt[3] = _mm256_load_si256((__m256i const *)filt4_global_avx2);

  /* Horizontal filter */
  {
    const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
        *filter_params_x, subpel_x_q4 & SUBPEL_MASK);

    const __m128i coeffs_x8 = _mm_loadu_si128((__m128i *)x_filter);
    // since not all compilers yet support _mm256_set_m128i()
    const __m256i coeffs_x = _mm256_insertf128_si256(
        _mm256_castsi128_si256(coeffs_x8), coeffs_x8, 1);

    // right shift all filter co-efficients by 1 to reduce the bits required.
    // This extra right shift will be taken care of at the end while rounding
    // the result.
    // Since all filter co-efficients are even, this change will not affect the
    // end result
    const __m256i coeffs_x_1 = _mm256_srai_epi16(coeffs_x, 1);

    // coeffs 0 1 0 1 0 1 0 1
    const __m256i coeff_01 =
        _mm256_shuffle_epi8(coeffs_x_1, _mm256_set1_epi16(0x0200u));
    // coeffs 2 3 2 3 2 3 2 3
    const __m256i coeff_23 =
        _mm256_shuffle_epi8(coeffs_x_1, _mm256_set1_epi16(0x0604u));
    // coeffs 4 5 4 5 4 5 4 5
    const __m256i coeff_45 =
        _mm256_shuffle_epi8(coeffs_x_1, _mm256_set1_epi16(0x0a08u));
    // coeffs 6 7 6 7 6 7 6 7
    const __m256i coeff_67 =
        _mm256_shuffle_epi8(coeffs_x_1, _mm256_set1_epi16(0x0e0cu));

    const __m256i round_const =
        _mm256_set1_epi16(((1 << (conv_params->round_0 - 1)) >> 1) +
                          (1 << (bd + FILTER_BITS - 2)));
    const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_0 - 1);

    for (i = 0; i < im_h; ++i) {
      for (j = 0; j < w; j += 16) {
        // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 8 9 10 11 12 13 14 15 16 17 18
        // 19 20 21 22 23
        const __m256i data = _mm256_inserti128_si256(
            _mm256_loadu_si256((__m256i *)&src_ptr[(i * src_stride) + j]),
            _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + (j + 8)]),
            1);

        // filter the source buffer
        s[0] = _mm256_shuffle_epi8(data, filt[0]);
        s[1] = _mm256_shuffle_epi8(data, filt[1]);
        s[2] = _mm256_shuffle_epi8(data, filt[2]);
        s[3] = _mm256_shuffle_epi8(data, filt[3]);

        const __m256i res_0 = _mm256_maddubs_epi16(s[0], coeff_01);
        const __m256i res_1 = _mm256_maddubs_epi16(s[1], coeff_23);
        const __m256i res_2 = _mm256_maddubs_epi16(s[2], coeff_45);
        const __m256i res_3 = _mm256_maddubs_epi16(s[3], coeff_67);

        const __m256i res_a = _mm256_add_epi16(res_0, res_2);
        const __m256i res_b = _mm256_add_epi16(res_1, res_3);

        __m256i res = _mm256_add_epi16(res_a, res_b);
        res = _mm256_sra_epi16(_mm256_add_epi16(res, round_const), round_shift);
        res = _mm256_permute4x64_epi64(res, 216);

        // 0 1 2 3 8 9 10 11 4 5 6 7 12 13 14 15
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
        // Filter 0 1 2 3 4 5 6 7
        const int16_t *data = &im_block[i * im_stride + j];
        const __m256i src_0 =
            _mm256_unpacklo_epi16(*(__m256i *)(data + 0 * im_stride),
                                  *(__m256i *)(data + 1 * im_stride));
        const __m256i src_1 =
            _mm256_unpacklo_epi16(*(__m256i *)(data + 2 * im_stride),
                                  *(__m256i *)(data + 3 * im_stride));
        const __m256i src_2 =
            _mm256_unpacklo_epi16(*(__m256i *)(data + 4 * im_stride),
                                  *(__m256i *)(data + 5 * im_stride));
        const __m256i src_3 =
            _mm256_unpacklo_epi16(*(__m256i *)(data + 6 * im_stride),
                                  *(__m256i *)(data + 7 * im_stride));

        const __m256i res_0 = _mm256_madd_epi16(src_0, coeff_01);
        const __m256i res_1 = _mm256_madd_epi16(src_1, coeff_23);
        const __m256i res_2 = _mm256_madd_epi16(src_2, coeff_45);
        const __m256i res_3 = _mm256_madd_epi16(src_3, coeff_67);

        const __m256i res_a = _mm256_add_epi32(_mm256_add_epi32(res_0, res_1),
                                               _mm256_add_epi32(res_2, res_3));

        // Filter 8 9 10 11 12 13 14 15
        const __m256i src_4 =
            _mm256_unpackhi_epi16(*(__m256i *)(data + 0 * im_stride),
                                  *(__m256i *)(data + 1 * im_stride));
        const __m256i src_5 =
            _mm256_unpackhi_epi16(*(__m256i *)(data + 2 * im_stride),
                                  *(__m256i *)(data + 3 * im_stride));
        const __m256i src_6 =
            _mm256_unpackhi_epi16(*(__m256i *)(data + 4 * im_stride),
                                  *(__m256i *)(data + 5 * im_stride));
        const __m256i src_7 =
            _mm256_unpackhi_epi16(*(__m256i *)(data + 6 * im_stride),
                                  *(__m256i *)(data + 7 * im_stride));

        const __m256i res_4 = _mm256_madd_epi16(src_4, coeff_01);
        const __m256i res_5 = _mm256_madd_epi16(src_5, coeff_23);
        const __m256i res_6 = _mm256_madd_epi16(src_6, coeff_45);
        const __m256i res_7 = _mm256_madd_epi16(src_7, coeff_67);

        const __m256i res_b = _mm256_add_epi32(_mm256_add_epi32(res_4, res_5),
                                               _mm256_add_epi32(res_6, res_7));

        const __m256i res_a_round =
            _mm256_sra_epi32(_mm256_add_epi32(res_a, round_const), round_shift);
        const __m256i res_b_round =
            _mm256_sra_epi32(_mm256_add_epi32(res_b, round_const), round_shift);

        // Accumulate values into the destination buffer
        __m256i *const p = (__m256i *)&dst[i * dst_stride + j];
        if (do_average) {
          _mm256_storeu_si256(
              p + 0, _mm256_add_epi32(_mm256_loadu_si256(p + 0), res_a_round));
          if (w - j > 8) {
            _mm256_storeu_si256(
                p + 1,
                _mm256_add_epi32(_mm256_loadu_si256(p + 1), res_b_round));
          }
        } else {
          _mm256_storeu_si256(p + 0, res_a_round);
          if (w - j > 8) {
            _mm256_storeu_si256(p + 1, res_b_round);
          }
        }
      }
    }
  }
}

void av1_convolve_2d_sr_avx2(const uint8_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h,
                             InterpFilterParams *filter_params_x,
                             InterpFilterParams *filter_params_y,
                             const int subpel_x_q4, const int subpel_y_q4,
                             ConvolveParams *conv_params) {
  const int bd = 8;

  DECLARE_ALIGNED(32, int16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = MAX_SB_SIZE;
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

  __m256i filt[4], s[4];

  filt[0] = _mm256_load_si256((__m256i const *)filt1_global_avx2);
  filt[1] = _mm256_load_si256((__m256i const *)filt2_global_avx2);
  filt[2] = _mm256_load_si256((__m256i const *)filt3_global_avx2);
  filt[3] = _mm256_load_si256((__m256i const *)filt4_global_avx2);

  /* Horizontal filter */
  {
    const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
        *filter_params_x, subpel_x_q4 & SUBPEL_MASK);

    const __m128i coeffs_x8 = _mm_loadu_si128((__m128i *)x_filter);
    // since not all compilers yet support _mm256_set_m128i()
    const __m256i coeffs_x = _mm256_insertf128_si256(
        _mm256_castsi128_si256(coeffs_x8), coeffs_x8, 1);

    // right shift all filter co-efficients by 1 to reduce the bits required.
    // This extra right shift will be taken care of at the end while rounding
    // the result.
    // Since all filter co-efficients are even, this change will not affect
    // the end result
    const __m256i coeffs_x_1 = _mm256_srai_epi16(coeffs_x, 1);

    // coeffs 0 1 0 1 0 1 0 1
    const __m256i coeff_01 =
        _mm256_shuffle_epi8(coeffs_x_1, _mm256_set1_epi16(0x0200u));
    // coeffs 2 3 2 3 2 3 2 3
    const __m256i coeff_23 =
        _mm256_shuffle_epi8(coeffs_x_1, _mm256_set1_epi16(0x0604u));
    // coeffs 4 5 4 5 4 5 4 5
    const __m256i coeff_45 =
        _mm256_shuffle_epi8(coeffs_x_1, _mm256_set1_epi16(0x0a08u));
    // coeffs 6 7 6 7 6 7 6 7
    const __m256i coeff_67 =
        _mm256_shuffle_epi8(coeffs_x_1, _mm256_set1_epi16(0x0e0cu));

    const __m256i round_const =
        _mm256_set1_epi16(((1 << (conv_params->round_0 - 1)) >> 1) +
                          (1 << (bd + FILTER_BITS - 2)));
    const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_0 - 1);

    for (i = 0; i < im_h; ++i) {
      for (j = 0; j < w; j += 16) {
        // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 8 9 10 11 12 13 14 15 16 17
        // 18 19 20 21 22 23
        const __m256i data = _mm256_inserti128_si256(
            _mm256_loadu_si256((__m256i *)&src_ptr[(i * src_stride) + j]),
            _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + (j + 8)]),
            1);

        // filter the source buffer
        s[0] = _mm256_shuffle_epi8(data, filt[0]);
        s[1] = _mm256_shuffle_epi8(data, filt[1]);
        s[2] = _mm256_shuffle_epi8(data, filt[2]);
        s[3] = _mm256_shuffle_epi8(data, filt[3]);

        const __m256i res_0 = _mm256_maddubs_epi16(s[0], coeff_01);
        const __m256i res_1 = _mm256_maddubs_epi16(s[1], coeff_23);
        const __m256i res_2 = _mm256_maddubs_epi16(s[2], coeff_45);
        const __m256i res_3 = _mm256_maddubs_epi16(s[3], coeff_67);

        const __m256i res_a = _mm256_add_epi16(res_0, res_2);
        const __m256i res_b = _mm256_add_epi16(res_1, res_3);

        __m256i res = _mm256_add_epi16(res_a, res_b);
        res = _mm256_sra_epi16(_mm256_add_epi16(res, round_const), round_shift);

        // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
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
        (1 << (bd + 2 * FILTER_BITS - conv_params->round_0 - 1)) +
        ((1 << (2 * FILTER_BITS - conv_params->round_0)) >> 1));
    const __m128i round_shift =
        _mm_cvtsi32_si128(2 * FILTER_BITS - conv_params->round_0);

    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 16) {
        // Filter 0 1 2 3 8 9 10 11
        const int16_t *data = &im_block[i * im_stride + j];
        const __m256i src_0 =
            _mm256_unpacklo_epi16(*(__m256i *)(data + 0 * im_stride),
                                  *(__m256i *)(data + 1 * im_stride));
        const __m256i src_1 =
            _mm256_unpacklo_epi16(*(__m256i *)(data + 2 * im_stride),
                                  *(__m256i *)(data + 3 * im_stride));
        const __m256i src_2 =
            _mm256_unpacklo_epi16(*(__m256i *)(data + 4 * im_stride),
                                  *(__m256i *)(data + 5 * im_stride));
        const __m256i src_3 =
            _mm256_unpacklo_epi16(*(__m256i *)(data + 6 * im_stride),
                                  *(__m256i *)(data + 7 * im_stride));

        const __m256i res_0 = _mm256_madd_epi16(src_0, coeff_01);
        const __m256i res_1 = _mm256_madd_epi16(src_1, coeff_23);
        const __m256i res_2 = _mm256_madd_epi16(src_2, coeff_45);
        const __m256i res_3 = _mm256_madd_epi16(src_3, coeff_67);

        const __m256i res_a = _mm256_add_epi32(_mm256_add_epi32(res_0, res_1),
                                               _mm256_add_epi32(res_2, res_3));

        // Filter 4 5 6 7 12 13 14 15
        const __m256i src_4 =
            _mm256_unpackhi_epi16(*(__m256i *)(data + 0 * im_stride),
                                  *(__m256i *)(data + 1 * im_stride));
        const __m256i src_5 =
            _mm256_unpackhi_epi16(*(__m256i *)(data + 2 * im_stride),
                                  *(__m256i *)(data + 3 * im_stride));
        const __m256i src_6 =
            _mm256_unpackhi_epi16(*(__m256i *)(data + 4 * im_stride),
                                  *(__m256i *)(data + 5 * im_stride));
        const __m256i src_7 =
            _mm256_unpackhi_epi16(*(__m256i *)(data + 6 * im_stride),
                                  *(__m256i *)(data + 7 * im_stride));

        const __m256i res_4 = _mm256_madd_epi16(src_4, coeff_01);
        const __m256i res_5 = _mm256_madd_epi16(src_5, coeff_23);
        const __m256i res_6 = _mm256_madd_epi16(src_6, coeff_45);
        const __m256i res_7 = _mm256_madd_epi16(src_7, coeff_67);

        const __m256i res_b = _mm256_add_epi32(_mm256_add_epi32(res_4, res_5),
                                               _mm256_add_epi32(res_6, res_7));

        // Combine V round and 2F-H-V round into a single rounding
        const __m256i res_a_round =
            _mm256_sra_epi32(_mm256_add_epi32(res_a, round_const), round_shift);
        const __m256i res_b_round =
            _mm256_sra_epi32(_mm256_add_epi32(res_b, round_const), round_shift);

        /* rounding code */
        // 16 bit conversion
        const __m256i res_16bit = _mm256_packs_epi32(res_a_round, res_b_round);
        // 8 bit conversion and saturation to uint8
        __m256i res_8b = _mm256_packus_epi16(res_16bit, res_16bit);
        res_8b = _mm256_permute4x64_epi64(res_8b, 216);
        // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
        const __m128i res = _mm256_castsi256_si128(res_8b);

        // Store values into the destination buffer
        __m128i *const p = (__m128i *)&dst[i * dst_stride + j];
        if (w - j > 8) {
          _mm_storeu_si128(p, res);
        } else if (w - j > 4) {
          _mm_storel_epi64(p, res);
        } else if (w == 4) {
          xx_storel_32(&dst[i * dst_stride + j], res);
        } else {
          *(uint16_t *)p = _mm_cvtsi128_si32(res);
        }
      }
    }
  }
}
