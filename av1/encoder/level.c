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

#include "av1/encoder/encoder.h"
#include "av1/encoder/level.h"

typedef enum {
  LUMA_PIC_SIZE_TOO_LARGE,
  LUMA_PIC_H_SIZE_TOO_LARGE,
  LUMA_PIC_V_SIZE_TOO_LARGE,
  TOO_MANY_TILE_COLUMNS,
  TOO_MANY_TILES,

  TARGET_LEVEL_FAIL_IDS
} TARGET_LEVEL_FAIL_ID;

static const char *level_fail_messages[TARGET_LEVEL_FAIL_IDS] = {
  "The picture size is too large.",   "The picture width is too large.",
  "The picture height is too large.", "Too many tile columns are used.",
  "Too many tiles are used.",
};

static void check_level_constraints(AV1_COMP *cpi,
                                    const AV1LevelSpec *level_spec) {
  const AV1_LEVEL target_seq_level_idx = cpi->target_seq_level_idx;
  if (target_seq_level_idx > LEVEL_DISABLED &&
      target_seq_level_idx < LEVEL_END) {
    // Check level conformance
    // TODO(kyslov@) implement all constraints
    AV1_COMMON *const cm = &cpi->common;
    const AV1LevelSpec *const target_level_spec =
        av1_level_defs + target_seq_level_idx;
    const int target_level_major = 2 + (target_seq_level_idx >> 2);
    const int target_level_minor = target_seq_level_idx & 3;

    if (level_spec->max_picture_size > target_level_spec->max_picture_size) {
      aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                         "Failed to encode to the target level %d_%d. %s",
                         target_level_major, target_level_minor,
                         level_fail_messages[LUMA_PIC_SIZE_TOO_LARGE]);
    }

    if (level_spec->max_h_size > target_level_spec->max_h_size) {
      aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                         "Failed to encode to the target level %d_%d. %s",
                         target_level_major, target_level_minor,
                         level_fail_messages[LUMA_PIC_H_SIZE_TOO_LARGE]);
    }

    if (level_spec->max_v_size > target_level_spec->max_v_size) {
      aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                         "Failed to encode to the target level %d_%d. %s",
                         target_level_major, target_level_minor,
                         level_fail_messages[LUMA_PIC_V_SIZE_TOO_LARGE]);
    }

    if (level_spec->max_tile_cols > target_level_spec->max_tile_cols) {
      aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                         "Failed to encode to the target level %d_%d. %s",
                         target_level_major, target_level_minor,
                         level_fail_messages[TOO_MANY_TILE_COLUMNS]);
    }

    if (level_spec->max_tiles > target_level_spec->max_tiles) {
      aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                         "Failed to encode to the target level %d_%d. %s",
                         target_level_major, target_level_minor,
                         level_fail_messages[TOO_MANY_TILES]);
    }
  }
}

void av1_update_level_info(AV1_COMP *cpi, size_t size, int64_t ts_start,
                           int64_t ts_end) {
  const AV1_COMMON *const cm = &cpi->common;
  AV1LevelInfo *const level_info = &cpi->level_info;
  AV1LevelSpec *const level_spec = &level_info->level_spec;
  AV1LevelStats *const level_stats = &level_info->level_stats;

  const uint32_t luma_pic_size = cm->superres_upscaled_width * cm->height;
  const uint32_t pic_size_profile_factor =
      cm->seq_params.profile == PROFILE_0
          ? 15
          : (cm->seq_params.profile == PROFILE_1 ? 30 : 36);
  const size_t frame_compressed_size = (size > 129 ? size - 128 : 1);
  const size_t frame_uncompressed_size =
      (luma_pic_size * pic_size_profile_factor) >> 3;
  const double compression_ratio =
      frame_uncompressed_size / (double)frame_compressed_size;

  // update level_stats
  // TODO(kyslov@) fix the implementation according to buffer model
  level_stats->total_compressed_size += frame_compressed_size;
  if (cm->show_frame) {
    level_stats->total_time_encoded =
        (cpi->last_end_time_stamp_seen - cpi->first_time_stamp_ever) /
        (double)TICKS_PER_SEC;
  }

  // update level_spec
  // TODO(kyslov@) update all spec fields
  if (luma_pic_size > level_spec->max_picture_size) {
    level_spec->max_picture_size = luma_pic_size;
  }

  if (cm->superres_upscaled_width > (int)level_spec->max_h_size) {
    level_spec->max_h_size = cm->superres_upscaled_width;
  }

  if (cm->height > (int)level_spec->max_v_size) {
    level_spec->max_v_size = cm->height;
  }

  if (level_spec->max_tile_cols < (1 << cm->log2_tile_cols)) {
    level_spec->max_tile_cols = (1 << cm->log2_tile_cols);
  }

  if (level_spec->max_tiles <
      (1 << cm->log2_tile_cols) * (1 << cm->log2_tile_rows)) {
    level_spec->max_tiles =
        (1 << cm->log2_tile_cols) * (1 << cm->log2_tile_rows);
  }

  // TODO(kyslov@) These are needed for further level stat calculations
  (void)compression_ratio;
  (void)ts_start;
  (void)ts_end;

  check_level_constraints(cpi, level_spec);
}
