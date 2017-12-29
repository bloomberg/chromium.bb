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

#ifndef AV1_INV_TXFM2D_CFG_H_
#define AV1_INV_TXFM2D_CFG_H_
#include "av1/common/av1_inv_txfm1d.h"

// sum of fwd_shift_##
#if CONFIG_TX64X64
static const int8_t fwd_shift_sum[TX_SIZES] = { 2, 1, 0, -2, -4 };
#else  // CONFIG_TX64X64
static const int8_t fwd_shift_sum[TX_SIZES] = { 2, 1, 0, -2 };
#endif  // CONFIG_TX64X64

//  ---------------- 4x4 1D config -----------------------
// shift
static const int8_t inv_shift_4[2] = { 0, -4 };

// stage range
static const int8_t inv_stage_range_col_dct_4[4] = { 3, 3, 2, 2 };
static const int8_t inv_stage_range_row_dct_4[4] = { 3, 3, 3, 3 };
static const int8_t inv_stage_range_col_adst_4[6] = { 3, 3, 3, 3, 2, 2 };
static const int8_t inv_stage_range_row_adst_4[6] = { 3, 3, 3, 3, 3, 3 };
static const int8_t inv_stage_range_row_idx_4[1] = { 3 };
static const int8_t inv_stage_range_col_idx_4[1] = { 3 };

