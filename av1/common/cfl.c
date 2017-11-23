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
#include "av1/common/onyxc_int.h"

void cfl_init(CFL_CTX *cfl, AV1_COMMON *cm) {
  if ((cm->subsampling_x != 0 && cm->subsampling_x != 1) ||
      (cm->subsampling_y != 0 && cm->subsampling_y != 1)) {
    aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                       "Only 4:4:4, 4:4:0, 4:2:2 and 4:2:0 are currently "
                       "supported by CfL, %d %d "
                       "subsampling is not supported.\n",
                       cm->subsampling_x, cm->subsampling_y);
  }
  memset(&cfl->pred_buf_q3, 0, sizeof(cfl->pred_buf_q3));
  cfl->subsampling_x = cm->subsampling_x;
  cfl->subsampling_y = cm->subsampling_y;
  cfl->are_parameters_computed = 0;
  cfl->store_y = 0;
#if CONFIG_DEBUG
  cfl_clear_sub8x8_val(cfl);
  cfl->store_counter = 0;
  cfl->last_compute_counter = 0;
#endif  // CONFIG_DEBUG
}

// Due to frame boundary issues, it is possible that the total area covered by
// chroma exceeds that of luma. When this happens, we fill the missing pixels by
// repeating the last columns and/or rows.
static INLINE void cfl_pad(CFL_CTX *cfl, int width, int height) {
  const int diff_width = width - cfl->buf_width;
  const int diff_height = height - cfl->buf_height;

  if (diff_width > 0) {
    const int min_height = height - diff_height;
    int16_t *pred_buf_q3 = cfl->pred_buf_q3 + (width - diff_width);
    for (int j = 0; j < min_height; j++) {
      const int16_t last_pixel = pred_buf_q3[-1];
      for (int i = 0; i < diff_width; i++) {
        pred_buf_q3[i] = last_pixel;
      }
      pred_buf_q3 += MAX_SB_SIZE;
    }
    cfl->buf_width = width;
  }
  if (diff_height > 0) {
    int16_t *pred_buf_q3 =
        cfl->pred_buf_q3 + ((height - diff_height) * MAX_SB_SIZE);
    for (int j = 0; j < diff_height; j++) {
      const int16_t *last_row_q3 = pred_buf_q3 - MAX_SB_SIZE;
      for (int i = 0; i < width; i++) {
        pred_buf_q3[i] = last_row_q3[i];
      }
      pred_buf_q3 += MAX_SB_SIZE;
    }
    cfl->buf_height = height;
  }
}

static void cfl_subtract_averages(CFL_CTX *cfl, TX_SIZE tx_size) {
  const int width = cfl->uv_width;
  const int height = cfl->uv_height;
  const int tx_height = tx_size_high[tx_size];
  const int tx_width = tx_size_wide[tx_size];
  const int block_row_stride = MAX_SB_SIZE << tx_size_high_log2[tx_size];
  const int num_pel_log2 =
      (tx_size_high_log2[tx_size] + tx_size_wide_log2[tx_size]);

  int16_t *pred_buf_q3 = cfl->pred_buf_q3;

  cfl_pad(cfl, width, height);

  for (int b_j = 0; b_j < height; b_j += tx_height) {
    for (int b_i = 0; b_i < width; b_i += tx_width) {
      int sum_q3 = 0;
      int16_t *tx_pred_buf_q3 = pred_buf_q3;
      for (int t_j = 0; t_j < tx_height; t_j++) {
        for (int t_i = b_i; t_i < b_i + tx_width; t_i++) {
          sum_q3 += tx_pred_buf_q3[t_i];
        }
        tx_pred_buf_q3 += MAX_SB_SIZE;
      }
      int avg_q3 = (sum_q3 + (1 << (num_pel_log2 - 1))) >> num_pel_log2;
      // Loss is never more than 1/2 (in Q3)
      assert(abs((avg_q3 * (1 << num_pel_log2)) - sum_q3) <=
             1 << num_pel_log2 >> 1);

      tx_pred_buf_q3 = pred_buf_q3;
      for (int t_j = 0; t_j < tx_height; t_j++) {
        for (int t_i = b_i; t_i < b_i + tx_width; t_i++) {
          tx_pred_buf_q3[t_i] -= avg_q3;
        }

        tx_pred_buf_q3 += MAX_SB_SIZE;
      }
    }
    pred_buf_q3 += block_row_stride;
  }
}

