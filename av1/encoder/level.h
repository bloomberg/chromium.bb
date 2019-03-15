/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_ENCODER_LEVEL_H_
#define AOM_AV1_ENCODER_LEVEL_H_

#include "av1/common/enums.h"

struct AV1_COMP;

// AV1 Level Specifications
typedef struct {
  AV1_LEVEL level;
  uint32_t max_picture_size;
  uint32_t max_h_size;
  uint32_t max_v_size;
  uint64_t max_display_rate;
  uint64_t max_decode_rate;
  uint32_t max_header_rate;
  double main_mbps;
  double high_mbps;
  double main_cr;
  double high_cr;
  int32_t max_tiles;
  int32_t max_tile_cols;
} AV1LevelSpec;

// Used to keep track of AV1 Level Stats. Currently unimplemented.
typedef struct {
  uint64_t total_compressed_size;
  double total_time_encoded;
} AV1LevelStats;

typedef struct {
  AV1LevelStats level_stats;
  AV1LevelSpec level_spec;
} AV1LevelInfo;

void av1_update_level_info(struct AV1_COMP *cpi, size_t size, int64_t ts_start,
                           int64_t ts_end);

// Return sequence level indices in seq_level_idx[MAX_NUM_OPERATING_POINTS].
aom_codec_err_t av1_get_seq_level_idx(const struct AV1_COMP *cpi,
                                      int *seq_level_idx);

#endif  // AOM_AV1_ENCODER_LEVEL_H_
