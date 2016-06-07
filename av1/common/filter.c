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

#include "av1/common/filter.h"

DECLARE_ALIGNED(256, static const int16_t,
                bilinear_filters[SUBPEL_SHIFTS][8]) = {
  { 0, 0, 0, 128, 0, 0, 0, 0 },  { 0, 0, 0, 120, 8, 0, 0, 0 },
  { 0, 0, 0, 112, 16, 0, 0, 0 }, { 0, 0, 0, 104, 24, 0, 0, 0 },
  { 0, 0, 0, 96, 32, 0, 0, 0 },  { 0, 0, 0, 88, 40, 0, 0, 0 },
  { 0, 0, 0, 80, 48, 0, 0, 0 },  { 0, 0, 0, 72, 56, 0, 0, 0 },
  { 0, 0, 0, 64, 64, 0, 0, 0 },  { 0, 0, 0, 56, 72, 0, 0, 0 },
  { 0, 0, 0, 48, 80, 0, 0, 0 },  { 0, 0, 0, 40, 88, 0, 0, 0 },
  { 0, 0, 0, 32, 96, 0, 0, 0 },  { 0, 0, 0, 24, 104, 0, 0, 0 },
  { 0, 0, 0, 16, 112, 0, 0, 0 }, { 0, 0, 0, 8, 120, 0, 0, 0 }
};

// Lagrangian interpolation filter
DECLARE_ALIGNED(256, static const int16_t,
                sub_pel_filters_8[SUBPEL_SHIFTS][8]) = {
  { 0, 0, 0, 128, 0, 0, 0, 0 },        { 0, 1, -5, 126, 8, -3, 1, 0 },
  { -1, 3, -10, 122, 18, -6, 2, 0 },   { -1, 4, -13, 118, 27, -9, 3, -1 },
  { -1, 4, -16, 112, 37, -11, 4, -1 }, { -1, 5, -18, 105, 48, -14, 4, -1 },
  { -1, 5, -19, 97, 58, -16, 5, -1 },  { -1, 6, -19, 88, 68, -18, 5, -1 },
  { -1, 6, -19, 78, 78, -19, 6, -1 },  { -1, 5, -18, 68, 88, -19, 6, -1 },
  { -1, 5, -16, 58, 97, -19, 5, -1 },  { -1, 4, -14, 48, 105, -18, 5, -1 },
  { -1, 4, -11, 37, 112, -16, 4, -1 }, { -1, 3, -9, 27, 118, -13, 4, -1 },
  { 0, 2, -6, 18, 122, -10, 3, -1 },   { 0, 1, -3, 8, 126, -5, 1, 0 }
};

// DCT based filter
DECLARE_ALIGNED(256, static const int16_t,
                sub_pel_filters_8sharp[SUBPEL_SHIFTS][8]) = {
  { 0, 0, 0, 128, 0, 0, 0, 0 },         { -1, 3, -7, 127, 8, -3, 1, 0 },
  { -2, 5, -13, 125, 17, -6, 3, -1 },   { -3, 7, -17, 121, 27, -10, 5, -2 },
  { -4, 9, -20, 115, 37, -13, 6, -2 },  { -4, 10, -23, 108, 48, -16, 8, -3 },
  { -4, 10, -24, 100, 59, -19, 9, -3 }, { -4, 11, -24, 90, 70, -21, 10, -4 },
  { -4, 11, -23, 80, 80, -23, 11, -4 }, { -4, 10, -21, 70, 90, -24, 11, -4 },
  { -3, 9, -19, 59, 100, -24, 10, -4 }, { -3, 8, -16, 48, 108, -23, 10, -4 },
  { -2, 6, -13, 37, 115, -20, 9, -4 },  { -2, 5, -10, 27, 121, -17, 7, -3 },
  { -1, 3, -6, 17, 125, -13, 5, -2 },   { 0, 1, -3, 8, 127, -7, 3, -1 }
};

// freqmultiplier = 0.5
DECLARE_ALIGNED(256, static const int16_t,
                sub_pel_filters_8smooth[SUBPEL_SHIFTS][8]) = {
  { 0, 0, 0, 128, 0, 0, 0, 0 },       { -3, -1, 32, 64, 38, 1, -3, 0 },
  { -2, -2, 29, 63, 41, 2, -3, 0 },   { -2, -2, 26, 63, 43, 4, -4, 0 },
  { -2, -3, 24, 62, 46, 5, -4, 0 },   { -2, -3, 21, 60, 49, 7, -4, 0 },
  { -1, -4, 18, 59, 51, 9, -4, 0 },   { -1, -4, 16, 57, 53, 12, -4, -1 },
  { -1, -4, 14, 55, 55, 14, -4, -1 }, { -1, -4, 12, 53, 57, 16, -4, -1 },
  { 0, -4, 9, 51, 59, 18, -4, -1 },   { 0, -4, 7, 49, 60, 21, -3, -2 },
  { 0, -4, 5, 46, 62, 24, -3, -2 },   { 0, -4, 4, 43, 63, 26, -2, -2 },
  { 0, -3, 2, 41, 63, 29, -2, -2 },   { 0, -3, 1, 38, 64, 32, -1, -3 }
};

const InterpKernel *av1_filter_kernels[4] = { sub_pel_filters_8,
                                              sub_pel_filters_8smooth,
                                              sub_pel_filters_8sharp,
                                              bilinear_filters };
static const InterpFilterParams
    interp_filter_params_list[SWITCHABLE_FILTERS + 1] = {
      { (const int16_t *)sub_pel_filters_8, SUBPEL_TAPS, SUBPEL_SHIFTS },
      { (const int16_t *)sub_pel_filters_8smooth, SUBPEL_TAPS, SUBPEL_SHIFTS },
      { (const int16_t *)sub_pel_filters_8sharp, SUBPEL_TAPS, SUBPEL_SHIFTS },
      { (const int16_t *)bilinear_filters, SUBPEL_TAPS, SUBPEL_SHIFTS }
    };

InterpFilterParams get_interp_filter_params(InterpFilter interp_filter) {
  return interp_filter_params_list[interp_filter];
}