static INLINE int cfl_idx_to_alpha(int alpha_idx, int joint_sign,
                                   CFL_PRED_TYPE pred_type) {
  const int alpha_sign = (pred_type == CFL_PRED_U) ? CFL_SIGN_U(joint_sign)
                                                   : CFL_SIGN_V(joint_sign);
  if (alpha_sign == CFL_SIGN_ZERO) return 0;
  const int abs_alpha_q3 =
      (pred_type == CFL_PRED_U) ? CFL_IDX_U(alpha_idx) : CFL_IDX_V(alpha_idx);
  return (alpha_sign == CFL_SIGN_POS) ? abs_alpha_q3 + 1 : -abs_alpha_q3 - 1;
}

static void cfl_build_prediction_lbd(const int16_t *pred_buf_q3, uint8_t *dst,
                                     int dst_stride, int width, int height,
                                     int alpha_q3) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      dst[i] =
          clip_pixel(get_scaled_luma_q0(alpha_q3, pred_buf_q3[i]) + dst[i]);
    }
    dst += dst_stride;
    pred_buf_q3 += MAX_SB_SIZE;
  }
}

#if CONFIG_HIGHBITDEPTH
static void cfl_build_prediction_hbd(const int16_t *pred_buf_q3, uint16_t *dst,
                                     int dst_stride, int width, int height,
                                     int alpha_q3, int bit_depth) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      dst[i] = clip_pixel_highbd(
          get_scaled_luma_q0(alpha_q3, pred_buf_q3[i]) + dst[i], bit_depth);
    }
    dst += dst_stride;
    pred_buf_q3 += MAX_SB_SIZE;
  }
}
#endif  // CONFIG_HIGHBITDEPTH

static void cfl_compute_parameters(MACROBLOCKD *const xd, TX_SIZE tx_size) {
  CFL_CTX *const cfl = xd->cfl;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;

  // Do not call cfl_compute_parameters multiple time on the same values.
  assert(cfl->are_parameters_computed == 0);

  const BLOCK_SIZE plane_bsize = AOMMAX(
      BLOCK_4X4, get_plane_block_size(mbmi->sb_type, &xd->plane[AOM_PLANE_U]));
#if CONFIG_DEBUG
  BLOCK_SIZE bsize = mbmi->sb_type;
  if (block_size_high[bsize] == 4 || block_size_wide[bsize] == 4) {
    const uint16_t compute_counter = cfl->sub8x8_val[0];
    assert(compute_counter != cfl->last_compute_counter);
    bsize = scale_chroma_bsize(bsize, cfl->subsampling_x, cfl->subsampling_y);
    const int val_wide = mi_size_wide[bsize];
    const int val_high = mi_size_high[bsize];
    assert(val_wide <= CFL_SUB8X8_VAL_MI_SIZE);
    assert(val_high <= CFL_SUB8X8_VAL_MI_SIZE);
    for (int val_r = 0; val_r < val_high; val_r++) {
      for (int val_c = 0; val_c < val_wide; val_c++) {
        // If all counters in the validation buffer are equal then they are all
        // related to the same chroma reference block.
        assert(cfl->sub8x8_val[val_r * CFL_SUB8X8_VAL_MI_SIZE + val_c] ==
               compute_counter);
      }
    }
    cfl->last_compute_counter = compute_counter;
  }
#endif  // CONFIG_DEBUG

  // AOM_PLANE_U is used, but both planes will have the same sizes.
  cfl->uv_width = max_intra_block_width(xd, plane_bsize, AOM_PLANE_U, tx_size);
  cfl->uv_height =
      max_intra_block_height(xd, plane_bsize, AOM_PLANE_U, tx_size);

  cfl_subtract_averages(cfl, tx_size);
  cfl->are_parameters_computed = 1;
}

