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

#include <math.h>

#include "./av1_rtcd.h"
#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/aom_once.h"
#include "aom_ports/mem.h"
#include "aom_ports/system_state.h"
#include "av1/common/reconintra.h"
#include "av1/common/onyxc_int.h"
#if CONFIG_CFL
#include "av1/common/cfl.h"
#endif

enum {
  NEED_LEFT = 1 << 1,
  NEED_ABOVE = 1 << 2,
  NEED_ABOVERIGHT = 1 << 3,
  NEED_ABOVELEFT = 1 << 4,
  NEED_BOTTOMLEFT = 1 << 5,
};

#if CONFIG_INTRA_EDGE
#define INTRA_EDGE_FILT 3
#define INTRA_EDGE_TAPS 5
#define MAX_UPSAMPLE_SZ 16
#endif  // CONFIG_INTRA_EDGE

static const uint8_t extend_modes[INTRA_MODES] = {
  NEED_ABOVE | NEED_LEFT,                   // DC
  NEED_ABOVE,                               // V
  NEED_LEFT,                                // H
  NEED_ABOVE | NEED_ABOVERIGHT,             // D45
  NEED_LEFT | NEED_ABOVE | NEED_ABOVELEFT,  // D135
  NEED_LEFT | NEED_ABOVE | NEED_ABOVELEFT,  // D117
  NEED_LEFT | NEED_ABOVE | NEED_ABOVELEFT,  // D153
  NEED_LEFT | NEED_BOTTOMLEFT,              // D207
  NEED_ABOVE | NEED_ABOVERIGHT,             // D63
  NEED_LEFT | NEED_ABOVE,                   // SMOOTH
  NEED_LEFT | NEED_ABOVE,                   // SMOOTH_V
  NEED_LEFT | NEED_ABOVE,                   // SMOOTH_H
  NEED_LEFT | NEED_ABOVE | NEED_ABOVELEFT,  // PAETH
};

// Tables to store if the top-right reference pixels are available. The flags
// are represented with bits, packed into 8-bit integers. E.g., for the 32x32
// blocks in a 128x128 superblock, the index of the "o" block is 10 (in raster
// order), so its flag is stored at the 3rd bit of the 2nd entry in the table,
// i.e. (table[10 / 8] >> (10 % 8)) & 1.
//       . . . .
//       . . . .
//       . . o .
//       . . . .
uint8_t has_tr_4x4[128] = {
  255, 255, 255, 255, 85, 85, 85, 85, 119, 119, 119, 119, 85, 85, 85, 85,
  127, 127, 127, 127, 85, 85, 85, 85, 119, 119, 119, 119, 85, 85, 85, 85,
  255, 127, 255, 127, 85, 85, 85, 85, 119, 119, 119, 119, 85, 85, 85, 85,
  127, 127, 127, 127, 85, 85, 85, 85, 119, 119, 119, 119, 85, 85, 85, 85,
  255, 255, 255, 127, 85, 85, 85, 85, 119, 119, 119, 119, 85, 85, 85, 85,
  127, 127, 127, 127, 85, 85, 85, 85, 119, 119, 119, 119, 85, 85, 85, 85,
  255, 127, 255, 127, 85, 85, 85, 85, 119, 119, 119, 119, 85, 85, 85, 85,
  127, 127, 127, 127, 85, 85, 85, 85, 119, 119, 119, 119, 85, 85, 85, 85,
};
uint8_t has_tr_4x8[64] = {
  255, 255, 255, 255, 119, 119, 119, 119, 127, 127, 127, 127, 119,
  119, 119, 119, 255, 127, 255, 127, 119, 119, 119, 119, 127, 127,
  127, 127, 119, 119, 119, 119, 255, 255, 255, 127, 119, 119, 119,
  119, 127, 127, 127, 127, 119, 119, 119, 119, 255, 127, 255, 127,
  119, 119, 119, 119, 127, 127, 127, 127, 119, 119, 119, 119,
};
uint8_t has_tr_8x4[64] = {
  255, 255, 0, 0, 85, 85, 0, 0, 119, 119, 0, 0, 85, 85, 0, 0,
  127, 127, 0, 0, 85, 85, 0, 0, 119, 119, 0, 0, 85, 85, 0, 0,
  255, 127, 0, 0, 85, 85, 0, 0, 119, 119, 0, 0, 85, 85, 0, 0,
  127, 127, 0, 0, 85, 85, 0, 0, 119, 119, 0, 0, 85, 85, 0, 0,
};
uint8_t has_tr_8x8[32] = {
  255, 255, 85, 85, 119, 119, 85, 85, 127, 127, 85, 85, 119, 119, 85, 85,
  255, 127, 85, 85, 119, 119, 85, 85, 127, 127, 85, 85, 119, 119, 85, 85,
};
uint8_t has_tr_8x16[16] = {
  255, 255, 119, 119, 127, 127, 119, 119,
  255, 127, 119, 119, 127, 127, 119, 119,
};
uint8_t has_tr_16x8[16] = {
  255, 0, 85, 0, 119, 0, 85, 0, 127, 0, 85, 0, 119, 0, 85, 0,
};
uint8_t has_tr_16x16[8] = {
  255, 85, 119, 85, 127, 85, 119, 85,
};
uint8_t has_tr_16x32[4] = {
  255, 119, 127, 119,
};
uint8_t has_tr_32x16[4] = {
  15, 5, 7, 5,
};
uint8_t has_tr_32x32[2] = { 95, 87 };
uint8_t has_tr_32x64[1] = { 127 };
uint8_t has_tr_64x32[1] = { 19 };
uint8_t has_tr_64x64[1] = { 7 };
uint8_t has_tr_64x128[1] = { 3 };
uint8_t has_tr_128x64[1] = { 1 };
uint8_t has_tr_128x128[1] = { 1 };
uint8_t has_tr_4x16[32] = {
  255, 255, 255, 255, 127, 127, 127, 127, 255, 127, 255,
  127, 127, 127, 127, 127, 255, 255, 255, 127, 127, 127,
  127, 127, 255, 127, 255, 127, 127, 127, 127, 127,
};
uint8_t has_tr_16x4[32] = {
  255, 0, 0, 0, 85, 0, 0, 0, 119, 0, 0, 0, 85, 0, 0, 0,
  127, 0, 0, 0, 85, 0, 0, 0, 119, 0, 0, 0, 85, 0, 0, 0,
};
uint8_t has_tr_8x32[8] = {
  255, 255, 127, 127, 255, 127, 127, 127,
};
uint8_t has_tr_32x8[8] = {
  15, 0, 5, 0, 7, 0, 5, 0,
};
uint8_t has_tr_16x64[2] = {
  255, 127,
};
uint8_t has_tr_64x16[2] = {
  3, 1,
};
uint8_t has_tr_32x128[1] = {
  15,
};
uint8_t has_tr_128x32[1] = {
  1,
};

#if CONFIG_EXT_PARTITION
static const uint8_t *const has_tr_tables[BLOCK_SIZES_ALL] = {
  // 4X4
  has_tr_4x4,
  // 4X8,       8X4,            8X8
  has_tr_4x8, has_tr_8x4, has_tr_8x8,
  // 8X16,      16X8,           16X16
  has_tr_8x16, has_tr_16x8, has_tr_16x16,
  // 16X32,     32X16,          32X32
  has_tr_16x32, has_tr_32x16, has_tr_32x32,
  // 32X64,     64X32,          64X64
  has_tr_32x64, has_tr_64x32, has_tr_64x64,
  // 64x128,    128x64,         128x128
  has_tr_64x128, has_tr_128x64, has_tr_128x128,
#if CONFIG_EXT_PARTITION_TYPES
  // 4x16,      16x4,            8x32
  has_tr_4x16, has_tr_16x4, has_tr_8x32,
  // 32x8,      16x64,           64x16
  has_tr_32x8, has_tr_16x64, has_tr_64x16,
  // 32x128,    128x32
  has_tr_32x128, has_tr_128x32
#else
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
#endif
};
#else
static const uint8_t *const has_tr_tables[BLOCK_SIZES_ALL] = {
  // 4X4
  has_tr_8x8,
  // 4X8,         8X4,            8X8
  has_tr_8x16, has_tr_16x8, has_tr_16x16,
  // 8X16,        16X8,           16X16
  has_tr_16x32, has_tr_32x16, has_tr_32x32,
  // 16X32,       32X16,          32X32
  has_tr_32x64, has_tr_64x32, has_tr_64x64,
  // 32X64,       64X32,          64X64
  has_tr_64x128, has_tr_128x64, has_tr_128x128,

#if CONFIG_EXT_PARTITION_TYPES
  // 4x16,        16x4,           8x32
  has_tr_8x32, has_tr_32x8, has_tr_16x64,
  // 32x8,        16x64,          64x16
  has_tr_64x16, has_tr_32x128, has_tr_128x32,
#else
  NULL, NULL, NULL, NULL, NULL, NULL,
#endif
};
#endif  // CONFIG_EXT_PARTITION

#if CONFIG_EXT_PARTITION_TYPES
uint8_t has_tr_vert_8x8[32] = {
  255, 255, 0, 0, 119, 119, 0, 0, 127, 127, 0, 0, 119, 119, 0, 0,
  255, 127, 0, 0, 119, 119, 0, 0, 127, 127, 0, 0, 119, 119, 0, 0,
};
uint8_t has_tr_vert_16x16[8] = {
  255, 0, 119, 0, 127, 0, 119, 0,
};
uint8_t has_tr_vert_32x32[2] = { 15, 7 };
uint8_t has_tr_vert_64x64[1] = { 3 };

// The _vert_* tables are like the ordinary tables above, but describe the
// order we visit square blocks when doing a PARTITION_VERT_A or
// PARTITION_VERT_B. This is the same order as normal except for on the last
// split where we go vertically (TL, BL, TR, BR). We treat the rectangular block
// as a pair of squares, which means that these tables work correctly for both
// mixed vertical partition types.
//
// There are tables for each of the square sizes. Vertical rectangles (like
// BLOCK_16X32) use their respective "non-vert" table
#if CONFIG_EXT_PARTITION
static const uint8_t *const has_tr_vert_tables[BLOCK_SIZES] = {
  // 4X4
  NULL,
  // 4X8,      8X4,         8X8
  has_tr_4x8, NULL, has_tr_vert_8x8,
  // 8X16,     16X8,        16X16
  has_tr_8x16, NULL, has_tr_vert_16x16,
  // 16X32,    32X16,       32X32
  has_tr_16x32, NULL, has_tr_vert_32x32,
  // 32X64,    64X32,       64X64
  has_tr_32x64, NULL, has_tr_vert_64x64,
  // 64x128,   128x64,      128x128
  has_tr_64x128, NULL, has_tr_128x128,
};
#else
static const uint8_t *const has_tr_vert_tables[BLOCK_SIZES] = {
  // 4X4
  NULL,
  // 4X8,       8X4,         8X8
  has_tr_8x16, NULL, has_tr_vert_16x16,
  // 8X16,      16X8,        16X16
  has_tr_16x32, NULL, has_tr_vert_32x32,
  // 16X32,     32X16,       32X32
  has_tr_32x64, NULL, has_tr_vert_64x64,
  // 32X64,     64X32,       64X64
  has_tr_64x128, NULL, has_tr_128x128,
};
#endif
#endif  // CONFIG_EXT_PARTITION_TYPES

static const uint8_t *get_has_tr_table(PARTITION_TYPE partition,
                                       BLOCK_SIZE bsize) {
  const uint8_t *ret = NULL;
#if CONFIG_EXT_PARTITION_TYPES
  // If this is a mixed vertical partition, look up bsize in orders_vert.
  if (partition == PARTITION_VERT_A || partition == PARTITION_VERT_B) {
    assert(bsize < BLOCK_SIZES);
    ret = has_tr_vert_tables[bsize];
  } else {
    ret = has_tr_tables[bsize];
  }
#else
  (void)partition;
  ret = has_tr_tables[bsize];
#endif  // CONFIG_EXT_PARTITION_TYPES
  assert(ret);
  return ret;
}

