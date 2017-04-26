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

#include "av1/common/cfl.h"
#include "av1/common/common_data.h"

// CfL computes its own block-level DC_PRED. This is required to compute both
// alpha_cb and alpha_cr before the prediction are computed.
void cfl_dc_pred(MACROBLOCKD *xd, BLOCK_SIZE plane_bsize, TX_SIZE tx_size) {
  const struct macroblockd_plane *const pd_cb = &xd->plane[AOM_PLANE_U];
  const struct macroblockd_plane *const pd_cr = &xd->plane[AOM_PLANE_V];

  const uint8_t *const dst_cb = pd_cb->dst.buf;
  const uint8_t *const dst_cr = pd_cr->dst.buf;

  const int dst_cb_stride = pd_cb->dst.stride;
  const int dst_cr_stride = pd_cr->dst.stride;

  const int block_width = (plane_bsize != BLOCK_INVALID)
                              ? block_size_wide[plane_bsize]
                              : tx_size_wide[tx_size];
  const int block_height = (plane_bsize != BLOCK_INVALID)
                               ? block_size_high[plane_bsize]
                               : tx_size_high[tx_size];
  const int num_pel = block_width + block_height;

  int sum_cb = 0;
  int sum_cr = 0;

  // Match behavior of build_intra_predictors (reconintra.c) at superblock
  // boundaries:
  //
  // 127 127 127 .. 127 127 127 127 127 127
  // 129  A   B  ..  Y   Z
  // 129  C   D  ..  W   X
  // 129  E   F  ..  U   V
  // 129  G   H  ..  S   T   T   T   T   T
  // ..

  // TODO(ltrudeau) replace this with DC_PRED assembly
  if (xd->up_available && xd->mb_to_right_edge >= 0) {
    for (int i = 0; i < block_width; i++) {
      sum_cb += dst_cb[-dst_cb_stride + i];
      sum_cr += dst_cr[-dst_cr_stride + i];
    }
  } else {
    sum_cb = block_width * 127;
    sum_cr = block_width * 127;
  }

  if (xd->left_available && xd->mb_to_bottom_edge >= 0) {
    for (int i = 0; i < block_height; i++) {
      sum_cb += dst_cb[i * dst_cb_stride - 1];
      sum_cr += dst_cr[i * dst_cr_stride - 1];
    }
  } else {
    sum_cb += block_height * 129;
    sum_cr += block_height * 129;
  }

  xd->cfl->dc_pred[CFL_PRED_U] = (sum_cb + (num_pel >> 1)) / num_pel;
  xd->cfl->dc_pred[CFL_PRED_V] = (sum_cr + (num_pel >> 1)) / num_pel;
}

// Predict the current transform block using CfL.
// it is assumed that dst points at the start of the transform block
void cfl_predict_block(uint8_t *dst, int dst_stride, TX_SIZE tx_size,
                       int dc_pred) {
  const int tx_block_width = tx_size_wide[tx_size];
  const int tx_block_height = tx_size_high[tx_size];

  for (int j = 0; j < tx_block_height; j++) {
    for (int i = 0; i < tx_block_width; i++) {
      dst[i] = dc_pred;
    }
    dst += dst_stride;
  }
}

void cfl_store(CFL_CTX *cfl, const uint8_t *input, int input_stride, int row,
               int col, TX_SIZE tx_size) {
  const int tx_width = tx_size_wide[tx_size];
  const int tx_height = tx_size_high[tx_size];
  const int tx_off_log2 = tx_size_wide_log2[0];

  // Store the input into the CfL pixel buffer
  uint8_t *y_pix = &cfl->y_pix[(row * MAX_SB_SIZE + col) << tx_off_log2];

  // Check that we remain inside the pixel buffer.
  assert(MAX_SB_SIZE * (row + tx_height - 1) + col + tx_width - 1 <
         MAX_SB_SQUARE);

  for (int j = 0; j < tx_height; j++) {
    for (int i = 0; i < tx_width; i++) {
      y_pix[i] = input[i];
    }
    y_pix += MAX_SB_SIZE;
    input += input_stride;
  }

  // Store the surface of the pixel buffer that was written to, this way we
  // can manage chroma overrun (e.g. when the chroma surfaces goes beyond the
  // frame boundary)
  if (col == 0 && row == 0) {
    cfl->y_width = tx_width;
    cfl->y_height = tx_height;
  } else {
    cfl->y_width = OD_MAXI((col << tx_off_log2) + tx_width, cfl->y_width);
    cfl->y_height = OD_MAXI((row << tx_off_log2) + tx_height, cfl->y_height);
  }
}
