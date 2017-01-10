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

#include "./av1_rtcd.h"
#include "av1/common/warped_motion.h"

/* SSE2 version of the rotzoom/affine warp filter */
void warp_affine_sse2(int32_t *mat, uint8_t *ref, int width, int height,
                      int stride, uint8_t *pred, int p_col, int p_row,
                      int p_width, int p_height, int p_stride,
                      int subsampling_x, int subsampling_y, int ref_frm,
                      int32_t alpha, int32_t beta, int32_t gamma,
                      int32_t delta) {
  __m128i tmp[15];
  int i, j, k, l;

  /* Note: For this code to work, the left/right frame borders need to be
     extended by at least 13 pixels each. By the time we get here, other
     code will have set up this border, but we allow an explicit check
     for debugging purposes.
  */
  /*for (i = 0; i < height; ++i) {
    for (j = 0; j < 13; ++j) {
      assert(ref[i * stride - 13 + j] == ref[i * stride]);
      assert(ref[i * stride + width + j] == ref[i * stride + (width - 1)]);
    }
  }*/

  for (i = p_row; i < p_row + p_height; i += 8) {
    for (j = p_col; j < p_col + p_width; j += 8) {
      int32_t x4, y4, ix4, sx4, iy4, sy4;
      if (subsampling_x)
        x4 = ROUND_POWER_OF_TWO_SIGNED(
            mat[2] * 2 * (j + 4) + mat[3] * 2 * (i + 4) + mat[0] +
                (mat[2] + mat[3] - (1 << WARPEDMODEL_PREC_BITS)) / 2,
            1);
      else
        x4 = mat[2] * (j + 4) + mat[3] * (i + 4) + mat[0];

      if (subsampling_y)
        y4 = ROUND_POWER_OF_TWO_SIGNED(
            mat[4] * 2 * (j + 4) + mat[5] * 2 * (i + 4) + mat[1] +
                (mat[4] + mat[5] - (1 << WARPEDMODEL_PREC_BITS)) / 2,
            1);
      else
        y4 = mat[4] * (j + 4) + mat[5] * (i + 4) + mat[1];

      ix4 = x4 >> WARPEDMODEL_PREC_BITS;
      sx4 = x4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);
      iy4 = y4 >> WARPEDMODEL_PREC_BITS;
      sy4 = y4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);

      // Horizontal filter
      for (k = -7; k < 8; ++k) {
        int iy = iy4 + k;
        if (iy < 0)
          iy = 0;
        else if (iy > height - 1)
          iy = height - 1;

        if (ix4 <= -7) {
          // In this case, the rightmost pixel sampled is in column
          // ix4 + 3 + 7 - 3 = ix4 + 7 <= 0, ie. the entire block
          // will sample only from the leftmost column
          // (once border extension is taken into account)
          for (l = 0; l < 8; ++l) {
            ((int16_t *)tmp)[(k + 7) * 8 + l] =
                ref[iy * stride] * (1 << WARPEDPIXEL_FILTER_BITS);
          }
        } else if (ix4 >= width + 6) {
          // In this case, the leftmost pixel sampled is in column
          // ix4 - 3 + 0 - 3 = ix4 - 6 >= width, ie. the entire block
          // will sample only from the rightmost column
          // (once border extension is taken into account)
          for (l = 0; l < 8; ++l) {
            ((int16_t *)tmp)[(k + 7) * 8 + l] =
                ref[iy * stride + (width - 1)] * (1 << WARPEDPIXEL_FILTER_BITS);
          }
        } else {
          // If we get here, then
          // the leftmost pixel sampled is
          // ix4 - 4 + 0 - 3 = ix4 - 7 >= -13
          // and the rightmost pixel sampled is at most
          // ix4 + 3 + 7 - 3 = ix4 + 7 <= width + 12
          // So, assuming that border extension has been done, we
          // don't need to explicitly clamp values.
          int sx = sx4 + alpha * (-4) + beta * k;

          // Load per-pixel coefficients
          __m128i coeffs[8];
          for (l = 0; l < 8; ++l) {
            coeffs[l] =
                ((__m128i *)warped_filter)[ROUND_POWER_OF_TWO_SIGNED(
                                               sx, WARPEDDIFF_PREC_BITS) +
                                           WARPEDPIXEL_PREC_SHIFTS];
            sx += alpha;
          }

          // Currently the coefficients are stored in the order
          // 0 ... 7 for each pixel separately.
          // Permute the coefficients into 8 registers:
          // coeffs (0,1), (2,3), (4,5), (6,7) for
          // i) the odd-index pixels, and ii) the even-index pixels
          {
            // 0 1 0 1 2 3 2 3 for 0, 2
            __m128i tmp_0 = _mm_unpacklo_epi32(coeffs[0], coeffs[2]);
            // 0 1 0 1 2 3 2 3 for 1, 3
            __m128i tmp_1 = _mm_unpacklo_epi32(coeffs[1], coeffs[3]);
            // 0 1 0 1 2 3 2 3 for 4, 6
            __m128i tmp_2 = _mm_unpacklo_epi32(coeffs[4], coeffs[6]);
            // 0 1 0 1 2 3 2 3 for 5, 7
            __m128i tmp_3 = _mm_unpacklo_epi32(coeffs[5], coeffs[7]);
            // 4 5 4 5 6 7 6 7 for 0, 2
            __m128i tmp_4 = _mm_unpackhi_epi32(coeffs[0], coeffs[2]);
            // 4 5 4 5 6 7 6 7 for 1, 3
            __m128i tmp_5 = _mm_unpackhi_epi32(coeffs[1], coeffs[3]);
            // 4 5 4 5 6 7 6 7 for 4, 6
            __m128i tmp_6 = _mm_unpackhi_epi32(coeffs[4], coeffs[6]);
            // 4 5 4 5 6 7 6 7 for 5, 7
            __m128i tmp_7 = _mm_unpackhi_epi32(coeffs[5], coeffs[7]);

            // 0 1 0 1 0 1 0 1 for 0, 2, 4, 6
            __m128i coeff_a = _mm_unpacklo_epi64(tmp_0, tmp_2);
            // 0 1 0 1 0 1 0 1 for 1, 3, 5, 7
            __m128i coeff_b = _mm_unpacklo_epi64(tmp_1, tmp_3);
            // 2 3 2 3 2 3 2 3 for 0, 2, 4, 6
            __m128i coeff_c = _mm_unpackhi_epi64(tmp_0, tmp_2);
            // 2 3 2 3 2 3 2 3 for 1, 3, 5, 7
            __m128i coeff_d = _mm_unpackhi_epi64(tmp_1, tmp_3);
            // 4 5 4 5 4 5 4 5 for 0, 2, 4, 6
            __m128i coeff_e = _mm_unpacklo_epi64(tmp_4, tmp_6);
            // 4 5 4 5 4 5 4 5 for 1, 3, 5, 7
            __m128i coeff_f = _mm_unpacklo_epi64(tmp_5, tmp_7);
            // 6 7 6 7 6 7 6 7 for 0, 2, 4, 6
            __m128i coeff_g = _mm_unpackhi_epi64(tmp_4, tmp_6);
            // 6 7 6 7 6 7 6 7 for 1, 3, 5, 7
            __m128i coeff_h = _mm_unpackhi_epi64(tmp_5, tmp_7);

            // Load source pixels and widen to 16 bits. We want the pixels
            // ref[iy * stride + ix4 - 7] ... ref[iy * stride + ix4 + 7]
            // inclusive.
            __m128i zero = _mm_setzero_si128();
            __m128i src =
                _mm_loadu_si128((__m128i *)(ref + iy * stride + ix4 - 7));

            // Calculate filtered results
            __m128i src_a = _mm_unpacklo_epi8(src, zero);
            __m128i res_a = _mm_madd_epi16(src_a, coeff_a);
            __m128i src_b = _mm_unpacklo_epi8(_mm_srli_si128(src, 1), zero);
            __m128i res_b = _mm_madd_epi16(src_b, coeff_b);
            __m128i src_c = _mm_unpacklo_epi8(_mm_srli_si128(src, 2), zero);
            __m128i res_c = _mm_madd_epi16(src_c, coeff_c);
            __m128i src_d = _mm_unpacklo_epi8(_mm_srli_si128(src, 3), zero);
            __m128i res_d = _mm_madd_epi16(src_d, coeff_d);
            __m128i src_e = _mm_unpacklo_epi8(_mm_srli_si128(src, 4), zero);
            __m128i res_e = _mm_madd_epi16(src_e, coeff_e);
            __m128i src_f = _mm_unpacklo_epi8(_mm_srli_si128(src, 5), zero);
            __m128i res_f = _mm_madd_epi16(src_f, coeff_f);
            __m128i src_g = _mm_unpacklo_epi8(_mm_srli_si128(src, 6), zero);
            __m128i res_g = _mm_madd_epi16(src_g, coeff_g);
            __m128i src_h = _mm_unpacklo_epi8(_mm_srli_si128(src, 7), zero);
            __m128i res_h = _mm_madd_epi16(src_h, coeff_h);

            __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_a, res_e),
                                             _mm_add_epi32(res_c, res_g));
            __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_b, res_f),
                                            _mm_add_epi32(res_d, res_h));

            // Combine results into one register.
            // We store the columns in the order 0, 2, 4, 6, 1, 3, 5, 7
            // as this order helps with the vertical filter.
            tmp[k + 7] = _mm_packs_epi32(res_even, res_odd);
          }
        }
      }

      // Vertical filter
      for (k = -4; k < AOMMIN(4, p_row + p_height - i - 4); ++k) {
        int sy = sy4 + gamma * (-4) + delta * k;

        // Load per-pixel coefficients
        __m128i coeffs[8];
        for (l = 0; l < 8; ++l) {
          coeffs[l] = ((__m128i *)warped_filter)[ROUND_POWER_OF_TWO_SIGNED(
                                                     sy, WARPEDDIFF_PREC_BITS) +
                                                 WARPEDPIXEL_PREC_SHIFTS];
          sy += gamma;
        }

        {
          // Rearrange coefficients in the same way as for the horizontal filter
          __m128i tmp_0 = _mm_unpacklo_epi32(coeffs[0], coeffs[2]);
          __m128i tmp_1 = _mm_unpacklo_epi32(coeffs[1], coeffs[3]);
          __m128i tmp_2 = _mm_unpacklo_epi32(coeffs[4], coeffs[6]);
          __m128i tmp_3 = _mm_unpacklo_epi32(coeffs[5], coeffs[7]);
          __m128i tmp_4 = _mm_unpackhi_epi32(coeffs[0], coeffs[2]);
          __m128i tmp_5 = _mm_unpackhi_epi32(coeffs[1], coeffs[3]);
          __m128i tmp_6 = _mm_unpackhi_epi32(coeffs[4], coeffs[6]);
          __m128i tmp_7 = _mm_unpackhi_epi32(coeffs[5], coeffs[7]);

          __m128i coeff_a = _mm_unpacklo_epi64(tmp_0, tmp_2);
          __m128i coeff_b = _mm_unpacklo_epi64(tmp_1, tmp_3);
          __m128i coeff_c = _mm_unpackhi_epi64(tmp_0, tmp_2);
          __m128i coeff_d = _mm_unpackhi_epi64(tmp_1, tmp_3);
          __m128i coeff_e = _mm_unpacklo_epi64(tmp_4, tmp_6);
          __m128i coeff_f = _mm_unpacklo_epi64(tmp_5, tmp_7);
          __m128i coeff_g = _mm_unpackhi_epi64(tmp_4, tmp_6);
          __m128i coeff_h = _mm_unpackhi_epi64(tmp_5, tmp_7);

          // Load from tmp and rearrange pairs of consecutive rows into the
          // column order 0 0 2 2 4 4 6 6; 1 1 3 3 5 5 7 7
          __m128i *src = tmp + (k + 4);
          __m128i src_a = _mm_unpacklo_epi16(src[0], src[1]);
          __m128i src_b = _mm_unpackhi_epi16(src[0], src[1]);
          __m128i src_c = _mm_unpacklo_epi16(src[2], src[3]);
          __m128i src_d = _mm_unpackhi_epi16(src[2], src[3]);
          __m128i src_e = _mm_unpacklo_epi16(src[4], src[5]);
          __m128i src_f = _mm_unpackhi_epi16(src[4], src[5]);
          __m128i src_g = _mm_unpacklo_epi16(src[6], src[7]);
          __m128i src_h = _mm_unpackhi_epi16(src[6], src[7]);

          // Calculate the filtered pixels
          __m128i res_a = _mm_madd_epi16(src_a, coeff_a);
          __m128i res_b = _mm_madd_epi16(src_b, coeff_b);
          __m128i res_c = _mm_madd_epi16(src_c, coeff_c);
          __m128i res_d = _mm_madd_epi16(src_d, coeff_d);
          __m128i res_e = _mm_madd_epi16(src_e, coeff_e);
          __m128i res_f = _mm_madd_epi16(src_f, coeff_f);
          __m128i res_g = _mm_madd_epi16(src_g, coeff_g);
          __m128i res_h = _mm_madd_epi16(src_h, coeff_h);

          __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_a, res_c),
                                           _mm_add_epi32(res_e, res_g));
          __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_b, res_d),
                                          _mm_add_epi32(res_f, res_h));

          // Round and pack down to pixels
          __m128i res_lo = _mm_unpacklo_epi32(res_even, res_odd);
          __m128i res_hi = _mm_unpackhi_epi32(res_even, res_odd);

          __m128i round_const =
              _mm_set1_epi32((1 << (2 * WARPEDPIXEL_FILTER_BITS)) >> 1);

          __m128i res_lo_round = _mm_srai_epi32(
              _mm_add_epi32(res_lo, round_const), 2 * WARPEDPIXEL_FILTER_BITS);
          __m128i res_hi_round = _mm_srai_epi32(
              _mm_add_epi32(res_hi, round_const), 2 * WARPEDPIXEL_FILTER_BITS);

          __m128i res_16bit = _mm_packs_epi32(res_lo_round, res_hi_round);
          __m128i res_8bit = _mm_packus_epi16(res_16bit, res_16bit);

          // Store, blending with 'pred' if needed
          __m128i *p =
              (__m128i *)&pred[(i - p_row + k + 4) * p_stride + (j - p_col)];

          if (ref_frm) {
            __m128i orig = _mm_loadl_epi64(p);
            _mm_storel_epi64(p, _mm_avg_epu8(res_8bit, orig));
          } else {
            _mm_storel_epi64(p, res_8bit);
          }
        }
      }
    }
  }
}