static int has_top_right(const AV1_COMMON *cm, BLOCK_SIZE bsize, int mi_row,
                         int mi_col, int top_available, int right_available,
                         PARTITION_TYPE partition, TX_SIZE txsz, int row_off,
                         int col_off, int ss_x) {
  if (!top_available || !right_available) return 0;

  const int bw_unit = block_size_wide[bsize] >> tx_size_wide_log2[0];
  const int plane_bw_unit = AOMMAX(bw_unit >> ss_x, 1);
  const int top_right_count_unit = tx_size_wide_unit[txsz];

  if (row_off > 0) {  // Just need to check if enough pixels on the right.
#if CONFIG_EXT_PARTITION
    if (col_off + top_right_count_unit >=
        (block_size_wide[BLOCK_64X64] >> (tx_size_wide_log2[0] + ss_x)))
      return 0;
#endif
    return col_off + top_right_count_unit < plane_bw_unit;
  } else {
    // All top-right pixels are in the block above, which is already available.
    if (col_off + top_right_count_unit < plane_bw_unit) return 1;

    const int bw_in_mi_log2 = mi_width_log2_lookup[bsize];
    const int bh_in_mi_log2 = mi_height_log2_lookup[bsize];
    const int sb_mi_size = mi_size_high[cm->seq_params.sb_size];
    const int blk_row_in_sb = (mi_row & (sb_mi_size - 1)) >> bh_in_mi_log2;
    const int blk_col_in_sb = (mi_col & (sb_mi_size - 1)) >> bw_in_mi_log2;

    // Top row of superblock: so top-right pixels are in the top and/or
    // top-right superblocks, both of which are already available.
    if (blk_row_in_sb == 0) return 1;

    // Rightmost column of superblock (and not the top row): so top-right pixels
    // fall in the right superblock, which is not available yet.
    if (((blk_col_in_sb + 1) << bw_in_mi_log2) >= sb_mi_size) {
      return 0;
    }

    // General case (neither top row nor rightmost column): check if the
    // top-right block is coded before the current block.
    const int this_blk_index =
        ((blk_row_in_sb + 0) << (MAX_MIB_SIZE_LOG2 - bw_in_mi_log2)) +
        blk_col_in_sb + 0;
    const int idx1 = this_blk_index / 8;
    const int idx2 = this_blk_index % 8;
    const uint8_t *has_tr_table = get_has_tr_table(partition, bsize);
    return (has_tr_table[idx1] >> idx2) & 1;
  }
}

// Similar to the has_tr_* tables, but store if the bottom-left reference
// pixels are available.
uint8_t has_bl_4x4[128] = {
  84, 85, 85, 85, 16, 17, 17, 17, 84, 85, 85, 85, 0,  1,  1,  1,  84, 85, 85,
  85, 16, 17, 17, 17, 84, 85, 85, 85, 0,  0,  1,  0,  84, 85, 85, 85, 16, 17,
  17, 17, 84, 85, 85, 85, 0,  1,  1,  1,  84, 85, 85, 85, 16, 17, 17, 17, 84,
  85, 85, 85, 0,  0,  0,  0,  84, 85, 85, 85, 16, 17, 17, 17, 84, 85, 85, 85,
  0,  1,  1,  1,  84, 85, 85, 85, 16, 17, 17, 17, 84, 85, 85, 85, 0,  0,  1,
  0,  84, 85, 85, 85, 16, 17, 17, 17, 84, 85, 85, 85, 0,  1,  1,  1,  84, 85,
  85, 85, 16, 17, 17, 17, 84, 85, 85, 85, 0,  0,  0,  0,
};
uint8_t has_bl_4x8[64] = {
  16, 17, 17, 17, 0, 1, 1, 1, 16, 17, 17, 17, 0, 0, 1, 0,
  16, 17, 17, 17, 0, 1, 1, 1, 16, 17, 17, 17, 0, 0, 0, 0,
  16, 17, 17, 17, 0, 1, 1, 1, 16, 17, 17, 17, 0, 0, 1, 0,
  16, 17, 17, 17, 0, 1, 1, 1, 16, 17, 17, 17, 0, 0, 0, 0,
};
uint8_t has_bl_8x4[64] = {
  254, 255, 84, 85, 254, 255, 16, 17, 254, 255, 84, 85, 254, 255, 0, 1,
  254, 255, 84, 85, 254, 255, 16, 17, 254, 255, 84, 85, 254, 255, 0, 0,
  254, 255, 84, 85, 254, 255, 16, 17, 254, 255, 84, 85, 254, 255, 0, 1,
  254, 255, 84, 85, 254, 255, 16, 17, 254, 255, 84, 85, 254, 255, 0, 0,
};
uint8_t has_bl_8x8[32] = {
  84, 85, 16, 17, 84, 85, 0, 1, 84, 85, 16, 17, 84, 85, 0, 0,
  84, 85, 16, 17, 84, 85, 0, 1, 84, 85, 16, 17, 84, 85, 0, 0,
};
uint8_t has_bl_8x16[16] = {
  16, 17, 0, 1, 16, 17, 0, 0, 16, 17, 0, 1, 16, 17, 0, 0,
};
uint8_t has_bl_16x8[16] = {
  254, 84, 254, 16, 254, 84, 254, 0, 254, 84, 254, 16, 254, 84, 254, 0,
};
uint8_t has_bl_16x16[8] = {
  84, 16, 84, 0, 84, 16, 84, 0,
};
uint8_t has_bl_16x32[4] = {
  16, 0, 16, 0,
};
uint8_t has_bl_32x16[4] = {
  78, 14, 78, 14,
};
uint8_t has_bl_32x32[2] = {
  4, 4,
};
uint8_t has_bl_32x64[1] = { 0 };
uint8_t has_bl_64x32[1] = { 34 };
uint8_t has_bl_64x64[1] = { 0 };
uint8_t has_bl_64x128[1] = { 0 };
uint8_t has_bl_128x64[1] = { 0 };
uint8_t has_bl_128x128[1] = { 0 };
uint8_t has_bl_4x16[32] = {
  0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0,
  0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0,
};
uint8_t has_bl_16x4[32] = {
  254, 254, 254, 84, 254, 254, 254, 16, 254, 254, 254, 84, 254, 254, 254, 0,
  254, 254, 254, 84, 254, 254, 254, 16, 254, 254, 254, 84, 254, 254, 254, 0,
};
uint8_t has_bl_8x32[8] = {
  0, 1, 0, 0, 0, 1, 0, 0,
};
uint8_t has_bl_32x8[8] = {
  238, 78, 238, 14, 238, 78, 238, 14,
};
uint8_t has_bl_16x64[2] = {
  0, 0,
};
uint8_t has_bl_64x16[2] = {
  42, 42,
};
uint8_t has_bl_32x128[1] = {
  0,
};
uint8_t has_bl_128x32[1] = {
  0,
};

#if CONFIG_EXT_PARTITION
static const uint8_t *const has_bl_tables[BLOCK_SIZES_ALL] = {
  // 4X4
  has_bl_4x4,
  // 4X8,         8X4,         8X8
  has_bl_4x8, has_bl_8x4, has_bl_8x8,
  // 8X16,        16X8,        16X16
  has_bl_8x16, has_bl_16x8, has_bl_16x16,
  // 16X32,       32X16,       32X32
  has_bl_16x32, has_bl_32x16, has_bl_32x32,
  // 32X64,       64X32,       64X64
  has_bl_32x64, has_bl_64x32, has_bl_64x64,
  // 64x128,      128x64,      128x128
  has_bl_64x128, has_bl_128x64, has_bl_128x128,
#if CONFIG_EXT_PARTITION_TYPES
  // 4x16,        16x4,        8x32
  has_bl_4x16, has_bl_16x4, has_bl_8x32,
  // 32x8,        16x64,       64x16
  has_bl_32x8, has_bl_16x64, has_bl_64x16,
  // 32x128,      128x32
  has_bl_32x128, has_bl_128x32
#else
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
#endif
};
#else
static const uint8_t *const has_bl_tables[BLOCK_SIZES_ALL] = {
  // 4X4
  has_bl_8x8,
  // 4X8,         8X4,            8X8
  has_bl_8x16, has_bl_16x8, has_bl_16x16,
  // 8X16,        16X8,           16X16
  has_bl_16x32, has_bl_32x16, has_bl_32x32,
  // 16X32,       32X16,          32X32
  has_bl_32x64, has_bl_64x32, has_bl_64x64,
  // 32X64,       64X32,          64X64
  has_bl_64x128, has_bl_128x64, has_bl_128x128,

#if CONFIG_EXT_PARTITION_TYPES
  // 4x16,        16x4,           8x32
  has_bl_8x32, has_bl_32x8, has_bl_16x64,
  // 32x8,        16x64,          64x16
  has_bl_64x16, has_bl_32x128, has_bl_128x32,
#else
  NULL, NULL, NULL, NULL, NULL, NULL,
#endif
};
#endif  // CONFIG_EXT_PARTITION

#if CONFIG_EXT_PARTITION_TYPES
uint8_t has_bl_vert_8x8[32] = {
  254, 255, 16, 17, 254, 255, 0, 1, 254, 255, 16, 17, 254, 255, 0, 0,
  254, 255, 16, 17, 254, 255, 0, 1, 254, 255, 16, 17, 254, 255, 0, 0,
};
uint8_t has_bl_vert_16x16[8] = {
  254, 16, 254, 0, 254, 16, 254, 0,
};
uint8_t has_bl_vert_32x32[2] = { 14, 14 };
uint8_t has_bl_vert_64x64[1] = { 2 };

// The _vert_* tables are like the ordinary tables above, but describe the
// order we visit square blocks when doing a PARTITION_VERT_A or
// PARTITION_VERT_B. This is the same order as normal except for on the last
// split where we go vertically (TL, BL, TR, BR). We treat the rectangular block
// as a pair of squares, which means that these tables work correctly for both
// mixed vertical partition types.
//
// There are tables for each of the square sizes. Vertical rectangles (like
// BLOCK_16X32) use their respective "non-vert" table
#if CONFIG_EXT_PARTITION
static const uint8_t *const has_bl_vert_tables[BLOCK_SIZES] = {
  // 4X4
  NULL,
  // 4X8,     8X4,         8X8
  has_bl_4x8, NULL, has_bl_vert_8x8,
  // 8X16,    16X8,        16X16
  has_bl_8x16, NULL, has_bl_vert_16x16,
  // 16X32,   32X16,       32X32
  has_bl_16x32, NULL, has_bl_vert_32x32,
  // 32X64,   64X32,       64X64
  has_bl_32x64, NULL, has_bl_vert_64x64,
  // 64x128,  128x64,      128x128
  has_bl_64x128, NULL, has_bl_128x128,
};
#else
static const uint8_t *const has_bl_vert_tables[BLOCK_SIZES] = {
  // 4X4
  NULL,
  // 4X8,      8X4,         8X8
  has_bl_8x16, NULL, has_bl_vert_16x16,
  // 8X16,     16X8,        16X16
  has_bl_16x32, NULL, has_bl_vert_32x32,
  // 16X32,    32X16,       32X32
  has_bl_32x64, NULL, has_bl_vert_64x64,
  // 32X64,    64X32,       64X64
  has_bl_64x128, NULL, has_bl_128x128,
};
#endif
#endif  // CONFIG_EXT_PARTITION_TYPES

