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

#ifndef AV1_FWD_TXFM2D_CFG_H_
#define AV1_FWD_TXFM2D_CFG_H_
#include "av1/common/enums.h"
#include "av1/encoder/av1_fwd_txfm1d.h"

//  ---------------- 4x4 1D constants -----------------------
// shift
static const int8_t fwd_shift_4[3] = { 2, 0, 0 };

// stage range
static const int8_t fwd_stage_range_col_dct_4[4] = { 0, 1, 2, 2 };
static const int8_t fwd_stage_range_row_dct_4[4] = { 2, 3, 3, 3 };
static const int8_t fwd_stage_range_col_adst_4[6] = { 0, 0, 1, 2, 2, 2 };
static const int8_t fwd_stage_range_row_adst_4[6] = { 2, 2, 2, 3, 3, 3 };
static const int8_t fwd_stage_range_idx_4[1] = { 0 };

// cos bit
static const int8_t fwd_cos_bit_col_dct_4[4] = { 13, 13, 13, 13 };
static const int8_t fwd_cos_bit_row_dct_4[4] = { 13, 13, 13, 13 };
static const int8_t fwd_cos_bit_col_adst_4[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t fwd_cos_bit_row_adst_4[6] = { 13, 13, 13, 13, 13, 13 };

//  ---------------- 8x8 1D constants -----------------------
// shift
static const int8_t fwd_shift_8[3] = { 2, -1, 0 };

// stage range
static const int8_t fwd_stage_range_col_dct_8[6] = { 0, 1, 2, 3, 3, 3 };
static const int8_t fwd_stage_range_row_dct_8[6] = { 3, 4, 5, 5, 5, 5 };
static const int8_t fwd_stage_range_col_adst_8[8] = { 0, 0, 1, 2, 2, 3, 3, 3 };
static const int8_t fwd_stage_range_row_adst_8[8] = { 3, 3, 3, 4, 4, 5, 5, 5 };
static const int8_t fwd_stage_range_idx_8[1] = { 0 };

// cos bit
static const int8_t fwd_cos_bit_col_dct_8[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t fwd_cos_bit_row_dct_8[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t fwd_cos_bit_col_adst_8[8] = {
  13, 13, 13, 13, 13, 13, 13, 13
};
static const int8_t fwd_cos_bit_row_adst_8[8] = {
  13, 13, 13, 13, 13, 13, 13, 13
};

//  ---------------- 16x16 1D constants -----------------------
// shift
static const int8_t fwd_shift_16[3] = { 2, -2, 0 };

// stage range
static const int8_t fwd_stage_range_col_dct_16[8] = { 0, 1, 2, 3, 4, 4, 4, 4 };
static const int8_t fwd_stage_range_row_dct_16[8] = { 4, 5, 6, 7, 7, 7, 7, 7 };
static const int8_t fwd_stage_range_col_adst_16[10] = { 0, 0, 1, 2, 2,
                                                        3, 3, 4, 4, 4 };
static const int8_t fwd_stage_range_row_adst_16[10] = {
  4, 4, 4, 5, 5, 6, 6, 7, 7, 7,
};
static const int8_t fwd_stage_range_idx_16[1] = { 0 };

// cos bit
static const int8_t fwd_cos_bit_col_dct_16[8] = {
  13, 13, 13, 13, 13, 13, 13, 13
};
static const int8_t fwd_cos_bit_row_dct_16[8] = {
  12, 12, 12, 12, 12, 12, 12, 12
};
static const int8_t fwd_cos_bit_col_adst_16[10] = { 13, 13, 13, 13, 13,
                                                    13, 13, 13, 13, 13 };
static const int8_t fwd_cos_bit_row_adst_16[10] = { 12, 12, 12, 12, 12,
                                                    12, 12, 12, 12, 12 };

//  ---------------- 32x32 1D constants -----------------------
// shift
static const int8_t fwd_shift_32[3] = { 2, -4, 0 };

// stage range
static const int8_t fwd_stage_range_col_dct_32[10] = { 0, 1, 2, 3, 4,
                                                       5, 5, 5, 5, 5 };
static const int8_t fwd_stage_range_row_dct_32[10] = { 5, 6, 7, 8, 9,
                                                       9, 9, 9, 9, 9 };
static const int8_t fwd_stage_range_col_adst_32[12] = { 0, 0, 1, 2, 2, 3,
                                                        3, 4, 4, 5, 5, 5 };
static const int8_t fwd_stage_range_row_adst_32[12] = { 5, 5, 5, 6, 6, 7,
                                                        7, 8, 8, 9, 9, 9 };
static const int8_t fwd_stage_range_idx_32[1] = { 0 };

// cos bit
static const int8_t fwd_cos_bit_col_dct_32[10] = { 12, 12, 12, 12, 12,
                                                   12, 12, 12, 12, 12 };
static const int8_t fwd_cos_bit_row_dct_32[10] = { 12, 12, 12, 12, 12,
                                                   12, 12, 12, 12, 12 };
static const int8_t fwd_cos_bit_col_adst_32[12] = { 12, 12, 12, 12, 12, 12,
                                                    12, 12, 12, 12, 12, 12 };
static const int8_t fwd_cos_bit_row_adst_32[12] = { 12, 12, 12, 12, 12, 12,
                                                    12, 12, 12, 12, 12, 12 };

//  ---------------- 64x64 1D constants -----------------------
// shift
static const int8_t fwd_shift_64[3] = { 0, -2, -2 };

// stage range
static const int8_t fwd_stage_range_col_dct_64[12] = { 0, 1, 2, 3, 4, 5,
                                                       6, 6, 6, 6, 6, 6 };
static const int8_t fwd_stage_range_row_dct_64[12] = { 6,  7,  8,  9,  10, 11,
                                                       11, 11, 11, 11, 11, 11 };
static const int8_t fwd_stage_range_idx_64[1] = { 0 };

// cos bit
static const int8_t fwd_cos_bit_col_dct_64[12] = { 13, 13, 13, 13, 13, 13,
                                                   13, 13, 13, 13, 13, 13 };
static const int8_t fwd_cos_bit_row_dct_64[12] = { 13, 13, 12, 11, 10, 10,
                                                   10, 10, 10, 10, 10, 10 };

//  ---------------- 4x8 1D constants -----------------------
#define fwd_shift_4x8 fwd_shift_8
static const int8_t fwd_stage_range_row_dct_4x8[4] =
    ARRAYOFFSET4(3, 0, 1, 2, 2);
static const int8_t fwd_stage_range_row_adst_4x8[6] =
    ARRAYOFFSET6(3, 0, 0, 1, 2, 2, 2);
static const int8_t fwd_cos_bit_row_dct_4x8[6] = { 13, 13, 13, 13 };
static const int8_t fwd_cos_bit_row_adst_4x8[6] = { 13, 13, 13, 13, 13, 13 };

//  ---------------- 8x4 1D constants -----------------------
#define fwd_shift_8x4 fwd_shift_8
static const int8_t fwd_stage_range_row_dct_8x4[6] =
    ARRAYOFFSET6(2, 0, 1, 2, 3, 3, 3);
static const int8_t fwd_stage_range_row_adst_8x4[8] =
    ARRAYOFFSET8(2, 0, 0, 1, 2, 2, 3, 3, 3);
static const int8_t fwd_cos_bit_row_dct_8x4[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t fwd_cos_bit_row_adst_8x4[8] = { 13, 13, 13, 13,
                                                    13, 13, 13, 13 };

//  ---------------- 8x16 1D constants -----------------------
#define fwd_shift_8x16 fwd_shift_16
static const int8_t fwd_stage_range_row_dct_8x16[6] =
    ARRAYOFFSET6(4, 0, 1, 2, 3, 3, 3);
static const int8_t fwd_stage_range_row_adst_8x16[8] =
    ARRAYOFFSET8(4, 0, 0, 1, 2, 2, 3, 3, 3);
static const int8_t fwd_cos_bit_row_dct_8x16[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t fwd_cos_bit_row_adst_8x16[8] = { 13, 13, 13, 13,
                                                     13, 13, 13, 13 };

//  ---------------- 8x16 1D constants -----------------------
#define fwd_shift_16x8 fwd_shift_16
static const int8_t fwd_stage_range_row_dct_16x8[8] =
    ARRAYOFFSET8(3, 0, 1, 2, 3, 4, 4, 4, 4);
static const int8_t fwd_stage_range_row_adst_16x8[10] =
    ARRAYOFFSET10(3, 0, 0, 1, 2, 2, 3, 3, 4, 4, 4);
static const int8_t fwd_cos_bit_row_dct_16x8[8] = { 13, 13, 13, 13,
                                                    13, 13, 13, 13 };
static const int8_t fwd_cos_bit_row_adst_16x8[10] = { 13, 13, 13, 13, 13,
                                                      13, 13, 13, 13, 13 };

//  ---------------- 16x32 1D constants -----------------------
#define fwd_shift_16x32 fwd_shift_32
static const int8_t fwd_stage_range_row_dct_16x32[8] =
    ARRAYOFFSET8(5, 0, 1, 2, 3, 4, 4, 4, 4);
static const int8_t fwd_stage_range_row_adst_16x32[10] =
    ARRAYOFFSET10(5, 0, 0, 1, 2, 2, 3, 3, 4, 4, 4);
static const int8_t fwd_cos_bit_row_dct_16x32[8] = { 13, 13, 13, 13,
                                                     13, 13, 13, 13 };
static const int8_t fwd_cos_bit_row_adst_16x32[10] = { 12, 12, 12, 12, 12,
                                                       12, 12, 12, 12, 12 };

//  ---------------- 32x16 1D constants -----------------------
#define fwd_shift_32x16 fwd_shift_32
static const int8_t fwd_stage_range_row_dct_32x16[10] =
    ARRAYOFFSET10(4, 0, 1, 2, 3, 4, 5, 5, 5, 5, 5);
static const int8_t fwd_stage_range_row_adst_32x16[12] =
    ARRAYOFFSET12(4, 0, 0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 5);
static const int8_t fwd_cos_bit_row_dct_32x16[10] = { 12, 12, 12, 12, 12,
                                                      12, 12, 12, 12, 12 };
static const int8_t fwd_cos_bit_row_adst_32x16[12] = { 12, 12, 12, 12, 12, 12,
                                                       12, 12, 12, 12, 12, 12 };

//  ---------------- 32x64 1D constants -----------------------
#define fwd_shift_32x64 fwd_shift_64
static const int8_t fwd_stage_range_row_dct_32x64[10] =
    ARRAYOFFSET10(6, 0, 1, 2, 3, 4, 5, 5, 5, 5, 5);
static const int8_t fwd_cos_bit_row_dct_32x64[10] = { 12, 12, 12, 12, 12,
                                                      12, 12, 12, 12, 12 };

//  ---------------- 64x32 1D constants -----------------------
#define fwd_shift_64x32 fwd_shift_64
static const int8_t fwd_stage_range_row_dct_64x32[12] =
    ARRAYOFFSET12(5, 0, 1, 2, 3, 4, 5, 6, 6, 6, 6, 6, 6);
static const int8_t fwd_cos_bit_row_dct_64x32[12] = { 13, 13, 12, 11, 10, 10,
                                                      10, 10, 10, 10, 10, 10 };

//  ---------------- 4x16 1D constants -----------------------
#define fwd_shift_4x16 fwd_shift_16
static const int8_t fwd_stage_range_row_dct_4x16[4] =
    ARRAYOFFSET4(4, 0, 1, 2, 2);
static const int8_t fwd_stage_range_row_adst_4x16[6] =
    ARRAYOFFSET6(4, 0, 0, 1, 2, 2, 2);
static const int8_t fwd_cos_bit_row_dct_4x16[6] = { 13, 13, 13, 13 };
static const int8_t fwd_cos_bit_row_adst_4x16[6] = { 13, 13, 13, 13, 13, 13 };

//  ---------------- 16x4 1D constants -----------------------
#define fwd_shift_16x4 fwd_shift_16
static const int8_t fwd_stage_range_row_dct_16x4[8] =
    ARRAYOFFSET8(2, 0, 1, 2, 3, 4, 4, 4, 4);
static const int8_t fwd_stage_range_row_adst_16x4[10] =
    ARRAYOFFSET10(2, 0, 0, 1, 2, 2, 3, 3, 4, 4, 4);
static const int8_t fwd_cos_bit_row_dct_16x4[8] = { 13, 13, 13, 13,
                                                    13, 13, 13, 13 };
static const int8_t fwd_cos_bit_row_adst_16x4[10] = { 13, 13, 13, 13, 13,
                                                      13, 13, 13, 13, 13 };

//  ---------------- 8x32 1D constants -----------------------
#define fwd_shift_8x32 fwd_shift_32
static const int8_t fwd_stage_range_row_dct_8x32[6] =
    ARRAYOFFSET6(5, 0, 1, 2, 3, 3, 3);
static const int8_t fwd_stage_range_row_adst_8x32[8] =
    ARRAYOFFSET8(5, 0, 0, 1, 2, 2, 3, 3, 3);
static const int8_t fwd_cos_bit_row_dct_8x32[6] = { 13, 13, 13, 13, 13, 13 };
static const int8_t fwd_cos_bit_row_adst_8x32[8] = { 13, 13, 13, 13,
                                                     13, 13, 13, 13 };

//  ---------------- 32x8 1D constants -----------------------
#define fwd_shift_32x8 fwd_shift_32
static const int8_t fwd_stage_range_row_dct_32x8[10] =
    ARRAYOFFSET10(3, 0, 1, 2, 3, 4, 5, 5, 5, 5, 5);
static const int8_t fwd_stage_range_row_adst_32x8[12] =
    ARRAYOFFSET12(3, 0, 0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 5);
static const int8_t fwd_cos_bit_row_dct_32x8[10] = { 12, 12, 12, 12, 12,
                                                     12, 12, 12, 12, 12 };
static const int8_t fwd_cos_bit_row_adst_32x8[12] = { 12, 12, 12, 12, 12, 12,
                                                      12, 12, 12, 12, 12, 12 };

//  ---------------- 16x64 1D constants -----------------------
#define fwd_shift_16x64 fwd_shift_64
static const int8_t fwd_stage_range_row_dct_16x64[8] =
    ARRAYOFFSET8(6, 0, 1, 2, 3, 4, 4, 4, 4);
static const int8_t fwd_cos_bit_row_dct_16x64[8] = { 13, 13, 13, 13,
                                                     13, 13, 13, 13 };

//  ---------------- 64x16 1D constants -----------------------
#define fwd_shift_64x16 fwd_shift_64
static const int8_t fwd_stage_range_row_dct_64x16[12] =
    ARRAYOFFSET12(4, 0, 1, 2, 3, 4, 5, 6, 6, 6, 6, 6, 6);
static const int8_t fwd_cos_bit_row_dct_64x16[12] = { 13, 13, 12, 11, 10, 10,
                                                      10, 10, 10, 10, 10, 10 };

//
//  ---------------- row config fwd_dct_4 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_4 = {
  4,  // .txfm_size
  4,  // .stage_num
  // 0,  // .log_scale
  fwd_shift_4,                // .shift
  fwd_stage_range_row_dct_4,  // .stage_range
  fwd_cos_bit_row_dct_4,      // .cos_bit
  TXFM_TYPE_DCT4              // .txfm_type
};

//  ---------------- row config fwd_dct_8 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_8 = {
  8,  // .txfm_size
  6,  // .stage_num
  // 0,  // .log_scale
  fwd_shift_8,                // .shift
  fwd_stage_range_row_dct_8,  // .stage_range
  fwd_cos_bit_row_dct_8,      // .cos_bit_
  TXFM_TYPE_DCT8              // .txfm_type
};
//  ---------------- row config fwd_dct_16 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_16 = {
  16,  // .txfm_size
  8,   // .stage_num
  // 0,  // .log_scale
  fwd_shift_16,                // .shift
  fwd_stage_range_row_dct_16,  // .stage_range
  fwd_cos_bit_row_dct_16,      // .cos_bit
  TXFM_TYPE_DCT16              // .txfm_type
};

//  ---------------- row config fwd_dct_32 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_32 = {
  32,  // .txfm_size
  10,  // .stage_num
  // 1,  // .log_scale
  fwd_shift_32,                // .shift
  fwd_stage_range_row_dct_32,  // .stage_range
  fwd_cos_bit_row_dct_32,      // .cos_bit_row
  TXFM_TYPE_DCT32              // .txfm_type
};

//  ---------------- row config fwd_dct_64 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_64 = {
  64,                          // .txfm_size
  12,                          // .stage_num
  fwd_shift_64,                // .shift
  fwd_stage_range_row_dct_64,  // .stage_range
  fwd_cos_bit_row_dct_64,      // .cos_bit
  TXFM_TYPE_DCT64,             // .txfm_type_col
};

