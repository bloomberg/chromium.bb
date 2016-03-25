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

#include "av1/common/tile_common.h"
#include "av1/common/onyxc_int.h"
#include "aom_dsp/aom_dsp_common.h"

#define MIN_TILE_WIDTH_B64 4
#define MAX_TILE_WIDTH_B64 64

static int get_tile_offset(int idx, int mis, int log2) {
  const int sb_cols = mi_cols_aligned_to_sb(mis) >> MI_BLOCK_SIZE_LOG2;
  const int offset = ((idx * sb_cols) >> log2) << MI_BLOCK_SIZE_LOG2;
  return VPXMIN(offset, mis);
}

void vp10_tile_set_row(TileInfo *tile, const VP10_COMMON *cm, int row) {
  tile->mi_row_start = get_tile_offset(row, cm->mi_rows, cm->log2_tile_rows);
  tile->mi_row_end = get_tile_offset(row + 1, cm->mi_rows, cm->log2_tile_rows);
}

void vp10_tile_set_col(TileInfo *tile, const VP10_COMMON *cm, int col) {
  tile->mi_col_start = get_tile_offset(col, cm->mi_cols, cm->log2_tile_cols);
  tile->mi_col_end = get_tile_offset(col + 1, cm->mi_cols, cm->log2_tile_cols);
}

void vp10_tile_init(TileInfo *tile, const VP10_COMMON *cm, int row, int col) {
  vp10_tile_set_row(tile, cm, row);
  vp10_tile_set_col(tile, cm, col);
}

static int get_min_log2_tile_cols(const int sb64_cols) {
  int min_log2 = 0;
  while ((MAX_TILE_WIDTH_B64 << min_log2) < sb64_cols) ++min_log2;
  return min_log2;
}

static int get_max_log2_tile_cols(const int sb64_cols) {
  int max_log2 = 1;
  while ((sb64_cols >> max_log2) >= MIN_TILE_WIDTH_B64) ++max_log2;
  return max_log2 - 1;
}

void vp10_get_tile_n_bits(int mi_cols, int *min_log2_tile_cols,
                          int *max_log2_tile_cols) {
  const int sb64_cols = mi_cols_aligned_to_sb(mi_cols) >> MI_BLOCK_SIZE_LOG2;
  *min_log2_tile_cols = get_min_log2_tile_cols(sb64_cols);
  *max_log2_tile_cols = get_max_log2_tile_cols(sb64_cols);
  assert(*min_log2_tile_cols <= *max_log2_tile_cols);
}
