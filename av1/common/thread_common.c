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

#include "./aom_config.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "av1/common/entropymode.h"
#include "av1/common/thread_common.h"
#include "av1/common/reconinter.h"

// Deallocate lf synchronization related mutex and data
void av1_loop_filter_dealloc(AV1LfSync *lf_sync) {
  if (lf_sync != NULL) {
#if CONFIG_MULTITHREAD
    int i;

    if (lf_sync->mutex_ != NULL) {
      for (i = 0; i < lf_sync->rows; ++i) {
        pthread_mutex_destroy(&lf_sync->mutex_[i]);
      }
      aom_free(lf_sync->mutex_);
    }
    if (lf_sync->cond_ != NULL) {
      for (i = 0; i < lf_sync->rows; ++i) {
        pthread_cond_destroy(&lf_sync->cond_[i]);
      }
      aom_free(lf_sync->cond_);
    }
#endif  // CONFIG_MULTITHREAD
    aom_free(lf_sync->lfdata);
    aom_free(lf_sync->cur_sb_col);
    // clear the structure as the source of this call may be a resize in which
    // case this call will be followed by an _alloc() which may fail.
    av1_zero(*lf_sync);
  }
}

// Accumulate frame counts. FRAME_COUNTS consist solely of 'unsigned int'
// members, so we treat it as an array, and sum over the whole length.
void av1_accumulate_frame_counts(FRAME_COUNTS *acc_counts,
                                 FRAME_COUNTS *counts) {
  unsigned int *const acc = (unsigned int *)acc_counts;
  const unsigned int *const cnt = (unsigned int *)counts;

  const unsigned int n_counts = sizeof(FRAME_COUNTS) / sizeof(unsigned int);
  unsigned int i;

  for (i = 0; i < n_counts; i++) acc[i] += cnt[i];
}
