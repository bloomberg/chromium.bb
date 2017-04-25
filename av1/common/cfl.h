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

#ifndef AV1_COMMON_CFL_H_
#define AV1_COMMON_CFL_H_

#include "av1/common/enums.h"

// Forward declaration of AV1_COMMON, in order to avoid creating a cyclic
// dependency by importing av1/common/onyxc_int.h
typedef struct AV1Common AV1_COMMON;

// Forward declaration of MACROBLOCK, in order to avoid creating a cyclic
// dependency by importing av1/common/blockd.h
typedef struct macroblockd MACROBLOCKD;

// Forward declaration of MB_MODE_INFO, in order to avoid creating a cyclic
// dependency by importing av1/common/blockd.h
typedef struct MB_MODE_INFO MB_MODE_INFO;

typedef struct {
  // Pixel buffer containing the luma pixels used as prediction for chroma
  uint8_t y_pix[MAX_SB_SQUARE];

  // Height and width of the luma prediction block currently in the pixel buffer
  int y_height, y_width;

  // Chroma subsampling
  int subsampling_x, subsampling_y;

  // CfL Performs its own block level DC_PRED for each chromatic plane
  int dc_pred[CFL_PRED_PLANES];

  // Count the number of TX blocks in a predicted block to know when you are at
  // the last one, so you can check for skips.
  // TODO(any) Is there a better way to do this?
  int num_tx_blk[CFL_PRED_PLANES];
} CFL_CTX;

static const double cfl_alpha_codes[CFL_ALPHABET_SIZE][CFL_PRED_PLANES] = {
  // barrbrain's simple 1D quant ordered by subset 3 likelihood
  { 0., 0. },    { 0.125, 0.125 }, { 0.25, 0. },   { 0.25, 0.125 },
  { 0.125, 0. }, { 0.25, 0.25 },   { 0., 0.125 },  { 0.5, 0.5 },
  { 0.5, 0.25 }, { 0.125, 0.25 },  { 0.5, 0. },    { 0.25, 0.5 },
  { 0., 0.25 },  { 0.5, 0.125 },   { 0.125, 0.5 }, { 0., 0.5 }
};

void cfl_init(CFL_CTX *cfl, AV1_COMMON *cm, int subsampling_x,
              int subsampling_y);

void cfl_dc_pred(MACROBLOCKD *xd, BLOCK_SIZE plane_bsize, TX_SIZE tx_size);

double cfl_ind_to_alpha(const MB_MODE_INFO *mbmi, CFL_PRED_TYPE pred_type);

void cfl_predict_block(const CFL_CTX *cfl, uint8_t *dst, int dst_stride,
                       int row, int col, TX_SIZE tx_size, int dc_pred,
                       double alpha);

void cfl_store(CFL_CTX *cfl, const uint8_t *input, int input_stride, int row,
               int col, TX_SIZE tx_size);

double cfl_load(const CFL_CTX *cfl, uint8_t *output, int output_stride, int row,
                int col, TX_SIZE tx_size);
#endif  // AV1_COMMON_CFL_H_
