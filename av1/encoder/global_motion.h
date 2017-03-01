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
#include "aom_scale/yv12config.h"
#include "av1/common/mv.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RANSAC_NUM_MOTIONS 1

extern const double gm_advantage_thresh[TRANS_TYPES];

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
  Computes "num_motions" candidate global motion parameters between two frames.
  The array "params_by_motion" should be length 8 * "num_motions", where the
  first 2 slots in each group of 8 parameters are the translation parameters in
  (row, col) order, and the remaining slots correspond to values in the
  transformation matrix of the corresponding motion model. They are arranged in
  "params" such that values on the tx-matrix diagonal have odd numbered indices
  so the folowing matrix: A | B C | D would produce params = [trans row, trans
  col, B, A, C, D].
  "num_inliers" should be length "num_motions", and will be populated with the
  number of inlier feature points for each motion. Params for which the
  num_inliers entry is 0 should be ignored by the caller.
*/
int compute_global_motion_feature_based(
    TransformationType type, YV12_BUFFER_CONFIG *frm, YV12_BUFFER_CONFIG *ref,
#if CONFIG_AOM_HIGHBITDEPTH
    int bit_depth,
#endif
    int *num_inliers_by_motion, double *params_by_motion, int num_motions);
#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // AV1_ENCODER_GLOBAL_MOTION_H_
