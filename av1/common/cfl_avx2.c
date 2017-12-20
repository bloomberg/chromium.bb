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

#include "./av1_rtcd.h"

#include "av1/common/cfl.h"

/**
 * Subtracts avg_q3 from the active part of the CfL prediction buffer.
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 */
void av1_cfl_subtract_avx2(int16_t *pred_buf_q3, int width, int height,
                           int16_t avg_q3) {
  const __m256i avg_x16 = _mm256_set1_epi16(avg_q3);

  // Sixteen int16 values fit in one __m256i register. If this is enough to do
  // the entire row, we move to the next row (stride ==32), otherwise we move to
  // the next sixteen values.
  //   width   next
  //     4      32
  //     8      32
  //    16      32
  //    32      16
  const int stride = CFL_BUF_LINE >> (width == 32);

  const int16_t *end = pred_buf_q3 + height * CFL_BUF_LINE;
  do {
    __m256i val_x16 = _mm256_loadu_si256((__m256i *)pred_buf_q3);
    _mm256_storeu_si256((__m256i *)pred_buf_q3,
                        _mm256_sub_epi16(val_x16, avg_x16));
  } while ((pred_buf_q3 += stride) < end);
}

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
static void cfl_luma_subsampling_420_lbd_avx2(const uint8_t *input,
                                              int input_stride,
                                              int16_t *pred_buf_q3, int width,
                                              int height) {
  (void)width;  // Max chroma width is 16, so all widths fit in one __m256i

  const __m256i twos = _mm256_set1_epi8(2);  // Thirty two twos
  const int luma_stride = input_stride << 1;
  const int16_t *end = pred_buf_q3 + height * CFL_BUF_LINE;
  do {
    // Load 32 values for the top and bottom rows.
    // t_0, t_1, ... t_31
    __m256i top = _mm256_loadu_si256((__m256i *)(input));
    // b_0, b_1, ... b_31
    __m256i bot = _mm256_loadu_si256((__m256i *)(input + input_stride));

    // Horizontal add of the 32 values into 16 values that are multiplied by 2
    // (t_0 + t_1) * 2, (t_2 + t_3) * 2, ... (t_30 + t_31) *2
    top = _mm256_maddubs_epi16(top, twos);
    // (b_0 + b_1) * 2, (b_2 + b_3) * 2, ... (b_30 + b_31) *2
    bot = _mm256_maddubs_epi16(bot, twos);

    // Add the 16 values in top with the 16 values in bottom
    _mm256_storeu_si256((__m256i *)pred_buf_q3, _mm256_add_epi16(top, bot));

    input += luma_stride;
    pred_buf_q3 += CFL_BUF_LINE;
  } while (pred_buf_q3 < end);
}

cfl_subsample_lbd_fn get_subsample_lbd_fn_avx2(int sub_x, int sub_y) {
  static const cfl_subsample_lbd_fn subsample_lbd[2][2] = {
    //  (sub_y == 0, sub_x == 0)       (sub_y == 0, sub_x == 1)
    //  (sub_y == 1, sub_x == 0)       (sub_y == 1, sub_x == 1)
    { cfl_luma_subsampling_444_lbd, cfl_luma_subsampling_422_lbd },
    { cfl_luma_subsampling_440_lbd, cfl_luma_subsampling_420_lbd_avx2 },
  };
  // AND sub_x and sub_y with 1 to ensures that an attacker won't be able to
  // index the function pointer array out of bounds.
  return subsample_lbd[sub_y & 1][sub_x & 1];
}