//  ---------------- row config fwd_adst_4 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_adst_4 = {
  4,  // .txfm_size
  6,  // .stage_num
  // 0,  // .log_scale
  fwd_shift_4,                 // .shift
  fwd_stage_range_row_adst_4,  // .stage_range
  fwd_cos_bit_row_adst_4,      // .cos_bit
  TXFM_TYPE_ADST4,             // .txfm_type
};

//  ---------------- row config fwd_adst_8 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_adst_8 = {
  8,  // .txfm_size
  8,  // .stage_num
  // 0,  // .log_scale
  fwd_shift_8,                 // .shift
  fwd_stage_range_row_adst_8,  // .stage_range
  fwd_cos_bit_row_adst_8,      // .cos_bit
  TXFM_TYPE_ADST8,             // .txfm_type_col
};

//  ---------------- row config fwd_adst_16 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_adst_16 = {
  16,  // .txfm_size
  10,  // .stage_num
  // 0,  // .log_scale
  fwd_shift_16,                 // .shift
  fwd_stage_range_row_adst_16,  // .stage_range
  fwd_cos_bit_row_adst_16,      // .cos_bit
  TXFM_TYPE_ADST16,             // .txfm_type
};

//  ---------------- row config fwd_adst_32 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_adst_32 = {
  32,  // .txfm_size
  12,  // .stage_num
  // 1,  // .log_scale
  fwd_shift_32,                 // .shift
  fwd_stage_range_row_adst_32,  // .stage_range
  fwd_cos_bit_row_adst_32,      // .cos_bit
  TXFM_TYPE_ADST32,             // .txfm_type
};

