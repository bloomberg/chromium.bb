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

#ifndef AV1_COMMON_COMMON_DATA_H_
#define AV1_COMMON_COMMON_DATA_H_

#include "av1/common/enums.h"
#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_EXT_PARTITION
#define IF_EXT_PARTITION(...) __VA_ARGS__,
#else
#define IF_EXT_PARTITION(...)
#endif

// Log 2 conversion lookup tables for block width and height
static const uint8_t b_width_log2_lookup[BLOCK_SIZES_ALL] = {
  0,
  0,
  0,
  0,
  0,
  1,
  1,
  1,
  2,
  2,
  2,
  3,
  3,
  3,
  4,
  4,
  IF_EXT_PARTITION(4, 5, 5) 0,
  2,
  1,
  3,
  2,
  4,
  IF_EXT_PARTITION(3, 5)
};
static const uint8_t b_height_log2_lookup[BLOCK_SIZES_ALL] = {
  0,
  0,
  0,
  0,
  1,
  0,
  1,
  2,
  1,
  2,
  3,
  2,
  3,
  4,
  3,
  4,
  IF_EXT_PARTITION(5, 4, 5) 2,
  0,
  3,
  1,
  4,
  2,
  IF_EXT_PARTITION(5, 3)
};
// Log 2 conversion lookup tables for modeinfo width and height
static const uint8_t mi_width_log2_lookup[BLOCK_SIZES_ALL] = {
  0,
  0,
  0,
  0,
  0,
  1,
  1,
  1,
  2,
  2,
  2,
  3,
  3,
  3,
  4,
  4,
  IF_EXT_PARTITION(4, 5, 5) 0,
  2,
  1,
  3,
  2,
  4,
  IF_EXT_PARTITION(3, 5)
};
static const uint8_t mi_height_log2_lookup[BLOCK_SIZES_ALL] = {
  0,
  0,
  0,
  0,
  1,
  0,
  1,
  2,
  1,
  2,
  3,
  2,
  3,
  4,
  3,
  4,
  IF_EXT_PARTITION(5, 4, 5) 2,
  0,
  3,
  1,
  4,
  2,
  IF_EXT_PARTITION(5, 3)
};

/* clang-format off */
static const uint8_t mi_size_wide[BLOCK_SIZES_ALL] = {
  1, 1, 1, 1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 8, 16, 16,
  IF_EXT_PARTITION(16, 32, 32)  1, 4, 2, 8, 4, 16, IF_EXT_PARTITION(8, 32)
};
static const uint8_t mi_size_high[BLOCK_SIZES_ALL] = {
  1, 1, 1, 1, 2, 1, 2, 4, 2, 4, 8, 4, 8, 16, 8, 16,
  IF_EXT_PARTITION(32, 16, 32)  4, 1, 8, 2, 16, 4, IF_EXT_PARTITION(32, 8)
};
/* clang-format on */

// Width/height lookup tables in units of various block sizes
static const uint8_t block_size_wide[BLOCK_SIZES_ALL] = {
  2,
  2,
  4,
  4,
  4,
  8,
  8,
  8,
  16,
  16,
  16,
  32,
  32,
  32,
  64,
  64,
  IF_EXT_PARTITION(64, 128, 128) 4,
  16,
  8,
  32,
  16,
  64,
  IF_EXT_PARTITION(32, 128)
};

static const uint8_t block_size_high[BLOCK_SIZES_ALL] = {
  2,
  4,
  2,
  4,
  8,
  4,
  8,
  16,
  8,
  16,
  32,
  16,
  32,
  64,
  32,
  64,
  IF_EXT_PARTITION(128, 64, 128) 16,
  4,
  32,
  8,
  64,
  16,
  IF_EXT_PARTITION(128, 32)
};

static const uint8_t num_4x4_blocks_wide_lookup[BLOCK_SIZES_ALL] = {
  1,
  1,
  1,
  1,
  1,
  2,
  2,
  2,
  4,
  4,
  4,
  8,
  8,
  8,
  16,
  16,
  IF_EXT_PARTITION(16, 32, 32) 1,
  4,
  2,
  8,
  4,
  16,
  IF_EXT_PARTITION(8, 32)
};
static const uint8_t num_4x4_blocks_high_lookup[BLOCK_SIZES_ALL] = {
  1,
  1,
  1,
  1,
  2,
  1,
  2,
  4,
  2,
  4,
  8,
  4,
  8,
  16,
  8,
  16,
  IF_EXT_PARTITION(32, 16, 32) 4,
  1,
  8,
  2,
  16,
  4,
  IF_EXT_PARTITION(32, 8)
};
static const uint8_t num_8x8_blocks_wide_lookup[BLOCK_SIZES_ALL] = {
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  2,
  2,
  2,
  4,
  4,
  4,
  8,
  8,
  IF_EXT_PARTITION(8, 16, 16) 1,
  2,
  1,
  4,
  2,
  8,
  IF_EXT_PARTITION(4, 16)
};
static const uint8_t num_8x8_blocks_high_lookup[BLOCK_SIZES_ALL] = {
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  2,
  1,
  2,
  4,
  2,
  4,
  8,
  4,
  8,
  IF_EXT_PARTITION(16, 8, 16) 2,
  1,
  4,
  1,
  8,
  2,
  IF_EXT_PARTITION(16, 4)
};

// AOMMIN(3, AOMMIN(b_width_log2(bsize), b_height_log2(bsize)))
static const uint8_t size_group_lookup[BLOCK_SIZES_ALL] = {
  0,
  0,
  0,
  0,
  0,
  0,
  1,
  1,
  1,
  2,
  2,
  2,
  3,
  3,
  3,
  3,
  IF_EXT_PARTITION(3, 3, 3) 0,
  0,
  1,
  1,
  2,
  2,
  IF_EXT_PARTITION(3, 3)
};

static const uint8_t num_pels_log2_lookup[BLOCK_SIZES_ALL] = {
  2,
  3,
  3,
  4,
  5,
  5,
  6,
  7,
  7,
  8,
  9,
  9,
  10,
  11,
  11,
  12,
  IF_EXT_PARTITION(13, 13, 14) 6,
  6,
  8,
  8,
  10,
  10,
  IF_EXT_PARTITION(12, 12)
};