// cos bit
static const int8_t inv_cos_bit_col_dct_4[4] = { 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_dct_4[4] = { 13, 13, 13, 13 };
static const int8_t inv_cos_bit_col_adst_4[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_adst_4[6] = { 13, 13, 13, 13, 13, 13 };

//  ---------------- 8x8 1D constants -----------------------
// shift
static const int8_t inv_shift_8[2] = { 0, -5 };
// stage range
static const int8_t inv_stage_range_col_dct_8[6] = { 5, 5, 5, 5, 4, 4 };
static const int8_t inv_stage_range_row_dct_8[6] = { 5, 5, 5, 5, 5, 5 };
static const int8_t inv_stage_range_col_adst_8[8] = { 5, 5, 5, 5, 5, 5, 4, 4 };
static const int8_t inv_stage_range_row_adst_8[8] = { 5, 5, 5, 5, 5, 5, 5, 5 };
static const int8_t inv_stage_range_row_idx_8[1] = { 5 };
static const int8_t inv_stage_range_col_idx_8[1] = { 5 };

// cos bit
static const int8_t inv_cos_bit_col_dct_8[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_dct_8[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_col_adst_8[8] = {
  13, 13, 13, 13, 13, 13, 13, 13
};
static const int8_t inv_cos_bit_row_adst_8[8] = {
  13, 13, 13, 13, 13, 13, 13, 13
};

//  ---------------- 4x8 1D constants -----------------------
#define inv_shift_4x8 inv_shift_8
// stage range
static const int8_t inv_stage_range_col_dct_4x8[6] =
    ARRAYOFFSET6(-2, 5, 5, 5, 5, 5, 5);
static const int8_t inv_stage_range_col_adst_4x8[8] =
    ARRAYOFFSET8(-2, 5, 5, 5, 5, 5, 5, 5, 5);
// cos bit
static const int8_t inv_cos_bit_col_dct_4x8[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_col_adst_4x8[8] = { 13, 13, 13, 13,
                                                    13, 13, 13, 13 };

//  ---------------- 8x4 1D constants -----------------------
#define inv_shift_8x4 inv_shift_8
// stage range
static const int8_t inv_stage_range_col_dct_8x4[4] =
    ARRAYOFFSET4(2, 3, 3, 3, 3);
static const int8_t inv_stage_range_col_adst_8x4[6] =
    ARRAYOFFSET6(2, 3, 3, 3, 3, 3, 3);
// cos bit
static const int8_t inv_cos_bit_col_dct_8x4[4] = { 13, 13, 13, 13 };
static const int8_t inv_cos_bit_col_adst_8x4[6] = { 13, 13, 13, 13, 13, 13 };

//  ---------------- 16x16 1D constants -----------------------
// shift
static const int8_t inv_shift_16[2] = { -1, -5 };

// stage range
static const int8_t inv_stage_range_col_dct_16[8] = { 7, 7, 7, 7, 7, 7, 6, 6 };
static const int8_t inv_stage_range_row_dct_16[8] = { 7, 7, 7, 7, 7, 7, 7, 7 };
static const int8_t inv_stage_range_col_adst_16[10] = { 7, 7, 7, 7, 7,
                                                        7, 7, 7, 6, 6 };
static const int8_t inv_stage_range_row_adst_16[10] = { 7, 7, 7, 7, 7,
                                                        7, 7, 7, 7, 7 };
static const int8_t inv_stage_range_row_idx_16[1] = { 7 };
static const int8_t inv_stage_range_col_idx_16[1] = { 7 };

// cos bit
static const int8_t inv_cos_bit_col_dct_16[8] = {
  13, 13, 13, 13, 13, 13, 13, 13
};
static const int8_t inv_cos_bit_row_dct_16[8] = {
  12, 12, 12, 12, 12, 12, 12, 12
};
static const int8_t inv_cos_bit_col_adst_16[10] = { 13, 13, 13, 13, 13,
                                                    13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_adst_16[10] = { 12, 12, 12, 12, 12,
                                                    12, 12, 12, 12, 12 };

//  ---------------- 8x16 1D constants -----------------------
#define inv_shift_8x16 inv_shift_16
// stage range
static const int8_t inv_stage_range_col_dct_8x16[8] =
    ARRAYOFFSET8(-2, 7, 7, 7, 7, 7, 7, 7, 7);
static const int8_t inv_stage_range_col_adst_8x16[10] =
    ARRAYOFFSET10(-2, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7);
// cos bit
static const int8_t inv_cos_bit_col_dct_8x16[8] = { 13, 13, 13, 13,
                                                    13, 13, 13, 13 };
static const int8_t inv_cos_bit_col_adst_8x16[10] = { 13, 13, 13, 13, 13,
                                                      13, 13, 13, 13, 13 };

//  ---------------- 16x8 1D constants -----------------------
#define inv_shift_16x8 inv_shift_16
// stage range
static const int8_t inv_stage_range_col_dct_16x8[6] =
    ARRAYOFFSET6(2, 5, 5, 5, 5, 5, 5);
static const int8_t inv_stage_range_col_adst_16x8[8] =
    ARRAYOFFSET8(2, 5, 5, 5, 5, 5, 5, 5, 5);
// cos bit
static const int8_t inv_cos_bit_col_dct_16x8[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_col_adst_16x8[8] = { 13, 13, 13, 13,
                                                     13, 13, 13, 13 };

//  ---------------- 32x32 1D constants -----------------------
// shift
static const int8_t inv_shift_32[2] = { -1, -5 };

// stage range
static const int8_t inv_stage_range_col_dct_32[10] = { 9, 9, 9, 9, 9,
                                                       9, 9, 9, 9, 9 };
static const int8_t inv_stage_range_row_dct_32[10] = { 9, 9, 9, 9, 9,
                                                       9, 9, 9, 9, 9 };
static const int8_t inv_stage_range_col_adst_32[12] = { 9, 9, 9, 9, 9, 9,
                                                        9, 9, 9, 9, 9, 9 };
static const int8_t inv_stage_range_row_adst_32[12] = { 9, 9, 9, 9, 9, 9,
                                                        9, 9, 9, 9, 9, 9 };
static const int8_t inv_stage_range_row_idx_32[1] = { 9 };
static const int8_t inv_stage_range_col_idx_32[1] = { 9 };

// cos bit
static const int8_t inv_cos_bit_col_dct_32[10] = { 13, 13, 13, 13, 13,
                                                   13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_dct_32[10] = { 12, 12, 12, 12, 12,
                                                   12, 12, 12, 12, 12 };
static const int8_t inv_cos_bit_col_adst_32[12] = { 13, 13, 13, 13, 13, 13,
                                                    13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_adst_32[12] = { 12, 12, 12, 12, 12, 12,
                                                    12, 12, 12, 12, 12, 12 };

//  ---------------- 16x32 1D constants -----------------------
#define inv_shift_16x32 inv_shift_32
// stage range
static const int8_t inv_stage_range_col_dct_16x32[10] =
    ARRAYOFFSET10(-2, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9);
static const int8_t inv_stage_range_col_adst_16x32[12] =
    ARRAYOFFSET12(-2, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9);
// cos bit
static const int8_t inv_cos_bit_col_dct_16x32[10] = { 13, 13, 13, 13, 13,
                                                      13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_col_adst_16x32[12] = { 13, 13, 13, 13, 13, 13,
                                                       13, 13, 13, 13, 13, 13 };

//  ---------------- 32x16 1D constants -----------------------
#define inv_shift_32x16 inv_shift_32
// stage range
static const int8_t inv_stage_range_col_dct_32x16[8] =
    ARRAYOFFSET8(2, 7, 7, 7, 7, 7, 7, 7, 7);
static const int8_t inv_stage_range_col_adst_32x16[10] =
    ARRAYOFFSET10(2, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7);
// cos bit
static const int8_t inv_cos_bit_col_dct_32x16[8] = { 13, 13, 13, 13,
                                                     13, 13, 13, 13 };
static const int8_t inv_cos_bit_col_adst_32x16[10] = { 13, 13, 13, 13, 13,
                                                       13, 13, 13, 13, 13 };

//  ---------------- 64x64 1D constants -----------------------
// shift
static const int8_t inv_shift_64[2] = { -1, -5 };

// stage range
static const int8_t inv_stage_range_col_dct_64[12] = { 11, 11, 11, 11, 11, 11,
                                                       11, 11, 11, 11, 11, 11 };
static const int8_t inv_stage_range_row_dct_64[12] = { 11, 11, 11, 11, 11, 11,
                                                       11, 11, 11, 11, 11, 11 };

static const int8_t inv_stage_range_row_idx_64[1] = { 11 };
static const int8_t inv_stage_range_col_idx_64[1] = { 11 };

// cos bit
static const int8_t inv_cos_bit_col_dct_64[12] = { 13, 13, 13, 13, 13, 13,
                                                   13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_row_dct_64[12] = { 12, 12, 12, 12, 12, 12,
                                                   12, 12, 12, 12, 12, 12 };

//  ---------------- 32x64 1D constants -----------------------
#define inv_shift_32x64 inv_shift_64
// stage range
static const int8_t inv_stage_range_col_dct_32x64[12] =
    ARRAYOFFSET12(-2, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11);
// cos bit
static const int8_t inv_cos_bit_col_dct_32x64[12] = { 13, 13, 13, 13, 13, 13,
                                                      13, 13, 13, 13, 13, 13 };

//  ---------------- 64x32 1D constants -----------------------
#define inv_shift_64x32 inv_shift_64
// stage range
static const int8_t inv_stage_range_col_dct_64x32[10] =
    ARRAYOFFSET10(2, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9);
// cos bit
static const int8_t inv_cos_bit_col_dct_64x32[10] = { 13, 13, 13, 13, 13,
                                                      13, 13, 13, 13, 13 };

//  ---------------- 4x16 1D constants -----------------------
#define inv_shift_4x16 inv_shift_16
// stage range
static const int8_t inv_stage_range_col_dct_4x16[8] =
    ARRAYOFFSET8(-4, 7, 7, 7, 7, 7, 7, 7, 7);
static const int8_t inv_stage_range_col_adst_4x16[10] =
    ARRAYOFFSET10(-4, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7);
// cos bit
static const int8_t inv_cos_bit_col_dct_4x16[8] = { 13, 13, 13, 13,
                                                    13, 13, 13, 13 };
static const int8_t inv_cos_bit_col_adst_4x16[10] = { 13, 13, 13, 13, 13,
                                                      13, 13, 13, 13, 13 };

//  ---------------- 16x4 1D constants -----------------------
#define inv_shift_16x4 inv_shift_16
// stage range
static const int8_t inv_stage_range_col_dct_16x4[4] =
    ARRAYOFFSET4(4, 3, 3, 3, 3);
static const int8_t inv_stage_range_col_adst_16x4[6] =
    ARRAYOFFSET6(4, 3, 3, 3, 3, 3, 3);
// cos bit
static const int8_t inv_cos_bit_col_dct_16x4[4] = { 13, 13, 13, 13 };
static const int8_t inv_cos_bit_col_adst_16x4[6] = { 13, 13, 13, 13, 13, 13 };

//  ---------------- 8x32 1D constants -----------------------
#define inv_shift_8x32 inv_shift_32
// stage range
static const int8_t inv_stage_range_col_dct_8x32[10] =
    ARRAYOFFSET10(-4, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9);
static const int8_t inv_stage_range_col_adst_8x32[12] =
    ARRAYOFFSET12(-4, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9);
// cos bit
static const int8_t inv_cos_bit_col_dct_8x32[10] = { 13, 13, 13, 13, 13,
                                                     13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_col_adst_8x32[12] = { 13, 13, 13, 13, 13, 13,
                                                      13, 13, 13, 13, 13, 13 };

//  ---------------- 32x8 1D constants -----------------------
#define inv_shift_32x8 inv_shift_32
// stage range
static const int8_t inv_stage_range_col_dct_32x8[6] =
    ARRAYOFFSET6(4, 5, 5, 5, 5, 5, 5);
static const int8_t inv_stage_range_col_adst_32x8[8] =
    ARRAYOFFSET8(4, 5, 5, 5, 5, 5, 5, 5, 5);
// cos bit
static const int8_t inv_cos_bit_col_dct_32x8[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t inv_cos_bit_col_adst_32x8[8] = { 13, 13, 13, 13,
                                                     13, 13, 13, 13 };

//  ---------------- 16x64 1D constants -----------------------
#define inv_shift_16x64 inv_shift_64
// stage range
static const int8_t inv_stage_range_col_dct_16x64[12] =
    ARRAYOFFSET12(-4, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11);
// cos bit
static const int8_t inv_cos_bit_col_dct_16x64[12] = { 13, 13, 13, 13, 13, 13,
                                                      13, 13, 13, 13, 13, 13 };

//  ---------------- 64x16 1D constants -----------------------
#define inv_shift_64x16 inv_shift_64
// stage range
static const int8_t inv_stage_range_col_dct_64x16[8] =
    ARRAYOFFSET8(4, 7, 7, 7, 7, 7, 7, 7, 7);
// cos bit
static const int8_t inv_cos_bit_col_dct_64x16[8] = { 13, 13, 13, 13,
                                                     13, 13, 13, 13 };

//  ---------------- row config inv_dct_4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_dct_4 = {
  4,                          // .txfm_size
  4,                          // .stage_num
  inv_shift_4,                // .shift
  inv_stage_range_row_dct_4,  // .stage_range
  inv_cos_bit_row_dct_4,      // .cos_bit
  TXFM_TYPE_DCT4              // .txfm_type
};

//  ---------------- row config inv_dct_8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_dct_8 = {
  8,                          // .txfm_size
  6,                          // .stage_num
  inv_shift_8,                // .shift
  inv_stage_range_row_dct_8,  // .stage_range
  inv_cos_bit_row_dct_8,      // .cos_bit_
  TXFM_TYPE_DCT8              // .txfm_type
};
//  ---------------- row config inv_dct_16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_dct_16 = {
  16,                          // .txfm_size
  8,                           // .stage_num
  inv_shift_16,                // .shift
  inv_stage_range_row_dct_16,  // .stage_range
  inv_cos_bit_row_dct_16,      // .cos_bit
  TXFM_TYPE_DCT16              // .txfm_type
};

//  ---------------- row config inv_dct_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_dct_32 = {
  32,                          // .txfm_size
  10,                          // .stage_num
  inv_shift_32,                // .shift
  inv_stage_range_row_dct_32,  // .stage_range
  inv_cos_bit_row_dct_32,      // .cos_bit_row
  TXFM_TYPE_DCT32              // .txfm_type
};

#if CONFIG_TX64X64
//  ---------------- row config inv_dct_64 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_dct_64 = {
  64,                          // .txfm_size
  12,                          // .stage_num
  inv_shift_64,                // .shift
  inv_stage_range_row_dct_64,  // .stage_range
  inv_cos_bit_row_dct_64,      // .cos_bit
  TXFM_TYPE_DCT64,             // .txfm_type_col
};
#endif  // CONFIG_TX64X64

//  ---------------- row config inv_adst_4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_adst_4 = {
  4,                           // .txfm_size
  6,                           // .stage_num
  inv_shift_4,                 // .shift
  inv_stage_range_row_adst_4,  // .stage_range
  inv_cos_bit_row_adst_4,      // .cos_bit
  TXFM_TYPE_ADST4,             // .txfm_type
};

//  ---------------- row config inv_adst_8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_adst_8 = {
  8,                           // .txfm_size
  8,                           // .stage_num
  inv_shift_8,                 // .shift
  inv_stage_range_row_adst_8,  // .stage_range
  inv_cos_bit_row_adst_8,      // .cos_bit
  TXFM_TYPE_ADST8,             // .txfm_type_col
};

//  ---------------- row config inv_adst_16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_adst_16 = {
  16,                           // .txfm_size
  10,                           // .stage_num
  inv_shift_16,                 // .shift
  inv_stage_range_row_adst_16,  // .stage_range
  inv_cos_bit_row_adst_16,      // .cos_bit
  TXFM_TYPE_ADST16,             // .txfm_type
};

//  ---------------- row config inv_adst_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_adst_32 = {
  32,                           // .txfm_size
  12,                           // .stage_num
  inv_shift_32,                 // .shift
  inv_stage_range_row_adst_32,  // .stage_range
  inv_cos_bit_row_adst_32,      // .cos_bit
  TXFM_TYPE_ADST32,             // .txfm_type
};

//  ---------------- col config inv_dct_4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_4 = {
  4,                          // .txfm_size
  4,                          // .stage_num
  inv_shift_4,                // .shift
  inv_stage_range_col_dct_4,  // .stage_range
  inv_cos_bit_col_dct_4,      // .cos_bit
  TXFM_TYPE_DCT4              // .txfm_type
};

//  ---------------- col config inv_dct_8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_8 = {
  8,                          // .txfm_size
  6,                          // .stage_num
  inv_shift_8,                // .shift
  inv_stage_range_col_dct_8,  // .stage_range
  inv_cos_bit_col_dct_8,      // .cos_bit_
  TXFM_TYPE_DCT8              // .txfm_type
};

//  ---------------- col config inv_dct_16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_16 = {
  16,                          // .txfm_size
  8,                           // .stage_num
  inv_shift_16,                // .shift
  inv_stage_range_col_dct_16,  // .stage_range
  inv_cos_bit_col_dct_16,      // .cos_bit
  TXFM_TYPE_DCT16              // .txfm_type
};

//  ---------------- col config inv_dct_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_32 = {
  32,                          // .txfm_size
  10,                          // .stage_num
  inv_shift_32,                // .shift
  inv_stage_range_col_dct_32,  // .stage_range
  inv_cos_bit_col_dct_32,      // .cos_bit_col
  TXFM_TYPE_DCT32              // .txfm_type
};

