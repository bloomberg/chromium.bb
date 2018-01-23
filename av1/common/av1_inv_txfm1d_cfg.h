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
static const int8_t inv_start_range[TX_SIZES_ALL] = {
  5,  // 4x4 transform
  6,  // 8x8 transform
  7,  // 16x16 transform
  7,  // 32x32 transform
#if CONFIG_TX64X64
  7,    // 64x64 transform
#endif  // CONFIG_TX64X64
  6,    // 4x8 transform
  6,    // 8x4 transform
  6,    // 8x16 transform
  6,    // 16x8 transform
  6,    // 16x32 transform
  6,    // 32x16 transform
#if CONFIG_TX64X64
  6,    // 32x64 transform
  6,    // 64x32 transform
#endif  // CONFIG_TX64X64
  6,    // 4x16 transform
  6,    // 16x4 transform
  7,    // 8x32 transform
  7,    // 32x8 transform
#if CONFIG_TX64X64
  7,  // 16x64 transform
  7,  // 64x16 transform
#endif
};

//  ---------------- 4x4 1D config -----------------------
// stage range
static const int8_t inv_stage_range_col_dct_4[4] = { 0, 0, 0, 0 };
static const int8_t inv_stage_range_row_dct_4[4] = { 0, 0, 0, 0 };
static const int8_t inv_stage_range_col_adst_4[6] = { 0, 0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_row_adst_4[6] = { 0, 0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_row_idx_4[1] = { 0 };
static const int8_t inv_stage_range_col_idx_4[1] = { 0 };

//  ---------------- 8x8 1D constants -----------------------
// stage range
static const int8_t inv_stage_range_col_dct_8[6] = { 0, 0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_row_dct_8[6] = { 0, 0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_col_adst_8[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_row_adst_8[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_row_idx_8[1] = { 0 };
static const int8_t inv_stage_range_col_idx_8[1] = { 0 };

//  ---------------- 4x8 1D constants -----------------------
// stage range
static const int8_t inv_stage_range_col_dct_4x8[6] = { 0, 0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_col_adst_4x8[8] = {
  0, 0, 0, 0, 0, 0, 0, 0
};

//  ---------------- 8x4 1D constants -----------------------
// stage range
static const int8_t inv_stage_range_col_dct_8x4[4] = { 0, 0, 0, 0 };
static const int8_t inv_stage_range_col_adst_8x4[6] = { 0, 0, 0, 0, 0, 0 };

//  ---------------- 16x16 1D constants -----------------------

// stage range
static const int8_t inv_stage_range_col_dct_16[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_row_dct_16[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_col_adst_16[10] = { 0, 0, 0, 0, 0,
                                                        0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_row_adst_16[10] = { 0, 0, 0, 0, 0,
                                                        0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_row_idx_16[1] = { 0 };
static const int8_t inv_stage_range_col_idx_16[1] = { 0 };

//  ---------------- 8x16 1D constants -----------------------
// stage range
static const int8_t inv_stage_range_row_dct_8x16[6] = { 0, 0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_row_adst_8x16[8] = {
  0, 0, 0, 0, 0, 0, 0, 0
};
static const int8_t inv_stage_range_col_dct_8x16[8] = {
  0, 0, 0, 0, 0, 0, 0, 0
};
static const int8_t inv_stage_range_col_adst_8x16[10] = { 0, 0, 0, 0, 0,
                                                          0, 0, 0, 0, 0 };
//  ---------------- 16x8 1D constants -----------------------
// stage range
static const int8_t inv_stage_range_col_dct_16x8[6] = { 0, 0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_col_adst_16x8[8] = {
  0, 0, 0, 0, 0, 0, 0, 0
};

//  ---------------- 32x32 1D constants -----------------------
// stage range
static const int8_t inv_stage_range_col_dct_32[10] = { 0, 0, 0, 0, 0,
                                                       0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_row_dct_32[10] = { 0, 0, 0, 0, 0,
                                                       0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_col_adst_32[12] = { 0, 0, 0, 0, 0, 0,
                                                        0, 0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_row_adst_32[12] = { 0, 0, 0, 0, 0, 0,
                                                        0, 0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_row_idx_32[1] = { 0 };
static const int8_t inv_stage_range_col_idx_32[1] = { 0 };

//  ---------------- 16x32 1D constants -----------------------
// stage range
static const int8_t inv_stage_range_col_dct_16x32[10] = { 0, 0, 0, 0, 0,
                                                          0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_col_adst_16x32[12] = { 0, 0, 0, 0, 0, 0,
                                                           0, 0, 0, 0, 0, 0 };
//  ---------------- 32x16 1D constants -----------------------
// stage range
static const int8_t inv_stage_range_col_dct_32x16[8] = {
  0, 0, 0, 0, 0, 0, 0, 0
};
static const int8_t inv_stage_range_col_adst_32x16[10] = { 0, 0, 0, 0, 0,
                                                           0, 0, 0, 0, 0 };
//  ---------------- 64x64 1D constants -----------------------
// stage range
static const int8_t inv_stage_range_col_dct_64[12] = { 0, 0, 0, 0, 0, 0,
                                                       0, 0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_row_dct_64[12] = { 0, 0, 0, 0, 0, 0,
                                                       0, 0, 0, 0, 0, 0 };

static const int8_t inv_stage_range_row_idx_64[1] = { 0 };
static const int8_t inv_stage_range_col_idx_64[1] = { 0 };

//  ---------------- 32x64 1D constants -----------------------
// stage range
static const int8_t inv_stage_range_col_dct_32x64[12] = { 0, 0, 0, 0, 0, 0,
                                                          0, 0, 0, 0, 0, 0 };
//  ---------------- 64x32 1D constants -----------------------
// stage range
static const int8_t inv_stage_range_col_dct_64x32[10] = { 0, 0, 0, 0, 0,
                                                          0, 0, 0, 0, 0 };
//  ---------------- 4x16 1D constants -----------------------
// stage range
static const int8_t inv_stage_range_col_dct_4x16[8] = {
  0, 0, 0, 0, 0, 0, 0, 0
};
static const int8_t inv_stage_range_col_adst_4x16[10] = { 0, 0, 0, 0, 0,
                                                          0, 0, 0, 0, 0 };
//  ---------------- 16x4 1D constants -----------------------
// stage range
static const int8_t inv_stage_range_col_dct_16x4[4] = { 0, 0, 0, 0 };
static const int8_t inv_stage_range_col_adst_16x4[6] = { 0, 0, 0, 0, 0, 0 };

//  ---------------- 8x32 1D constants -----------------------
// stage range
static const int8_t inv_stage_range_row_dct_8x32[6] = { 0, 0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_row_adst_8x32[8] = {
  0, 0, 0, 0, 0, 0, 0, 0
};
static const int8_t inv_stage_range_col_dct_8x32[10] = { 0, 0, 0, 0, 0,
                                                         0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_col_adst_8x32[12] = { 0, 0, 0, 0, 0, 0,
                                                          0, 0, 0, 0, 0, 0 };
//  ---------------- 32x8 1D constants -----------------------
// stage range
static const int8_t inv_stage_range_col_dct_32x8[6] = { 0, 0, 0, 0, 0, 0 };
static const int8_t inv_stage_range_col_adst_32x8[8] = {
  0, 0, 0, 0, 0, 0, 0, 0
};

//  ---------------- 16x64 1D constants -----------------------
// stage range
static const int8_t inv_stage_range_col_dct_16x64[12] = { 0, 0, 0, 0, 0, 0,
                                                          0, 0, 0, 0, 0, 0 };
//  ---------------- 64x16 1D constants -----------------------
// stage range
static const int8_t inv_stage_range_col_dct_64x16[8] = {
  0, 0, 0, 0, 0, 0, 0, 0
};
//  ---------------- row config inv_dct_4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_dct_4 = {
  4,                          // .txfm_size
  4,                          // .stage_num
  inv_stage_range_row_dct_4,  // .stage_range
  TXFM_TYPE_DCT4              // .txfm_type
};

//  ---------------- row config inv_dct_8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_dct_8 = {
  8,                          // .txfm_size
  6,                          // .stage_num
  inv_stage_range_row_dct_8,  // .stage_range
  TXFM_TYPE_DCT8              // .txfm_type
};
//  ---------------- row config inv_dct_16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_dct_16 = {
  16,                          // .txfm_size
  8,                           // .stage_num
  inv_stage_range_row_dct_16,  // .stage_range
  TXFM_TYPE_DCT16              // .txfm_type
};

//  ---------------- row config inv_dct_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_dct_32 = {
  32,                          // .txfm_size
  10,                          // .stage_num
  inv_stage_range_row_dct_32,  // .stage_range
  TXFM_TYPE_DCT32              // .txfm_type
};

#if CONFIG_TX64X64
//  ---------------- row config inv_dct_64 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_dct_64 = {
  64,                          // .txfm_size
  12,                          // .stage_num
  inv_stage_range_row_dct_64,  // .stage_range
  TXFM_TYPE_DCT64,             // .txfm_type_col
};
#endif  // CONFIG_TX64X64

//  ---------------- row config inv_adst_4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_adst_4 = {
  4,                           // .txfm_size
  6,                           // .stage_num
  inv_stage_range_row_adst_4,  // .stage_range
  TXFM_TYPE_ADST4,             // .txfm_type
};

//  ---------------- row config inv_adst_8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_adst_8 = {
  8,                           // .txfm_size
  8,                           // .stage_num
  inv_stage_range_row_adst_8,  // .stage_range
  TXFM_TYPE_ADST8,             // .txfm_type_col
};

//  ---------------- row config inv_adst_16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_adst_16 = {
  16,                           // .txfm_size
  10,                           // .stage_num
  inv_stage_range_row_adst_16,  // .stage_range
  TXFM_TYPE_ADST16,             // .txfm_type
};

//  ---------------- row config inv_adst_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_adst_32 = {
  32,                           // .txfm_size
  12,                           // .stage_num
  inv_stage_range_row_adst_32,  // .stage_range
  TXFM_TYPE_ADST32,             // .txfm_type
};

//  ---------------- col config inv_dct_4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_4 = {
  4,                          // .txfm_size
  4,                          // .stage_num
  inv_stage_range_col_dct_4,  // .stage_range
  TXFM_TYPE_DCT4              // .txfm_type
};

//  ---------------- col config inv_dct_8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_8 = {
  8,                          // .txfm_size
  6,                          // .stage_num
  inv_stage_range_col_dct_8,  // .stage_range
  TXFM_TYPE_DCT8              // .txfm_type
};

//  ---------------- col config inv_dct_16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_16 = {
  16,                          // .txfm_size
  8,                           // .stage_num
  inv_stage_range_col_dct_16,  // .stage_range
  TXFM_TYPE_DCT16              // .txfm_type
};

//  ---------------- col config inv_dct_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_32 = {
  32,                          // .txfm_size
  10,                          // .stage_num
  inv_stage_range_col_dct_32,  // .stage_range
  TXFM_TYPE_DCT32              // .txfm_type
};

//  ---------------- col config inv_dct_64 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_64 = {
  64,                          // .txfm_size
  12,                          // .stage_num
  inv_stage_range_col_dct_64,  // .stage_range
  TXFM_TYPE_DCT64,             // .txfm_type_col
};

//  ---------------- col config inv_adst_4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_4 = {
  4,                           // .txfm_size
  6,                           // .stage_num
  inv_stage_range_col_adst_4,  // .stage_range
  TXFM_TYPE_ADST4,             // .txfm_type
};

//  ---------------- col config inv_adst_8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_8 = {
  8,                           // .txfm_size
  8,                           // .stage_num
  inv_stage_range_col_adst_8,  // .stage_range
  TXFM_TYPE_ADST8,             // .txfm_type_col
};

//  ---------------- col config inv_adst_16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_16 = {
  16,                           // .txfm_size
  10,                           // .stage_num
  inv_stage_range_col_adst_16,  // .stage_range
  TXFM_TYPE_ADST16,             // .txfm_type
};

//  ---------------- col config inv_adst_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_32 = {
  32,                           // .txfm_size
  12,                           // .stage_num
  inv_stage_range_col_adst_32,  // .stage_range
  TXFM_TYPE_ADST32,             // .txfm_type
};

//  ---------------- col config inv_identity_4 ----------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_identity_4 = {
  4,                          // .txfm_size
  1,                          // .stage_num
  inv_stage_range_col_idx_4,  // .stage_range
  TXFM_TYPE_IDENTITY4,        // .txfm_type
};

//  ---------------- row config inv_identity_4 ----------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_identity_4 = {
  4,                          // .txfm_size
  1,                          // .stage_num
  inv_stage_range_row_idx_4,  // .stage_range
  TXFM_TYPE_IDENTITY4,        // .txfm_type
};

//  ---------------- col config inv_identity_8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_identity_8 = {
  8,                          // .txfm_size
  1,                          // .stage_num
  inv_stage_range_col_idx_8,  // .stage_range
  TXFM_TYPE_IDENTITY8,        // .txfm_type
};

