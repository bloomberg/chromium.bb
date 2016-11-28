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

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <assert.h>

#include "av1/common/warped_motion.h"

#include "av1/encoder/segmentation.h"
#include "av1/encoder/global_motion.h"
#include "av1/encoder/corner_detect.h"
#include "av1/encoder/corner_match.h"
#include "av1/encoder/ransac.h"

#define MAX_CORNERS 4096
#define MIN_INLIER_PROB 0.1

static INLINE RansacFunc get_ransac_type(TransformationType type) {
  switch (type) {
    case HOMOGRAPHY: return ransac_homography;
    case AFFINE: return ransac_affine;
    case ROTZOOM: return ransac_rotzoom;
    case TRANSLATION: return ransac_translation;
    default: assert(0); return NULL;
  }
}

// computes global motion parameters by fitting a model using RANSAC
static int compute_global_motion_params(TransformationType type,
                                        double *correspondences,
                                        int num_correspondences, double *params,
                                        int *inlier_map) {
  int result;
  int num_inliers = 0;
  RansacFunc ransac = get_ransac_type(type);
  if (ransac == NULL) return 0;

  result = ransac(correspondences, num_correspondences, &num_inliers,
                  inlier_map, params);
  if (!result && num_inliers < MIN_INLIER_PROB * num_correspondences) {
    result = 1;
    num_inliers = 0;
  }
  return num_inliers;
}

#if CONFIG_AOM_HIGHBITDEPTH
unsigned char *downconvert_frame(YV12_BUFFER_CONFIG *frm, int bit_depth) {
  int i, j;
  uint16_t *orig_buf = CONVERT_TO_SHORTPTR(frm->y_buffer);
  uint8_t *buf = malloc(frm->y_height * frm->y_stride * sizeof(*buf));

  for (i = 0; i < frm->y_height; ++i)
    for (j = 0; j < frm->y_width; ++j)
      buf[i * frm->y_stride + j] =
          orig_buf[i * frm->y_stride + j] >> (bit_depth - 8);

  return buf;
}
#endif

int compute_global_motion_feature_based(TransformationType type,
                                        YV12_BUFFER_CONFIG *frm,
                                        YV12_BUFFER_CONFIG *ref,
#if CONFIG_AOM_HIGHBITDEPTH
                                        int bit_depth,
#endif
                                        double *params) {
  int num_frm_corners, num_ref_corners;
  int num_correspondences;
  double *correspondences;
  int num_inliers;
  int frm_corners[2 * MAX_CORNERS], ref_corners[2 * MAX_CORNERS];
  int *inlier_map = NULL;
  unsigned char *frm_buffer = frm->y_buffer;
  unsigned char *ref_buffer = ref->y_buffer;

#if CONFIG_AOM_HIGHBITDEPTH
  if (frm->flags & YV12_FLAG_HIGHBITDEPTH) {
    // The frame buffer is 16-bit, so we need to convert to 8 bits for the
    // following code. We cache the result until the frame is released.
    if (frm->y_buffer_8bit)
      frm_buffer = frm->y_buffer_8bit;
    else
      frm_buffer = frm->y_buffer_8bit = downconvert_frame(frm, bit_depth);
  }
  if (ref->flags & YV12_FLAG_HIGHBITDEPTH) {
    if (ref->y_buffer_8bit)
      ref_buffer = ref->y_buffer_8bit;
    else
      ref_buffer = ref->y_buffer_8bit = downconvert_frame(ref, bit_depth);
  }
#endif

  // compute interest points in images using FAST features
  num_frm_corners = fast_corner_detect(frm_buffer, frm->y_width, frm->y_height,
                                       frm->y_stride, frm_corners, MAX_CORNERS);
  num_ref_corners = fast_corner_detect(ref_buffer, ref->y_width, ref->y_height,
                                       ref->y_stride, ref_corners, MAX_CORNERS);

  // find correspondences between the two images
  correspondences =
      (double *)malloc(num_frm_corners * 4 * sizeof(*correspondences));
  num_correspondences = determine_correspondence(
      frm_buffer, (int *)frm_corners, num_frm_corners, ref_buffer,
      (int *)ref_corners, num_ref_corners, frm->y_width, frm->y_height,
      frm->y_stride, ref->y_stride, correspondences);

  inlier_map = (int *)malloc(num_correspondences * sizeof(*inlier_map));
  num_inliers = compute_global_motion_params(
      type, correspondences, num_correspondences, params, inlier_map);
  free(correspondences);
  free(inlier_map);
  return (num_inliers > 0);
}