//  ---------------- col config inv_dct_64 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_64 = {
  64,                          // .txfm_size
  12,                          // .stage_num
  inv_shift_64,                // .shift
  inv_stage_range_col_dct_64,  // .stage_range
  inv_cos_bit_col_dct_64,      // .cos_bit
  TXFM_TYPE_DCT64,             // .txfm_type_col
};

//  ---------------- col config inv_adst_4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_4 = {
  4,                           // .txfm_size
  6,                           // .stage_num
  inv_shift_4,                 // .shift
  inv_stage_range_col_adst_4,  // .stage_range
  inv_cos_bit_col_adst_4,      // .cos_bit
  TXFM_TYPE_ADST4,             // .txfm_type
};

//  ---------------- col config inv_adst_8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_8 = {
  8,                           // .txfm_size
  8,                           // .stage_num
  inv_shift_8,                 // .shift
  inv_stage_range_col_adst_8,  // .stage_range
  inv_cos_bit_col_adst_8,      // .cos_bit
  TXFM_TYPE_ADST8,             // .txfm_type_col
};

//  ---------------- col config inv_adst_16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_16 = {
  16,                           // .txfm_size
  10,                           // .stage_num
  inv_shift_16,                 // .shift
  inv_stage_range_col_adst_16,  // .stage_range
  inv_cos_bit_col_adst_16,      // .cos_bit
  TXFM_TYPE_ADST16,             // .txfm_type
};