//  ---------------- col config fwd_dct_4 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_col_cfg_dct_4 = {
  4,  // .txfm_size
  4,  // .stage_num
  // 0,  // .log_scale
  fwd_shift_4,                // .shift
  fwd_stage_range_col_dct_4,  // .stage_range
  fwd_cos_bit_col_dct_4,      // .cos_bit
  TXFM_TYPE_DCT4              // .txfm_type
};

//  ---------------- col config fwd_dct_8 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_col_cfg_dct_8 = {
  8,  // .txfm_size
  6,  // .stage_num
  // 0,  // .log_scale
  fwd_shift_8,                // .shift
  fwd_stage_range_col_dct_8,  // .stage_range
  fwd_cos_bit_col_dct_8,      // .cos_bit_
  TXFM_TYPE_DCT8              // .txfm_type
};
//  ---------------- col config fwd_dct_16 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_col_cfg_dct_16 = {
  16,  // .txfm_size
  8,   // .stage_num
  // 0,  // .log_scale
  fwd_shift_16,                // .shift
  fwd_stage_range_col_dct_16,  // .stage_range
  fwd_cos_bit_col_dct_16,      // .cos_bit
  TXFM_TYPE_DCT16              // .txfm_type
};

//  ---------------- col config fwd_dct_32 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_col_cfg_dct_32 = {
  32,  // .txfm_size
  10,  // .stage_num
  // 1,  // .log_scale
  fwd_shift_32,                // .shift
  fwd_stage_range_col_dct_32,  // .stage_range
  fwd_cos_bit_col_dct_32,      // .cos_bit_col
  TXFM_TYPE_DCT32              // .txfm_type
};