//  ---------------- row config inv_identity_8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_identity_8 = {
  8,                          // .txfm_size
  1,                          // .stage_num
  inv_stage_range_row_idx_8,  // .stage_range
  TXFM_TYPE_IDENTITY8,        // .txfm_type
};

//  ---------------- col config inv_identity_16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_identity_16 = {
  16,                          // .txfm_size
  1,                           // .stage_num
  inv_stage_range_col_idx_16,  // .stage_range
  TXFM_TYPE_IDENTITY16,        // .txfm_type
};

//  ---------------- row config inv_identity_16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_identity_16 = {
  16,                          // .txfm_size
  1,                           // .stage_num
  inv_stage_range_row_idx_16,  // .stage_range
  TXFM_TYPE_IDENTITY16,        // .txfm_type
};

//  ---------------- col config inv_identity_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_identity_32 = {
  32,                          // .txfm_size
  1,                           // .stage_num
  inv_stage_range_col_idx_32,  // .stage_range
  TXFM_TYPE_IDENTITY32,        // .txfm_type
};

//  ---------------- row config inv_identity_32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_identity_32 = {
  32,                          // .txfm_size
  1,                           // .stage_num
  inv_stage_range_row_idx_32,  // .stage_range
  TXFM_TYPE_IDENTITY32,        // .txfm_type
};