static const uint8_t *get_has_bl_table(PARTITION_TYPE partition,
                                       BLOCK_SIZE bsize) {
  const uint8_t *ret = NULL;
#if CONFIG_EXT_PARTITION_TYPES
  // If this is a mixed vertical partition, look up bsize in orders_vert.
  if (partition == PARTITION_VERT_A || partition == PARTITION_VERT_B) {
    assert(bsize < BLOCK_SIZES);
    ret = has_bl_vert_tables[bsize];
  } else {
    ret = has_bl_tables[bsize];
  }
#else
  (void)partition;
  ret = has_bl_tables[bsize];
#endif  // CONFIG_EXT_PARTITION_TYPES
  assert(ret);
  return ret;
}

static int has_bottom_left(const AV1_COMMON *cm, BLOCK_SIZE bsize, int mi_row,
                           int mi_col, int bottom_available, int left_available,
                           PARTITION_TYPE partition, TX_SIZE txsz, int row_off,
                           int col_off, int ss_y) {
  if (!bottom_available || !left_available) return 0;

  if (col_off > 0) {
    // Bottom-left pixels are in the bottom-left block, which is not available.
    return 0;
  } else {
    const int bh_unit = block_size_high[bsize] >> tx_size_high_log2[0];
    const int plane_bh_unit = AOMMAX(bh_unit >> ss_y, 1);
    const int bottom_left_count_unit = tx_size_high_unit[txsz];

    // All bottom-left pixels are in the left block, which is already available.
    if (row_off + bottom_left_count_unit < plane_bh_unit) return 1;

    const int bw_in_mi_log2 = mi_width_log2_lookup[bsize];
    const int bh_in_mi_log2 = mi_height_log2_lookup[bsize];
    const int sb_mi_size = mi_size_high[cm->seq_params.sb_size];
    const int blk_row_in_sb = (mi_row & (sb_mi_size - 1)) >> bh_in_mi_log2;
    const int blk_col_in_sb = (mi_col & (sb_mi_size - 1)) >> bw_in_mi_log2;

    // Leftmost column of superblock: so bottom-left pixels maybe in the left
    // and/or bottom-left superblocks. But only the left superblock is
    // available, so check if all required pixels fall in that superblock.
    if (blk_col_in_sb == 0) {
      const int blk_start_row_off = blk_row_in_sb
                                        << (bh_in_mi_log2 + MI_SIZE_LOG2 -
                                            tx_size_wide_log2[0]) >>
                                    ss_y;
      const int row_off_in_sb = blk_start_row_off + row_off;
      const int sb_height_unit =
          sb_mi_size << (MI_SIZE_LOG2 - tx_size_wide_log2[0]) >> ss_y;
      return row_off_in_sb + bottom_left_count_unit < sb_height_unit;
    }

    // Bottom row of superblock (and not the leftmost column): so bottom-left
    // pixels fall in the bottom superblock, which is not available yet.
    if (((blk_row_in_sb + 1) << bh_in_mi_log2) >= sb_mi_size) return 0;

    // General case (neither leftmost column nor bottom row): check if the
    // bottom-left block is coded before the current block.
    const int this_blk_index =
        ((blk_row_in_sb + 0) << (MAX_MIB_SIZE_LOG2 - bw_in_mi_log2)) +
        blk_col_in_sb + 0;
    const int idx1 = this_blk_index / 8;
    const int idx2 = this_blk_index % 8;
    const uint8_t *has_bl_table = get_has_bl_table(partition, bsize);
    return (has_bl_table[idx1] >> idx2) & 1;
  }
}

typedef void (*intra_pred_fn)(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left);

static intra_pred_fn pred[INTRA_MODES][TX_SIZES_ALL];
static intra_pred_fn dc_pred[2][2][TX_SIZES_ALL];

typedef void (*intra_high_pred_fn)(uint16_t *dst, ptrdiff_t stride,
                                   const uint16_t *above, const uint16_t *left,
                                   int bd);
static intra_high_pred_fn pred_high[INTRA_MODES][TX_SIZES_ALL];
static intra_high_pred_fn dc_pred_high[2][2][TX_SIZES_ALL];

static void av1_init_intra_predictors_internal(void) {
  assert(NELEMENTS(mode_to_angle_map) == INTRA_MODES);

#if CONFIG_TX64X64
#define INIT_RECTANGULAR(p, type)             \
  p[TX_4X8] = aom_##type##_predictor_4x8;     \
  p[TX_8X4] = aom_##type##_predictor_8x4;     \
  p[TX_8X16] = aom_##type##_predictor_8x16;   \
  p[TX_16X8] = aom_##type##_predictor_16x8;   \
  p[TX_16X32] = aom_##type##_predictor_16x32; \
  p[TX_32X16] = aom_##type##_predictor_32x16; \
  p[TX_32X64] = aom_##type##_predictor_32x64; \
  p[TX_64X32] = aom_##type##_predictor_64x32; \
  p[TX_4X16] = aom_##type##_predictor_4x16;   \
  p[TX_16X4] = aom_##type##_predictor_16x4;   \
  p[TX_8X32] = aom_##type##_predictor_8x32;   \
  p[TX_32X8] = aom_##type##_predictor_32x8;   \
  p[TX_16X64] = aom_##type##_predictor_16x64; \
  p[TX_64X16] = aom_##type##_predictor_64x16;
#else
#define INIT_RECTANGULAR(p, type)             \
  p[TX_4X8] = aom_##type##_predictor_4x8;     \
  p[TX_8X4] = aom_##type##_predictor_8x4;     \
  p[TX_8X16] = aom_##type##_predictor_8x16;   \
  p[TX_16X8] = aom_##type##_predictor_16x8;   \
  p[TX_16X32] = aom_##type##_predictor_16x32; \
  p[TX_32X16] = aom_##type##_predictor_32x16; \
  p[TX_4X16] = aom_##type##_predictor_4x16;   \
  p[TX_16X4] = aom_##type##_predictor_16x4;   \
  p[TX_8X32] = aom_##type##_predictor_8x32;   \
  p[TX_32X8] = aom_##type##_predictor_32x8;
#endif  // CONFIG_TX64X64

#if CONFIG_TX64X64
#define INIT_NO_4X4(p, type)                  \
  p[TX_8X8] = aom_##type##_predictor_8x8;     \
  p[TX_16X16] = aom_##type##_predictor_16x16; \
  p[TX_32X32] = aom_##type##_predictor_32x32; \
  p[TX_64X64] = aom_##type##_predictor_64x64; \
  INIT_RECTANGULAR(p, type)
#else
#define INIT_NO_4X4(p, type)                  \
  p[TX_8X8] = aom_##type##_predictor_8x8;     \
  p[TX_16X16] = aom_##type##_predictor_16x16; \
  p[TX_32X32] = aom_##type##_predictor_32x32; \
  INIT_RECTANGULAR(p, type)
#endif  // CONFIG_TX64X64

#define INIT_ALL_SIZES(p, type)           \
  p[TX_4X4] = aom_##type##_predictor_4x4; \
  INIT_NO_4X4(p, type)

  INIT_ALL_SIZES(pred[V_PRED], v);
  INIT_ALL_SIZES(pred[H_PRED], h);
  INIT_ALL_SIZES(pred[D207_PRED], d207e);
  INIT_ALL_SIZES(pred[D45_PRED], d45e);
  INIT_ALL_SIZES(pred[D63_PRED], d63e);
  INIT_ALL_SIZES(pred[D117_PRED], d117);
  INIT_ALL_SIZES(pred[D135_PRED], d135);
  INIT_ALL_SIZES(pred[D153_PRED], d153);

  INIT_ALL_SIZES(pred[PAETH_PRED], paeth);
  INIT_ALL_SIZES(pred[SMOOTH_PRED], smooth);
  INIT_ALL_SIZES(pred[SMOOTH_V_PRED], smooth_v);
  INIT_ALL_SIZES(pred[SMOOTH_H_PRED], smooth_h);

  INIT_ALL_SIZES(dc_pred[0][0], dc_128);
  INIT_ALL_SIZES(dc_pred[0][1], dc_top);
  INIT_ALL_SIZES(dc_pred[1][0], dc_left);
  INIT_ALL_SIZES(dc_pred[1][1], dc);

  INIT_ALL_SIZES(pred_high[V_PRED], highbd_v);
  INIT_ALL_SIZES(pred_high[H_PRED], highbd_h);
  INIT_ALL_SIZES(pred_high[D207_PRED], highbd_d207e);
  INIT_ALL_SIZES(pred_high[D45_PRED], highbd_d45e);
  INIT_ALL_SIZES(pred_high[D63_PRED], highbd_d63e);
  INIT_ALL_SIZES(pred_high[D117_PRED], highbd_d117);
  INIT_ALL_SIZES(pred_high[D135_PRED], highbd_d135);
  INIT_ALL_SIZES(pred_high[D153_PRED], highbd_d153);

  INIT_ALL_SIZES(pred_high[PAETH_PRED], highbd_paeth);
  INIT_ALL_SIZES(pred_high[SMOOTH_PRED], highbd_smooth);
  INIT_ALL_SIZES(pred_high[SMOOTH_V_PRED], highbd_smooth_v);
  INIT_ALL_SIZES(pred_high[SMOOTH_H_PRED], highbd_smooth_h);

  INIT_ALL_SIZES(dc_pred_high[0][0], highbd_dc_128);
  INIT_ALL_SIZES(dc_pred_high[0][1], highbd_dc_top);
  INIT_ALL_SIZES(dc_pred_high[1][0], highbd_dc_left);
  INIT_ALL_SIZES(dc_pred_high[1][1], highbd_dc);
#undef intra_pred_allsizes
}

// Directional prediction, zone 1: 0 < angle < 90
static void dr_prediction_z1(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                             const uint8_t *above, const uint8_t *left,
#if CONFIG_INTRA_EDGE
                             int upsample_above,
#endif  // CONFIG_INTRA_EDGE
                             int dx, int dy) {
  int r, c, x, base, shift, val;

  (void)left;
  (void)dy;
  assert(dy == 1);
  assert(dx > 0);

#if !CONFIG_INTRA_EDGE
  const int upsample_above = 0;
#endif  // !CONFIG_INTRA_EDGE
  const int max_base_x = ((bw + bh) - 1) << upsample_above;
#if CONFIG_EXT_INTRA_MOD2
  const int frac_bits = 6 - upsample_above;
#else
  const int frac_bits = 8 - upsample_above;
#endif
  const int base_inc = 1 << upsample_above;
  x = dx;
  for (r = 0; r < bh; ++r, dst += stride, x += dx) {
    base = x >> frac_bits;
#if CONFIG_EXT_INTRA_MOD2
    shift = ((x << upsample_above) & 0x3F) >> 1;
#else
    shift = (x << upsample_above) & 0xFF;
#endif

    if (base >= max_base_x) {
      for (int i = r; i < bh; ++i) {
        memset(dst, above[max_base_x], bw * sizeof(dst[0]));
        dst += stride;
      }
      return;
    }

    for (c = 0; c < bw; ++c, base += base_inc) {
      if (base < max_base_x) {
#if CONFIG_EXT_INTRA_MOD2
        val = above[base] * (32 - shift) + above[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 5);
#else
        val = above[base] * (256 - shift) + above[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 8);
#endif
        dst[c] = clip_pixel(val);
      } else {
        dst[c] = above[max_base_x];
      }
    }
  }
}