//  ---------------- col config inv_adst_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_32 = {
  32,                           // .txfm_size
  12,                           // .stage_num
  inv_shift_32,                 // .shift
  inv_stage_range_col_adst_32,  // .stage_range
  inv_cos_bit_col_adst_32,      // .cos_bit
  TXFM_TYPE_ADST32,             // .txfm_type
};

//  ---------------- col config inv_identity_4 ----------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_identity_4 = {
  4,                          // .txfm_size
  1,                          // .stage_num
  inv_shift_4,                // .shift
  inv_stage_range_col_idx_4,  // .stage_range
  NULL,                       // .cos_bit
  TXFM_TYPE_IDENTITY4,        // .txfm_type
};

//  ---------------- row config inv_identity_4 ----------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_identity_4 = {
  4,                          // .txfm_size
  1,                          // .stage_num
  inv_shift_4,                // .shift
  inv_stage_range_row_idx_4,  // .stage_range
  NULL,                       // .cos_bit
  TXFM_TYPE_IDENTITY4,        // .txfm_type
};

//  ---------------- col config inv_identity_8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_identity_8 = {
  8,                          // .txfm_size
  1,                          // .stage_num
  inv_shift_8,                // .shift
  inv_stage_range_col_idx_8,  // .stage_range
  NULL,                       // .cos_bit
  TXFM_TYPE_IDENTITY8,        // .txfm_type
};

