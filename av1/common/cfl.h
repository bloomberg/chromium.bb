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

#include <assert.h>
#include <string.h>

#include "av1/common/common.h"
#include "av1/common/blockd.h"
#include "av1/common/enums.h"

static INLINE int get_scaled_luma_q0(int alpha_q3, int y_pix, int avg_q3) {
  int scaled_luma_q6 = alpha_q3 * ((y_pix << 3) - avg_q3);
  return ROUND_POWER_OF_TWO_SIGNED(scaled_luma_q6, 6);
}

#if CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
static INLINE void cfl_clear_sub8x8_val(CFL_CTX *cfl) {
  memset(cfl->sub8x8_val, 0, sizeof(cfl->sub8x8_val));
}
#endif  // CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG

void cfl_predict_block(MACROBLOCKD *const xd, uint8_t *dst, int dst_stride,
                       int row, int col, TX_SIZE tx_size, int plane);

void cfl_store(CFL_CTX *cfl, const uint8_t *input, int input_stride, int row,
               int col, TX_SIZE tx_size, BLOCK_SIZE bsize);

void cfl_compute_parameters(MACROBLOCKD *const xd, TX_SIZE tx_size);

#endif  // AV1_COMMON_CFL_H_