/* clang-format off */
#if CONFIG_EXT_PARTITION_TYPES
static const BLOCK_SIZE subsize_lookup[EXT_PARTITION_TYPES][BLOCK_SIZES_ALL] =
#else
static const BLOCK_SIZE subsize_lookup[PARTITION_TYPES][BLOCK_SIZES_ALL] =
#endif  // CONFIG_EXT_PARTITION_TYPES
{
  {     // PARTITION_NONE
    // 2X2,        2X4,           4X2,
    BLOCK_2X2,     BLOCK_2X4,     BLOCK_4X2,
    //                            4X4
                                  BLOCK_4X4,
    // 4X8,        8X4,           8X8
    BLOCK_4X8,     BLOCK_8X4,     BLOCK_8X8,
    // 8X16,       16X8,          16X16
    BLOCK_8X16,    BLOCK_16X8,    BLOCK_16X16,
    // 16X32,      32X16,         32X32
    BLOCK_16X32,   BLOCK_32X16,   BLOCK_32X32,
    // 32X64,      64X32,         64X64
    BLOCK_32X64,   BLOCK_64X32,   BLOCK_64X64,
#if CONFIG_EXT_PARTITION
    // 64x128,     128x64,        128x128
    BLOCK_64X128,  BLOCK_128X64,  BLOCK_128X128,
#endif  // CONFIG_EXT_PARTITION
    // 4X16,       16X4,          8X32
    BLOCK_4X16,    BLOCK_16X4,    BLOCK_8X32,
    // 32X8,       16X64,         64X16
    BLOCK_32X8,    BLOCK_16X64,   BLOCK_64X16,
#if CONFIG_EXT_PARTITION
    // 32x128,     128x32
    BLOCK_32X128,  BLOCK_128X32
#endif  // CONFIG_EXT_PARTITION
  }, {  // PARTITION_HORZ
    // 2X2,        2X4,           4X2,
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    //                            4X4
                                  BLOCK_4X2,
    // 4X8,        8X4,           8X8
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X4,
    // 8X16,       16X8,          16X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X8,
    // 16X32,      32X16,         32X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X16,
    // 32X64,      64X32,         64X64
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X32,
#if CONFIG_EXT_PARTITION
    // 64x128,     128x64,        128x128
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_128X64,
#endif  // CONFIG_EXT_PARTITION
    // 4X16,       16X4,          8X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    // 32X8,       16X64,         64X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
#if CONFIG_EXT_PARTITION
    // 32x128,     128x32
    BLOCK_INVALID, BLOCK_INVALID
#endif  // CONFIG_EXT_PARTITION
  }, {  // PARTITION_VERT
    // 2X2,        2X4,           4X2,
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    //                            4X4
                                  BLOCK_2X4,
    // 4X8,        8X4,           8X8
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X8,
    // 8X16,       16X8,          16X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X16,
    // 16X32,      32X16,         32X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X32,
    // 32X64,      64X32,         64X64
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X64,
#if CONFIG_EXT_PARTITION
    // 64x128,     128x64,        128x128
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X128,
#endif  // CONFIG_EXT_PARTITION
    // 4X16,       16X4,          8X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    // 32X8,       16X64,         64X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
#if CONFIG_EXT_PARTITION
    // 32x128,     128x32
    BLOCK_INVALID, BLOCK_INVALID
#endif  // CONFIG_EXT_PARTITION
  }, {  // PARTITION_SPLIT
    // 2X2,        2X4,           4X2,
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    //                            4X4
                                  BLOCK_INVALID,
    // 4X8,        8X4,           8X8
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X4,
    // 8X16,       16X8,          16X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X8,
    // 16X32,      32X16,         32X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X16,
    // 32X64,      64X32,         64X64
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X32,
#if CONFIG_EXT_PARTITION
    // 64x128,     128x64,        128x128
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X64,
#endif  // CONFIG_EXT_PARTITION
    // 4X16,       16X4,          8X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    // 32X8,       16X64,         64X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
#if CONFIG_EXT_PARTITION
    // 32x128,     128x32
    BLOCK_INVALID, BLOCK_INVALID
#endif  // CONFIG_EXT_PARTITION
#if CONFIG_EXT_PARTITION_TYPES
  }, {  // PARTITION_HORZ_A
    // 2X2,        2X4,           4X2,
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    //                            4X4
                                  BLOCK_INVALID,
    // 4X8,        8X4,           8X8
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X4,
    // 8X16,       16X8,          16X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X8,
    // 16X32,      32X16,         32X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X16,
    // 32X64,      64X32,         64X64
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X32,
#if CONFIG_EXT_PARTITION
    // 64x128,     128x64,        128x128
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_128X64,
#endif  // CONFIG_EXT_PARTITION
    // 4X16,       16X4,          8X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    // 32X8,       16X64,         64X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
#if CONFIG_EXT_PARTITION
    // 32x128,     128x32
    BLOCK_INVALID, BLOCK_INVALID
#endif  // CONFIG_EXT_PARTITION
  }, {  // PARTITION_HORZ_B
    // 2X2,        2X4,           4X2,
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    //                            4X4
                                  BLOCK_INVALID,
    // 4X8,        8X4,           8X8
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X4,
    // 8X16,       16X8,          16X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X8,
    // 16X32,      32X16,         32X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X16,
    // 32X64,      64X32,         64X64
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X32,
#if CONFIG_EXT_PARTITION
    // 64x128,     128x64,        128x128
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_128X64,
#endif  // CONFIG_EXT_PARTITION
    // 4X16,       16X4,          8X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    // 32X8,       16X64,         64X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
#if CONFIG_EXT_PARTITION
    // 32x128,     128x32
    BLOCK_INVALID, BLOCK_INVALID
#endif  // CONFIG_EXT_PARTITION
  }, {  // PARTITION_VERT_A
    // 2X2,        2X4,           4X2,
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    //                            4X4
                                  BLOCK_INVALID,
    // 4X8,        8X4,           8X8
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X8,
    // 8X16,       16X8,          16X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X16,
    // 16X32,      32X16,         32X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X32,
    // 32X64,      64X32,         64X64
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X64,
#if CONFIG_EXT_PARTITION
    // 64x128,     128x64,        128x128
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X128,
#endif  // CONFIG_EXT_PARTITION
    // 4X16,       16X4,          8X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    // 32X8,       16X64,         64X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
#if CONFIG_EXT_PARTITION
    // 32x128,     128x32
    BLOCK_INVALID, BLOCK_INVALID
#endif  // CONFIG_EXT_PARTITION
  }, {  // PARTITION_VERT_B
    // 2X2,        2X4,           4X2,
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    //                            4X4
                                  BLOCK_INVALID,
    // 4X8,        8X4,           8X8
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X8,
    // 8X16,       16X8,          16X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X16,
    // 16X32,      32X16,         32X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X32,
    // 32X64,      64X32,         64X64
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X64,
#if CONFIG_EXT_PARTITION
    // 64x128,     128x64,        128x128
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X128,
#endif  // CONFIG_EXT_PARTITION
    // 4X16,       16X4,          8X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    // 32X8,       16X64,         64X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
#if CONFIG_EXT_PARTITION
    // 32x128,     128x32
    BLOCK_INVALID, BLOCK_INVALID
#endif  // CONFIG_EXT_PARTITION
  }, {  // PARTITION_HORZ_4
    // 2X2,        2X4,           4X2,
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    //                            4X4
                                  BLOCK_INVALID,
    // 4X8,        8X4,           8X8
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    // 8X16,       16X8,          16X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X4,
    // 16X32,      32X16,         32X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X8,
    // 32X64,      64X32,         64X64
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X16,
#if CONFIG_EXT_PARTITION
    // 64x128,     128x64,        128x128
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_128X32,
#endif  // CONFIG_EXT_PARTITION
    // 4X16,       16X4,          8X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    // 32X8,       16X64,         64X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
#if CONFIG_EXT_PARTITION
    // 32x128,     128x32
    BLOCK_INVALID, BLOCK_INVALID
#endif  // CONFIG_EXT_PARTITION
  }, {  // PARTITION_VERT_4
    // 2X2,        2X4,           4X2,
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    //                            4X4
                                  BLOCK_INVALID,
    // 4X8,        8X4,           8X8
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    // 8X16,       16X8,          16X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X16,
    // 16X32,      32X16,         32X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X32,
    // 32X64,      64X32,         64X64
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X64,
#if CONFIG_EXT_PARTITION
    // 64x128,     128x64,        128x128
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X128,
#endif  // CONFIG_EXT_PARTITION
    // 4X16,       16X4,          8X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    // 32X8,       16X64,         64X16
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
#if CONFIG_EXT_PARTITION
    // 32x128,     128x32
    BLOCK_INVALID, BLOCK_INVALID
#endif  // CONFIG_EXT_PARTITION
#endif  // CONFIG_EXT_PARTITION_TYPES
  }
};

static const TX_SIZE max_txsize_lookup[BLOCK_SIZES_ALL] = {
  // 2X2,    2X4,      4X2,
  TX_4X4,    TX_4X4,   TX_4X4,
  //                   4X4
                       TX_4X4,
  // 4X8,    8X4,      8X8
  TX_4X4,    TX_4X4,   TX_8X8,
  // 8X16,   16X8,     16X16
  TX_8X8,    TX_8X8,   TX_16X16,
  // 16X32,  32X16,    32X32
  TX_16X16,  TX_16X16, TX_32X32,
  // 32X64,  64X32,
  TX_32X32,  TX_32X32,
#if CONFIG_TX64X64
  // 64X64
  TX_64X64,
#if CONFIG_EXT_PARTITION
  // 64x128, 128x64,   128x128
  TX_64X64,  TX_64X64, TX_64X64,
#endif  // CONFIG_EXT_PARTITION
#else
  // 64X64
  TX_32X32,
#if CONFIG_EXT_PARTITION
  // 64x128, 128x64,   128x128
  TX_32X32,  TX_32X32, TX_32X32,
#endif  // CONFIG_EXT_PARTITION
#endif  // CONFIG_TX64X64
  // 4x16,   16x4,     8x32
  TX_4X4,    TX_4X4,   TX_8X8,
  // 32x8,   16x64     64x16
  TX_8X8,    TX_16X16, TX_16X16,
#if CONFIG_EXT_PARTITION
  // 32x128  128x32
  TX_32X32,  TX_32X32
#endif  // CONFIG_EXT_PARTITION
};

