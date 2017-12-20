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

/**
 * Adds 4 pixels (in a 2x2 grid) and multiplies them by 2. Resulting in a more
 * precise version of a box filter 4:2:0 pixel subsampling in Q3.
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 *
 * Note: For 4:2:0 luma subsampling, the width will never be greater than 16.
 */
static void cfl_luma_subsampling_420_lbd_ssse3(const uint8_t *input,
                                               int input_stride,
                                               int16_t *pred_buf_q3, int width,
                                               int height) {
  const __m128i twos = _mm_set1_epi8(2);  // Sixteen twos

  // Sixteen int8 values fit in one __m128i register. If this is enough to do
  // the entire row, the next value is two rows down, otherwise we move to the
  // next sixteen values.
  const int next = (width == 16) ? 16 : input_stride << 1;

  // Values in the prediction buffer are subsampled, so we only need to move
  // down one row or forward by eight values.
  const int next_chroma = (width == 16) ? 8 : CFL_BUF_LINE;

  // When the width is less than 16, we double the stride, because we process
  // four lines by iteration (instead of two).
  const int luma_stride = input_stride << (1 + (width < 16));
  const int chroma_stride = CFL_BUF_LINE << (width < 16);

  const int16_t *end = pred_buf_q3 + height * CFL_BUF_LINE;
  do {
    // Load 16 values for the top and bottom rows.
    // t_0, t_1, ... t_15
    __m128i top = _mm_loadu_si128((__m128i *)(input));
    // b_0, b_1, ... b_15
    __m128i bot = _mm_loadu_si128((__m128i *)(input + input_stride));

    // Load either the next line or the next 16 values
    __m128i next_top = _mm_loadu_si128((__m128i *)(input + next));
    __m128i next_bot =
        _mm_loadu_si128((__m128i *)(input + next + input_stride));

    // Horizontal add of the 16 values into 8 values that are multiplied by 2
    // (t_0 + t_1) * 2, (t_2 + t_3) * 2, ... (t_14 + t_15) *2
    top = _mm_maddubs_epi16(top, twos);
    next_top = _mm_maddubs_epi16(next_top, twos);
    // (b_0 + b_1) * 2, (b_2 + b_3) * 2, ... (b_14 + b_15) *2
    bot = _mm_maddubs_epi16(bot, twos);
    next_bot = _mm_maddubs_epi16(next_bot, twos);

    // Add the 8 values in top with the 8 values in bottom
    _mm_storeu_si128((__m128i *)pred_buf_q3, _mm_add_epi16(top, bot));
    _mm_storeu_si128((__m128i *)(pred_buf_q3 + next_chroma),
                     _mm_add_epi16(next_top, next_bot));

    input += luma_stride;
    pred_buf_q3 += chroma_stride;
  } while (pred_buf_q3 < end);
}

cfl_subsample_lbd_fn get_subsample_lbd_fn_ssse3(int sub_x, int sub_y) {
  static const cfl_subsample_lbd_fn subsample_lbd[2][2] = {
    //  (sub_y == 0, sub_x == 0)       (sub_y == 0, sub_x == 1)
    //  (sub_y == 1, sub_x == 0)       (sub_y == 1, sub_x == 1)
    { cfl_luma_subsampling_444_lbd, cfl_luma_subsampling_422_lbd },
    { cfl_luma_subsampling_440_lbd, cfl_luma_subsampling_420_lbd_ssse3 },
  };
  // AND sub_x and sub_y with 1 to ensures that an attacker won't be able to
  // index the function pointer array out of bounds.
  return subsample_lbd[sub_y & 1][sub_x & 1];
}
