/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_ENCODER_TPL_MODEL_H_
#define AOM_AV1_ENCODER_TPL_MODEL_H_

#ifdef __cplusplus
extern "C" {
#endif

struct AV1_COMP;
struct EncodeFrameParams;
struct EncodeFrameInput;

static INLINE BLOCK_SIZE convert_length_to_bsize(int length) {
  switch (length) {
    case 64: return BLOCK_64X64;
    case 32: return BLOCK_32X32;
    case 16: return BLOCK_16X16;
    case 8: return BLOCK_8X8;
    case 4: return BLOCK_4X4;
    default:
      assert(0 && "Invalid block size for tpl model");
      return BLOCK_16X16;
  }
}

typedef struct AV1TplRowMultiThreadSync {
#if CONFIG_MULTITHREAD
  // Synchronization objects for top-right dependency.
  pthread_mutex_t *mutex_;
  pthread_cond_t *cond_;
#endif
  // Buffer to store the macroblock whose encoding is complete.
  // num_finished_cols[i] stores the number of macroblocks which finished
  // encoding in the ith macroblock row.
  int *num_finished_cols;
  // Number of extra macroblocks of the top row to be complete for encoding
  // of the current macroblock to start. A value of 1 indicates top-right
  // dependency.
  int sync_range;
  // Number of macroblock rows.
  int rows;
  // Number of threads processing the current tile.
  int num_threads_working;
} AV1TplRowMultiThreadSync;

typedef struct AV1TplRowMultiThreadInfo {
  // Row synchronization related function pointers.
  void (*sync_read_ptr)(AV1TplRowMultiThreadSync *const tpl_mt_sync, int r,
                        int c);
  void (*sync_write_ptr)(AV1TplRowMultiThreadSync *const tpl_mt_sync, int r,
                         int c, const int cols);
} AV1TplRowMultiThreadInfo;

int av1_tpl_setup_stats(struct AV1_COMP *cpi, int gop_eval,
                        const struct EncodeFrameParams *const frame_params,
                        const struct EncodeFrameInput *const frame_input);

int av1_tpl_ptr_pos(int mi_row, int mi_col, int stride, uint8_t right_shift);

void av1_tpl_rdmult_setup(struct AV1_COMP *cpi);

void av1_tpl_rdmult_setup_sb(struct AV1_COMP *cpi, MACROBLOCK *const x,
                             BLOCK_SIZE sb_size, int mi_row, int mi_col);
void av1_tpl_row_mt_sync_read_dummy(AV1TplRowMultiThreadSync *const tpl_mt_sync,
                                    int r, int c);
void av1_tpl_row_mt_sync_write_dummy(
    AV1TplRowMultiThreadSync *const tpl_mt_sync, int r, int c, const int cols);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_TPL_MODEL_H_