//  ---------------- col config fwd_dct_64 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_col_cfg_dct_64 = {
  64,                          // .txfm_size
  12,                          // .stage_num
  fwd_shift_64,                // .shift
  fwd_stage_range_col_dct_64,  // .stage_range
  fwd_cos_bit_col_dct_64,      // .cos_bit
  TXFM_TYPE_DCT64,             // .txfm_type_col
};

//  ---------------- col config fwd_adst_4 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_col_cfg_adst_4 = {
  4,                           // .txfm_size
  6,                           // .stage_num
  fwd_shift_4,                 // .shift
  fwd_stage_range_col_adst_4,  // .stage_range
  fwd_cos_bit_col_adst_4,      // .cos_bit
  TXFM_TYPE_ADST4,             // .txfm_type
};

//  ---------------- col config fwd_adst_8 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_col_cfg_adst_8 = {
  8,                           // .txfm_size
  8,                           // .stage_num
  fwd_shift_8,                 // .shift
  fwd_stage_range_col_adst_8,  // .stage_range
  fwd_cos_bit_col_adst_8,      // .cos_bit
  TXFM_TYPE_ADST8,             // .txfm_type_col
};

//  ---------------- col config fwd_adst_16 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_col_cfg_adst_16 = {
  16,                           // .txfm_size
  10,                           // .stage_num
  fwd_shift_16,                 // .shift
  fwd_stage_range_col_adst_16,  // .stage_range
  fwd_cos_bit_col_adst_16,      // .cos_bit
  TXFM_TYPE_ADST16,             // .txfm_type
};