//  ---------------- row config inv_identity_8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_identity_8 = {
  8,                          // .txfm_size
  1,                          // .stage_num
  inv_shift_8,                // .shift
  inv_stage_range_row_idx_8,  // .stage_range
  NULL,                       // .cos_bit
  TXFM_TYPE_IDENTITY8,        // .txfm_type
};

//  ---------------- col config inv_identity_16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_identity_16 = {
  16,                          // .txfm_size
  1,                           // .stage_num
  inv_shift_16,                // .shift
  inv_stage_range_col_idx_16,  // .stage_range
  NULL,                        // .cos_bit
  TXFM_TYPE_IDENTITY16,        // .txfm_type
};

//  ---------------- row config inv_identity_16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_identity_16 = {
  16,                          // .txfm_size
  1,                           // .stage_num
  inv_shift_16,                // .shift
  inv_stage_range_row_idx_16,  // .stage_range
  NULL,                        // .cos_bit
  TXFM_TYPE_IDENTITY16,        // .txfm_type
};

//  ---------------- col config inv_identity_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_identity_32 = {
  32,                          // .txfm_size
  1,                           // .stage_num
  inv_shift_32,                // .shift
  inv_stage_range_col_idx_32,  // .stage_range
  NULL,                        // .cos_bit
  TXFM_TYPE_IDENTITY32,        // .txfm_type
};