// Directional prediction, zone 2: 90 < angle < 180
static void dr_prediction_z2(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                             const uint8_t *above, const uint8_t *left,
#if CONFIG_INTRA_EDGE
                             int upsample_above, int upsample_left,
#endif  // CONFIG_INTRA_EDGE
                             int dx, int dy) {
  int r, c, x, y, shift1, shift2, val, base1, base2;

  assert(dx > 0);
  assert(dy > 0);

#if !CONFIG_INTRA_EDGE
  const int upsample_above = 0;
  const int upsample_left = 0;
#endif  // !CONFIG_INTRA_EDGE
  const int min_base_x = -(1 << upsample_above);
#if CONFIG_EXT_INTRA_MOD2
  const int frac_bits_x = 6 - upsample_above;
  const int frac_bits_y = 6 - upsample_left;
#else
  const int frac_bits_x = 8 - upsample_above;
  const int frac_bits_y = 8 - upsample_left;
#endif
  const int base_inc_x = 1 << upsample_above;
  x = -dx;
  for (r = 0; r < bh; ++r, x -= dx, dst += stride) {
    base1 = x >> frac_bits_x;
#if CONFIG_EXT_INTRA_MOD2
    y = (r << 6) - dy;
#else
    y = (r << 8) - dy;
#endif
    for (c = 0; c < bw; ++c, base1 += base_inc_x, y -= dy) {
      if (base1 >= min_base_x) {
#if CONFIG_EXT_INTRA_MOD2
        shift1 = ((x * (1 << upsample_above)) & 0x3F) >> 1;
        val = above[base1] * (32 - shift1) + above[base1 + 1] * shift1;
        val = ROUND_POWER_OF_TWO(val, 5);
#else
        shift1 = (x * (1 << upsample_above)) & 0xFF;
        val = above[base1] * (256 - shift1) + above[base1 + 1] * shift1;
        val = ROUND_POWER_OF_TWO(val, 8);
#endif
      } else {
        base2 = y >> frac_bits_y;
        assert(base2 >= -(1 << upsample_left));
#if CONFIG_EXT_INTRA_MOD2
        shift2 = ((y * (1 << upsample_left)) & 0x3F) >> 1;
        val = left[base2] * (32 - shift2) + left[base2 + 1] * shift2;
        val = ROUND_POWER_OF_TWO(val, 5);
#else
        shift2 = (y * (1 << upsample_left)) & 0xFF;
        val = left[base2] * (256 - shift2) + left[base2 + 1] * shift2;
        val = ROUND_POWER_OF_TWO(val, 8);
#endif
      }
      dst[c] = clip_pixel(val);
    }
  }
}

// Directional prediction, zone 3: 180 < angle < 270
static void dr_prediction_z3(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                             const uint8_t *above, const uint8_t *left,
#if CONFIG_INTRA_EDGE
                             int upsample_left,
#endif  // CONFIG_INTRA_EDGE
                             int dx, int dy) {
  int r, c, y, base, shift, val;

  (void)above;
  (void)dx;

  assert(dx == 1);
  assert(dy > 0);

#if !CONFIG_INTRA_EDGE
  const int upsample_left = 0;
#endif  // !CONFIG_INTRA_EDGE
  const int max_base_y = (bw + bh - 1) << upsample_left;
#if CONFIG_EXT_INTRA_MOD2
  const int frac_bits = 6 - upsample_left;
#else
  const int frac_bits = 8 - upsample_left;
#endif
  const int base_inc = 1 << upsample_left;
  y = dy;
  for (c = 0; c < bw; ++c, y += dy) {
    base = y >> frac_bits;
#if CONFIG_EXT_INTRA_MOD2
    shift = ((y << upsample_left) & 0x3F) >> 1;
#else
    shift = (y << upsample_left) & 0xFF;
#endif

    for (r = 0; r < bh; ++r, base += base_inc) {
      if (base < max_base_y) {
#if CONFIG_EXT_INTRA_MOD2
        val = left[base] * (32 - shift) + left[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 5);
#else
        val = left[base] * (256 - shift) + left[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 8);
#endif
        dst[r * stride + c] = clip_pixel(val);
      } else {
        for (; r < bh; ++r) dst[r * stride + c] = left[max_base_y];
        break;
      }
    }
  }
}

// Get the shift (up-scaled by 256) in X w.r.t a unit change in Y.
// If angle > 0 && angle < 90, dx = -((int)(256 / t));
// If angle > 90 && angle < 180, dx = (int)(256 / t);
// If angle > 180 && angle < 270, dx = 1;
static INLINE int get_dx(int angle) {
  if (angle > 0 && angle < 90) {
    return dr_intra_derivative[angle];
  } else if (angle > 90 && angle < 180) {
    return dr_intra_derivative[180 - angle];
  } else {
    // In this case, we are not really going to use dx. We may return any value.
    return 1;
  }
}

// Get the shift (up-scaled by 256) in Y w.r.t a unit change in X.
// If angle > 0 && angle < 90, dy = 1;
// If angle > 90 && angle < 180, dy = (int)(256 * t);
// If angle > 180 && angle < 270, dy = -((int)(256 * t));
static INLINE int get_dy(int angle) {
  if (angle > 90 && angle < 180) {
    return dr_intra_derivative[angle - 90];
  } else if (angle > 180 && angle < 270) {
    return dr_intra_derivative[270 - angle];
  } else {
    // In this case, we are not really going to use dy. We may return any value.
    return 1;
  }
}

static void dr_predictor(uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size,
                         const uint8_t *above, const uint8_t *left,
#if CONFIG_INTRA_EDGE
                         int upsample_above, int upsample_left,
#endif  // CONFIG_INTRA_EDGE
                         int angle) {
  const int dx = get_dx(angle);
  const int dy = get_dy(angle);
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];
  assert(angle > 0 && angle < 270);

  if (angle > 0 && angle < 90) {
    dr_prediction_z1(dst, stride, bw, bh, above, left,
#if CONFIG_INTRA_EDGE
                     upsample_above,
#endif  // CONFIG_INTRA_EDGE
                     dx, dy);
  } else if (angle > 90 && angle < 180) {
    dr_prediction_z2(dst, stride, bw, bh, above, left,
#if CONFIG_INTRA_EDGE
                     upsample_above, upsample_left,
#endif  // CONFIG_INTRA_EDGE
                     dx, dy);
  } else if (angle > 180 && angle < 270) {
    dr_prediction_z3(dst, stride, bw, bh, above, left,
#if CONFIG_INTRA_EDGE
                     upsample_left,
#endif  // CONFIG_INTRA_EDGE
                     dx, dy);
  } else if (angle == 90) {
    pred[V_PRED][tx_size](dst, stride, above, left);
  } else if (angle == 180) {
    pred[H_PRED][tx_size](dst, stride, above, left);
  }
}

// Directional prediction, zone 1: 0 < angle < 90
static void highbd_dr_prediction_z1(uint16_t *dst, ptrdiff_t stride, int bw,
                                    int bh, const uint16_t *above,
                                    const uint16_t *left,
#if CONFIG_INTRA_EDGE
                                    int upsample_above,
#endif  // CONFIG_INTRA_EDGE
                                    int dx, int dy, int bd) {
  int r, c, x, base, shift, val;

  (void)left;
  (void)dy;
  assert(dy == 1);
  assert(dx > 0);

#if !CONFIG_INTRA_EDGE
  const int upsample_above = 0;
#endif  // !CONFIG_INTRA_EDGE
  const int max_base_x = ((bw + bh) - 1) << upsample_above;
#if CONFIG_EXT_INTRA_MOD2
  const int frac_bits = 6 - upsample_above;
#else
  const int frac_bits = 8 - upsample_above;
#endif
  const int base_inc = 1 << upsample_above;
  x = dx;
  for (r = 0; r < bh; ++r, dst += stride, x += dx) {
    base = x >> frac_bits;
#if CONFIG_EXT_INTRA_MOD2
    shift = ((x << upsample_above) & 0x3F) >> 1;
#else
    shift = (x << upsample_above) & 0xFF;
#endif

    if (base >= max_base_x) {
      for (int i = r; i < bh; ++i) {
        aom_memset16(dst, above[max_base_x], bw);
        dst += stride;
      }
      return;
    }

    for (c = 0; c < bw; ++c, base += base_inc) {
      if (base < max_base_x) {
#if CONFIG_EXT_INTRA_MOD2
        val = above[base] * (32 - shift) + above[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 5);
#else
        val = above[base] * (256 - shift) + above[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 8);
#endif
        dst[c] = clip_pixel_highbd(val, bd);
      } else {
        dst[c] = above[max_base_x];
      }
    }
  }
}

// Directional prediction, zone 2: 90 < angle < 180
static void highbd_dr_prediction_z2(uint16_t *dst, ptrdiff_t stride, int bw,
                                    int bh, const uint16_t *above,
                                    const uint16_t *left,
#if CONFIG_INTRA_EDGE
                                    int upsample_above, int upsample_left,
#endif  // CONFIG_INTRA_EDGE
                                    int dx, int dy, int bd) {
  int r, c, x, y, shift, val, base;

  assert(dx > 0);
  assert(dy > 0);

#if !CONFIG_INTRA_EDGE
  const int upsample_above = 0;
  const int upsample_left = 0;
#endif  // !CONFIG_INTRA_EDGE
  const int min_base_x = -(1 << upsample_above);
#if CONFIG_EXT_INTRA_MOD2
  const int frac_bits_x = 6 - upsample_above;
  const int frac_bits_y = 6 - upsample_left;
#else
  const int frac_bits_x = 8 - upsample_above;
  const int frac_bits_y = 8 - upsample_left;
#endif
  for (r = 0; r < bh; ++r) {
    for (c = 0; c < bw; ++c) {
      y = r + 1;
#if CONFIG_EXT_INTRA_MOD2
      x = (c << 6) - y * dx;
#else
      x = (c << 8) - y * dx;
#endif
      base = x >> frac_bits_x;
      if (base >= min_base_x) {
#if CONFIG_EXT_INTRA_MOD2
        shift = ((x * (1 << upsample_above)) & 0x3F) >> 1;
        val = above[base] * (32 - shift) + above[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 5);
#else
        shift = (x * (1 << upsample_above)) & 0xFF;
        val = above[base] * (256 - shift) + above[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 8);
#endif
      } else {
        x = c + 1;
#if CONFIG_EXT_INTRA_MOD2
        y = (r << 6) - x * dy;
#else
        y = (r << 8) - x * dy;
#endif
        base = y >> frac_bits_y;
#if CONFIG_EXT_INTRA_MOD2
        shift = ((y * (1 << upsample_left)) & 0x3F) >> 1;
        val = left[base] * (32 - shift) + left[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 5);
#else
        shift = (y * (1 << upsample_left)) & 0xFF;
        val = left[base] * (256 - shift) + left[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 8);
#endif
      }
      dst[c] = clip_pixel_highbd(val, bd);
    }
    dst += stride;
  }
}

// Directional prediction, zone 3: 180 < angle < 270
static void highbd_dr_prediction_z3(uint16_t *dst, ptrdiff_t stride, int bw,
                                    int bh, const uint16_t *above,
                                    const uint16_t *left,
#if CONFIG_INTRA_EDGE
                                    int upsample_left,
#endif  // CONFIG_INTRA_EDGE
                                    int dx, int dy, int bd) {
  int r, c, y, base, shift, val;

  (void)above;
  (void)dx;
  assert(dx == 1);
  assert(dy > 0);

#if !CONFIG_INTRA_EDGE
  const int upsample_left = 0;
#endif  // !CONFIG_INTRA_EDGE
  const int max_base_y = (bw + bh - 1) << upsample_left;
#if CONFIG_EXT_INTRA_MOD2
  const int frac_bits = 6 - upsample_left;
#else
  const int frac_bits = 8 - upsample_left;
#endif
  const int base_inc = 1 << upsample_left;
  y = dy;
  for (c = 0; c < bw; ++c, y += dy) {
    base = y >> frac_bits;
#if CONFIG_EXT_INTRA_MOD2
    shift = ((y << upsample_left) & 0x3F) >> 1;
#else
    shift = (y << upsample_left) & 0xFF;
#endif

    for (r = 0; r < bh; ++r, base += base_inc) {
      if (base < max_base_y) {
#if CONFIG_EXT_INTRA_MOD2
        val = left[base] * (32 - shift) + left[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 5);
#else
        val = left[base] * (256 - shift) + left[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 8);
#endif
        dst[r * stride + c] = clip_pixel_highbd(val, bd);
      } else {
        for (; r < bh; ++r) dst[r * stride + c] = left[max_base_y];
        break;
      }
    }
  }
}

