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

#include <assert.h>
#include <tmmintrin.h>

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "av1/common/filter.h"

#define WIDTH_BOUND (16)
#define HEIGHT_BOUND (16)

// Note:
//  This function assumes:
// (1) 10/12-taps filters
// (2) x_step_q4 = 16 then filter is fixed at the call

typedef struct SimdFilter {
  InterpFilter interp_filter;
  int8_t (*simd_horiz_filter)[2][16];
  int8_t (*simd_vert_filter)[6][16];
} SimdFilter;
