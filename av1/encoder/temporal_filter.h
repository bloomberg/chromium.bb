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

#ifndef AOM_AV1_ENCODER_TEMPORAL_FILTER_H_
#define AOM_AV1_ENCODER_TEMPORAL_FILTER_H_

#ifdef __cplusplus
extern "C" {
#endif

#define ARNR_FILT_QINDEX 128

// Block size used in temporal filtering
#define TF_BLOCK BLOCK_32X32
#define BH 32
#define BH_LOG2 5
#define BW 32
#define BW_LOG2 5
#define BLK_PELS 1024  // Pixels in the block
#define THR_SHIFT 2
#define TF_SUB_BLOCK BLOCK_16X16
#define SUB_BH 16
#define SUB_BW 16

#define NUM_KEY_FRAME_DENOISING 7
#define EDGE_THRESHOLD 50
#define SQRT_PI_BY_2 1.25331413732

int av1_temporal_filter(AV1_COMP *cpi, int distance,
                        int *show_existing_alt_ref);
double estimate_noise(const uint8_t *src, int width, int height, int stride,
                      int edge_thresh);
double highbd_estimate_noise(const uint8_t *src8, int width, int height,
                             int stride, int bd, int edge_thresh);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_TEMPORAL_FILTER_H_