void cfl_predict_block(MACROBLOCKD *const xd, uint8_t *dst, int dst_stride,
                       int row, int col, TX_SIZE tx_size, int plane) {
  CFL_CTX *const cfl = xd->cfl;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;

  if (!cfl->are_parameters_computed) cfl_compute_parameters(xd, tx_size);

  const int16_t *pred_buf_q3 =
      cfl->pred_buf_q3 + ((row * MAX_SB_SIZE + col) << tx_size_wide_log2[0]);
  const int alpha_q3 =
      cfl_idx_to_alpha(mbmi->cfl_alpha_idx, mbmi->cfl_alpha_signs, plane - 1);
#if CONFIG_HIGHBITDEPTH
  if (get_bitdepth_data_path_index(xd)) {
    uint16_t *dst_16 = CONVERT_TO_SHORTPTR(dst);
    cfl_build_prediction_hbd(pred_buf_q3, dst_16, dst_stride,
                             tx_size_wide[tx_size], tx_size_high[tx_size],
                             alpha_q3, xd->bd);
    return;
  }
#endif  // CONFIG_HIGHBITDEPTH
  cfl_build_prediction_lbd(pred_buf_q3, dst, dst_stride, tx_size_wide[tx_size],
                           tx_size_high[tx_size], alpha_q3);
}

static void cfl_luma_subsampling_420_lbd(const uint8_t *input, int input_stride,
                                         int16_t *output_q3, int width,
                                         int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      int top = i << 1;
      int bot = top + input_stride;
      output_q3[i] = (input[top] + input[top + 1] + input[bot] + input[bot + 1])
                     << 1;
    }
    input += input_stride << 1;
    output_q3 += MAX_SB_SIZE;
  }
}

static void cfl_luma_subsampling_422_lbd(const uint8_t *input, int input_stride,
                                         int16_t *output_q3, int width,
                                         int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      int left = i << 1;
      output_q3[i] = (input[left] + input[left + 1]) << 2;
    }
    input += input_stride;
    output_q3 += MAX_SB_SIZE;
  }
}

static void cfl_luma_subsampling_440_lbd(const uint8_t *input, int input_stride,
                                         int16_t *output_q3, int width,
                                         int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      output_q3[i] = (input[i] + input[i + input_stride]) << 2;
    }
    input += input_stride << 1;
    output_q3 += MAX_SB_SIZE;
  }
}

static void cfl_luma_subsampling_444_lbd(const uint8_t *input, int input_stride,
                                         int16_t *output_q3, int width,
                                         int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      output_q3[i] = input[i] << 3;
    }
    input += input_stride;
    output_q3 += MAX_SB_SIZE;
  }
}

typedef void (*cfl_subsample_lbd_fn)(const uint8_t *input, int input_stride,
                                     int16_t *output_q3, int width, int height);

static const cfl_subsample_lbd_fn subsample_lbd[2][2] = {
  //  (sub_y == 0, sub_x == 0)       (sub_y == 0, sub_x == 1)
  //  (sub_y == 1, sub_x == 0)       (sub_y == 1, sub_x == 1)
  { cfl_luma_subsampling_444_lbd, cfl_luma_subsampling_422_lbd },
  { cfl_luma_subsampling_440_lbd, cfl_luma_subsampling_420_lbd },
};

#if CONFIG_HIGHBITDEPTH
static void cfl_luma_subsampling_420_hbd(const uint16_t *input,
                                         int input_stride, int16_t *output_q3,
                                         int width, int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      int top = i << 1;
      int bot = top + input_stride;
      output_q3[i] = (input[top] + input[top + 1] + input[bot] + input[bot + 1])
                     << 1;
    }
    input += input_stride << 1;
    output_q3 += MAX_SB_SIZE;
  }
}

static void cfl_luma_subsampling_422_hbd(const uint16_t *input,
                                         int input_stride, int16_t *output_q3,
                                         int width, int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      int left = i << 1;
      output_q3[i] = (input[left] + input[left + 1]) << 2;
    }
    input += input_stride;
    output_q3 += MAX_SB_SIZE;
  }
}

static void cfl_luma_subsampling_440_hbd(const uint16_t *input,
                                         int input_stride, int16_t *output_q3,
                                         int width, int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      output_q3[i] = (input[i] + input[i + input_stride]) << 2;
    }
    input += input_stride << 1;
    output_q3 += MAX_SB_SIZE;
  }
}

static void cfl_luma_subsampling_444_hbd(const uint16_t *input,
                                         int input_stride, int16_t *output_q3,
                                         int width, int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      output_q3[i] = input[i] << 3;
    }
    input += input_stride;
    output_q3 += MAX_SB_SIZE;
  }
}

typedef void (*cfl_subsample_hbd_fn)(const uint16_t *input, int input_stride,
                                     int16_t *output_q3, int width, int height);

