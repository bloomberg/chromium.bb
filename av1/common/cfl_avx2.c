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
