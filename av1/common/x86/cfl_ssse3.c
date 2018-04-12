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

#include <tmmintrin.h>

#include "./av1_rtcd.h"

#include "av1/common/cfl.h"

#include "av1/common/x86/cfl_simd.h"

/**
 * Adds 4 pixels (in a 2x2 grid) and multiplies them by 2. Resulting in a more
 * precise version of a box filter 4:2:0 pixel subsampling in Q3.
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 */
static INLINE void cfl_luma_subsampling_420_lbd_ssse3(const uint8_t *input,
                                                      int input_stride,
                                                      int16_t *pred_buf_q3,
                                                      int width, int height) {
  const __m128i twos = _mm_set1_epi8(2);
  const int16_t *end = pred_buf_q3 + (height >> 1) * CFL_BUF_LINE;
  const int luma_stride = input_stride << 1;

  __m128i top, bot, next_top, next_bot, top_16x8, bot_16x8, next_top_16x8,
      next_bot_16x8, sum_16x8, next_sum_16x8;
  do {
    if (width == 4) {
      top = _mm_cvtsi32_si128(*((int *)input));
      bot = _mm_cvtsi32_si128(*((int *)(input + input_stride)));
    } else if (width == 8) {
      top = _mm_loadl_epi64((__m128i *)input);
      bot = _mm_loadl_epi64((__m128i *)(input + input_stride));
    } else {
      top = _mm_loadu_si128((__m128i *)input);
      bot = _mm_loadu_si128((__m128i *)(input + input_stride));
      if (width == 32) {
        next_top = _mm_loadu_si128((__m128i *)(input + 16));
        next_bot = _mm_loadu_si128((__m128i *)(input + 16 + input_stride));
      }
    }

    top_16x8 = _mm_maddubs_epi16(top, twos);
    bot_16x8 = _mm_maddubs_epi16(bot, twos);
    sum_16x8 = _mm_add_epi16(top_16x8, bot_16x8);
    if (width == 32) {
      next_top_16x8 = _mm_maddubs_epi16(next_top, twos);
      next_bot_16x8 = _mm_maddubs_epi16(next_bot, twos);
      next_sum_16x8 = _mm_add_epi16(next_top_16x8, next_bot_16x8);
    }

    if (width == 4) {
      *((int *)pred_buf_q3) = _mm_cvtsi128_si32(sum_16x8);
    } else if (width == 8) {
      _mm_storel_epi64((__m128i *)pred_buf_q3, sum_16x8);
    } else {
      _mm_storeu_si128((__m128i *)pred_buf_q3, sum_16x8);
      if (width == 32) {
        _mm_storeu_si128((__m128i *)(pred_buf_q3 + 8), next_sum_16x8);
      }
    }

    input += luma_stride;
    pred_buf_q3 += CFL_BUF_LINE;
  } while (pred_buf_q3 < end);
}

/**
 * Adds 2 pixels (in a 2x1 grid) and multiplies them by 4. Resulting in a more
 * precise version of a box filter 4:2:2 pixel subsampling in Q3.
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 */
static INLINE void cfl_luma_subsampling_422_lbd_ssse3(const uint8_t *input,
                                                      int input_stride,
                                                      int16_t *pred_buf_q3,
                                                      int width, int height) {
  const __m128i fours = _mm_set1_epi8(4);
  const int16_t *end = pred_buf_q3 + height * CFL_BUF_LINE;
  const int luma_stride = input_stride;
  __m128i top, next_top, top_16x8, next_top_16x8;
  do {
    switch (width) {
      case 4: top = _mm_cvtsi32_si128(*((int *)input)); break;
      case 8: top = _mm_loadl_epi64((__m128i *)input); break;
      case 16: top = _mm_loadu_si128((__m128i *)input); break;
      case 32:
        top = _mm_loadu_si128((__m128i *)input);
        next_top = _mm_loadu_si128((__m128i *)(input + 16));
        break;
      default: assert(0);
    }
    top_16x8 = _mm_maddubs_epi16(top, fours);
    if (width == 32) {
      next_top_16x8 = _mm_maddubs_epi16(next_top, fours);
    }
    switch (width) {
      case 4: *((int *)pred_buf_q3) = _mm_cvtsi128_si32(top_16x8); break;
      case 8: _mm_storel_epi64((__m128i *)pred_buf_q3, top_16x8); break;
      case 16: _mm_storeu_si128((__m128i *)pred_buf_q3, top_16x8); break;
      case 32:
        _mm_storeu_si128((__m128i *)pred_buf_q3, top_16x8);
        _mm_storeu_si128((__m128i *)(pred_buf_q3 + 8), next_top_16x8);
        break;
      default: assert(0);
    }
    input += luma_stride;
    pred_buf_q3 += CFL_BUF_LINE;
  } while (pred_buf_q3 < end);
}

CFL_GET_SUBSAMPLE_FUNCTION(ssse3)