#if CONFIG_TX64X64
//  ---------------- col config inv_identity_64 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_identity_64 = {
  64,                          // .txfm_size
  1,                           // .stage_num
  inv_stage_range_col_idx_64,  // .stage_range
  TXFM_TYPE_IDENTITY64,        // .txfm_type
};

//  ---------------- row config inv_identity_64 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_identity_64 = {
  64,                          // .txfm_size
  1,                           // .stage_num
  inv_stage_range_row_idx_64,  // .stage_range
  TXFM_TYPE_IDENTITY64,        // .txfm_type
};
#endif  // CONFIG_TX64X64

//  ---------------- col config inv_dct_8x4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_8x4 = {
  4,                            // .txfm_size
  4,                            // .stage_num
  inv_stage_range_col_dct_8x4,  // .stage_range
  TXFM_TYPE_DCT4                // .txfm_type
};

//  ---------------- col config inv_adst_8x4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_8x4 = {
  4,                             // .txfm_size
  6,                             // .stage_num
  inv_stage_range_col_adst_8x4,  // .stage_range
  TXFM_TYPE_ADST4,               // .txfm_type
};

//  ---------------- col config inv_dct_16x4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_16x4 = {
  4,                             // .txfm_size
  4,                             // .stage_num
  inv_stage_range_col_dct_16x4,  // .stage_range
  TXFM_TYPE_DCT4                 // .txfm_type
};

