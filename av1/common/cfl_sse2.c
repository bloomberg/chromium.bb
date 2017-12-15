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

#include <emmintrin.h>

#include "./av1_rtcd.h"

#include "av1/common/cfl.h"

#define INT16_IN_M128I (8)

#define TWO_BUFFER_LINES (64)

/**
 * Subtracts avg_q3 from the active part of the CfL prediction buffer.
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 */
void av1_cfl_subtract_sse2(int16_t *pred_buf_q3, int width, int height,
                           int16_t avg_q3) {
  const __m128i avg_x16 = _mm_set1_epi16(avg_q3);

  // Eight int16 values fit in one __m128i register. If this is enough to do the
  // entire row, the next value is in the next row, otherwise we move to the
  // next eight values.
  //   width   next
  //     4      32
  //     8      32
  //    16       8
  //    32       8
  const int next = CFL_BUF_LINE >> (2 * (width > INT16_IN_M128I));

  // If next was in the next row (next == 32), then we need to jump 2 rows
  // (stride == 64). Otherwise, if width is 16 we move to the next row, if width
  // is 32 we move 16 values.
  //   width     stride
  //     4         64
  //     8         64
  //    16         32
  //    32         16
  const int stride = TWO_BUFFER_LINES >> (width >> 4);

  const int16_t *end = pred_buf_q3 + height * CFL_BUF_LINE;
  do {
    __m128i val_x16 = _mm_loadu_si128((__m128i *)pred_buf_q3);
    __m128i next_val_x16 = _mm_loadu_si128((__m128i *)(pred_buf_q3 + next));

    _mm_storeu_si128((__m128i *)pred_buf_q3, _mm_sub_epi16(val_x16, avg_x16));
    _mm_storeu_si128((__m128i *)(pred_buf_q3 + next),
                     _mm_sub_epi16(next_val_x16, avg_x16));
  } while ((pred_buf_q3 += stride) < end);
}
