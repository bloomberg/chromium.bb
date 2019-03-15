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

#define UNDEFINED_LEVEL                                                 \
  {                                                                     \
    .level = SEQ_LEVEL_MAX, .max_picture_size = 0, .max_h_size = 0,     \
    .max_v_size = 0, .max_display_rate = 0, .max_decode_rate = 0,       \
    .max_header_rate = 0, .main_mbps = 0, .high_mbps = 0, .main_cr = 0, \
    .high_cr = 0, .max_tiles = 0, .max_tile_cols = 0                    \
  }

static const AV1LevelSpec av1_level_defs[SEQ_LEVELS] = {
  { .level = SEQ_LEVEL_2_0,
    .max_picture_size = 147456,
    .max_h_size = 2048,
    .max_v_size = 1152,
    .max_display_rate = 4423680L,
    .max_decode_rate = 5529600L,
    .max_header_rate = 150,
    .main_mbps = 1.5,
    .high_mbps = 0,
    .main_cr = 2.0,
    .high_cr = 0,
    .max_tiles = 8,
    .max_tile_cols = 4 },
  { .level = SEQ_LEVEL_2_1,
    .max_picture_size = 278784,
    .max_h_size = 2816,
    .max_v_size = 1584,
    .max_display_rate = 8363520L,
    .max_decode_rate = 10454400L,
    .max_header_rate = 150,
    .main_mbps = 3.0,
    .high_mbps = 0,
    .main_cr = 2.0,
    .high_cr = 0,
    .max_tiles = 8,
    .max_tile_cols = 4 },
  UNDEFINED_LEVEL,
  UNDEFINED_LEVEL,
  { .level = SEQ_LEVEL_3_0,
    .max_picture_size = 665856,
    .max_h_size = 4352,
    .max_v_size = 2448,
    .max_display_rate = 19975680L,
    .max_decode_rate = 24969600L,
    .max_header_rate = 150,
    .main_mbps = 6.0,
    .high_mbps = 0,
    .main_cr = 2.0,
    .high_cr = 0,
    .max_tiles = 16,
    .max_tile_cols = 6 },
  { .level = SEQ_LEVEL_3_1,
    .max_picture_size = 1065024,
    .max_h_size = 5504,
    .max_v_size = 3096,
    .max_display_rate = 31950720L,
    .max_decode_rate = 39938400L,
    .max_header_rate = 150,
    .main_mbps = 10.0,
    .high_mbps = 0,
    .main_cr = 2.0,
    .high_cr = 0,
    .max_tiles = 16,
    .max_tile_cols = 6 },
  UNDEFINED_LEVEL,
  UNDEFINED_LEVEL,
  { .level = SEQ_LEVEL_4_0,
    .max_picture_size = 2359296,
    .max_h_size = 6144,
    .max_v_size = 3456,
    .max_display_rate = 70778880L,
    .max_decode_rate = 77856768L,
    .max_header_rate = 300,
    .main_mbps = 12.0,
    .high_mbps = 30.0,
    .main_cr = 4.0,
    .high_cr = 4.0,
    .max_tiles = 32,
    .max_tile_cols = 8 },
  { .level = SEQ_LEVEL_4_1,
    .max_picture_size = 2359296,
    .max_h_size = 6144,
    .max_v_size = 3456,
    .max_display_rate = 141557760L,
    .max_decode_rate = 155713536L,
    .max_header_rate = 300,
    .main_mbps = 20.0,
    .high_mbps = 50.0,
    .main_cr = 4.0,
    .high_cr = 4.0,
    .max_tiles = 32,
    .max_tile_cols = 8 },
  UNDEFINED_LEVEL,
  UNDEFINED_LEVEL,
  { .level = SEQ_LEVEL_5_0,
    .max_picture_size = 8912896,
    .max_h_size = 8192,
    .max_v_size = 4352,
    .max_display_rate = 267386880L,
    .max_decode_rate = 273715200L,
    .max_header_rate = 300,
    .main_mbps = 30.0,
    .high_mbps = 100.0,
    .main_cr = 6.0,
    .high_cr = 4.0,
    .max_tiles = 64,
    .max_tile_cols = 8 },
  { .level = SEQ_LEVEL_5_1,
    .max_picture_size = 8912896,
    .max_h_size = 8192,
    .max_v_size = 4352,
    .max_display_rate = 534773760L,
    .max_decode_rate = 547430400L,
    .max_header_rate = 300,
    .main_mbps = 40.0,
    .high_mbps = 160.0,
    .main_cr = 8.0,
    .high_cr = 4.0,
    .max_tiles = 64,
    .max_tile_cols = 8 },
  { .level = SEQ_LEVEL_5_2,
    .max_picture_size = 8912896,
    .max_h_size = 8192,
    .max_v_size = 4352,
    .max_display_rate = 1069547520L,
    .max_decode_rate = 1094860800L,
    .max_header_rate = 300,
    .main_mbps = 60.0,
    .high_mbps = 240.0,
    .main_cr = 8.0,
    .high_cr = 4.0,
    .max_tiles = 64,
    .max_tile_cols = 8 },
  { .level = SEQ_LEVEL_5_3,
    .max_picture_size = 8912896,
    .max_h_size = 8192,
    .max_v_size = 4352,
    .max_display_rate = 1069547520L,
    .max_decode_rate = 1176502272L,
    .max_header_rate = 300,
    .main_mbps = 60.0,
    .high_mbps = 240.0,
    .main_cr = 8.0,
    .high_cr = 4.0,
    .max_tiles = 64,
    .max_tile_cols = 8 },
  { .level = SEQ_LEVEL_6_0,
    .max_picture_size = 35651584,
    .max_h_size = 16384,
    .max_v_size = 8704,
    .max_display_rate = 1069547520L,
    .max_decode_rate = 1176502272L,
    .max_header_rate = 300,
    .main_mbps = 60.0,
    .high_mbps = 240.0,
    .main_cr = 8.0,
    .high_cr = 4.0,
    .max_tiles = 128,
    .max_tile_cols = 16 },
  { .level = SEQ_LEVEL_6_1,
    .max_picture_size = 35651584,
    .max_h_size = 16384,
    .max_v_size = 8704,
    .max_display_rate = 2139095040L,
    .max_decode_rate = 2189721600L,
    .max_header_rate = 300,
    .main_mbps = 100.0,
    .high_mbps = 480.0,
    .main_cr = 8.0,
    .high_cr = 4.0,
    .max_tiles = 128,
    .max_tile_cols = 16 },
  { .level = SEQ_LEVEL_6_2,
    .max_picture_size = 35651584,
    .max_h_size = 16384,
    .max_v_size = 8704,
    .max_display_rate = 4278190080L,
    .max_decode_rate = 4379443200L,
    .max_header_rate = 300,
    .main_mbps = 160.0,
    .high_mbps = 800.0,
    .main_cr = 8.0,
    .high_cr = 4.0,
    .max_tiles = 128,
    .max_tile_cols = 16 },
  { .level = SEQ_LEVEL_6_3,
    .max_picture_size = 35651584,
    .max_h_size = 16384,
    .max_v_size = 8704,
    .max_display_rate = 4278190080L,
    .max_decode_rate = 4706009088L,
    .max_header_rate = 300,
    .main_mbps = 160.0,
    .high_mbps = 800.0,
    .main_cr = 8.0,
    .high_cr = 4.0,
    .max_tiles = 128,
    .max_tile_cols = 16 },
  UNDEFINED_LEVEL,
  UNDEFINED_LEVEL,
  UNDEFINED_LEVEL,
  UNDEFINED_LEVEL,
};

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