//  ---------------- col config fwd_adst_32 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_col_cfg_adst_32 = {
  32,                           // .txfm_size
  12,                           // .stage_num
  fwd_shift_32,                 // .shift
  fwd_stage_range_col_adst_32,  // .stage_range
  fwd_cos_bit_col_adst_32,      // .cos_bit
  TXFM_TYPE_ADST32,             // .txfm_type
};

// identity does not need to differentiate between row and col
//  ---------------- row/col config fwd_identity_4 ----------
static const TXFM_1D_CFG fwd_txfm_1d_cfg_identity_4 = {
  4,                      // .txfm_size
  1,                      // .stage_num
  fwd_shift_4,            // .shift
  fwd_stage_range_idx_4,  // .stage_range
  NULL,                   // .cos_bit
  TXFM_TYPE_IDENTITY4,    // .txfm_type
};

//  ---------------- row/col config fwd_identity_8 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_cfg_identity_8 = {
  8,                      // .txfm_size
  1,                      // .stage_num
  fwd_shift_8,            // .shift
  fwd_stage_range_idx_8,  // .stage_range
  NULL,                   // .cos_bit
  TXFM_TYPE_IDENTITY8,    // .txfm_type
};

//  ---------------- row/col config fwd_identity_16 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_cfg_identity_16 = {
  16,                      // .txfm_size
  1,                       // .stage_num
  fwd_shift_16,            // .shift
  fwd_stage_range_idx_16,  // .stage_range
  NULL,                    // .cos_bit
  TXFM_TYPE_IDENTITY16,    // .txfm_type
};