static INLINE __m128i predict_unclipped(const __m128i *input, __m128i alpha_q12,
                                        __m128i alpha_sign, __m128i dc_q0) {
  __m128i ac_q3 = _mm_loadu_si128(input);
  __m128i ac_sign = _mm_sign_epi16(alpha_sign, ac_q3);
  __m128i scaled_luma_q0 = _mm_mulhrs_epi16(_mm_abs_epi16(ac_q3), alpha_q12);
  scaled_luma_q0 = _mm_sign_epi16(scaled_luma_q0, ac_sign);
  return _mm_add_epi16(scaled_luma_q0, dc_q0);
}

static INLINE void cfl_predict_lbd_ssse3(const int16_t *pred_buf_q3,
                                         uint8_t *dst, int dst_stride,
                                         int alpha_q3, int width, int height) {
  const __m128i alpha_sign = _mm_set1_epi16(alpha_q3);
  const __m128i alpha_q12 = _mm_slli_epi16(_mm_abs_epi16(alpha_sign), 9);
  const __m128i dc_q0 = _mm_set1_epi16(*dst);
  __m128i *row = (__m128i *)pred_buf_q3;
  const __m128i *row_end = row + height * CFL_BUF_LINE_I128;
  do {
    __m128i res = predict_unclipped(row, alpha_q12, alpha_sign, dc_q0);
    if (width < 16) {
      res = _mm_packus_epi16(res, res);
      if (width == 4)
        *(uint32_t *)dst = _mm_cvtsi128_si32(res);
      else
        _mm_storel_epi64((__m128i *)dst, res);
    } else {
      __m128i next = predict_unclipped(row + 1, alpha_q12, alpha_sign, dc_q0);
      res = _mm_packus_epi16(res, next);
      _mm_storeu_si128((__m128i *)dst, res);
      if (width == 32) {
        res = predict_unclipped(row + 2, alpha_q12, alpha_sign, dc_q0);
        next = predict_unclipped(row + 3, alpha_q12, alpha_sign, dc_q0);
        res = _mm_packus_epi16(res, next);
        _mm_storeu_si128((__m128i *)(dst + 16), res);
      }
    }
    dst += dst_stride;
  } while ((row += CFL_BUF_LINE_I128) < row_end);
}

CFL_PREDICT_FN(ssse3, lbd)

static INLINE __m128i highbd_max_epi16(int bd) {
  const __m128i neg_one = _mm_set1_epi16(-1);
  // (1 << bd) - 1 => -(-1 << bd) -1 => -1 - (-1 << bd) => -1 ^ (-1 << bd)
  return _mm_xor_si128(_mm_slli_epi16(neg_one, bd), neg_one);
}

static INLINE __m128i highbd_clamp_epi16(__m128i u, __m128i zero, __m128i max) {
  return _mm_max_epi16(_mm_min_epi16(u, max), zero);
}

static INLINE void cfl_predict_hbd_ssse3(const int16_t *pred_buf_q3,
                                         uint16_t *dst, int dst_stride,
                                         int alpha_q3, int bd, int width,
                                         int height) {
  const __m128i alpha_sign = _mm_set1_epi16(alpha_q3);
  const __m128i alpha_q12 = _mm_slli_epi16(_mm_abs_epi16(alpha_sign), 9);
  const __m128i dc_q0 = _mm_set1_epi16(*dst);
  const __m128i max = highbd_max_epi16(bd);
  const __m128i zeros = _mm_setzero_si128();
  __m128i *row = (__m128i *)pred_buf_q3;
  const __m128i *row_end = row + height * CFL_BUF_LINE_I128;
  do {
    __m128i res = predict_unclipped(row, alpha_q12, alpha_sign, dc_q0);
    res = highbd_clamp_epi16(res, zeros, max);
    if (width == 4) {
      _mm_storel_epi64((__m128i *)dst, res);
    } else {
      _mm_storeu_si128((__m128i *)dst, res);
    }
    if (width >= 16) {
      const __m128i res_1 =
          predict_unclipped(row + 1, alpha_q12, alpha_sign, dc_q0);
      _mm_storeu_si128(((__m128i *)dst) + 1,
                       highbd_clamp_epi16(res_1, zeros, max));
    }
    if (width == 32) {
      const __m128i res_2 =
          predict_unclipped(row + 2, alpha_q12, alpha_sign, dc_q0);
      _mm_storeu_si128((__m128i *)(dst + 16),
                       highbd_clamp_epi16(res_2, zeros, max));
      const __m128i res_3 =
          predict_unclipped(row + 3, alpha_q12, alpha_sign, dc_q0);
      _mm_storeu_si128((__m128i *)(dst + 24),
                       highbd_clamp_epi16(res_3, zeros, max));
    }
    dst += dst_stride;
  } while ((row += CFL_BUF_LINE_I128) < row_end);
}

CFL_PREDICT_FN(ssse3, hbd)