//  ---------------- col config inv_adst_16x4 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_16x4 = {
  4,                              // .txfm_size
  6,                              // .stage_num
  inv_stage_range_col_adst_16x4,  // .stage_range
  TXFM_TYPE_ADST4,                // .txfm_type
};

//  ---------------- col config inv_dct_4x8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_4x8 = {
  8,                            // .txfm_size
  6,                            // .stage_num
  inv_stage_range_col_dct_4x8,  // .stage_range
  TXFM_TYPE_DCT8                // .txfm_type
};

//  ---------------- col config inv_adst_16x8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_4x8 = {
  8,                             // .txfm_size
  8,                             // .stage_num
  inv_stage_range_col_adst_4x8,  // .stage_range
  TXFM_TYPE_ADST8,               // .txfm_type_col
};

//  ---------------- col config inv_dct_16x8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_16x8 = {
  8,                             // .txfm_size
  6,                             // .stage_num
  inv_stage_range_col_dct_16x8,  // .stage_range
  TXFM_TYPE_DCT8                 // .txfm_type
};

//  ---------------- col config inv_adst_16x8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_16x8 = {
  8,                              // .txfm_size
  8,                              // .stage_num
  inv_stage_range_col_adst_16x8,  // .stage_range
  TXFM_TYPE_ADST8,                // .txfm_type_col
};

