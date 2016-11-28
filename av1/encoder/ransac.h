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

#ifndef AV1_ENCODER_RANSAC_H_
#define AV1_ENCODER_RANSAC_H_

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>

#include "av1/common/warped_motion.h"

typedef int (*RansacFunc)(double *matched_points, int npoints,
                          int *number_of_inliers, int *best_inlier_mask,
                          double *best_params);

/* Each of these functions fits a motion model from a set of
corresponding points in 2 frames using RANSAC.*/
int ransac_homography(double *matched_points, int npoints,
                      int *number_of_inliers, int *best_inlier_indices,
                      double *best_params);
int ransac_affine(double *matched_points, int npoints, int *number_of_inliers,
                  int *best_inlier_indices, double *best_params);
int ransac_rotzoom(double *matched_points, int npoints, int *number_of_inliers,
                   int *best_inlier_indices, double *best_params);
int ransac_translation(double *matched_points, int npoints,
                       int *number_of_inliers, int *best_inlier_indices,
                       double *best_params);
#endif  // AV1_ENCODER_RANSAC_H_