static const cfl_subsample_hbd_fn subsample_hbd[2][2] = {
  //  (sub_y == 0, sub_x == 0)       (sub_y == 0, sub_x == 1)
  //  (sub_y == 1, sub_x == 0)       (sub_y == 1, sub_x == 1)
  { cfl_luma_subsampling_444_hbd, cfl_luma_subsampling_422_hbd },
  { cfl_luma_subsampling_440_hbd, cfl_luma_subsampling_420_hbd },
};
#endif  // CONFIG_HIGHBITDEPTH

static void cfl_store(CFL_CTX *cfl, const uint8_t *input, int input_stride,
                      int row, int col, int width, int height, int use_hbd) {
  const int tx_off_log2 = tx_size_wide_log2[0];
  const int sub_x = cfl->subsampling_x;
  const int sub_y = cfl->subsampling_y;
  const int store_row = row << (tx_off_log2 - sub_y);
  const int store_col = col << (tx_off_log2 - sub_x);
  const int store_height = height >> sub_y;
  const int store_width = width >> sub_x;

  // Invalidate current parameters
  cfl->are_parameters_computed = 0;

  // Store the surface of the pixel buffer that was written to, this way we
  // can manage chroma overrun (e.g. when the chroma surfaces goes beyond the
  // frame boundary)
  if (col == 0 && row == 0) {
    cfl->buf_width = store_width;
    cfl->buf_height = store_height;
  } else {
    cfl->buf_width = OD_MAXI(store_col + store_width, cfl->buf_width);
    cfl->buf_height = OD_MAXI(store_row + store_height, cfl->buf_height);
  }

  // Check that we will remain inside the pixel buffer.
  assert(store_row + store_height <= MAX_SB_SIZE);
  assert(store_col + store_width <= MAX_SB_SIZE);

  // Store the input into the CfL pixel buffer
  int16_t *pred_buf_q3 =
      cfl->pred_buf_q3 + (store_row * MAX_SB_SIZE + store_col);

#if CONFIG_HIGHBITDEPTH
  if (use_hbd) {
    const uint16_t *input_16 = CONVERT_TO_SHORTPTR(input);
    // AND sub_x and sub_y with 1 to ensures that an attacker won't be able to
    // index the function pointer array out of bounds.
    subsample_hbd[sub_y & 1][sub_x & 1](input_16, input_stride, pred_buf_q3,
                                        store_width, store_height);
    return;
  }
#endif  // CONFIG_HIGHBITDEPTH
  (void)use_hbd;
  // AND sub_x and sub_y with 1 to ensures that an attacker won't be able to
  // index the function pointer array out of bounds.
  subsample_lbd[sub_y & 1][sub_x & 1](input, input_stride, pred_buf_q3,
                                      store_width, store_height);
}