//  ---------------- col config inv_dct_32x8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_32x8 = {
  8,                             // .txfm_size
  6,                             // .stage_num
  inv_stage_range_col_dct_32x8,  // .stage_range
  TXFM_TYPE_DCT8                 // .txfm_type
};

//  ---------------- col config inv_adst_32x8 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_32x8 = {
  8,                              // .txfm_size
  8,                              // .stage_num
  inv_stage_range_col_adst_32x8,  // .stage_range
  TXFM_TYPE_ADST8,                // .txfm_type_col
};

//  ---------------- col config inv_dct_4x16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_4x16 = {
  16,                            // .txfm_size
  8,                             // .stage_num
  inv_stage_range_col_dct_4x16,  // .stage_range
  TXFM_TYPE_DCT16                // .txfm_type
};

//  ---------------- col config inv_adst_4x16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_4x16 = {
  16,                             // .txfm_size
  10,                             // .stage_num
  inv_stage_range_col_adst_4x16,  // .stage_range
  TXFM_TYPE_ADST16,               // .txfm_type
};

//  ---------------- row config inv_dct_8x16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_dct_8x16 = {
  8,                             // .txfm_size
  6,                             // .stage_num
  inv_stage_range_row_dct_8x16,  // .stage_range
  TXFM_TYPE_DCT8                 // .txfm_type
};

//  ---------------- col config inv_dct_8x16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_8x16 = {
  16,                            // .txfm_size
  8,                             // .stage_num
  inv_stage_range_col_dct_8x16,  // .stage_range
  TXFM_TYPE_DCT16                // .txfm_type
};

//  ---------------- row config inv_adst_8x16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_adst_8x16 = {
  8,                              // .txfm_size
  8,                              // .stage_num
  inv_stage_range_row_adst_8x16,  // .stage_range
  TXFM_TYPE_ADST8,                // .txfm_type
};

//  ---------------- col config inv_adst_8x16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_8x16 = {
  16,                             // .txfm_size
  10,                             // .stage_num
  inv_stage_range_col_adst_8x16,  // .stage_range
  TXFM_TYPE_ADST16,               // .txfm_type
};