//  ---------------- row config inv_identity_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_identity_32 = {
  32,                          // .txfm_size
  1,                           // .stage_num
  inv_shift_32,                // .shift
  inv_stage_range_row_idx_32,  // .stage_range
  NULL,                        // .cos_bit
  TXFM_TYPE_IDENTITY32,        // .txfm_type
};

#if CONFIG_TX64X64
//  ---------------- col config inv_identity_64 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_identity_64 = {
  64,                          // .txfm_size
  1,                           // .stage_num
  inv_shift_64,                // .shift
  inv_stage_range_col_idx_64,  // .stage_range
  NULL,                        // .cos_bit
  TXFM_TYPE_IDENTITY64,        // .txfm_type
};

//  ---------------- row config inv_identity_64 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_identity_64 = {
  64,                          // .txfm_size
  1,                           // .stage_num
  inv_shift_64,                // .shift
  inv_stage_range_row_idx_64,  // .stage_range
  NULL,                        // .cos_bit
  TXFM_TYPE_IDENTITY64,        // .txfm_type
};
#endif  // CONFIG_TX64X64

//  ---------------- col config inv_dct_8x4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_8x4 = {
  4,                            // .txfm_size
  4,                            // .stage_num
  inv_shift_8x4,                // .shift
  inv_stage_range_col_dct_8x4,  // .stage_range
  inv_cos_bit_col_dct_8x4,      // .cos_bit
  TXFM_TYPE_DCT4                // .txfm_type
};

//  ---------------- col config inv_adst_8x4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_8x4 = {
  4,                             // .txfm_size
  6,                             // .stage_num
  inv_shift_8x4,                 // .shift
  inv_stage_range_col_adst_8x4,  // .stage_range
  inv_cos_bit_col_adst_8x4,      // .cos_bit
  TXFM_TYPE_ADST4,               // .txfm_type
};