static const TX_SIZE max_txsize_rect_intra_lookup[BLOCK_SIZES_ALL] = {
  // 2X2,    2X4,      4X2,
  TX_4X4,    TX_4X4,   TX_4X4,
  //                   4X4
                       TX_4X4,
  // 4X8,    8X4,      8X8
  TX_4X8,    TX_8X4,   TX_8X8,
  // 8X16,   16X8,     16X16
  TX_8X16,   TX_16X8,  TX_16X16,
  // 16X32,  32X16,    32X32
  TX_16X32,  TX_32X16, TX_32X32,
#if CONFIG_TX64X64
  // 32X64,  64X32,
  TX_32X64,  TX_64X32,
  // 64X64
  TX_64X64,
#if CONFIG_EXT_PARTITION
  // 64x128, 128x64,   128x128
  TX_64X64,  TX_64X64, TX_64X64,
#endif  // CONFIG_EXT_PARTITION
#else
  // 32X64,  64X32,
  TX_32X32,  TX_32X32,
  // 64X64
  TX_32X32,
#if CONFIG_EXT_PARTITION
  // 64x128, 128x64,   128x128
  TX_32X32,  TX_32X32, TX_32X32,
#endif  // CONFIG_EXT_PARTITION
#endif  // CONFIG_TX64X64
  // 4x16,   16x4,     8x32
  TX_4X8,    TX_8X4,   TX_8X16,
  // 32x8    16x64,    64x16
  TX_16X8,   TX_16X32, TX_32X16,
#if CONFIG_EXT_PARTITION
  // 32x128  128x32
  TX_32X32,  TX_32X32
#endif  // CONFIG_EXT_PARTITION
};

static const TX_SIZE max_txsize_rect_lookup[BLOCK_SIZES_ALL] = {
  // 2X2,    2X4,      4X2,
  TX_4X4,    TX_4X4,   TX_4X4,
  //                   4X4
                       TX_4X4,
  // 4X8,    8X4,      8X8
  TX_4X8,    TX_8X4,   TX_8X8,
  // 8X16,   16X8,     16X16
  TX_8X16,   TX_16X8,  TX_16X16,
  // 16X32,  32X16,    32X32
  TX_16X32,  TX_32X16, TX_32X32,
#if CONFIG_TX64X64
  // 32X64,  64X32,
  TX_32X64,  TX_64X32,
  // 64X64
  TX_64X64,
#if CONFIG_EXT_PARTITION
  // 64x128, 128x64,   128x128
  TX_64X64,  TX_64X64, TX_64X64,
#endif  // CONFIG_EXT_PARTITION
#else
  // 32X64,  64X32,
  TX_32X32,  TX_32X32,
  // 64X64
  TX_32X32,
#if CONFIG_EXT_PARTITION
  // 64x128, 128x64,   128x128
  TX_32X32,  TX_32X32, TX_32X32,
#endif  // CONFIG_EXT_PARTITION
#endif  // CONFIG_TX64X64
#if CONFIG_EXT_PARTITION_TYPES && CONFIG_RECT_TX_EXT
  // 4x16,   16x4,     8x32
  TX_4X16,   TX_16X4,  TX_8X32,
  // 32x8
  TX_32X8,
#else
  // 4x16,   16x4,     8x32
  TX_4X8,    TX_8X4,   TX_8X16,
  // 32x8
  TX_16X8,
#endif  // CONFIG_EXT_PARTITION_TYPES && CONFIG_RECT_TX_EXT
  // 16x64,  64x16
  TX_16X32,  TX_32X16,
#if CONFIG_EXT_PARTITION
  // 32x128  128x32
  TX_32X32,  TX_32X32
#endif  // CONFIG_EXT_PARTITION
};

static const TX_TYPE_1D vtx_tab[TX_TYPES] = {
  DCT_1D,      ADST_1D, DCT_1D,      ADST_1D,
  FLIPADST_1D, DCT_1D,  FLIPADST_1D, ADST_1D, FLIPADST_1D, IDTX_1D,
  DCT_1D,      IDTX_1D, ADST_1D,     IDTX_1D, FLIPADST_1D, IDTX_1D,
};

static const TX_TYPE_1D htx_tab[TX_TYPES] = {
  DCT_1D,  DCT_1D,      ADST_1D,     ADST_1D,
  DCT_1D,  FLIPADST_1D, FLIPADST_1D, FLIPADST_1D, ADST_1D, IDTX_1D,
  IDTX_1D, DCT_1D,      IDTX_1D,     ADST_1D,     IDTX_1D, FLIPADST_1D,
};

// Same as "max_txsize_lookup[bsize] - TX_8X8", except for rectangular
// block which may use a rectangular transform, in which  case it is
// "(max_txsize_lookup[bsize] + 1) - TX_8X8", invalid for bsize < 8X8
static const int32_t intra_tx_size_cat_lookup[BLOCK_SIZES_ALL] = {
  // 2X2,             2X4,                4X2,
  INT32_MIN,          INT32_MIN,          INT32_MIN,
  //                                      4X4,
                                          INT32_MIN,
  // 4X8,             8X4,                8X8,
  TX_8X8 - TX_8X8,    TX_8X8 - TX_8X8,    TX_8X8 - TX_8X8,
  // 8X16,            16X8,               16X16
  TX_16X16 - TX_8X8,  TX_16X16 - TX_8X8,  TX_16X16 - TX_8X8,
  // 16X32,           32X16,              32X32
  TX_32X32 - TX_8X8,  TX_32X32 - TX_8X8,  TX_32X32 - TX_8X8,
#if CONFIG_TX64X64
  // 32X64,           64X32,
  TX_64X64 - TX_8X8,  TX_64X64 - TX_8X8,
  // 64X64
  TX_64X64 - TX_8X8,
#if CONFIG_EXT_PARTITION
  // 64x128,          128x64,             128x128
  TX_64X64 - TX_8X8,  TX_64X64 - TX_8X8,  TX_64X64 - TX_8X8,
#endif  // CONFIG_EXT_PARTITION
#else
  // 32X64,           64X32,
  TX_32X32 - TX_8X8,  TX_32X32 - TX_8X8,
  // 64X64
  TX_32X32 - TX_8X8,
#if CONFIG_EXT_PARTITION
  // 64x128,          128x64,             128x128
  TX_32X32 - TX_8X8,  TX_32X32 - TX_8X8,  TX_32X32 - TX_8X8,
#endif  // CONFIG_EXT_PARTITION
#endif  // CONFIG_TX64X64
  // TODO(david.barker): Change these if we support rectangular transforms
  // for 4:1 shaped partitions
  // 4x16,            16x4,               8x32
  TX_8X8 - TX_8X8,    TX_8X8 - TX_8X8,    TX_8X8 - TX_8X8,
  // 32x8,            16x64,              64x16
  TX_8X8 - TX_8X8,    TX_16X16 - TX_8X8,  TX_16X16 - TX_8X8,
#if CONFIG_EXT_PARTITION
  // 32x128,          128x32
  TX_32X32 - TX_8X8,  TX_32X32 - TX_8X8
#endif  // CONFIG_EXT_PARTITION
};

#define inter_tx_size_cat_lookup intra_tx_size_cat_lookup

/* clang-format on */