//  ---------------- row/col config fwd_identity_32 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_cfg_identity_32 = {
  32,                      // .txfm_size
  1,                       // .stage_num
  fwd_shift_32,            // .shift
  fwd_stage_range_idx_32,  // .stage_range
  NULL,                    // .cos_bit
  TXFM_TYPE_IDENTITY32,    // .txfm_type
};

#if CONFIG_TX64X64
//  ---------------- row/col config fwd_identity_64 ----------------
static const TXFM_1D_CFG fwd_txfm_1d_cfg_identity_64 = {
  64,                      // .txfm_size
  1,                       // .stage_num
  fwd_shift_64,            // .shift
  fwd_stage_range_idx_64,  // .stage_range
  NULL,                    // .cos_bit
  TXFM_TYPE_IDENTITY64,    // .txfm_type
};
#endif  // CONFIG_TX64X64

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_4x8 = {
  4,                            // .txfm_size
  4,                            // .stage_num
  fwd_shift_4x8,                // .shift
  fwd_stage_range_row_dct_4x8,  // .stage_range
  fwd_cos_bit_row_dct_4x8,      // .cos_bit
  TXFM_TYPE_DCT4                // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_adst_4x8 = {
  4,                             // .txfm_size
  6,                             // .stage_num
  fwd_shift_4x8,                 // .shift
  fwd_stage_range_row_adst_4x8,  // .stage_range
  fwd_cos_bit_row_adst_4x8,      // .cos_bit
  TXFM_TYPE_ADST4                // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_8x4 = {
  8,                            // .txfm_size
  6,                            // .stage_num
  fwd_shift_8x4,                // .shift
  fwd_stage_range_row_dct_8x4,  // .stage_range
  fwd_cos_bit_row_dct_8x4,      // .cos_bit
  TXFM_TYPE_DCT8                // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_adst_8x4 = {
  8,                             // .txfm_size
  8,                             // .stage_num
  fwd_shift_8x4,                 // .shift
  fwd_stage_range_row_adst_8x4,  // .stage_range
  fwd_cos_bit_row_adst_8x4,      // .cos_bit
  TXFM_TYPE_ADST8                // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_8x16 = {
  8,                             // .txfm_size
  6,                             // .stage_num
  fwd_shift_8x16,                // .shift
  fwd_stage_range_row_dct_8x16,  // .stage_range
  fwd_cos_bit_row_dct_8x16,      // .cos_bit
  TXFM_TYPE_DCT8                 // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_adst_8x16 = {
  8,                              // .txfm_size
  8,                              // .stage_num
  fwd_shift_8x16,                 // .shift
  fwd_stage_range_row_adst_8x16,  // .stage_range
  fwd_cos_bit_row_adst_8x16,      // .cos_bit
  TXFM_TYPE_ADST8                 // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_16x8 = {
  16,                            // .txfm_size
  8,                             // .stage_num
  fwd_shift_16x8,                // .shift
  fwd_stage_range_row_dct_16x8,  // .stage_range
  fwd_cos_bit_row_dct_16x8,      // .cos_bit
  TXFM_TYPE_DCT16                // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_adst_16x8 = {
  16,                             // .txfm_size
  10,                             // .stage_num
  fwd_shift_16x8,                 // .shift
  fwd_stage_range_row_adst_16x8,  // .stage_range
  fwd_cos_bit_row_adst_16x8,      // .cos_bit
  TXFM_TYPE_ADST16                // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_16x32 = {
  16,                             // .txfm_size
  8,                              // .stage_num
  fwd_shift_16x32,                // .shift
  fwd_stage_range_row_dct_16x32,  // .stage_range
  fwd_cos_bit_row_dct_16x32,      // .cos_bit
  TXFM_TYPE_DCT16                 // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_adst_16x32 = {
  16,                              // .txfm_size
  10,                              // .stage_num
  fwd_shift_16x32,                 // .shift
  fwd_stage_range_row_adst_16x32,  // .stage_range
  fwd_cos_bit_row_adst_16x32,      // .cos_bit
  TXFM_TYPE_ADST16                 // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_32x16 = {
  32,                             // .txfm_size
  10,                             // .stage_num
  fwd_shift_32x16,                // .shift
  fwd_stage_range_row_dct_32x16,  // .stage_range
  fwd_cos_bit_row_dct_32x16,      // .cos_bit
  TXFM_TYPE_DCT32                 // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_adst_32x16 = {
  32,                              // .txfm_size
  12,                              // .stage_num
  fwd_shift_32x16,                 // .shift
  fwd_stage_range_row_adst_32x16,  // .stage_range
  fwd_cos_bit_row_adst_32x16,      // .cos_bit
  TXFM_TYPE_ADST32                 // .txfm_type
};