//  ---------------- col config inv_dct_16x4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_16x4 = {
  4,                             // .txfm_size
  4,                             // .stage_num
  inv_shift_16x4,                // .shift
  inv_stage_range_col_dct_16x4,  // .stage_range
  inv_cos_bit_col_dct_16x4,      // .cos_bit
  TXFM_TYPE_DCT4                 // .txfm_type
};

//  ---------------- col config inv_adst_16x4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_16x4 = {
  4,                              // .txfm_size
  6,                              // .stage_num
  inv_shift_16x4,                 // .shift
  inv_stage_range_col_adst_16x4,  // .stage_range
  inv_cos_bit_col_adst_16x4,      // .cos_bit
  TXFM_TYPE_ADST4,                // .txfm_type
};

//  ---------------- col config inv_dct_4x8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_4x8 = {
  8,                            // .txfm_size
  6,                            // .stage_num
  inv_shift_4x8,                // .shift
  inv_stage_range_col_dct_4x8,  // .stage_range
  inv_cos_bit_col_dct_4x8,      // .cos_bit_
  TXFM_TYPE_DCT8                // .txfm_type
};

//  ---------------- col config inv_adst_16x8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_4x8 = {
  8,                             // .txfm_size
  8,                             // .stage_num
  inv_shift_4x8,                 // .shift
  inv_stage_range_col_adst_4x8,  // .stage_range
  inv_cos_bit_col_adst_4x8,      // .cos_bit
  TXFM_TYPE_ADST8,               // .txfm_type_col
};

//  ---------------- col config inv_dct_16x8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_16x8 = {
  8,                             // .txfm_size
  6,                             // .stage_num
  inv_shift_16x8,                // .shift
  inv_stage_range_col_dct_16x8,  // .stage_range
  inv_cos_bit_col_dct_16x8,      // .cos_bit_
  TXFM_TYPE_DCT8                 // .txfm_type
};

//  ---------------- col config inv_adst_16x8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_16x8 = {
  8,                              // .txfm_size
  8,                              // .stage_num
  inv_shift_16x8,                 // .shift
  inv_stage_range_col_adst_16x8,  // .stage_range
  inv_cos_bit_col_adst_16x8,      // .cos_bit
  TXFM_TYPE_ADST8,                // .txfm_type_col
};

//  ---------------- col config inv_dct_32x8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_32x8 = {
  8,                             // .txfm_size
  6,                             // .stage_num
  inv_shift_32x8,                // .shift
  inv_stage_range_col_dct_32x8,  // .stage_range
  inv_cos_bit_col_dct_32x8,      // .cos_bit_
  TXFM_TYPE_DCT8                 // .txfm_type
};

//  ---------------- col config inv_adst_32x8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_32x8 = {
  8,                              // .txfm_size
  8,                              // .stage_num
  inv_shift_32x8,                 // .shift
  inv_stage_range_col_adst_32x8,  // .stage_range
  inv_cos_bit_col_adst_32x8,      // .cos_bit
  TXFM_TYPE_ADST8,                // .txfm_type_col
};

//  ---------------- col config inv_dct_4x16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_4x16 = {
  16,                            // .txfm_size
  8,                             // .stage_num
  inv_shift_4x16,                // .shift
  inv_stage_range_col_dct_4x16,  // .stage_range
  inv_cos_bit_col_dct_4x16,      // .cos_bit
  TXFM_TYPE_DCT16                // .txfm_type
};

//  ---------------- col config inv_adst_4x16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_4x16 = {
  16,                             // .txfm_size
  10,                             // .stage_num
  inv_shift_4x16,                 // .shift
  inv_stage_range_col_adst_4x16,  // .stage_range
  inv_cos_bit_col_adst_4x16,      // .cos_bit
  TXFM_TYPE_ADST16,               // .txfm_type
};

//  ---------------- col config inv_dct_8x16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_8x16 = {
  16,                            // .txfm_size
  8,                             // .stage_num
  inv_shift_8x16,                // .shift
  inv_stage_range_col_dct_8x16,  // .stage_range
  inv_cos_bit_col_dct_8x16,      // .cos_bit
  TXFM_TYPE_DCT16                // .txfm_type
};