static void highbd_dr_predictor(uint16_t *dst, ptrdiff_t stride,
                                TX_SIZE tx_size, const uint16_t *above,
                                const uint16_t *left,
#if CONFIG_INTRA_EDGE
                                int upsample_above, int upsample_left,
#endif  // CONFIG_INTRA_EDGE
                                int angle, int bd) {
  const int dx = get_dx(angle);
  const int dy = get_dy(angle);
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];
  assert(angle > 0 && angle < 270);

  if (angle > 0 && angle < 90) {
    highbd_dr_prediction_z1(dst, stride, bw, bh, above, left,
#if CONFIG_INTRA_EDGE
                            upsample_above,
#endif  // CONFIG_INTRA_EDGE
                            dx, dy, bd);
  } else if (angle > 90 && angle < 180) {
    highbd_dr_prediction_z2(dst, stride, bw, bh, above, left,
#if CONFIG_INTRA_EDGE
                            upsample_above, upsample_left,
#endif  // CONFIG_INTRA_EDGE
                            dx, dy, bd);
  } else if (angle > 180 && angle < 270) {
    highbd_dr_prediction_z3(dst, stride, bw, bh, above, left,
#if CONFIG_INTRA_EDGE
                            upsample_left,
#endif  // CONFIG_INTRA_EDGE
                            dx, dy, bd);
  } else if (angle == 90) {
    pred_high[V_PRED][tx_size](dst, stride, above, left, bd);
  } else if (angle == 180) {
    pred_high[H_PRED][tx_size](dst, stride, above, left, bd);
  }
}

#if CONFIG_FILTER_INTRA
static int filter_intra_taps_4x2procunit[FILTER_INTRA_MODES][8][7] = {
  {
      { -6, 10, 0, 0, 0, 12, 0 },
      { -5, 2, 10, 0, 0, 9, 0 },
      { -3, 1, 1, 10, 0, 7, 0 },
      { -3, 1, 1, 2, 10, 5, 0 },
      { -4, 6, 0, 0, 0, 2, 12 },
      { -3, 2, 6, 0, 0, 2, 9 },
      { -3, 2, 2, 6, 0, 2, 7 },
      { -3, 1, 2, 2, 6, 3, 5 },
  },
  {
      { -10, 16, 0, 0, 0, 10, 0 },
      { -6, 0, 16, 0, 0, 6, 0 },
      { -4, 0, 0, 16, 0, 4, 0 },
      { -2, 0, 0, 0, 16, 2, 0 },
      { -10, 16, 0, 0, 0, 0, 10 },
      { -6, 0, 16, 0, 0, 0, 6 },
      { -4, 0, 0, 16, 0, 0, 4 },
      { -2, 0, 0, 0, 16, 0, 2 },
  },
  {
      { -8, 8, 0, 0, 0, 16, 0 },
      { -8, 0, 8, 0, 0, 16, 0 },
      { -8, 0, 0, 8, 0, 16, 0 },
      { -8, 0, 0, 0, 8, 16, 0 },
      { -4, 4, 0, 0, 0, 0, 16 },
      { -4, 0, 4, 0, 0, 0, 16 },
      { -4, 0, 0, 4, 0, 0, 16 },
      { -4, 0, 0, 0, 4, 0, 16 },
  },
  {
      { -2, 8, 0, 0, 0, 10, 0 },
      { -1, 3, 8, 0, 0, 6, 0 },
      { -1, 2, 3, 8, 0, 4, 0 },
      { 0, 1, 2, 3, 8, 2, 0 },
      { -1, 4, 0, 0, 0, 3, 10 },
      { -1, 3, 4, 0, 0, 4, 6 },
      { -1, 2, 3, 4, 0, 4, 4 },
      { -1, 2, 2, 3, 4, 3, 3 },
  },
  {
      { -12, 14, 0, 0, 0, 14, 0 },
      { -10, 0, 14, 0, 0, 12, 0 },
      { -9, 0, 0, 14, 0, 11, 0 },
      { -8, 0, 0, 0, 14, 10, 0 },
      { -10, 12, 0, 0, 0, 0, 14 },
      { -9, 1, 12, 0, 0, 0, 12 },
      { -8, 0, 0, 12, 0, 1, 11 },
      { -7, 0, 0, 1, 12, 1, 9 },
  },
};

static void filter_intra_predictor(uint8_t *dst, ptrdiff_t stride,
                                   TX_SIZE tx_size, const uint8_t *above,
                                   const uint8_t *left, int mode) {
  int r, c;
  int buffer[33][33];
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];

  assert(bw <= 32 && bh <= 32);

  // The initialization is just for silencing Jenkins static analysis warnings
  for (r = 0; r < bh + 1; ++r)
    memset(buffer[r], 0, (bw + 1) * sizeof(buffer[0][0]));

  for (r = 0; r < bh; ++r) buffer[r + 1][0] = (int)left[r];

  for (c = 0; c < bw + 1; ++c) buffer[0][c] = (int)above[c - 1];

  for (r = 1; r < bh + 1; r += 2)
    for (c = 1; c < bw + 1; c += 4) {
      const int p0 = buffer[r - 1][c - 1];
      const int p1 = buffer[r - 1][c];
      const int p2 = buffer[r - 1][c + 1];
      const int p3 = buffer[r - 1][c + 2];
      const int p4 = buffer[r - 1][c + 3];
      const int p5 = buffer[r][c - 1];
      const int p6 = buffer[r + 1][c - 1];
      for (int k = 0; k < 8; ++k) {
        int r_offset = k >> 2;
        int c_offset = k & 0x03;
        buffer[r + r_offset][c + c_offset] =
            filter_intra_taps_4x2procunit[mode][k][0] * p0 +
            filter_intra_taps_4x2procunit[mode][k][1] * p1 +
            filter_intra_taps_4x2procunit[mode][k][2] * p2 +
            filter_intra_taps_4x2procunit[mode][k][3] * p3 +
            filter_intra_taps_4x2procunit[mode][k][4] * p4 +
            filter_intra_taps_4x2procunit[mode][k][5] * p5 +
            filter_intra_taps_4x2procunit[mode][k][6] * p6;
        buffer[r + r_offset][c + c_offset] =
            clip_pixel(ROUND_POWER_OF_TWO_SIGNED(
                buffer[r + r_offset][c + c_offset], FILTER_INTRA_SCALE_BITS));
      }
    }

  for (r = 0; r < bh; ++r) {
    for (c = 0; c < bw; ++c) {
      dst[c] = buffer[r + 1][c + 1];
    }
    dst += stride;
  }
}

void av1_dc_filter_predictor_c(uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size,
                               const uint8_t *above, const uint8_t *left) {
  filter_intra_predictor(dst, stride, tx_size, above, left, FILTER_DC_PRED);
}

void av1_v_filter_predictor_c(uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size,
                              const uint8_t *above, const uint8_t *left) {
  filter_intra_predictor(dst, stride, tx_size, above, left, FILTER_V_PRED);
}

void av1_h_filter_predictor_c(uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size,
                              const uint8_t *above, const uint8_t *left) {
  filter_intra_predictor(dst, stride, tx_size, above, left, FILTER_H_PRED);
}

void av1_d153_filter_predictor_c(uint8_t *dst, ptrdiff_t stride,
                                 TX_SIZE tx_size, const uint8_t *above,
                                 const uint8_t *left) {
  filter_intra_predictor(dst, stride, tx_size, above, left, FILTER_D153_PRED);
}

void av1_paeth_filter_predictor_c(uint8_t *dst, ptrdiff_t stride,
                                  TX_SIZE tx_size, const uint8_t *above,
                                  const uint8_t *left) {
  filter_intra_predictor(dst, stride, tx_size, above, left, FILTER_PAETH_PRED);
}

static void filter_intra_predictors(FILTER_INTRA_MODE mode, uint8_t *dst,
                                    ptrdiff_t stride, TX_SIZE tx_size,
                                    const uint8_t *above, const uint8_t *left) {
  switch (mode) {
    case FILTER_DC_PRED:
      av1_dc_filter_predictor(dst, stride, tx_size, above, left);
      break;
    case FILTER_V_PRED:
      av1_v_filter_predictor(dst, stride, tx_size, above, left);
      break;
    case FILTER_H_PRED:
      av1_h_filter_predictor(dst, stride, tx_size, above, left);
      break;
    case FILTER_D153_PRED:
      av1_d153_filter_predictor(dst, stride, tx_size, above, left);
      break;
    case FILTER_PAETH_PRED:
      av1_paeth_filter_predictor(dst, stride, tx_size, above, left);
      break;
    default: assert(0);
  }
}
static void highbd_filter_intra_predictor(uint16_t *dst, ptrdiff_t stride,
                                          TX_SIZE tx_size,
                                          const uint16_t *above,
                                          const uint16_t *left, int mode,
                                          int bd) {
  int r, c;
  int buffer[33][33];
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];

  assert(bw <= 32 && bh <= 32);

  // The initialization is just for silencing Jenkins static analysis warnings
  for (r = 0; r < bh + 1; ++r)
    memset(buffer[r], 0, (bw + 1) * sizeof(buffer[0][0]));

  for (r = 0; r < bh; ++r) buffer[r + 1][0] = (int)left[r];

  for (c = 0; c < bw + 1; ++c) buffer[0][c] = (int)above[c - 1];

  for (r = 1; r < bh + 1; r += 2)
    for (c = 1; c < bw + 1; c += 4) {
      const int p0 = buffer[r - 1][c - 1];
      const int p1 = buffer[r - 1][c];
      const int p2 = buffer[r - 1][c + 1];
      const int p3 = buffer[r - 1][c + 2];
      const int p4 = buffer[r - 1][c + 3];
      const int p5 = buffer[r][c - 1];
      const int p6 = buffer[r + 1][c - 1];
      for (int k = 0; k < 8; ++k) {
        int r_offset = k >> 2;
        int c_offset = k & 0x03;
        buffer[r + r_offset][c + c_offset] =
            filter_intra_taps_4x2procunit[mode][k][0] * p0 +
            filter_intra_taps_4x2procunit[mode][k][1] * p1 +
            filter_intra_taps_4x2procunit[mode][k][2] * p2 +
            filter_intra_taps_4x2procunit[mode][k][3] * p3 +
            filter_intra_taps_4x2procunit[mode][k][4] * p4 +
            filter_intra_taps_4x2procunit[mode][k][5] * p5 +
            filter_intra_taps_4x2procunit[mode][k][6] * p6;
        buffer[r + r_offset][c + c_offset] = clip_pixel_highbd(
            ROUND_POWER_OF_TWO_SIGNED(buffer[r + r_offset][c + c_offset],
                                      FILTER_INTRA_SCALE_BITS),
            bd);
      }
    }

  for (r = 0; r < bh; ++r) {
    for (c = 0; c < bw; ++c) {
      dst[c] = buffer[r + 1][c + 1];
    }
    dst += stride;
  }
}

void av1_highbd_dc_filter_predictor_c(uint16_t *dst, ptrdiff_t stride,
                                      TX_SIZE tx_size, const uint16_t *above,
                                      const uint16_t *left, int bd) {
  highbd_filter_intra_predictor(dst, stride, tx_size, above, left,
                                FILTER_DC_PRED, bd);
}

void av1_highbd_v_filter_predictor_c(uint16_t *dst, ptrdiff_t stride,
                                     TX_SIZE tx_size, const uint16_t *above,
                                     const uint16_t *left, int bd) {
  highbd_filter_intra_predictor(dst, stride, tx_size, above, left,
                                FILTER_V_PRED, bd);
}

