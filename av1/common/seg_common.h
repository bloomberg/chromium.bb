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

#ifndef AV1_COMMON_SEG_COMMON_H_
#define AV1_COMMON_SEG_COMMON_H_

#include "aom_dsp/prob.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SEGMENTS 8
#define SEG_TREE_PROBS (MAX_SEGMENTS - 1)

#define SEG_TEMPORAL_PRED_CTXS 3
#if CONFIG_SPATIAL_SEGMENTATION
#define SPATIAL_PREDICTION_PROBS 3
#endif

#if CONFIG_LOOPFILTER_LEVEL
typedef enum {
  SEG_LVL_ALT_Q,       // Use alternate Quantizer ....
  SEG_LVL_ALT_LF_Y_V,  // Use alternate loop filter value on y plane vertical
  SEG_LVL_ALT_LF_Y_H,  // Use alternate loop filter value on y plane horizontal
  SEG_LVL_ALT_LF_U,    // Use alternate loop filter value on u plane
  SEG_LVL_ALT_LF_V,    // Use alternate loop filter value on v plane
  SEG_LVL_REF_FRAME,   // Optional Segment reference frame
  SEG_LVL_SKIP,        // Optional Segment (0,0) + skip mode
#if CONFIG_SEGMENT_GLOBALMV
  SEG_LVL_GLOBALMV,
#endif
  SEG_LVL_MAX
} SEG_LVL_FEATURES;
#else  // CONFIG_LOOPFILTER_LEVEL
// Segment level features.
typedef enum {
  SEG_LVL_ALT_Q = 0,      // Use alternate Quantizer ....
  SEG_LVL_ALT_LF = 1,     // Use alternate loop filter value...
  SEG_LVL_REF_FRAME = 2,  // Optional Segment reference frame
  SEG_LVL_SKIP = 3,       // Optional Segment (0,0) + skip mode
#if CONFIG_SEGMENT_GLOBALMV
  SEG_LVL_GLOBALMV = 4,
#endif
  SEG_LVL_MAX
} SEG_LVL_FEATURES;
#endif  // CONFIG_LOOPFILTER_LEVEL

struct segmentation {
  uint8_t enabled;
  uint8_t update_map;
  uint8_t update_data;
  uint8_t temporal_update;

  int16_t feature_data[MAX_SEGMENTS][SEG_LVL_MAX];
  unsigned int feature_mask[MAX_SEGMENTS];
};

struct segmentation_probs {
  aom_cdf_prob tree_cdf[CDF_SIZE(MAX_SEGMENTS)];
  aom_cdf_prob pred_cdf[SEG_TEMPORAL_PRED_CTXS][CDF_SIZE(2)];
#if CONFIG_SPATIAL_SEGMENTATION
  aom_cdf_prob spatial_pred_seg_cdf[SPATIAL_PREDICTION_PROBS]
                                   [CDF_SIZE(MAX_SEGMENTS)];
#endif
};

static INLINE int segfeature_active(const struct segmentation *seg,
                                    int segment_id,
                                    SEG_LVL_FEATURES feature_id) {
  return seg->enabled && (seg->feature_mask[segment_id] & (1 << feature_id));
}

void av1_clearall_segfeatures(struct segmentation *seg);

void av1_enable_segfeature(struct segmentation *seg, int segment_id,
                           SEG_LVL_FEATURES feature_id);

int av1_seg_feature_data_max(SEG_LVL_FEATURES feature_id);

int av1_is_segfeature_signed(SEG_LVL_FEATURES feature_id);

void av1_set_segdata(struct segmentation *seg, int segment_id,
                     SEG_LVL_FEATURES feature_id, int seg_data);

static INLINE int get_segdata(const struct segmentation *seg, int segment_id,
                              SEG_LVL_FEATURES feature_id) {
  return seg->feature_data[segment_id][feature_id];
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_SEG_COMMON_H_
