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

#ifndef AV1_ENCODER_GLOBAL_MOTION_H_
#define AV1_ENCODER_GLOBAL_MOTION_H_

#include "aom/aom_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

const double gm_advantage_thresh[TRANS_TYPES] = {
  1.00,  // Identity (not used)
  0.85,  // Translation
  0.75,  // Rot zoom
  0.65,  // Affine
  0.50,  // Homography
};

void convert_to_params(const double *params, int32_t *model);

void convert_model_to_params(const double *params, WarpedMotionParams *model);

// Adds some offset to a global motion parameter and handles
// all of the necessary precision shifts, clamping, and
// zero-centering.
int32_t add_param_offset(int param_index, int32_t param_value, int32_t offset);

void force_wmtype(WarpedMotionParams *wm, TransformationType wmtype);

double refine_integerized_param(WarpedMotionParams *wm,
                                TransformationType wmtype,
#if CONFIG_AOM_HIGHBITDEPTH
                                int use_hbd, int bd,
#endif  // CONFIG_AOM_HIGHBITDEPTH
                                uint8_t *ref, int r_width, int r_height,
                                int r_stride, uint8_t *dst, int d_width,
                                int d_height, int d_stride, int n_refinements);

/*
  Computes global motion parameters between two frames. The array
  "params" should be length 9, where the first 2 slots are translation
  parameters in (row, col) order, and the remaining slots correspond
  to values in the transformation matrix of the corresponding motion
  model. They are arranged in "params" such that values on the tx-matrix
  diagonal have odd numbered indices so the folowing matrix:
  A | B
  C | D
  would produce params = [trans row, trans col, B, A, C, D]
*/
int compute_global_motion_feature_based(TransformationType type,
                                        YV12_BUFFER_CONFIG *frm,
                                        YV12_BUFFER_CONFIG *ref,
#if CONFIG_AOM_HIGHBITDEPTH
                                        int bit_depth,
#endif
                                        double *params);
#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // AV1_ENCODER_GLOBAL_MOTION_H_