void av1_highbd_h_filter_predictor_c(uint16_t *dst, ptrdiff_t stride,
                                     TX_SIZE tx_size, const uint16_t *above,
                                     const uint16_t *left, int bd) {
  highbd_filter_intra_predictor(dst, stride, tx_size, above, left,
                                FILTER_H_PRED, bd);
}

void av1_highbd_d153_filter_predictor_c(uint16_t *dst, ptrdiff_t stride,
                                        TX_SIZE tx_size, const uint16_t *above,
                                        const uint16_t *left, int bd) {
  highbd_filter_intra_predictor(dst, stride, tx_size, above, left,
                                FILTER_D153_PRED, bd);
}

void av1_highbd_paeth_filter_predictor_c(uint16_t *dst, ptrdiff_t stride,
                                         TX_SIZE tx_size, const uint16_t *above,
                                         const uint16_t *left, int bd) {
  highbd_filter_intra_predictor(dst, stride, tx_size, above, left,
                                FILTER_PAETH_PRED, bd);
}

static void highbd_filter_intra_predictors(FILTER_INTRA_MODE mode,
                                           uint16_t *dst, ptrdiff_t stride,
                                           TX_SIZE tx_size,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  switch (mode) {
    case FILTER_DC_PRED:
      av1_highbd_dc_filter_predictor(dst, stride, tx_size, above, left, bd);
      break;
    case FILTER_V_PRED:
      av1_highbd_v_filter_predictor(dst, stride, tx_size, above, left, bd);
      break;
    case FILTER_H_PRED:
      av1_highbd_h_filter_predictor(dst, stride, tx_size, above, left, bd);
      break;
    case FILTER_D153_PRED:
      av1_highbd_d153_filter_predictor(dst, stride, tx_size, above, left, bd);
      break;
    case FILTER_PAETH_PRED:
      av1_highbd_paeth_filter_predictor(dst, stride, tx_size, above, left, bd);
      break;
    default: assert(0);
  }
}
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_INTRA_EDGE
static int is_smooth(const MB_MODE_INFO *mbmi, int plane) {
  if (plane == 0) {
    const PREDICTION_MODE mode = mbmi->mode;
    return (mode == SMOOTH_PRED || mode == SMOOTH_V_PRED ||
            mode == SMOOTH_H_PRED);
  } else {
    // uv_mode is not set for inter blocks, so need to explicitly
    // detect that case.
    if (is_inter_block(mbmi)) return 0;

    const UV_PREDICTION_MODE uv_mode = mbmi->uv_mode;
    return (uv_mode == UV_SMOOTH_PRED || uv_mode == UV_SMOOTH_V_PRED ||
            uv_mode == UV_SMOOTH_H_PRED);
  }
}

static int get_filt_type(const MACROBLOCKD *xd, int plane) {
  const MB_MODE_INFO *ab = xd->up_available ? &xd->mi[-xd->mi_stride]->mbmi : 0;
  const MB_MODE_INFO *le = xd->left_available ? &xd->mi[-1]->mbmi : 0;

  const int ab_sm = ab ? is_smooth(ab, plane) : 0;
  const int le_sm = le ? is_smooth(le, plane) : 0;

  return (ab_sm || le_sm) ? 1 : 0;
}

static int intra_edge_filter_strength(int bs0, int bs1, int delta, int type) {
  const int d = abs(delta);
  int strength = 0;

#if CONFIG_EXT_INTRA_MOD
  const int blk_wh = bs0 + bs1;
  if (type == 0) {
    if (blk_wh <= 8) {
      if (d >= 56) strength = 1;
    } else if (blk_wh <= 12) {
      if (d >= 40) strength = 1;
    } else if (blk_wh <= 16) {
      if (d >= 40) strength = 1;
    } else if (blk_wh <= 24) {
      if (d >= 8) strength = 1;
      if (d >= 16) strength = 2;
      if (d >= 32) strength = 3;
    } else if (blk_wh <= 32) {
      if (d >= 1) strength = 1;
      if (d >= 4) strength = 2;
      if (d >= 32) strength = 3;
    } else {
      if (d >= 1) strength = 3;
    }
  } else {
    if (blk_wh <= 8) {
      if (d >= 40) strength = 1;
      if (d >= 64) strength = 2;
    } else if (blk_wh <= 16) {
      if (d >= 20) strength = 1;
      if (d >= 48) strength = 2;
    } else if (blk_wh <= 24) {
      if (d >= 4) strength = 3;
    } else {
      if (d >= 1) strength = 3;
    }
  }
#else
  (void)type;
  (void)bs1;
  switch (bs0) {
    case 4:
      if (d < 56) {
        strength = 0;
      } else if (d < 90) {
        strength = 1;
      }
      break;
    case 8:
      if (d < 8) {
        strength = 0;
      } else if (d < 32) {
        strength = 1;
      } else if (d < 90) {
        strength = 3;
      }
      break;
    case 16:
      if (d < 4) {
        strength = 0;
      } else if (d < 16) {
        strength = 1;
      } else if (d < 90) {
        strength = 3;
      }
      break;
    case 32:
      if (d < 16) {
        strength = 2;
      } else if (d < 90) {
        strength = 3;
      }
      break;
    default: strength = 0; break;
  }
#endif  // CONFIG_EXT_INTRA_MOD
  return strength;
}

void av1_filter_intra_edge_c(uint8_t *p, int sz, int strength) {
  if (!strength) return;

  const int kernel[INTRA_EDGE_FILT][INTRA_EDGE_TAPS] = {
    { 0, 4, 8, 4, 0 }, { 0, 5, 6, 5, 0 }, { 2, 4, 4, 4, 2 }
  };
  const int filt = strength - 1;
  uint8_t edge[129];

  memcpy(edge, p, sz * sizeof(*p));
  for (int i = 1; i < sz; i++) {
    int s = 0;
    for (int j = 0; j < INTRA_EDGE_TAPS; j++) {
      int k = i - 2 + j;
      k = (k < 0) ? 0 : k;
      k = (k > sz - 1) ? sz - 1 : k;
      s += edge[k] * kernel[filt][j];
    }
    s = (s + 8) >> 4;
    p[i] = s;
  }
}

#if CONFIG_EXT_INTRA_MOD
static void av1_filter_intra_edge_corner(uint8_t *p_above, uint8_t *p_left) {
  const int kernel[3] = { 5, 6, 5 };

  int s = (p_left[0] * kernel[0]) + (p_above[-1] * kernel[1]) +
          (p_above[0] * kernel[2]);
  s = (s + 8) >> 4;
  p_above[-1] = s;
  p_left[-1] = s;
}
#endif  // CONFIG_EXT_INTRA_MOD

void av1_filter_intra_edge_high_c(uint16_t *p, int sz, int strength) {
  if (!strength) return;

  const int kernel[INTRA_EDGE_FILT][INTRA_EDGE_TAPS] = {
    { 0, 4, 8, 4, 0 }, { 0, 5, 6, 5, 0 }, { 2, 4, 4, 4, 2 }
  };
  const int filt = strength - 1;
  uint16_t edge[129];

  memcpy(edge, p, sz * sizeof(*p));
  for (int i = 1; i < sz; i++) {
    int s = 0;
    for (int j = 0; j < INTRA_EDGE_TAPS; j++) {
      int k = i - 2 + j;
      k = (k < 0) ? 0 : k;
      k = (k > sz - 1) ? sz - 1 : k;
      s += edge[k] * kernel[filt][j];
    }
    s = (s + 8) >> 4;
    p[i] = s;
  }
}

#if CONFIG_EXT_INTRA_MOD
static void av1_filter_intra_edge_corner_high(uint16_t *p_above,
                                              uint16_t *p_left) {
  const int kernel[3] = { 5, 6, 5 };

  int s = (p_left[0] * kernel[0]) + (p_above[-1] * kernel[1]) +
          (p_above[0] * kernel[2]);
  s = (s + 8) >> 4;
  p_above[-1] = s;
  p_left[-1] = s;
}
#endif  // CONFIG_EXT_INTRA_MOD

static int use_intra_edge_upsample(int bs0, int bs1, int delta, int type) {
  const int d = abs(delta);

#if CONFIG_EXT_INTRA_MOD
  const int blk_wh = bs0 + bs1;
  if (d <= 0 || d >= 40) return 0;
  return type ? (blk_wh <= 8) : (blk_wh <= 16);
#else
  (void)type;
  (void)bs1;
  return (bs0 == 4 && d > 0 && d < 56);
#endif  // CONFIG_EXT_INTRA_MOD
}

void av1_upsample_intra_edge_c(uint8_t *p, int sz) {
  // interpolate half-sample positions
  assert(sz <= MAX_UPSAMPLE_SZ);

  uint8_t in[MAX_UPSAMPLE_SZ + 3];
  // copy p[-1..(sz-1)] and extend first and last samples
  in[0] = p[-1];
  in[1] = p[-1];
  for (int i = 0; i < sz; i++) {
    in[i + 2] = p[i];
  }
  in[sz + 2] = p[sz - 1];

  // interpolate half-sample edge positions
  p[-2] = in[0];
  for (int i = 0; i < sz; i++) {
    int s = -in[i] + (9 * in[i + 1]) + (9 * in[i + 2]) - in[i + 3];
    s = clip_pixel((s + 8) >> 4);
    p[2 * i - 1] = s;
    p[2 * i] = in[i + 2];
  }
}

void av1_upsample_intra_edge_high_c(uint16_t *p, int sz, int bd) {
  // interpolate half-sample positions
  assert(sz <= MAX_UPSAMPLE_SZ);

  uint16_t in[MAX_UPSAMPLE_SZ + 3];
  // copy p[-1..(sz-1)] and extend first and last samples
  in[0] = p[-1];
  in[1] = p[-1];
  for (int i = 0; i < sz; i++) {
    in[i + 2] = p[i];
  }
  in[sz + 2] = p[sz - 1];

  // interpolate half-sample edge positions
  p[-2] = in[0];
  for (int i = 0; i < sz; i++) {
    int s = -in[i] + (9 * in[i + 1]) + (9 * in[i + 2]) - in[i + 3];
    s = (s + 8) >> 4;
    s = clip_pixel_highbd(s, bd);
    p[2 * i - 1] = s;
    p[2 * i] = in[i + 2];
  }
}
#endif  // CONFIG_INTRA_EDGE