static const TX_SIZE sub_tx_size_map[TX_SIZES_ALL] = {
  TX_4X4,    // TX_4X4
  TX_4X4,    // TX_8X8
  TX_8X8,    // TX_16X16
  TX_16X16,  // TX_32X32
#if CONFIG_TX64X64
  TX_32X32,  // TX_64X64
#endif       // CONFIG_TX64X64
  TX_4X4,    // TX_4X8
  TX_4X4,    // TX_8X4
  TX_8X8,    // TX_8X16
  TX_8X8,    // TX_16X8
  TX_16X16,  // TX_16X32
  TX_16X16,  // TX_32X16
#if CONFIG_TX64X64
  TX_32X32,  // TX_32X64
  TX_32X32,  // TX_64X32
#endif       // CONFIG_TX64X64
  TX_4X4,    // TX_4X16
  TX_4X4,    // TX_16X4
  TX_8X8,    // TX_8X32
  TX_8X8,    // TX_32X8
};

static const TX_SIZE txsize_horz_map[TX_SIZES_ALL] = {
  TX_4X4,    // TX_4X4
  TX_8X8,    // TX_8X8
  TX_16X16,  // TX_16X16
  TX_32X32,  // TX_32X32
#if CONFIG_TX64X64
  TX_64X64,  // TX_64X64
#endif       // CONFIG_TX64X64
  TX_4X4,    // TX_4X8
  TX_8X8,    // TX_8X4
  TX_8X8,    // TX_8X16
  TX_16X16,  // TX_16X8
  TX_16X16,  // TX_16X32
  TX_32X32,  // TX_32X16
#if CONFIG_TX64X64
  TX_32X32,  // TX_32X64
  TX_64X64,  // TX_64X32
#endif       // CONFIG_TX64X64
  TX_4X4,    // TX_4X16
  TX_16X16,  // TX_16X4
  TX_8X8,    // TX_8X32
  TX_32X32,  // TX_32X8
};

static const TX_SIZE txsize_vert_map[TX_SIZES_ALL] = {
  TX_4X4,    // TX_4X4
  TX_8X8,    // TX_8X8
  TX_16X16,  // TX_16X16
  TX_32X32,  // TX_32X32
#if CONFIG_TX64X64
  TX_64X64,  // TX_64X64
#endif       // CONFIG_TX64X64
  TX_8X8,    // TX_4X8
  TX_4X4,    // TX_8X4
  TX_16X16,  // TX_8X16
  TX_8X8,    // TX_16X8
  TX_32X32,  // TX_16X32
  TX_16X16,  // TX_32X16
#if CONFIG_TX64X64
  TX_64X64,  // TX_32X64
  TX_32X32,  // TX_64X32
#endif       // CONFIG_TX64X64
  TX_16X16,  // TX_4X16
  TX_4X4,    // TX_16X4
  TX_32X32,  // TX_8X32
  TX_8X8,    // TX_32X8
};

#define TX_SIZE_W_MIN 4

// Transform block width in pixels
static const int tx_size_wide[TX_SIZES_ALL] = { 4,  8,  16, 32,
#if CONFIG_TX64X64
                                                64,
#endif  // CONFIG_TX64X64
                                                4,  8,  8,  16, 16, 32,
#if CONFIG_TX64X64
                                                32, 64,
#endif  // CONFIG_TX64X64
                                                4,  16, 8,  32 };

#define TX_SIZE_H_MIN 4

// Transform block height in pixels
static const int tx_size_high[TX_SIZES_ALL] = { 4,  8,  16, 32,
#if CONFIG_TX64X64
                                                64,
#endif  // CONFIG_TX64X64
                                                8,  4,  16, 8,  32, 16,
#if CONFIG_TX64X64
                                                64, 32,
#endif  // CONFIG_TX64X64
                                                16, 4,  32, 8 };

// Transform block width in unit
static const int tx_size_wide_unit[TX_SIZES_ALL] = { 1,  2,  4, 8,
#if CONFIG_TX64X64
                                                     16,
#endif  // CONFIG_TX64X64
                                                     1,  2,  2, 4, 4, 8,
#if CONFIG_TX64X64
                                                     8,  16,
#endif  // CONFIG_TX64X64
                                                     1,  4,  2, 8 };

// Transform block height in unit
static const int tx_size_high_unit[TX_SIZES_ALL] = { 1,  2, 4, 8,
#if CONFIG_TX64X64
                                                     16,
#endif  // CONFIG_TX64X64
                                                     2,  1, 4, 2, 8, 4,
#if CONFIG_TX64X64
                                                     16, 8,
#endif  // CONFIG_TX64X64
                                                     4,  1, 8, 2 };

// Transform block width in log2
static const int tx_size_wide_log2[TX_SIZES_ALL] = { 2, 3, 4, 5,
#if CONFIG_TX64X64
                                                     6,
#endif  // CONFIG_TX64X64
                                                     2, 3, 3, 4, 4, 5,
#if CONFIG_TX64X64
                                                     5, 6,
#endif  // CONFIG_TX64X64
                                                     2, 4, 3, 5 };

// Transform block height in log2
static const int tx_size_high_log2[TX_SIZES_ALL] = { 2, 3, 4, 5,
#if CONFIG_TX64X64
                                                     6,
#endif  // CONFIG_TX64X64
                                                     3, 2, 4, 3, 5, 4,
#if CONFIG_TX64X64
                                                     6, 5,
#endif  // CONFIG_TX64X64
                                                     4, 2, 5, 3 };

#define TX_UNIT_WIDE_LOG2 (MI_SIZE_LOG2 - tx_size_wide_log2[0])
#define TX_UNIT_HIGH_LOG2 (MI_SIZE_LOG2 - tx_size_high_log2[0])

static const int tx_size_2d[TX_SIZES_ALL] = { 16,   64,   256, 1024,
#if CONFIG_TX64X64
                                              4096,
#endif  // CONFIG_TX64X64
                                              32,   32,   128, 128,  512, 512,
#if CONFIG_TX64X64
                                              2048, 2048,
#endif  // CONFIG_TX64X64
                                              64,   64,   256, 256 };

static const BLOCK_SIZE txsize_to_bsize[TX_SIZES_ALL] = {
  BLOCK_4X4,    // TX_4X4
  BLOCK_8X8,    // TX_8X8
  BLOCK_16X16,  // TX_16X16
  BLOCK_32X32,  // TX_32X32
#if CONFIG_TX64X64
  BLOCK_64X64,  // TX_64X64
#endif          // CONFIG_TX64X64
  BLOCK_4X8,    // TX_4X8
  BLOCK_8X4,    // TX_8X4
  BLOCK_8X16,   // TX_8X16
  BLOCK_16X8,   // TX_16X8
  BLOCK_16X32,  // TX_16X32
  BLOCK_32X16,  // TX_32X16
#if CONFIG_TX64X64
  BLOCK_32X64,  // TX_32X64
  BLOCK_64X32,  // TX_64X32
#endif          // CONFIG_TX64X64
  BLOCK_4X16,   // TX_4X16
  BLOCK_16X4,   // TX_16X4
  BLOCK_8X32,   // TX_8X32
  BLOCK_32X8,   // TX_32X8
};

static const TX_SIZE txsize_sqr_map[TX_SIZES_ALL] = {
  TX_4X4,    // TX_4X4
  TX_8X8,    // TX_8X8
  TX_16X16,  // TX_16X16
  TX_32X32,  // TX_32X32
#if CONFIG_TX64X64
  TX_64X64,  // TX_64X64
#endif       // CONFIG_TX64X64
  TX_4X4,    // TX_4X8
  TX_4X4,    // TX_8X4
  TX_8X8,    // TX_8X16
  TX_8X8,    // TX_16X8
  TX_16X16,  // TX_16X32
  TX_16X16,  // TX_32X16
#if CONFIG_TX64X64
  TX_32X32,  // TX_32X64
  TX_32X32,  // TX_64X32
#endif       // CONFIG_TX64X64
  TX_4X4,    // TX_4X16
  TX_4X4,    // TX_16X4
  TX_8X8,    // TX_8X32
  TX_8X8,    // TX_32X8
};