// Adjust the row and column of blocks smaller than 8X8, as chroma-referenced
// and non-chroma-referenced blocks are stored together in the CfL buffer.
static INLINE void sub8x8_adjust_offset(const CFL_CTX *cfl, int *row_out,
                                        int *col_out) {
  // Increment row index for bottom: 8x4, 16x4 or both bottom 4x4s.
  if ((cfl->mi_row & 0x01) && cfl->subsampling_y) {
    assert(*row_out == 0);
    (*row_out)++;
  }

  // Increment col index for right: 4x8, 4x16 or both right 4x4s.
  if ((cfl->mi_col & 0x01) && cfl->subsampling_x) {
    assert(*col_out == 0);
    (*col_out)++;
  }
}
#if CONFIG_DEBUG
// Since the chroma surface of sub8x8 block span across multiple luma blocks,
// this function validates that the reconstructed luma area required to predict
// the chroma block using CfL has been stored during the previous luma encode.
//
//   Issue 1: Chroma intra prediction is not always performed after luma. One
//   such example is when luma RD cost is really high and the mode decision
//   algorithm decides to terminate instead of evaluating chroma.
//
//   Issue 2: When multiple CfL predictions are computed for a given sub8x8
//   block. The reconstructed luma that belongs to the non-reference sub8x8
//   blocks must remain in the buffer (we cannot clear the buffer when we
//   compute the CfL prediction
//
// To resolve these issues, we increment the store_counter on each store. if
// other sub8x8 blocks have already been coded and the counter corresponds to
// the previous value they are also set to the current value. If a sub8x8 block
// is not stored the store_counter won't match which will be detected when the
// CfL parements are computed.
static void sub8x8_set_val(CFL_CTX *cfl, int row, int col, TX_SIZE y_tx_size) {
  const int y_tx_wide_unit = tx_size_wide_unit[y_tx_size];
  const int y_tx_high_unit = tx_size_high_unit[y_tx_size];

  // How many 4x4 are in tx_size
  const int y_tx_unit_len = y_tx_wide_unit * y_tx_high_unit;
  assert(y_tx_unit_len == 1 || y_tx_unit_len == 2 || y_tx_unit_len == 4);

  // Invalidate other counters if (0,0)
  const int is_first = row + col == 0;
  cfl->store_counter += is_first ? 2 : 1;

  const int inc =
      (y_tx_wide_unit >= y_tx_high_unit) ? 1 : CFL_SUB8X8_VAL_MI_SIZE;
  uint16_t *sub8x8_val = cfl->sub8x8_val + (row * CFL_SUB8X8_VAL_MI_SIZE + col);
  for (int i = 0; i < y_tx_unit_len; i++) {
    *sub8x8_val = cfl->store_counter;
    sub8x8_val += inc;
  }

  if (!is_first) {
    const uint16_t prev_store_counter = cfl->store_counter - 1;
    int found = 0;
    (void)found;
    sub8x8_val = cfl->sub8x8_val;
    for (int y = 0; y < CFL_SUB8X8_VAL_MI_SIZE; y++) {
      for (int x = 0; x < CFL_SUB8X8_VAL_MI_SIZE; x++) {
        if (sub8x8_val[x] == prev_store_counter) {
          sub8x8_val[x] = cfl->store_counter;
          found = 1;
        }
      }
      sub8x8_val += CFL_SUB8X8_VAL_MI_SIZE;
    }
    // Something is wrong if (0,0) is missing
    assert(found);
  }
}
#endif  // CONFIG_DEBUG

void cfl_store_tx(MACROBLOCKD *const xd, int row, int col, TX_SIZE tx_size,
                  BLOCK_SIZE bsize) {
  CFL_CTX *const cfl = xd->cfl;
  struct macroblockd_plane *const pd = &xd->plane[AOM_PLANE_Y];
  uint8_t *dst =
      &pd->dst.buf[(row * pd->dst.stride + col) << tx_size_wide_log2[0]];
  if (block_size_high[bsize] == 4 || block_size_wide[bsize] == 4) {
    // Only dimensions of size 4 can have an odd offset.
    assert(!((col & 1) && tx_size_wide[tx_size] != 4));
    assert(!((row & 1) && tx_size_high[tx_size] != 4));
    sub8x8_adjust_offset(cfl, &row, &col);
#if CONFIG_DEBUG
    sub8x8_set_val(cfl, row, col, tx_size);
#endif  // CONFIG_DEBUG
  }
  cfl_store(cfl, dst, pd->dst.stride, row, col, tx_size_wide[tx_size],
            tx_size_high[tx_size], get_bitdepth_data_path_index(xd));
}

void cfl_store_block(MACROBLOCKD *const xd, BLOCK_SIZE bsize, TX_SIZE tx_size) {
  CFL_CTX *const cfl = xd->cfl;
  struct macroblockd_plane *const pd = &xd->plane[AOM_PLANE_Y];
  int row = 0;
  int col = 0;
  bsize = AOMMAX(BLOCK_4X4, bsize);
  if (block_size_high[bsize] == 4 || block_size_wide[bsize] == 4) {
    sub8x8_adjust_offset(cfl, &row, &col);
#if CONFIG_DEBUG
    // Point to the last transform block inside the partition.
    const int off_row =
        row + (mi_size_high[bsize] - tx_size_high_unit[tx_size]);
    const int off_col =
        col + (mi_size_wide[bsize] - tx_size_wide_unit[tx_size]);
    sub8x8_set_val(cfl, off_row, off_col, tx_size);
#endif  // CONFIG_DEBUG
  }
  const int width = max_intra_block_width(xd, bsize, AOM_PLANE_Y, tx_size);
  const int height = max_intra_block_height(xd, bsize, AOM_PLANE_Y, tx_size);
  cfl_store(cfl, pd->dst.buf, pd->dst.stride, row, col, width, height,
            get_bitdepth_data_path_index(xd));
}