//  ---------------- col config inv_adst_8x16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_8x16 = {
  16,                             // .txfm_size
  10,                             // .stage_num
  inv_shift_8x16,                 // .shift
  inv_stage_range_col_adst_8x16,  // .stage_range
  inv_cos_bit_col_adst_8x16,      // .cos_bit
  TXFM_TYPE_ADST16,               // .txfm_type
};

//  ---------------- col config inv_dct_32x16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_32x16 = {
  16,                             // .txfm_size
  8,                              // .stage_num
  inv_shift_32x16,                // .shift
  inv_stage_range_col_dct_32x16,  // .stage_range
  inv_cos_bit_col_dct_32x16,      // .cos_bit
  TXFM_TYPE_DCT16                 // .txfm_type
};

//  ---------------- col config inv_adst_32x16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_32x16 = {
  16,                              // .txfm_size
  10,                              // .stage_num
  inv_shift_32x16,                 // .shift
  inv_stage_range_col_adst_32x16,  // .stage_range
  inv_cos_bit_col_adst_32x16,      // .cos_bit
  TXFM_TYPE_ADST16,                // .txfm_type
};

#if CONFIG_TX64X64
//  ---------------- col config inv_dct_64x16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_64x16 = {
  16,                             // .txfm_size
  8,                              // .stage_num
  inv_shift_64x16,                // .shift
  inv_stage_range_col_dct_64x16,  // .stage_range
  inv_cos_bit_col_dct_64x16,      // .cos_bit
  TXFM_TYPE_DCT16                 // .txfm_type
};
#endif  // CONFIG_TX64X64

//  ---------------- col config inv_dct_8x32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_8x32 = {
  32,                            // .txfm_size
  10,                            // .stage_num
  inv_shift_8x32,                // .shift
  inv_stage_range_col_dct_8x32,  // .stage_range
  inv_cos_bit_col_dct_8x32,      // .cos_bit_col
  TXFM_TYPE_DCT32                // .txfm_type
};

//  ---------------- col config inv_adst_8x32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_8x32 = {
  32,                             // .txfm_size
  12,                             // .stage_num
  inv_shift_8x32,                 // .shift
  inv_stage_range_col_adst_8x32,  // .stage_range
  inv_cos_bit_col_adst_8x32,      // .cos_bit
  TXFM_TYPE_ADST32,               // .txfm_type
};

//  ---------------- col config inv_dct_16x32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_16x32 = {
  32,                             // .txfm_size
  10,                             // .stage_num
  inv_shift_16x32,                // .shift
  inv_stage_range_col_dct_16x32,  // .stage_range
  inv_cos_bit_col_dct_16x32,      // .cos_bit_col
  TXFM_TYPE_DCT32                 // .txfm_type
};

//  ---------------- col config inv_adst_16x32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_16x32 = {
  32,                              // .txfm_size
  12,                              // .stage_num
  inv_shift_16x32,                 // .shift
  inv_stage_range_col_adst_16x32,  // .stage_range
  inv_cos_bit_col_adst_16x32,      // .cos_bit
  TXFM_TYPE_ADST32,                // .txfm_type
};

#if CONFIG_TX64X64
//  ---------------- col config inv_dct_64x32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_64x32 = {
  32,                             // .txfm_size
  10,                             // .stage_num
  inv_shift_64x32,                // .shift
  inv_stage_range_col_dct_64x32,  // .stage_range
  inv_cos_bit_col_dct_64x32,      // .cos_bit_col
  TXFM_TYPE_DCT32                 // .txfm_type
};

//  ---------------- col config inv_dct_16x64 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_16x64 = {
  64,                             // .txfm_size
  12,                             // .stage_num
  inv_shift_16x64,                // .shift
  inv_stage_range_col_dct_16x64,  // .stage_range
  inv_cos_bit_col_dct_16x64,      // .cos_bit
  TXFM_TYPE_DCT64,                // .txfm_type_col
};

//  ---------------- col config inv_dct_32x64 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_32x64 = {
  64,                             // .txfm_size
  12,                             // .stage_num
  inv_shift_32x64,                // .shift
  inv_stage_range_col_dct_32x64,  // .stage_range
  inv_cos_bit_col_dct_32x64,      // .cos_bit
  TXFM_TYPE_DCT64,                // .txfm_type_col
};
#endif  // CONFIG_TX64X64
#endif  // AV1_INV_TXFM2D_CFG_H_