static const TX_SIZE txsize_sqr_up_map[TX_SIZES_ALL] = {
  TX_4X4,    // TX_4X4
  TX_8X8,    // TX_8X8
  TX_16X16,  // TX_16X16
  TX_32X32,  // TX_32X32
#if CONFIG_TX64X64
  TX_64X64,  // TX_64X64
#endif       // CONFIG_TX64X64
  TX_8X8,    // TX_4X8
  TX_8X8,    // TX_8X4
  TX_16X16,  // TX_8X16
  TX_16X16,  // TX_16X8
  TX_32X32,  // TX_16X32
  TX_32X32,  // TX_32X16
#if CONFIG_TX64X64
  TX_64X64,  // TX_32X64
  TX_64X64,  // TX_64X32
#endif       // CONFIG_TX64X64
  TX_16X16,  // TX_4X16
  TX_16X16,  // TX_16X4
  TX_32X32,  // TX_8X32
  TX_32X32,  // TX_32X8
};

/* clang-format off */
#if CONFIG_SIMPLIFY_TX_MODE
static const TX_SIZE tx_mode_to_biggest_tx_size[TX_MODES] = {
  TX_4X4,    // ONLY_4X4
#if CONFIG_TX64X64
  TX_64X64,  // TX_MODE_LARGEST
  TX_64X64,  // TX_MODE_SELECT
#else
  TX_32X32,  // TX_MODE_LARGEST
  TX_32X32,  // TX_MODE_SELECT
#endif  // CONFIG_TX64X64
};
#else
static const TX_SIZE tx_mode_to_biggest_tx_size[TX_MODES] = {
  TX_4X4,    // ONLY_4X4
  TX_8X8,    // ALLOW_8X8
  TX_16X16,  // ALLOW_16X16
  TX_32X32,  // ALLOW_32X32
#if CONFIG_TX64X64
  TX_64X64,  // ALLOW_64X64
  TX_64X64,  // TX_MODE_LARGEST
#else
  TX_32X32,  // TX_MODE_SELECT
#endif  // CONFIG_TX64X64
};
#endif  // CONFIG_SIMPLIFY_TX_MODE
/* clang-format on */

static const BLOCK_SIZE ss_size_lookup[BLOCK_SIZES_ALL][2][2] = {
  //  ss_x == 0    ss_x == 0        ss_x == 1      ss_x == 1
  //  ss_y == 0    ss_y == 1        ss_y == 0      ss_y == 1
  { { BLOCK_2X2, BLOCK_INVALID }, { BLOCK_INVALID, BLOCK_INVALID } },
  { { BLOCK_2X4, BLOCK_INVALID }, { BLOCK_INVALID, BLOCK_INVALID } },
  { { BLOCK_4X2, BLOCK_INVALID }, { BLOCK_INVALID, BLOCK_INVALID } },
  { { BLOCK_4X4, BLOCK_4X2 }, { BLOCK_2X4, BLOCK_2X2 } },
  { { BLOCK_4X8, BLOCK_4X4 }, { BLOCK_INVALID, BLOCK_2X4 } },
  { { BLOCK_8X4, BLOCK_INVALID }, { BLOCK_4X4, BLOCK_4X2 } },
  { { BLOCK_8X8, BLOCK_8X4 }, { BLOCK_4X8, BLOCK_4X4 } },
  { { BLOCK_8X16, BLOCK_8X8 }, { BLOCK_INVALID, BLOCK_4X8 } },
  { { BLOCK_16X8, BLOCK_INVALID }, { BLOCK_8X8, BLOCK_8X4 } },
  { { BLOCK_16X16, BLOCK_16X8 }, { BLOCK_8X16, BLOCK_8X8 } },
  { { BLOCK_16X32, BLOCK_16X16 }, { BLOCK_INVALID, BLOCK_8X16 } },
  { { BLOCK_32X16, BLOCK_INVALID }, { BLOCK_16X16, BLOCK_16X8 } },
  { { BLOCK_32X32, BLOCK_32X16 }, { BLOCK_16X32, BLOCK_16X16 } },
  { { BLOCK_32X64, BLOCK_32X32 }, { BLOCK_INVALID, BLOCK_16X32 } },
  { { BLOCK_64X32, BLOCK_INVALID }, { BLOCK_32X32, BLOCK_32X16 } },
  { { BLOCK_64X64, BLOCK_64X32 }, { BLOCK_32X64, BLOCK_32X32 } },
#if CONFIG_EXT_PARTITION
  { { BLOCK_64X128, BLOCK_64X64 }, { BLOCK_INVALID, BLOCK_32X64 } },
  { { BLOCK_128X64, BLOCK_INVALID }, { BLOCK_64X64, BLOCK_64X32 } },
  { { BLOCK_128X128, BLOCK_128X64 }, { BLOCK_64X128, BLOCK_64X64 } },
#endif  // CONFIG_EXT_PARTITION
  { { BLOCK_4X16, BLOCK_4X8 }, { BLOCK_INVALID, BLOCK_4X8 } },
  { { BLOCK_16X4, BLOCK_INVALID }, { BLOCK_8X4, BLOCK_8X4 } },
  { { BLOCK_8X32, BLOCK_8X16 }, { BLOCK_INVALID, BLOCK_4X16 } },
  { { BLOCK_32X8, BLOCK_INVALID }, { BLOCK_16X8, BLOCK_16X4 } },
  { { BLOCK_16X64, BLOCK_16X32 }, { BLOCK_INVALID, BLOCK_8X32 } },
  { { BLOCK_64X16, BLOCK_INVALID }, { BLOCK_32X16, BLOCK_32X8 } },
#if CONFIG_EXT_PARTITION
  { { BLOCK_32X128, BLOCK_32X64 }, { BLOCK_INVALID, BLOCK_16X64 } },
  { { BLOCK_128X32, BLOCK_INVALID }, { BLOCK_64X32, BLOCK_64X16 } },
#endif  // CONFIG_EXT_PARTITION
};

static const TX_SIZE uv_txsize_lookup[BLOCK_SIZES_ALL][TX_SIZES_ALL][2][2] = {
  //  ss_x == 0    ss_x == 0        ss_x == 1      ss_x == 1
  //  ss_y == 0    ss_y == 1        ss_y == 0      ss_y == 1
  {
      // BLOCK_2x2
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
  },
  {
      // BLOCK_2X4
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
  },
  {
      // BLOCK_4X2
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
  },
  {
      // BLOCK_4X4
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
  },
  {
      // BLOCK_4X8
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
#endif                                             // CONFIG_TX64X64
      { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },  // used
      { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
  },
  {
      // BLOCK_8X4
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },  // used
      { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
  },
  {
      // BLOCK_8X8
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_8X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X4 }, { TX_4X8, TX_4X4 } },
      { { TX_8X4, TX_8X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X4 }, { TX_4X8, TX_4X4 } },
      { { TX_8X8, TX_8X4 }, { TX_4X8, TX_4X4 } },
      { { TX_8X8, TX_8X4 }, { TX_4X8, TX_4X4 } },
      { { TX_8X8, TX_8X4 }, { TX_4X8, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_8X8, TX_8X4 }, { TX_4X8, TX_4X4 } },
      { { TX_8X8, TX_8X4 }, { TX_4X8, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X4 }, { TX_4X8, TX_4X4 } },
      { { TX_8X4, TX_8X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X4 }, { TX_4X8, TX_4X4 } },
      { { TX_8X8, TX_8X4 }, { TX_4X8, TX_4X4 } },
  },
  {
      // BLOCK_8X16
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_8X16, TX_8X8 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X4, TX_8X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X16, TX_8X8 }, { TX_4X8, TX_4X8 } },  // used
      { { TX_8X16, TX_8X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X16, TX_8X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X16, TX_8X8 }, { TX_4X8, TX_4X8 } },
#if CONFIG_TX64X64
      { { TX_8X16, TX_8X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X16, TX_8X8 }, { TX_4X8, TX_4X8 } },