static void check_level_constraints(AV1_COMP *cpi, int operating_point_idx,
                                    const AV1LevelSpec *level_spec) {
  const AV1_LEVEL target_seq_level_idx =
      cpi->target_seq_level_idx[operating_point_idx];
  if (target_seq_level_idx >= SEQ_LEVELS) return;
  TARGET_LEVEL_FAIL_ID fail_id = TARGET_LEVEL_FAIL_IDS;
  // Check level conformance
  // TODO(kyslov@) implement all constraints
  do {
    const AV1LevelSpec *const target_level_spec =
        av1_level_defs + target_seq_level_idx;
    if (level_spec->max_picture_size > target_level_spec->max_picture_size) {
      fail_id = LUMA_PIC_SIZE_TOO_LARGE;
      break;
    }

    if (level_spec->max_h_size > target_level_spec->max_h_size) {
      fail_id = LUMA_PIC_H_SIZE_TOO_LARGE;
      break;
    }

    if (level_spec->max_v_size > target_level_spec->max_v_size) {
      fail_id = LUMA_PIC_V_SIZE_TOO_LARGE;
      break;
    }

    if (level_spec->max_tile_cols > target_level_spec->max_tile_cols) {
      fail_id = TOO_MANY_TILE_COLUMNS;
      break;
    }

    if (level_spec->max_tiles > target_level_spec->max_tiles) {
      fail_id = TOO_MANY_TILES;
      break;
    }
  } while (0);

  if (fail_id != TARGET_LEVEL_FAIL_IDS) {
    AV1_COMMON *const cm = &cpi->common;
    const int target_level_major = 2 + (target_seq_level_idx >> 2);
    const int target_level_minor = target_seq_level_idx & 3;
    aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                       "Failed to encode to the target level %d_%d. %s",
                       target_level_major, target_level_minor,
                       level_fail_messages[fail_id]);
  }
}