static void build_intra_predictors_high(
    const MACROBLOCKD *xd, const uint8_t *ref8, int ref_stride, uint8_t *dst8,
    int dst_stride, PREDICTION_MODE mode, TX_SIZE tx_size, int n_top_px,
    int n_topright_px, int n_left_px, int n_bottomleft_px, int plane) {
  int i;
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  DECLARE_ALIGNED(16, uint16_t, left_data[MAX_TX_SIZE * 2 + 32]);
  DECLARE_ALIGNED(16, uint16_t, above_data[MAX_TX_SIZE * 2 + 32]);
  uint16_t *const above_row = above_data + 16;
  uint16_t *const left_col = left_data + 16;
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
  int need_left = extend_modes[mode] & NEED_LEFT;
  int need_above = extend_modes[mode] & NEED_ABOVE;
  int need_above_left = extend_modes[mode] & NEED_ABOVELEFT;
  const uint16_t *above_ref = ref - ref_stride;
  const uint16_t *left_ref = ref - 1;
  int p_angle = 0;
  const int is_dr_mode = av1_is_directional_mode(mode, xd->mi[0]->mbmi.sb_type);
#if CONFIG_FILTER_INTRA
  const int use_filter_intra =
      plane > 0 ? 0 : xd->mi[0]->mbmi.filter_intra_mode_info.use_filter_intra;
  const FILTER_INTRA_MODE filter_intra_mode =
      xd->mi[0]->mbmi.filter_intra_mode_info.filter_intra_mode;
#endif  // CONFIG_FILTER_INTRA
  int base = 128 << (xd->bd - 8);

  // The default values if ref pixels are not available:
  // base-1 base-1 base-1 .. base-1 base-1 base-1 base-1 base-1 base-1
  // base+1   A      B  ..     Y      Z
  // base+1   C      D  ..     W      X
  // base+1   E      F  ..     U      V
  // base+1   G      H  ..     S      T      T      T      T      T

  if (is_dr_mode) {
    p_angle = mode_to_angle_map[mode] +
              xd->mi[0]->mbmi.angle_delta[plane != 0] * ANGLE_STEP;
    if (p_angle <= 90)
      need_above = 1, need_left = 0, need_above_left = 1;
    else if (p_angle < 180)
      need_above = 1, need_left = 1, need_above_left = 1;
    else
      need_above = 0, need_left = 1, need_above_left = 1;
  }
#if CONFIG_FILTER_INTRA
  if (use_filter_intra) need_left = need_above = need_above_left = 1;
#endif  // CONFIG_FILTER_INTRA

  (void)plane;
  assert(n_top_px >= 0);
  assert(n_topright_px >= 0);
  assert(n_left_px >= 0);
  assert(n_bottomleft_px >= 0);

  if ((!need_above && n_left_px == 0) || (!need_left && n_top_px == 0)) {
#if CONFIG_INTRA_EDGE
    int val;
    if (need_left) {
      val = (n_top_px > 0) ? above_ref[0] : base + 1;
    } else {
      val = (n_left_px > 0) ? left_ref[0] : base - 1;
    }
#else
    const int val = need_left ? base + 1 : base - 1;
#endif  // CONFIG_INTRA_EDGE
    for (i = 0; i < txhpx; ++i) {
      aom_memset16(dst, val, txwpx);
      dst += dst_stride;
    }
    return;
  }

  // NEED_LEFT
  if (need_left) {
    int need_bottom = !!(extend_modes[mode] & NEED_BOTTOMLEFT);
#if CONFIG_FILTER_INTRA
    if (use_filter_intra) need_bottom = 0;
#endif  // CONFIG_FILTER_INTRA
    if (is_dr_mode) need_bottom = p_angle > 180;
    const int num_left_pixels_needed = txhpx + (need_bottom ? txwpx : 0);
    i = 0;
    if (n_left_px > 0) {
      for (; i < n_left_px; i++) left_col[i] = left_ref[i * ref_stride];
      if (need_bottom && n_bottomleft_px > 0) {
        assert(i == txhpx);
        for (; i < txhpx + n_bottomleft_px; i++)
          left_col[i] = left_ref[i * ref_stride];
      }
      if (i < num_left_pixels_needed)
        aom_memset16(&left_col[i], left_col[i - 1], num_left_pixels_needed - i);
    } else {
#if CONFIG_INTRA_EDGE
      if (n_top_px > 0) {
        aom_memset16(left_col, above_ref[0], num_left_pixels_needed);
      } else {
#endif  // CONFIG_INTRA_EDGE
        aom_memset16(left_col, base + 1, num_left_pixels_needed);
#if CONFIG_INTRA_EDGE
      }
#endif  // CONFIG_INTRA_EDGE
    }
  }

  // NEED_ABOVE
  if (need_above) {
    int need_right = !!(extend_modes[mode] & NEED_ABOVERIGHT);
#if CONFIG_FILTER_INTRA
    if (use_filter_intra) need_right = 0;
#endif  // CONFIG_FILTER_INTRA
    if (is_dr_mode) need_right = p_angle < 90;
    const int num_top_pixels_needed = txwpx + (need_right ? txhpx : 0);
    if (n_top_px > 0) {
      memcpy(above_row, above_ref, n_top_px * sizeof(above_ref[0]));
      i = n_top_px;
      if (need_right && n_topright_px > 0) {
        assert(n_top_px == txwpx);
        memcpy(above_row + txwpx, above_ref + txwpx,
               n_topright_px * sizeof(above_ref[0]));
        i += n_topright_px;
      }
      if (i < num_top_pixels_needed)
        aom_memset16(&above_row[i], above_row[i - 1],
                     num_top_pixels_needed - i);
    } else {
#if CONFIG_INTRA_EDGE
      if (n_left_px > 0) {
        aom_memset16(above_row, left_ref[0], num_top_pixels_needed);
      } else {
#endif  // CONFIG_INTRA_EDGE
        aom_memset16(above_row, base - 1, num_top_pixels_needed);
#if CONFIG_INTRA_EDGE
      }
#endif  // CONFIG_INTRA_EDGE
    }
  }

  if (need_above_left) {
#if CONFIG_INTRA_EDGE
    if (n_top_px > 0 && n_left_px > 0) {
      above_row[-1] = above_ref[-1];
    } else if (n_top_px > 0) {
      above_row[-1] = above_ref[0];
    } else if (n_left_px > 0) {
      above_row[-1] = left_ref[0];
    } else {
      above_row[-1] = base;
    }
#else
    above_row[-1] =
        n_top_px > 0 ? (n_left_px > 0 ? above_ref[-1] : base + 1) : base - 1;
#endif  // CONFIG_INTRA_EDGE
    left_col[-1] = above_row[-1];
  }

#if CONFIG_FILTER_INTRA
  if (use_filter_intra) {
    highbd_filter_intra_predictors(filter_intra_mode, dst, dst_stride, tx_size,
                                   above_row, left_col, xd->bd);
    return;
  }
#endif  // CONFIG_FILTER_INTRA

  if (is_dr_mode) {
#if CONFIG_INTRA_EDGE
    const int need_right = p_angle < 90;
    const int need_bottom = p_angle > 180;
    const int filt_type = get_filt_type(xd, plane);
    if (p_angle != 90 && p_angle != 180) {
      const int ab_le = need_above_left ? 1 : 0;
#if CONFIG_EXT_INTRA_MOD
      if (need_above && need_left && (txwpx + txhpx >= 24)) {
        av1_filter_intra_edge_corner_high(above_row, left_col);
      }
#endif  // CONFIG_EXT_INTRA_MOD
      if (need_above && n_top_px > 0) {
        const int strength =
            intra_edge_filter_strength(txwpx, txhpx, p_angle - 90, filt_type);
        const int n_px = n_top_px + ab_le + (need_right ? txhpx : 0);
        av1_filter_intra_edge_high(above_row - ab_le, n_px, strength);
      }
      if (need_left && n_left_px > 0) {
        const int strength =
            intra_edge_filter_strength(txhpx, txwpx, p_angle - 180, filt_type);
        const int n_px = n_left_px + ab_le + (need_bottom ? txwpx : 0);
        av1_filter_intra_edge_high(left_col - ab_le, n_px, strength);
      }
    }
    const int upsample_above =
        use_intra_edge_upsample(txwpx, txhpx, p_angle - 90, filt_type);
    if (need_above && upsample_above) {
      const int n_px = txwpx + (need_right ? txhpx : 0);
      av1_upsample_intra_edge_high(above_row, n_px, xd->bd);
    }
    const int upsample_left =
        use_intra_edge_upsample(txhpx, txwpx, p_angle - 180, filt_type);
    if (need_left && upsample_left) {
      const int n_px = txhpx + (need_bottom ? txwpx : 0);
      av1_upsample_intra_edge_high(left_col, n_px, xd->bd);
    }
#endif  // CONFIG_INTRA_EDGE
    highbd_dr_predictor(dst, dst_stride, tx_size, above_row, left_col,
#if CONFIG_INTRA_EDGE
                        upsample_above, upsample_left,
#endif  // CONFIG_INTRA_EDGE
                        p_angle, xd->bd);
    return;
  }

  // predict
  if (mode == DC_PRED) {
    dc_pred_high[n_left_px > 0][n_top_px > 0][tx_size](
        dst, dst_stride, above_row, left_col, xd->bd);
  } else {
    pred_high[mode][tx_size](dst, dst_stride, above_row, left_col, xd->bd);
  }
}

