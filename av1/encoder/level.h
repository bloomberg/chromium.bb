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

#define UNDEFINED_LEVEL                                                 \
  {                                                                     \
    .level = SEQ_LEVEL_MAX, .max_picture_size = 0, .max_h_size = 0,     \
    .max_v_size = 0, .max_display_rate = 0, .max_decode_rate = 0,       \
    .max_header_rate = 0, .main_mbps = 0, .high_mbps = 0, .main_cr = 0, \
    .high_cr = 0, .max_tiles = 0, .max_tile_cols = 0                    \
  }

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

void av1_update_level_info(struct AV1_COMP *cpi, size_t size, int64_t ts_start,
                           int64_t ts_end);

#endif  // AOM_AV1_ENCODER_LEVEL_H_