static INLINE int is_in_operating_point(int operating_point,
                                        int temporal_layer_id,
                                        int spatial_layer_id) {
  if (!operating_point) return 1;

  return ((operating_point >> temporal_layer_id) & 1) &&
         ((operating_point >> (spatial_layer_id + 8)) & 1);
}

void av1_update_level_info(AV1_COMP *cpi, size_t size, int64_t ts_start,
                           int64_t ts_end) {
  const AV1_COMMON *const cm = &cpi->common;
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

  const SequenceHeader *const seq = &cm->seq_params;
  const int temporal_layer_id = cm->temporal_layer_id;
  const int spatial_layer_id = cm->spatial_layer_id;
  // update level_stats
  // TODO(kyslov@) fix the implementation according to buffer model
  for (int i = 0; i < seq->operating_points_cnt_minus_1 + 1; ++i) {
    if (!is_in_operating_point(seq->operating_point_idc[i], temporal_layer_id,
                               spatial_layer_id)) {
      continue;
    }

    AV1LevelInfo *const level_info = &cpi->level_info[i];
    AV1LevelStats *const level_stats = &level_info->level_stats;
    level_stats->total_compressed_size += frame_compressed_size;
    if (cm->show_frame) {
      level_stats->total_time_encoded =
          (cpi->last_end_time_stamp_seen - cpi->first_time_stamp_ever) /
          (double)TICKS_PER_SEC;
    }

    // update level_spec
    // TODO(kyslov@) update all spec fields
    AV1LevelSpec *const level_spec = &level_info->level_spec;
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

    check_level_constraints(cpi, i, level_spec);
  }
}

aom_codec_err_t av1_get_seq_level_idx(const AV1_COMP *cpi, int *seq_level_idx) {
  // TODO(chiyotsai@, kyslov@, huisu@): put in real implementations.
  (void)cpi;
  for (int i = 0; i < MAX_NUM_OPERATING_POINTS; ++i) {
    seq_level_idx[i] = (int)SEQ_LEVEL_MAX;
  }

  return AOM_CODEC_OK;
}