#if CONFIG_TX64X64
static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_32x64 = {
  32,                             // .txfm_size
  10,                             // .stage_num
  fwd_shift_32x64,                // .shift
  fwd_stage_range_row_dct_32x64,  // .stage_range
  fwd_cos_bit_row_dct_32x64,      // .cos_bit
  TXFM_TYPE_DCT32                 // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_64x32 = {
  64,                             // .txfm_size
  12,                             // .stage_num
  fwd_shift_64x32,                // .shift
  fwd_stage_range_row_dct_64x32,  // .stage_range
  fwd_cos_bit_row_dct_64x32,      // .cos_bit
  TXFM_TYPE_DCT64                 // .txfm_type
};
#endif  // CONFIG_TX64X64

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_4x16 = {
  4,                             // .txfm_size
  4,                             // .stage_num
  fwd_shift_4x16,                // .shift
  fwd_stage_range_row_dct_4x16,  // .stage_range
  fwd_cos_bit_row_dct_4x16,      // .cos_bit
  TXFM_TYPE_DCT4                 // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_adst_4x16 = {
  4,                              // .txfm_size
  6,                              // .stage_num
  fwd_shift_4x16,                 // .shift
  fwd_stage_range_row_adst_4x16,  // .stage_range
  fwd_cos_bit_row_adst_4x16,      // .cos_bit
  TXFM_TYPE_ADST4                 // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_16x4 = {
  16,                            // .txfm_size
  8,                             // .stage_num
  fwd_shift_16x4,                // .shift
  fwd_stage_range_row_dct_16x4,  // .stage_range
  fwd_cos_bit_row_dct_16x4,      // .cos_bit
  TXFM_TYPE_DCT16                // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_adst_16x4 = {
  16,                             // .txfm_size
  10,                             // .stage_num
  fwd_shift_16x4,                 // .shift
  fwd_stage_range_row_adst_16x4,  // .stage_range
  fwd_cos_bit_row_adst_16x4,      // .cos_bit
  TXFM_TYPE_ADST16                // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_8x32 = {
  8,                             // .txfm_size
  6,                             // .stage_num
  fwd_shift_8x32,                // .shift
  fwd_stage_range_row_dct_8x32,  // .stage_range
  fwd_cos_bit_row_dct_8x32,      // .cos_bit
  TXFM_TYPE_DCT8                 // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_adst_8x32 = {
  8,                              // .txfm_size
  8,                              // .stage_num
  fwd_shift_8x32,                 // .shift
  fwd_stage_range_row_adst_8x32,  // .stage_range
  fwd_cos_bit_row_adst_8x32,      // .cos_bit
  TXFM_TYPE_ADST8                 // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_32x8 = {
  32,                            // .txfm_size
  10,                            // .stage_num
  fwd_shift_32x8,                // .shift
  fwd_stage_range_row_dct_32x8,  // .stage_range
  fwd_cos_bit_row_dct_32x8,      // .cos_bit
  TXFM_TYPE_DCT32                // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_adst_32x8 = {
  32,                             // .txfm_size
  12,                             // .stage_num
  fwd_shift_32x8,                 // .shift
  fwd_stage_range_row_adst_32x8,  // .stage_range
  fwd_cos_bit_row_adst_32x8,      // .cos_bit
  TXFM_TYPE_ADST32                // .txfm_type
};

#if CONFIG_TX64X64
static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_16x64 = {
  16,                             // .txfm_size
  8,                              // .stage_num
  fwd_shift_16x64,                // .shift
  fwd_stage_range_row_dct_16x64,  // .stage_range
  fwd_cos_bit_row_dct_16x64,      // .cos_bit
  TXFM_TYPE_DCT16                 // .txfm_type
};

static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_64x16 = {
  64,                             // .txfm_size
  12,                             // .stage_num
  fwd_shift_64x16,                // .shift
  fwd_stage_range_row_dct_64x16,  // .stage_range
  fwd_cos_bit_row_dct_64x16,      // .cos_bit
  TXFM_TYPE_DCT64                 // .txfm_type
};
#endif  // CONFIG_TX64X64
#endif  // AV1_FWD_TXFM2D_CFG_H_