//  ---------------- col config inv_dct_32x16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_32x16 = {
  16,                             // .txfm_size
  8,                              // .stage_num
  inv_stage_range_col_dct_32x16,  // .stage_range
  TXFM_TYPE_DCT16                 // .txfm_type
};

//  ---------------- col config inv_adst_32x16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_32x16 = {
  16,                              // .txfm_size
  10,                              // .stage_num
  inv_stage_range_col_adst_32x16,  // .stage_range
  TXFM_TYPE_ADST16,                // .txfm_type
};

#if CONFIG_TX64X64
//  ---------------- col config inv_dct_64x16 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_64x16 = {
  16,                             // .txfm_size
  8,                              // .stage_num
  inv_stage_range_col_dct_64x16,  // .stage_range
  TXFM_TYPE_DCT16                 // .txfm_type
};
#endif  // CONFIG_TX64X64

//  ---------------- row config inv_dct_8x32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_dct_8x32 = {
  8,                             // .txfm_size
  6,                             // .stage_num
  inv_stage_range_row_dct_8x32,  // .stage_range
  TXFM_TYPE_DCT8                 // .txfm_type
};

//  ---------------- col config inv_dct_8x32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_8x32 = {
  32,                            // .txfm_size
  10,                            // .stage_num
  inv_stage_range_col_dct_8x32,  // .stage_range
  TXFM_TYPE_DCT32                // .txfm_type
};

//  ---------------- row config inv_adst_8x32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_row_cfg_adst_8x32 = {
  8,                              // .txfm_size
  8,                              // .stage_num
  inv_stage_range_row_adst_8x32,  // .stage_range
  TXFM_TYPE_ADST8,                // .txfm_type
};

//  ---------------- col config inv_adst_8x32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_8x32 = {
  32,                             // .txfm_size
  12,                             // .stage_num
  inv_stage_range_col_adst_8x32,  // .stage_range
  TXFM_TYPE_ADST32,               // .txfm_type
};

//  ---------------- col config inv_dct_16x32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_16x32 = {
  32,                             // .txfm_size
  10,                             // .stage_num
  inv_stage_range_col_dct_16x32,  // .stage_range
  TXFM_TYPE_DCT32                 // .txfm_type
};

//  ---------------- col config inv_adst_16x32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_adst_16x32 = {
  32,                              // .txfm_size
  12,                              // .stage_num
  inv_stage_range_col_adst_16x32,  // .stage_range
  TXFM_TYPE_ADST32,                // .txfm_type
};

#if CONFIG_TX64X64
//  ---------------- col config inv_dct_64x32 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_64x32 = {
  32,                             // .txfm_size
  10,                             // .stage_num
  inv_stage_range_col_dct_64x32,  // .stage_range
  TXFM_TYPE_DCT32                 // .txfm_type
};

//  ---------------- col config inv_dct_16x64 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_16x64 = {
  64,                             // .txfm_size
  12,                             // .stage_num
  inv_stage_range_col_dct_16x64,  // .stage_range
  TXFM_TYPE_DCT64,                // .txfm_type_col
};

//  ---------------- col config inv_dct_32x64 ----------------
static const TXFM_1D_CFG inv_txfm_1d_col_cfg_dct_32x64 = {
  64,                             // .txfm_size
  12,                             // .stage_num
  inv_stage_range_col_dct_32x64,  // .stage_range
  TXFM_TYPE_DCT64,                // .txfm_type_col
};
#endif  // CONFIG_TX64X64

extern const int8_t *inv_txfm_shift_ls[TX_SIZES_ALL];
extern const int8_t inv_cos_bit_col[5 /*row*/][5 /*col*/];
extern const int8_t inv_cos_bit_row[5 /*row*/][5 /*col*/];

#endif  // AV1_INV_TXFM2D_CFG_H_