static void build_intra_predictors(const MACROBLOCKD *xd, const uint8_t *ref,
                                   int ref_stride, uint8_t *dst, int dst_stride,
                                   PREDICTION_MODE mode, TX_SIZE tx_size,
                                   int n_top_px, int n_topright_px,
                                   int n_left_px, int n_bottomleft_px,
                                   int plane) {
  int i;
  const uint8_t *above_ref = ref - ref_stride;
  const uint8_t *left_ref = ref - 1;
  DECLARE_ALIGNED(16, uint8_t, left_data[MAX_TX_SIZE * 2 + 32]);
  DECLARE_ALIGNED(16, uint8_t, above_data[MAX_TX_SIZE * 2 + 32]);
  uint8_t *const above_row = above_data + 16;
  uint8_t *const left_col = left_data + 16;
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
  int need_left = extend_modes[mode] & NEED_LEFT;
  int need_above = extend_modes[mode] & NEED_ABOVE;
  int need_above_left = extend_modes[mode] & NEED_ABOVELEFT;
  int p_angle = 0;
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int is_dr_mode = av1_is_directional_mode(mode, mbmi->sb_type);
#if CONFIG_FILTER_INTRA
  const int use_filter_intra =
      plane > 0 ? 0 : xd->mi[0]->mbmi.filter_intra_mode_info.use_filter_intra;
  const FILTER_INTRA_MODE filter_intra_mode =
      xd->mi[0]->mbmi.filter_intra_mode_info.filter_intra_mode;
#endif  // CONFIG_FILTER_INTRA

  // The default values if ref pixels are not available:
  // 127 127 127 .. 127 127 127 127 127 127
  // 129  A   B  ..  Y   Z
  // 129  C   D  ..  W   X
  // 129  E   F  ..  U   V
  // 129  G   H  ..  S   T   T   T   T   T
  // ..

  if (is_dr_mode) {
    p_angle = mode_to_angle_map[mode] +
              xd->mi[0]->mbmi.angle_delta[plane != 0] * ANGLE_STEP;
    if (p_angle <= 90)
      need_above = 1, need_left = 0, need_above_left = 1;
    else if (p_angle < 180)
      need_above = 1, need_left = 1, need_above_left = 1;
    else
      need_above = 0, need_left = 1, need_above_left = 1;
  }
#if CONFIG_FILTER_INTRA
  if (use_filter_intra) need_left = need_above = need_above_left = 1;
#endif  // CONFIG_FILTER_INTRA

  (void)xd;
  (void)plane;
  assert(n_top_px >= 0);
  assert(n_topright_px >= 0);
  assert(n_left_px >= 0);
  assert(n_bottomleft_px >= 0);

  if ((!need_above && n_left_px == 0) || (!need_left && n_top_px == 0)) {
#if CONFIG_INTRA_EDGE
    int val;
    if (need_left) {
      val = (n_top_px > 0) ? above_ref[0] : 129;
    } else {
      val = (n_left_px > 0) ? left_ref[0] : 127;
    }
#else
    const int val = need_left ? 129 : 127;
#endif  // CONFIG_INTRA_EDGE
    for (i = 0; i < txhpx; ++i) {
      memset(dst, val, txwpx);
      dst += dst_stride;
    }
    return;
  }

  // NEED_LEFT
  if (need_left) {
    int need_bottom = !!(extend_modes[mode] & NEED_BOTTOMLEFT);
#if CONFIG_FILTER_INTRA
    if (use_filter_intra) need_bottom = 0;
#endif  // CONFIG_FILTER_INTRA
    if (is_dr_mode) need_bottom = p_angle > 180;
    const int num_left_pixels_needed = txhpx + (need_bottom ? txwpx : 0);
    i = 0;
    if (n_left_px > 0) {
      for (; i < n_left_px; i++) left_col[i] = left_ref[i * ref_stride];
      if (need_bottom && n_bottomleft_px > 0) {
        assert(i == txhpx);
        for (; i < txhpx + n_bottomleft_px; i++)
          left_col[i] = left_ref[i * ref_stride];
      }
      if (i < num_left_pixels_needed)
        memset(&left_col[i], left_col[i - 1], num_left_pixels_needed - i);
    } else {
#if CONFIG_INTRA_EDGE
      if (n_top_px > 0) {
        memset(left_col, above_ref[0], num_left_pixels_needed);
      } else {
#endif  // CONFIG_INTRA_EDGE
        memset(left_col, 129, num_left_pixels_needed);
#if CONFIG_INTRA_EDGE
      }
#endif  // CONFIG_INTRA_EDGE
    }
  }

  // NEED_ABOVE
  if (need_above) {
    int need_right = !!(extend_modes[mode] & NEED_ABOVERIGHT);
#if CONFIG_FILTER_INTRA
    if (use_filter_intra) need_right = 0;
#endif  // CONFIG_FILTER_INTRA
    if (is_dr_mode) need_right = p_angle < 90;
    const int num_top_pixels_needed = txwpx + (need_right ? txhpx : 0);
    if (n_top_px > 0) {
      memcpy(above_row, above_ref, n_top_px);
      i = n_top_px;
      if (need_right && n_topright_px > 0) {
        assert(n_top_px == txwpx);
        memcpy(above_row + txwpx, above_ref + txwpx, n_topright_px);
        i += n_topright_px;
      }
      if (i < num_top_pixels_needed)
        memset(&above_row[i], above_row[i - 1], num_top_pixels_needed - i);
    } else {
#if CONFIG_INTRA_EDGE
      if (n_left_px > 0) {
        memset(above_row, left_ref[0], num_top_pixels_needed);
      } else {
#endif  // CONFIG_INTRA_EDGE
        memset(above_row, 127, num_top_pixels_needed);
#if CONFIG_INTRA_EDGE
      }
#endif  // CONFIG_INTRA_EDGE
    }
  }

  if (need_above_left) {
#if CONFIG_INTRA_EDGE
    if (n_top_px > 0 && n_left_px > 0) {
      above_row[-1] = above_ref[-1];
    } else if (n_top_px > 0) {
      above_row[-1] = above_ref[0];
    } else if (n_left_px > 0) {
      above_row[-1] = left_ref[0];
    } else {
      above_row[-1] = 128;
    }
#else
    above_row[-1] = n_top_px > 0 ? (n_left_px > 0 ? above_ref[-1] : 129) : 127;
#endif  // CONFIG_INTRA_EDGE
    left_col[-1] = above_row[-1];
  }

#if CONFIG_FILTER_INTRA
  if (use_filter_intra) {
    filter_intra_predictors(filter_intra_mode, dst, dst_stride, tx_size,
                            above_row, left_col);
    return;
  }
#endif  // CONFIG_FILTER_INTRA

  if (is_dr_mode) {
#if CONFIG_INTRA_EDGE
    const int need_right = p_angle < 90;
    const int need_bottom = p_angle > 180;
    const int filt_type = get_filt_type(xd, plane);
    if (p_angle != 90 && p_angle != 180) {
      const int ab_le = need_above_left ? 1 : 0;
#if CONFIG_EXT_INTRA_MOD
      if (need_above && need_left && (txwpx + txhpx >= 24)) {
        av1_filter_intra_edge_corner(above_row, left_col);
      }
#endif  // CONFIG_EXT_INTRA_MOD
      if (need_above && n_top_px > 0) {
        const int strength =
            intra_edge_filter_strength(txwpx, txhpx, p_angle - 90, filt_type);
        const int n_px = n_top_px + ab_le + (need_right ? txhpx : 0);
        av1_filter_intra_edge(above_row - ab_le, n_px, strength);
      }
      if (need_left && n_left_px > 0) {
        const int strength =
            intra_edge_filter_strength(txhpx, txwpx, p_angle - 180, filt_type);
        const int n_px = n_left_px + ab_le + (need_bottom ? txwpx : 0);
        av1_filter_intra_edge(left_col - ab_le, n_px, strength);
      }
    }
    const int upsample_above =
        use_intra_edge_upsample(txwpx, txhpx, p_angle - 90, filt_type);
    if (need_above && upsample_above) {
      const int n_px = txwpx + (need_right ? txhpx : 0);
      av1_upsample_intra_edge(above_row, n_px);
    }
    const int upsample_left =
        use_intra_edge_upsample(txhpx, txwpx, p_angle - 180, filt_type);
    if (need_left && upsample_left) {
      const int n_px = txhpx + (need_bottom ? txwpx : 0);
      av1_upsample_intra_edge(left_col, n_px);
    }
#endif  // CONFIG_INTRA_EDGE
    dr_predictor(dst, dst_stride, tx_size, above_row, left_col,
#if CONFIG_INTRA_EDGE
                 upsample_above, upsample_left,
#endif  // CONFIG_INTRA_EDGE
                 p_angle);
    return;
  }

  // predict
  if (mode == DC_PRED) {
    dc_pred[n_left_px > 0][n_top_px > 0][tx_size](dst, dst_stride, above_row,
                                                  left_col);
  } else {
    pred[mode][tx_size](dst, dst_stride, above_row, left_col);
  }
}

void av1_predict_intra_block(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                             int wpx, int hpx, TX_SIZE tx_size,
                             PREDICTION_MODE mode, const uint8_t *ref,
                             int ref_stride, uint8_t *dst, int dst_stride,
                             int col_off, int row_off, int plane) {
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  BLOCK_SIZE bsize = mbmi->sb_type;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const int txw = tx_size_wide_unit[tx_size];
  const int txh = tx_size_high_unit[tx_size];
  const int have_top = row_off || (pd->subsampling_y ? xd->chroma_up_available
                                                     : xd->up_available);
  const int have_left =
      col_off ||
      (pd->subsampling_x ? xd->chroma_left_available : xd->left_available);
  const int x = col_off << tx_size_wide_log2[0];
  const int y = row_off << tx_size_high_log2[0];
  const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
  const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
  const int xr_chr_offset = 0;
  const int yd_chr_offset = 0;

  // Distance between the right edge of this prediction block to
  // the frame right edge
  const int xr = (xd->mb_to_right_edge >> (3 + pd->subsampling_x)) +
                 (wpx - x - txwpx) - xr_chr_offset;
  // Distance between the bottom edge of this prediction block to
  // the frame bottom edge
  const int yd = (xd->mb_to_bottom_edge >> (3 + pd->subsampling_y)) +
                 (hpx - y - txhpx) - yd_chr_offset;
  const int right_available = mi_col + ((col_off + txw) << pd->subsampling_x >>
                                        (MI_SIZE_LOG2 - tx_size_wide_log2[0])) <
                              xd->tile.mi_col_end;
  const int bottom_available =
      (yd > 0) && (mi_row + (((row_off + txh) << pd->subsampling_y) >>
                             (MI_SIZE_LOG2 - tx_size_high_log2[0])) <
                   xd->tile.mi_row_end);

#if CONFIG_EXT_PARTITION_TYPES
  const PARTITION_TYPE partition = mbmi->partition;
#else
  const PARTITION_TYPE partition = PARTITION_NONE;
#endif

  // force 4x4 chroma component block size.
  bsize = scale_chroma_bsize(bsize, pd->subsampling_x, pd->subsampling_y);

  const int have_top_right =
      has_top_right(cm, bsize, mi_row, mi_col, have_top, right_available,
                    partition, tx_size, row_off, col_off, pd->subsampling_x);
  const int have_bottom_left =
      has_bottom_left(cm, bsize, mi_row, mi_col, bottom_available, have_left,
                      partition, tx_size, row_off, col_off, pd->subsampling_y);

  if (mbmi->palette_mode_info.palette_size[plane != 0] > 0) {
    const int stride = wpx;
    int r, c;
    const uint8_t *const map = xd->plane[plane != 0].color_index_map;
    const uint16_t *const palette =
        mbmi->palette_mode_info.palette_colors + plane * PALETTE_MAX_SIZE;

    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      uint16_t *dst16 = CONVERT_TO_SHORTPTR(dst);
      for (r = 0; r < txhpx; ++r) {
        for (c = 0; c < txwpx; ++c) {
          dst16[r * dst_stride + c] = palette[map[(r + y) * stride + c + x]];
        }
      }
    } else {
      for (r = 0; r < txhpx; ++r) {
        for (c = 0; c < txwpx; ++c) {
          dst[r * dst_stride + c] =
              (uint8_t)palette[map[(r + y) * stride + c + x]];
        }
      }
    }
    return;
  }

  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    build_intra_predictors_high(
        xd, ref, ref_stride, dst, dst_stride, mode, tx_size,
        have_top ? AOMMIN(txwpx, xr + txwpx) : 0,
        have_top_right ? AOMMIN(txwpx, xr) : 0,
        have_left ? AOMMIN(txhpx, yd + txhpx) : 0,
        have_bottom_left ? AOMMIN(txhpx, yd) : 0, plane);
    return;
  }

  build_intra_predictors(xd, ref, ref_stride, dst, dst_stride, mode, tx_size,
                         have_top ? AOMMIN(txwpx, xr + txwpx) : 0,
                         have_top_right ? AOMMIN(txwpx, xr) : 0,
                         have_left ? AOMMIN(txhpx, yd + txhpx) : 0,
                         have_bottom_left ? AOMMIN(txhpx, yd) : 0, plane);
}

void av1_predict_intra_block_facade(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                    int plane, int blk_col, int blk_row,
                                    TX_SIZE tx_size) {
  const MODE_INFO *mi = xd->mi[0];
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const int dst_stride = pd->dst.stride;
  uint8_t *dst =
      &pd->dst.buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];
  const PREDICTION_MODE mode =
      (plane == AOM_PLANE_Y) ? mbmi->mode : get_uv_mode(mbmi->uv_mode);

#if CONFIG_CFL
  if (plane != AOM_PLANE_Y && mbmi->uv_mode == UV_CFL_PRED) {
#if CONFIG_DEBUG
    assert(is_cfl_allowed(mbmi));
    const BLOCK_SIZE plane_bsize = get_plane_block_size(mbmi->sb_type, pd);
    (void)plane_bsize;
    assert(plane_bsize < BLOCK_SIZES_ALL);
    if (!xd->lossless[mbmi->segment_id]) {
      assert(blk_col == 0);
      assert(blk_row == 0);
      assert(block_size_wide[plane_bsize] == tx_size_wide[tx_size]);
      assert(block_size_high[plane_bsize] == tx_size_high[tx_size]);
    }
#endif
    CFL_CTX *const cfl = &xd->cfl;
    CFL_PRED_TYPE pred_plane = get_cfl_pred_type(plane);
    if (cfl->dc_pred_is_cached[pred_plane] == 0) {
      av1_predict_intra_block(cm, xd, pd->width, pd->height, tx_size, mode, dst,
                              dst_stride, dst, dst_stride, blk_col, blk_row,
                              plane);
      if (cfl->use_dc_pred_cache) {
        cfl_store_dc_pred(xd, dst, pred_plane, tx_size_wide[tx_size]);
        cfl->dc_pred_is_cached[pred_plane] = 1;
      }
    } else {
      cfl_load_dc_pred(xd, dst, dst_stride, tx_size, pred_plane);
    }
    cfl_predict_block(xd, dst, dst_stride, tx_size, plane);
    return;
  }
#endif
  av1_predict_intra_block(cm, xd, pd->width, pd->height, tx_size, mode, dst,
                          dst_stride, dst, dst_stride, blk_col, blk_row, plane);
}

void av1_init_intra_predictors(void) {
  once(av1_init_intra_predictors_internal);
}