#endif  // CONFIG_TX64X64
      { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
      { { TX_8X4, TX_8X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X16, TX_8X8 }, { TX_4X16, TX_4X8 } },
      { { TX_8X8, TX_8X8 }, { TX_4X8, TX_4X8 } },
  },
  {
      // BLOCK_16X8
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_4X4 }, { TX_8X8, TX_4X4 } },
      { { TX_8X8, TX_4X4 }, { TX_8X8, TX_4X4 } },
      { { TX_8X8, TX_4X4 }, { TX_8X8, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_16X8, TX_4X4 }, { TX_8X8, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X4 }, { TX_4X8, TX_4X4 } },
      { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
      { { TX_16X8, TX_8X4 }, { TX_8X8, TX_8X4 } },
      { { TX_16X8, TX_8X4 }, { TX_8X8, TX_8X4 } },  // used
      { { TX_16X8, TX_8X4 }, { TX_8X8, TX_8X4 } },
      { { TX_16X8, TX_8X4 }, { TX_8X8, TX_8X4 } },
#if CONFIG_TX64X64
      { { TX_16X8, TX_8X4 }, { TX_8X8, TX_8X4 } },
      { { TX_16X8, TX_8X4 }, { TX_8X8, TX_8X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X4 }, { TX_4X8, TX_4X4 } },
      { { TX_16X4, TX_16X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X8, TX_8X4 }, { TX_8X8, TX_8X4 } },
      { { TX_16X8, TX_16X4 }, { TX_8X8, TX_8X4 } },
  },
  {
      // BLOCK_16X16
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_8X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_8X8 }, { TX_8X8, TX_8X8 } },
#if CONFIG_TX64X64
      { { TX_16X16, TX_8X8 }, { TX_8X8, TX_8X8 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
      { { TX_16X8, TX_16X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_16X8 }, { TX_8X16, TX_8X8 } },
      { { TX_16X16, TX_16X8 }, { TX_8X16, TX_8X8 } },
#if CONFIG_TX64X64
      { { TX_16X16, TX_16X8 }, { TX_8X16, TX_8X8 } },
      { { TX_16X16, TX_16X8 }, { TX_8X16, TX_8X8 } },
#endif  // CONFIG_TX64X64
      { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
      { { TX_16X4, TX_16X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
      { { TX_16X8, TX_16X8 }, { TX_8X8, TX_8X8 } },
  },
  {
      // BLOCK_16X32
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_16X16 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_16X16 }, { TX_8X8, TX_8X8 } },
#if CONFIG_TX64X64
      { { TX_16X32, TX_16X16 }, { TX_8X8, TX_8X8 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X16, TX_8X16 }, { TX_8X16, TX_8X16 } },
      { { TX_16X8, TX_16X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X32, TX_16X16 }, { TX_8X16, TX_8X16 } },  // used
      { { TX_16X32, TX_16X16 }, { TX_8X16, TX_8X16 } },
#if CONFIG_TX64X64
      { { TX_16X32, TX_16X16 }, { TX_8X16, TX_8X16 } },
      { { TX_16X32, TX_16X16 }, { TX_8X16, TX_8X16 } },
#endif  // CONFIG_TX64X64
      { { TX_4X16, TX_4X16 }, { TX_4X16, TX_4X16 } },
      { { TX_16X4, TX_16X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X32, TX_8X16 }, { TX_8X32, TX_8X16 } },
      { { TX_16X8, TX_16X8 }, { TX_8X8, TX_8X8 } },
  },
  {
      // BLOCK_32X16
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_8X8 }, { TX_16X16, TX_8X8 } },
      { { TX_16X16, TX_8X8 }, { TX_16X16, TX_8X8 } },
#if CONFIG_TX64X64
      { { TX_32X16, TX_8X8 }, { TX_16X16, TX_8X8 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
      { { TX_16X8, TX_16X8 }, { TX_16X8, TX_16X8 } },
      { { TX_32X16, TX_16X8 }, { TX_16X16, TX_16X8 } },
      { { TX_32X16, TX_16X8 }, { TX_16X16, TX_16X8 } },  // used
#if CONFIG_TX64X64
      { { TX_32X16, TX_16X8 }, { TX_16X16, TX_16X8 } },
      { { TX_32X16, TX_16X8 }, { TX_16X16, TX_16X8 } },
#endif  // CONFIG_TX64X64
      { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
      { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
      { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
      { { TX_32X8, TX_32X8 }, { TX_16X8, TX_16X8 } },
  },
  {
      // BLOCK_32X32
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_16X16 }, { TX_16X16, TX_16X16 } },
      { { TX_32X32, TX_16X16 }, { TX_16X16, TX_16X16 } },
#if CONFIG_TX64X64
      { { TX_32X32, TX_16X16 }, { TX_16X16, TX_16X16 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X16, TX_8X16 }, { TX_8X16, TX_8X16 } },
      { { TX_16X8, TX_16X8 }, { TX_16X8, TX_16X8 } },
      { { TX_16X32, TX_16X16 }, { TX_16X32, TX_16X16 } },
      { { TX_32X16, TX_32X16 }, { TX_16X16, TX_16X16 } },
#if CONFIG_TX64X64
      { { TX_32X32, TX_16X16 }, { TX_16X16, TX_16X16 } },
      { { TX_32X32, TX_16X16 }, { TX_16X16, TX_16X16 } },
#endif  // CONFIG_TX64X64
      { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
      { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
      { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
      { { TX_32X8, TX_32X8 }, { TX_16X8, TX_16X8 } },
  },
  {
      // BLOCK_32X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_16X16 }, { TX_16X16, TX_16X16 } },
      { { TX_32X32, TX_32X32 }, { TX_16X16, TX_16X16 } },
#if CONFIG_TX64X64
      { { TX_32X32, TX_32X32 }, { TX_16X16, TX_16X16 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X16, TX_8X16 }, { TX_8X16, TX_8X16 } },
      { { TX_16X8, TX_16X8 }, { TX_16X8, TX_16X8 } },
      { { TX_16X32, TX_16X32 }, { TX_16X16, TX_16X16 } },
      { { TX_32X16, TX_32X16 }, { TX_16X16, TX_16X16 } },
#if CONFIG_TX64X64
      { { TX_32X64, TX_32X32 }, { TX_16X16, TX_16X32 } },
      { { TX_32X32, TX_32X32 }, { TX_16X16, TX_16X16 } },
#endif  // CONFIG_TX64X64
      { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
      { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
      { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
      { { TX_32X8, TX_32X8 }, { TX_16X8, TX_16X8 } },
  },
  {
      // BLOCK_64X32
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_16X16 }, { TX_16X16, TX_16X16 } },
      { { TX_32X32, TX_16X16 }, { TX_32X32, TX_16X16 } },
#if CONFIG_TX64X64
      { { TX_32X32, TX_16X16 }, { TX_32X32, TX_16X16 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X16, TX_8X16 }, { TX_8X16, TX_8X16 } },
      { { TX_16X8, TX_16X8 }, { TX_16X8, TX_16X8 } },
      { { TX_16X32, TX_16X16 }, { TX_16X32, TX_16X16 } },
      { { TX_32X16, TX_16X16 }, { TX_32X16, TX_16X16 } },
#if CONFIG_TX64X64
      { { TX_32X32, TX_16X16 }, { TX_32X32, TX_16X16 } },
      { { TX_64X32, TX_16X16 }, { TX_32X32, TX_32X16 } },
#endif  // CONFIG_TX64X64
      { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
      { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
      { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
      { { TX_32X8, TX_32X8 }, { TX_16X8, TX_16X8 } },
  },
  {
      // BLOCK_64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_16X16 }, { TX_16X16, TX_16X16 } },
      { { TX_32X32, TX_32X32 }, { TX_32X32, TX_32X32 } },
#if CONFIG_TX64X64
      { { TX_64X64, TX_32X32 }, { TX_32X32, TX_32X32 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X16, TX_8X16 }, { TX_8X16, TX_8X16 } },
      { { TX_16X8, TX_16X8 }, { TX_16X8, TX_16X8 } },
      { { TX_16X32, TX_16X32 }, { TX_16X32, TX_16X32 } },
      { { TX_32X16, TX_32X16 }, { TX_32X16, TX_16X16 } },
#if CONFIG_TX64X64
      { { TX_32X64, TX_32X32 }, { TX_16X16, TX_16X32 } },
      { { TX_64X32, TX_16X16 }, { TX_32X32, TX_32X16 } },
#endif  // CONFIG_TX64X64
      { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
      { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
      { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
      { { TX_32X8, TX_32X8 }, { TX_16X8, TX_16X8 } },
  },
#if CONFIG_EXT_PARTITION
  {
      // BLOCK_64X128
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_16X16 }, { TX_16X16, TX_16X16 } },
      { { TX_32X32, TX_32X32 }, { TX_32X32, TX_32X32 } },
#if CONFIG_TX64X64
      { { TX_64X64, TX_32X32 }, { TX_32X32, TX_32X32 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X16, TX_8X16 }, { TX_8X16, TX_8X16 } },
      { { TX_16X8, TX_16X8 }, { TX_16X8, TX_16X8 } },
      { { TX_16X32, TX_16X32 }, { TX_16X32, TX_16X32 } },
      { { TX_32X16, TX_32X16 }, { TX_32X16, TX_32X16 } },
#if CONFIG_TX64X64
      { { TX_32X64, TX_32X32 }, { TX_16X16, TX_16X16 } },
      { { TX_64X32, TX_16X16 }, { TX_32X32, TX_16X16 } },
#endif  // CONFIG_TX64X64
      { { TX_INVALID, TX_INVALID }, { TX_INVALID, TX_INVALID } },
      { { TX_INVALID, TX_INVALID }, { TX_INVALID, TX_INVALID } },
      { { TX_INVALID, TX_INVALID }, { TX_INVALID, TX_INVALID } },
      { { TX_INVALID, TX_INVALID }, { TX_INVALID, TX_INVALID } },
  },
  {
      // BLOCK_128X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_16X16 }, { TX_16X16, TX_16X16 } },
      { { TX_32X32, TX_32X32 }, { TX_32X32, TX_32X32 } },
#if CONFIG_TX64X64
      { { TX_64X64, TX_32X32 }, { TX_32X32, TX_32X32 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X16, TX_8X16 }, { TX_8X16, TX_8X16 } },
      { { TX_16X8, TX_16X8 }, { TX_16X8, TX_16X8 } },
      { { TX_16X32, TX_16X32 }, { TX_16X32, TX_16X32 } },
      { { TX_32X16, TX_32X16 }, { TX_32X16, TX_32X16 } },
#if CONFIG_TX64X64
      { { TX_32X64, TX_32X32 }, { TX_16X16, TX_16X16 } },
      { { TX_64X32, TX_16X16 }, { TX_32X32, TX_16X16 } },
#endif  // CONFIG_TX64X64
      { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
      { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
      { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
      { { TX_32X8, TX_32X8 }, { TX_16X8, TX_16X8 } },
  },
  {
      // BLOCK_128X128
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_16X16 }, { TX_16X16, TX_16X16 } },
      { { TX_32X32, TX_32X32 }, { TX_32X32, TX_32X32 } },
#if CONFIG_TX64X64
      { { TX_64X64, TX_32X32 }, { TX_32X32, TX_32X32 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X16, TX_8X16 }, { TX_8X16, TX_8X16 } },
      { { TX_16X8, TX_16X8 }, { TX_16X8, TX_16X8 } },
      { { TX_16X32, TX_16X32 }, { TX_16X32, TX_16X32 } },
      { { TX_32X16, TX_32X16 }, { TX_32X16, TX_32X16 } },
#if CONFIG_TX64X64
      { { TX_32X64, TX_32X32 }, { TX_16X16, TX_16X16 } },
      { { TX_64X32, TX_16X16 }, { TX_32X32, TX_16X16 } },
#endif  // CONFIG_TX64X64
      { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
      { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
      { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
      { { TX_32X8, TX_32X8 }, { TX_16X8, TX_16X8 } },
  },
#endif  // CONFIG_EXT_PARTITION
  {
      // BLOCK_4X16
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X8 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X8, TX_4X8 }, { TX_4X4, TX_4X4 } },
      { { TX_4X8, TX_4X8 }, { TX_4X4, TX_4X4 } },
      { { TX_4X8, TX_4X8 }, { TX_4X4, TX_4X4 } },
      { { TX_4X8, TX_4X8 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X16, TX_4X8 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X16, TX_4X8 }, { TX_4X4, TX_4X4 } },
      { { TX_4X8, TX_4X8 }, { TX_4X4, TX_4X4 } },
  },
  {
      // BLOCK_16X4
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X4, TX_4X4 }, { TX_8X4, TX_4X4 } },
      { { TX_8X4, TX_4X4 }, { TX_8X4, TX_4X4 } },
      { { TX_8X4, TX_4X4 }, { TX_8X4, TX_4X4 } },
      { { TX_8X4, TX_4X4 }, { TX_8X4, TX_4X4 } },
      { { TX_8X4, TX_4X4 }, { TX_8X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_16X4, TX_4X4 }, { TX_8X4, TX_4X4 } },
      { { TX_8X4, TX_4X4 }, { TX_8X4, TX_4X4 } },
      { { TX_16X4, TX_4X4 }, { TX_8X4, TX_4X4 } },
  },
  {
      // BLOCK_8X32
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_4X4, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_8X8, TX_8X8 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X4, TX_8X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X16, TX_8X16 }, { TX_4X8, TX_4X8 } },
      { { TX_8X8, TX_8X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X16, TX_8X16 }, { TX_4X8, TX_4X8 } },
      { { TX_8X16, TX_8X16 }, { TX_4X8, TX_4X8 } },
#if CONFIG_TX64X64
      { { TX_8X8, TX_8X8 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_4X4, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X16, TX_4X16 }, { TX_4X16, TX_4X16 } },
      { { TX_8X4, TX_8X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X32, TX_8X16 }, { TX_4X16, TX_4X16 } },
      { { TX_8X8, TX_8X8 }, { TX_4X8, TX_4X8 } },
  },
  {
      // BLOCK_32X8
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_4X4 }, { TX_8X8, TX_4X4 } },
      { { TX_8X8, TX_4X4 }, { TX_8X8, TX_4X4 } },
      { { TX_8X8, TX_4X4 }, { TX_8X8, TX_4X4 } },
#if CONFIG_TX64X64
      { { TX_8X8, TX_4X4 }, { TX_8X8, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X4 }, { TX_4X8, TX_4X4 } },
      { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X8, TX_8X4 }, { TX_8X8, TX_8X4 } },
      { { TX_16X8, TX_8X4 }, { TX_16X8, TX_8X4 } },
      { { TX_16X8, TX_8X4 }, { TX_16X8, TX_8X4 } },
      { { TX_16X8, TX_8X4 }, { TX_16X8, TX_8X4 } },
#if CONFIG_TX64X64
      { { TX_8X8, TX_4X4 }, { TX_8X8, TX_4X4 } },
      { { TX_8X8, TX_4X4 }, { TX_8X8, TX_4X4 } },
#endif  // CONFIG_TX64X64
      { { TX_4X8, TX_4X4 }, { TX_4X8, TX_4X4 } },
      { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
      { { TX_8X8, TX_8X4 }, { TX_8X8, TX_8X4 } },
      { { TX_32X8, TX_16X4 }, { TX_16X8, TX_16X4 } },
  },
  {
      // BLOCK_16X64
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_16X16 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_16X16 }, { TX_8X8, TX_8X8 } },
#if CONFIG_TX64X64
      { { TX_16X16, TX_16X16 }, { TX_8X8, TX_8X8 } },
#endif
      { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X16, TX_8X16 }, { TX_8X16, TX_8X16 } },
      { { TX_16X8, TX_16X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X32, TX_16X32 }, { TX_8X16, TX_8X16 } },
      { { TX_16X16, TX_16X16 }, { TX_8X16, TX_8X16 } },
#if CONFIG_TX64X64
      { { TX_16X16, TX_16X16 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_16X16 }, { TX_8X8, TX_8X8 } },
#endif
      { { TX_4X16, TX_4X16 }, { TX_4X16, TX_4X16 } },
      { { TX_16X4, TX_16X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X32, TX_8X32 }, { TX_8X32, TX_8X32 } },
      { { TX_16X8, TX_16X8 }, { TX_8X8, TX_8X8 } },
  },
  {
      // BLOCK_64X16
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_8X8 }, { TX_16X16, TX_8X8 } },
      { { TX_16X16, TX_8X8 }, { TX_16X16, TX_8X8 } },
#if CONFIG_TX64X64
      { { TX_16X16, TX_8X8 }, { TX_16X16, TX_8X8 } },
#endif
      { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
      { { TX_16X8, TX_16X8 }, { TX_16X8, TX_16X8 } },
      { { TX_16X16, TX_16X8 }, { TX_16X16, TX_16X8 } },
      { { TX_32X16, TX_16X8 }, { TX_32X16, TX_16X8 } },
#if CONFIG_TX64X64
      { { TX_16X16, TX_8X8 }, { TX_16X16, TX_8X8 } },
      { { TX_16X16, TX_8X8 }, { TX_16X16, TX_8X8 } },
#endif
      { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
      { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
      { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
      { { TX_32X8, TX_32X8 }, { TX_32X8, TX_32X8 } },
  },
#if CONFIG_EXT_PARTITION
  {
      // BLOCK_32X128
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_16X16 }, { TX_16X16, TX_16X16 } },
      { { TX_32X32, TX_32X32 }, { TX_16X16, TX_16X16 } },
#if CONFIG_TX64X64
      { { TX_32X32, TX_32X32 }, { TX_16X16, TX_16X16 } },
#endif
      { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X16, TX_8X16 }, { TX_8X16, TX_8X16 } },
      { { TX_16X8, TX_16X8 }, { TX_16X8, TX_16X8 } },
      { { TX_16X32, TX_16X32 }, { TX_16X32, TX_16X32 } },
      { { TX_32X16, TX_32X16 }, { TX_16X16, TX_16X16 } },
      { { TX_4X16, TX_4X16 }, { TX_4X16, TX_4X16 } },
      { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
      { { TX_8X32, TX_8X32 }, { TX_8X32, TX_8X32 } },
      { { TX_32X8, TX_32X8 }, { TX_16X8, TX_16X8 } },
  },
  {
      // BLOCK_128X32
      { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
      { { TX_16X16, TX_16X16 }, { TX_16X16, TX_16X16 } },
      { { TX_32X32, TX_16X16 }, { TX_32X32, TX_16X16 } },
#if CONFIG_TX64X64
      { { TX_32X32, TX_16X16 }, { TX_32X32, TX_16X16 } },
#endif
      { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
      { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
      { { TX_8X16, TX_8X16 }, { TX_8X16, TX_8X16 } },
      { { TX_16X8, TX_16X8 }, { TX_16X8, TX_16X8 } },
      { { TX_16X32, TX_16X16 }, { TX_16X32, TX_16X16 } },
      { { TX_32X16, TX_32X16 }, { TX_32X16, TX_32X16 } },
      { { TX_4X16, TX_4X16 }, { TX_4X16, TX_4X16 } },
      { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
      { { TX_8X32, TX_8X16 }, { TX_8X32, TX_8X16 } },
      { { TX_32X8, TX_32X8 }, { TX_32X8, TX_32X8 } },
  },
#endif
};

// Generates 4 bit field in which each bit set to 1 represents
// a blocksize partition  1111 means we split 64x64, 32x32, 16x16
// and 8x8.  1000 means we just split the 64x64 to 32x32
/* clang-format off */
static const struct {
  PARTITION_CONTEXT above;
  PARTITION_CONTEXT left;
} partition_context_lookup[BLOCK_SIZES_ALL] = {
#if CONFIG_EXT_PARTITION
  { 31, 31 },  // 2X2   - {0b11111, 0b11111}
  { 31, 31 },  // 2X4   - {0b11111, 0b11111}
  { 31, 31 },  // 4X2   - {0b11111, 0b11111}
  { 31, 31 },  // 4X4   - {0b11111, 0b11111}
  { 31, 30 },  // 4X8   - {0b11111, 0b11110}
  { 30, 31 },  // 8X4   - {0b11110, 0b11111}
  { 30, 30 },  // 8X8   - {0b11110, 0b11110}
  { 30, 28 },  // 8X16  - {0b11110, 0b11100}
  { 28, 30 },  // 16X8  - {0b11100, 0b11110}
  { 28, 28 },  // 16X16 - {0b11100, 0b11100}
  { 28, 24 },  // 16X32 - {0b11100, 0b11000}
  { 24, 28 },  // 32X16 - {0b11000, 0b11100}
  { 24, 24 },  // 32X32 - {0b11000, 0b11000}
  { 24, 16 },  // 32X64 - {0b11000, 0b10000}
  { 16, 24 },  // 64X32 - {0b10000, 0b11000}
  { 16, 16 },  // 64X64 - {0b10000, 0b10000}
  { 16, 0 },   // 64X128- {0b10000, 0b00000}
  { 0, 16 },   // 128X64- {0b00000, 0b10000}
  { 0, 0 },    // 128X128-{0b00000, 0b00000}

  { 31, 28 },  // 4X16  - {0b11111, 0b11100}
  { 28, 31 },  // 16X4  - {0b11100, 0b11111}
  { 30, 24 },  // 8X32  - {0b11110, 0b11000}
  { 24, 30 },  // 32X8  - {0b11000, 0b11110}
  { 28, 16 },  // 16X64 - {0b11100, 0b10000}
  { 16, 28 },  // 64X16 - {0b10000, 0b11100}
  { 24, 0 },   // 32X128- {0b11000, 0b00000}
  { 0, 24 },   // 128X32- {0b00000, 0b11000}
#else
  { 15, 15 },  // 2X2   - {0b1111, 0b1111}
  { 15, 15 },  // 2X4   - {0b1111, 0b1111}
  { 15, 15 },  // 4X2   - {0b1111, 0b1111}
  { 15, 15 },  // 4X4   - {0b1111, 0b1111}
  { 15, 14 },  // 4X8   - {0b1111, 0b1110}
  { 14, 15 },  // 8X4   - {0b1110, 0b1111}
  { 14, 14 },  // 8X8   - {0b1110, 0b1110}
  { 14, 12 },  // 8X16  - {0b1110, 0b1100}
  { 12, 14 },  // 16X8  - {0b1100, 0b1110}
  { 12, 12 },  // 16X16 - {0b1100, 0b1100}
  { 12, 8 },   // 16X32 - {0b1100, 0b1000}
  { 8, 12 },   // 32X16 - {0b1000, 0b1100}
  { 8, 8 },    // 32X32 - {0b1000, 0b1000}
  { 8, 0 },    // 32X64 - {0b1000, 0b0000}
  { 0, 8 },    // 64X32 - {0b0000, 0b1000}
  { 0, 0 },    // 64X64 - {0b0000, 0b0000}

  { 15, 12 },  // 4X16 - {0b1111, 0b1100}
  { 12, 15 },  // 16X4 - {0b1100, 0b1111}
  { 8, 14 },   // 8X32 - {0b1110, 0b1000}
  { 14, 8 },   // 32X8 - {0b1000, 0b1110}
  { 12, 0 },   // 16X64- {0b1100, 0b0000}
  { 0, 12 },   // 64X16- {0b0000, 0b1100}
#endif  // CONFIG_EXT_PARTITION
};
/* clang-format on */

#if CONFIG_KF_CTX
static const int intra_mode_context[INTRA_MODES] = {
  0, 1, 2, 3, 4, 4, 4, 4, 3, 0, 1, 2, 0,
};
#endif

#if CONFIG_ADAPT_SCAN
#define EOB_THRESHOLD_NUM 2
#endif

#if CONFIG_JNT_COMP
// Note: this is also used in unit tests. So whenever one changes the table,
// the unit tests need to be changed accordingly.
static const double quant_dist_category[4] = { 1.5, 2.5, 3.5, 255 };
static const int quant_dist_lookup_table[2][4][2] = {
  { { 9, 7 }, { 11, 5 }, { 12, 4 }, { 13, 3 } },
  { { 7, 9 }, { 5, 11 }, { 4, 12 }, { 3, 13 } },
};
#endif  // CONFIG_JNT_COMP

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_COMMON_DATA_H_
