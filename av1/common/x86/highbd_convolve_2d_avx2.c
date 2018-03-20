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
#include "aom_dsp/x86/convolve_avx2.h"
#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "av1/common/convolve.h"

void av1_highbd_convolve_2d_sr_avx2(const uint16_t *src, int src_stride,
                                    uint16_t *dst, int dst_stride, int w, int h,
                                    InterpFilterParams *filter_params_x,
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

  __m256i s[8], coeffs_y[4], coeffs_x[4];

  const __m256i round_const_x = _mm256_set1_epi32(
      ((1 << conv_params->round_0) >> 1) + (1 << (bd + FILTER_BITS - 1)));
  const __m128i round_shift_x = _mm_cvtsi32_si128(conv_params->round_0);

  const __m256i round_const_y = _mm256_set1_epi32(
      ((1 << conv_params->round_1) >> 1) -
      (1 << (bd + 2 * FILTER_BITS - conv_params->round_0 - 1)));
  const __m128i round_shift_y = _mm_cvtsi32_si128(conv_params->round_1);

  const int bits =
      FILTER_BITS * 2 - conv_params->round_0 - conv_params->round_1;
  const __m128i round_shift_bits = _mm_cvtsi32_si128(bits);
  const __m256i round_const_bits = _mm256_set1_epi32((1 << bits) >> 1);
  const __m256i clip_pixel =
      _mm256_set1_epi16(bd == 10 ? 1023 : (bd == 12 ? 4095 : 255));
  const __m256i zero = _mm256_setzero_si256();

  prepare_coeffs(filter_params_x, subpel_x_q4, coeffs_x);
  prepare_coeffs(filter_params_y, subpel_y_q4, coeffs_y);

  for (j = 0; j < w; j += 8) {
    /* Horizontal filter */
    {
      for (i = 0; i < im_h; i += 2) {
        const __m256i row0 =
            _mm256_loadu_si256((__m256i *)&src_ptr[i * src_stride + j]);
        __m256i row1 = _mm256_set1_epi16(0);
        if (i + 1 < im_h)
          row1 =
              _mm256_loadu_si256((__m256i *)&src_ptr[(i + 1) * src_stride + j]);

        const __m256i r0 = _mm256_permute2x128_si256(row0, row1, 0x20);
        const __m256i r1 = _mm256_permute2x128_si256(row0, row1, 0x31);

        // even pixels
        s[0] = _mm256_alignr_epi8(r1, r0, 0);
        s[1] = _mm256_alignr_epi8(r1, r0, 4);
        s[2] = _mm256_alignr_epi8(r1, r0, 8);
        s[3] = _mm256_alignr_epi8(r1, r0, 12);

        __m256i res_even = convolve(s, coeffs_x);
        res_even = _mm256_sra_epi32(_mm256_add_epi32(res_even, round_const_x),
                                    round_shift_x);

        // odd pixels
        s[0] = _mm256_alignr_epi8(r1, r0, 2);
        s[1] = _mm256_alignr_epi8(r1, r0, 6);
        s[2] = _mm256_alignr_epi8(r1, r0, 10);
        s[3] = _mm256_alignr_epi8(r1, r0, 14);

        __m256i res_odd = convolve(s, coeffs_x);
        res_odd = _mm256_sra_epi32(_mm256_add_epi32(res_odd, round_const_x),
                                   round_shift_x);

        __m256i res_even1 = _mm256_packs_epi32(res_even, res_even);
        __m256i res_odd1 = _mm256_packs_epi32(res_odd, res_odd);
        __m256i res = _mm256_unpacklo_epi16(res_even1, res_odd1);

        _mm256_store_si256((__m256i *)&im_block[i * im_stride], res);
      }
    }

    /* Vertical filter */
    {
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

        const __m256i res_a = convolve(s, coeffs_y);
        __m256i res_a_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_a, round_const_y), round_shift_y);

        res_a_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_a_round, round_const_bits), round_shift_bits);

        if (w - j > 4) {
          const __m256i res_b = convolve(s + 4, coeffs_y);
          __m256i res_b_round = _mm256_sra_epi32(
              _mm256_add_epi32(res_b, round_const_y), round_shift_y);
          res_b_round =
              _mm256_sra_epi32(_mm256_add_epi32(res_b_round, round_const_bits),
                               round_shift_bits);

          __m256i res_16bit = _mm256_packs_epi32(res_a_round, res_b_round);
          res_16bit = _mm256_min_epi16(res_16bit, clip_pixel);
          res_16bit = _mm256_max_epi16(res_16bit, zero);

          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j],
                           _mm256_castsi256_si128(res_16bit));
          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j + dst_stride],
                           _mm256_extracti128_si256(res_16bit, 1));
        } else if (w == 4) {
          res_a_round = _mm256_packs_epi32(res_a_round, res_a_round);
          res_a_round = _mm256_min_epi16(res_a_round, clip_pixel);
          res_a_round = _mm256_max_epi16(res_a_round, zero);

          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j],
                           _mm256_castsi256_si128(res_a_round));
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j + dst_stride],
                           _mm256_extracti128_si256(res_a_round, 1));
        } else {
          res_a_round = _mm256_packs_epi32(res_a_round, res_a_round);
          res_a_round = _mm256_min_epi16(res_a_round, clip_pixel);
          res_a_round = _mm256_max_epi16(res_a_round, zero);

          xx_storel_32((__m128i *)&dst[i * dst_stride + j],
                       _mm256_castsi256_si128(res_a_round));
          xx_storel_32((__m128i *)&dst[i * dst_stride + j + dst_stride],
                       _mm256_extracti128_si256(res_a_round, 1));
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

#if !CONFIG_LOWPRECISION_BLEND
void av1_highbd_convolve_2d_avx2(const uint16_t *src, int src_stride,
                                 uint16_t *dst0, int dst_stride0, int w, int h,
                                 InterpFilterParams *filter_params_x,
                                 InterpFilterParams *filter_params_y,
                                 const int subpel_x_q4, const int subpel_y_q4,
                                 ConvolveParams *conv_params, int bd) {
  DECLARE_ALIGNED(32, int16_t,
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
              p + 0, _mm_srai_epi32(_mm_add_epi32(_mm_loadu_si128(p + 0),
                                                  _mm256_extractf128_si256(
                                                      res_lo_round, 0)),
                                    1));
          _mm_storeu_si128(
              p + 1, _mm_srai_epi32(_mm_add_epi32(_mm_loadu_si128(p + 1),
                                                  _mm256_extractf128_si256(
                                                      res_hi_round, 0)),
                                    1));
          if (w - j > 8) {
            _mm_storeu_si128(
                p + 2, _mm_srai_epi32(_mm_add_epi32(_mm_loadu_si128(p + 2),
                                                    _mm256_extractf128_si256(
                                                        res_lo_round, 1)),
                                      1));
            _mm_storeu_si128(
                p + 3, _mm_srai_epi32(_mm_add_epi32(_mm_loadu_si128(p + 3),
                                                    _mm256_extractf128_si256(
                                                        res_hi_round, 1)),
                                      1));
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
#endif  // CONFIG_LOWPRECISION_BLEND

static INLINE void copy_64(const uint16_t *src, uint16_t *dst) {
  __m256i s[4];
  s[0] = _mm256_loadu_si256((__m256i *)(src + 0 * 16));
  s[1] = _mm256_loadu_si256((__m256i *)(src + 1 * 16));
  s[2] = _mm256_loadu_si256((__m256i *)(src + 2 * 16));
  s[3] = _mm256_loadu_si256((__m256i *)(src + 3 * 16));
  _mm256_storeu_si256((__m256i *)(dst + 0 * 16), s[0]);
  _mm256_storeu_si256((__m256i *)(dst + 1 * 16), s[1]);
  _mm256_storeu_si256((__m256i *)(dst + 2 * 16), s[2]);
  _mm256_storeu_si256((__m256i *)(dst + 3 * 16), s[3]);
}

static INLINE void copy_128(const uint16_t *src, uint16_t *dst) {
  __m256i s[8];
  s[0] = _mm256_loadu_si256((__m256i *)(src + 0 * 16));
  s[1] = _mm256_loadu_si256((__m256i *)(src + 1 * 16));
  s[2] = _mm256_loadu_si256((__m256i *)(src + 2 * 16));
  s[3] = _mm256_loadu_si256((__m256i *)(src + 3 * 16));
  s[4] = _mm256_loadu_si256((__m256i *)(src + 4 * 16));
  s[5] = _mm256_loadu_si256((__m256i *)(src + 5 * 16));
  s[6] = _mm256_loadu_si256((__m256i *)(src + 6 * 16));
  s[7] = _mm256_loadu_si256((__m256i *)(src + 7 * 16));

  _mm256_storeu_si256((__m256i *)(dst + 0 * 16), s[0]);
  _mm256_storeu_si256((__m256i *)(dst + 1 * 16), s[1]);
  _mm256_storeu_si256((__m256i *)(dst + 2 * 16), s[2]);
  _mm256_storeu_si256((__m256i *)(dst + 3 * 16), s[3]);
  _mm256_storeu_si256((__m256i *)(dst + 4 * 16), s[4]);
  _mm256_storeu_si256((__m256i *)(dst + 5 * 16), s[5]);
  _mm256_storeu_si256((__m256i *)(dst + 6 * 16), s[6]);
  _mm256_storeu_si256((__m256i *)(dst + 7 * 16), s[7]);
}

void av1_highbd_convolve_2d_copy_sr_avx2(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, InterpFilterParams *filter_params_x,
    InterpFilterParams *filter_params_y, const int subpel_x_q4,
    const int subpel_y_q4, ConvolveParams *conv_params, int bd) {
  (void)filter_params_x;
  (void)filter_params_y;
  (void)subpel_x_q4;
  (void)subpel_y_q4;
  (void)conv_params;
  (void)bd;

  if (w >= 16) {
    assert(!((intptr_t)dst % 16));
    assert(!(dst_stride % 16));
  }

  if (w == 2) {
    do {
      __m128i s = _mm_loadl_epi64((__m128i *)src);
      *(uint32_t *)dst = _mm_cvtsi128_si32(s);
      src += src_stride;
      dst += dst_stride;
      s = _mm_loadl_epi64((__m128i *)src);
      *(uint32_t *)dst = _mm_cvtsi128_si32(s);
      src += src_stride;
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 4) {
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
  } else if (w == 8) {
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
  } else if (w == 16) {
    do {
      __m256i s[2];
      s[0] = _mm256_loadu_si256((__m256i *)src);
      src += src_stride;
      s[1] = _mm256_loadu_si256((__m256i *)src);
      src += src_stride;
      _mm256_storeu_si256((__m256i *)dst, s[0]);
      dst += dst_stride;
      _mm256_storeu_si256((__m256i *)dst, s[1]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 32) {
    do {
      __m256i s[4];
      s[0] = _mm256_loadu_si256((__m256i *)(src + 0 * 16));
      s[1] = _mm256_loadu_si256((__m256i *)(src + 1 * 16));
      src += src_stride;
      s[2] = _mm256_loadu_si256((__m256i *)(src + 0 * 16));
      s[3] = _mm256_loadu_si256((__m256i *)(src + 1 * 16));
      src += src_stride;
      _mm256_storeu_si256((__m256i *)(dst + 0 * 16), s[0]);
      _mm256_storeu_si256((__m256i *)(dst + 1 * 16), s[1]);
      dst += dst_stride;
      _mm256_storeu_si256((__m256i *)(dst + 0 * 16), s[2]);
      _mm256_storeu_si256((__m256i *)(dst + 1 * 16), s[3]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 64) {
    do {
      copy_64(src, dst);
      src += src_stride;
      dst += dst_stride;
      copy_64(src, dst);
      src += src_stride;
      dst += dst_stride;
      h -= 2;
    } while (h);
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
  }
}
