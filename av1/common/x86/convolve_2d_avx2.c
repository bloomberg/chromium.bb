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

  DECLARE_ALIGNED(32, int16_t, im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * 8]);
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = 8;
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;
  const __m256i avg_mask = _mm256_set1_epi32(conv_params->do_average ? -1 : 0);

  __m256i filt[4], s[8], coeffs_x[4], coeffs_y[4];

  filt[0] = _mm256_load_si256((__m256i const *)filt1_global_avx2);
  filt[1] = _mm256_load_si256((__m256i const *)filt2_global_avx2);
  filt[2] = _mm256_load_si256((__m256i const *)filt3_global_avx2);
  filt[3] = _mm256_load_si256((__m256i const *)filt4_global_avx2);

  prepare_coeffs(filter_params_x, subpel_x_q4, coeffs_x);
  prepare_coeffs_y_2d(filter_params_y, subpel_y_q4, coeffs_y);

  for (j = 0; j < w; j += 8) {
    /* Horizontal filter */
    {
      const __m256i round_const =
          _mm256_set1_epi16(((1 << (conv_params->round_0 - 1)) >> 1) +
                            (1 << (bd + FILTER_BITS - 2)));
      const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_0 - 1);

      for (i = 0; i < im_h; i += 2) {
        __m256i data = _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + j]));
        if (i + 1 < im_h)
          data = _mm256_inserti128_si256(
              data,
              _mm_loadu_si128(
                  (__m128i *)&src_ptr[(i * src_stride) + j + src_stride]),
              1);
        __m256i res = convolve_x(data, coeffs_x, filt);

        res = _mm256_sra_epi16(_mm256_add_epi16(res, round_const), round_shift);

        // 0 1 2 3 8 9 10 11 4 5 6 7 12 13 14 15
        _mm256_store_si256((__m256i *)&im_block[i * im_stride], res);
      }
    }

    /* Vertical filter */
    {
      const __m256i round_const = _mm256_set1_epi32(
          ((1 << conv_params->round_1) >> 1) -
          (1 << (bd + 2 * FILTER_BITS - conv_params->round_0 - 1)));
      const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_1);

      __m256i s0 = _mm256_loadu_si256((__m256i *)(im_block + 0 * im_stride));
      __m256i s1 = _mm256_loadu_si256((__m256i *)(im_block + 1 * im_stride));
      __m256i s2 = _mm256_loadu_si256((__m256i *)(im_block + 2 * im_stride));
      __m256i s3 = _mm256_loadu_si256((__m256i *)(im_block + 3 * im_stride));
      __m256i s4 = _mm256_loadu_si256((__m256i *)(im_block + 4 * im_stride));
      __m256i s5 = _mm256_loadu_si256((__m256i *)(im_block + 5 * im_stride));

      s[0] = _mm256_unpacklo_epi16(s0, s1);
      s[1] = _mm256_unpacklo_epi16(s2, s3);
      s[2] = _mm256_unpacklo_epi16(s4, s5);

      s[4] = _mm256_unpackhi_epi16(s0, s1);
      s[5] = _mm256_unpackhi_epi16(s2, s3);
      s[6] = _mm256_unpackhi_epi16(s4, s5);

      for (i = 0; i < h; i += 2) {
        const int16_t *data = &im_block[i * im_stride];

        const __m256i s6 =
            _mm256_loadu_si256((__m256i *)(data + 6 * im_stride));
        const __m256i s7 =
            _mm256_loadu_si256((__m256i *)(data + 7 * im_stride));

        s[3] = _mm256_unpacklo_epi16(s6, s7);
        s[7] = _mm256_unpackhi_epi16(s6, s7);

        const __m256i res_a = convolve_y_2d(s, coeffs_y);
        const __m256i res_b = convolve_y_2d(s + 4, coeffs_y);

        const __m256i res_a_round =
            _mm256_sra_epi32(_mm256_add_epi32(res_a, round_const), round_shift);
        const __m256i res_b_round =
            _mm256_sra_epi32(_mm256_add_epi32(res_b, round_const), round_shift);

        if (w - j > 4) {
          const __m256i res_ax =
              _mm256_permute2x128_si256(res_a_round, res_b_round, 0x20);
          const __m256i res_bx =
              _mm256_permute2x128_si256(res_a_round, res_b_round, 0x31);

          add_store_aligned(&dst[i * dst_stride + j], &res_ax, &avg_mask);
          add_store_aligned(&dst[i * dst_stride + j + dst_stride], &res_bx,
                            &avg_mask);
        } else {
          const __m128i res_ax = _mm256_extracti128_si256(res_a_round, 0);
          const __m128i res_bx = _mm256_extracti128_si256(res_a_round, 1);

          __m128i r0 = _mm_load_si128((__m128i *)&dst[i * dst_stride + j]);
          __m128i r1 =
              _mm_load_si128((__m128i *)&dst[i * dst_stride + j + dst_stride]);
          r0 = _mm_and_si128(r0, _mm256_extracti128_si256(avg_mask, 0));
          r1 = _mm_and_si128(r1, _mm256_extracti128_si256(avg_mask, 0));
          r0 = _mm_add_epi32(r0, res_ax);
          r1 = _mm_add_epi32(r1, res_bx);
          _mm_store_si128((__m128i *)&dst[i * dst_stride + j], r0);
          _mm_store_si128((__m128i *)&dst[i * dst_stride + j + dst_stride], r1);
        }

        s[0] = s[1];
        s[1] = s[2];
        s[2] = s[3];

        s[4] = s[5];
        s[5] = s[6];
        s[6] = s[7];
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

  DECLARE_ALIGNED(32, int16_t, im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * 8]);
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = 8;
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

  __m256i filt[4], coeffs_h[4], coeffs_v[4];

  filt[0] = _mm256_load_si256((__m256i const *)filt1_global_avx2);
  filt[1] = _mm256_load_si256((__m256i const *)filt2_global_avx2);
  filt[2] = _mm256_load_si256((__m256i const *)filt3_global_avx2);
  filt[3] = _mm256_load_si256((__m256i const *)filt4_global_avx2);

  prepare_coeffs(filter_params_x, subpel_x_q4, coeffs_h);
  prepare_coeffs_y_2d(filter_params_y, subpel_y_q4, coeffs_v);

  const __m256i round_const_h = _mm256_set1_epi16(
      ((1 << (conv_params->round_0 - 1)) >> 1) + (1 << (bd + FILTER_BITS - 2)));
  const __m128i round_shift_h = _mm_cvtsi32_si128(conv_params->round_0 - 1);

  const __m256i round_const_v = _mm256_set1_epi32(
      ((1 << conv_params->round_1) >> 1) -
      (1 << (bd + 2 * FILTER_BITS - conv_params->round_0 - 1)) +
      ((1 << (2 * FILTER_BITS - conv_params->round_0)) >> 1));
  const __m128i round_shift_v =
      _mm_cvtsi32_si128(2 * FILTER_BITS - conv_params->round_0);

  for (j = 0; j < w; j += 8) {
    for (i = 0; i < im_h; i += 2) {
      __m256i data = _mm256_castsi128_si256(
          _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + j]));

      // Load the next line
      if (i + 1 < im_h)
        data = _mm256_inserti128_si256(
            data,
            _mm_loadu_si128(
                (__m128i *)&src_ptr[(i * src_stride) + j + src_stride]),
            1);

      __m256i res = convolve_x(data, coeffs_h, filt);

      res =
          _mm256_sra_epi16(_mm256_add_epi16(res, round_const_h), round_shift_h);

      _mm256_store_si256((__m256i *)&im_block[i * im_stride], res);
    }

    /* Vertical filter */
    {
      __m256i src_0 = _mm256_loadu_si256((__m256i *)(im_block + 0 * im_stride));
      __m256i src_1 = _mm256_loadu_si256((__m256i *)(im_block + 1 * im_stride));
      __m256i src_2 = _mm256_loadu_si256((__m256i *)(im_block + 2 * im_stride));
      __m256i src_3 = _mm256_loadu_si256((__m256i *)(im_block + 3 * im_stride));
      __m256i src_4 = _mm256_loadu_si256((__m256i *)(im_block + 4 * im_stride));
      __m256i src_5 = _mm256_loadu_si256((__m256i *)(im_block + 5 * im_stride));

      __m256i s[8];
      s[0] = _mm256_unpacklo_epi16(src_0, src_1);
      s[1] = _mm256_unpacklo_epi16(src_2, src_3);
      s[2] = _mm256_unpacklo_epi16(src_4, src_5);

      s[4] = _mm256_unpackhi_epi16(src_0, src_1);
      s[5] = _mm256_unpackhi_epi16(src_2, src_3);
      s[6] = _mm256_unpackhi_epi16(src_4, src_5);

      for (i = 0; i < h; i += 2) {
        const int16_t *data = &im_block[i * im_stride];

        const __m256i s6 =
            _mm256_loadu_si256((__m256i *)(data + 6 * im_stride));
        const __m256i s7 =
            _mm256_loadu_si256((__m256i *)(data + 7 * im_stride));

        s[3] = _mm256_unpacklo_epi16(s6, s7);
        s[7] = _mm256_unpackhi_epi16(s6, s7);

        const __m256i res_a = convolve_y_2d(s, coeffs_v);
        const __m256i res_b = convolve_y_2d(s + 4, coeffs_v);

        // Combine V round and 2F-H-V round into a single rounding
        const __m256i res_a_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_a, round_const_v), round_shift_v);
        const __m256i res_b_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_b, round_const_v), round_shift_v);

        /* rounding code */
        // 16 bit conversion
        const __m256i res_16bit = _mm256_packs_epi32(res_a_round, res_b_round);
        // 8 bit conversion and saturation to uint8
        const __m256i res_8b = _mm256_packus_epi16(res_16bit, res_16bit);

        const __m128i res_0 = _mm256_castsi256_si128(res_8b);
        const __m128i res_1 = _mm256_extracti128_si256(res_8b, 1);

        // Store values into the destination buffer
        __m128i *const p_0 = (__m128i *)&dst[i * dst_stride + j];
        __m128i *const p_1 = (__m128i *)&dst[i * dst_stride + j + dst_stride];
        if (w - j > 4) {
          _mm_storel_epi64(p_0, res_0);
          _mm_storel_epi64(p_1, res_1);
        } else if (w == 4) {
          xx_storel_32(p_0, res_0);
          xx_storel_32(p_1, res_1);
        } else {
          *(uint16_t *)p_0 = _mm_cvtsi128_si32(res_0);
          *(uint16_t *)p_1 = _mm_cvtsi128_si32(res_1);
        }

        s[0] = s[1];
        s[1] = s[2];
        s[2] = s[3];

        s[4] = s[5];
        s[5] = s[6];
        s[6] = s[7];
      }
    }
  }
}
