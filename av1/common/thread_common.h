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

#ifndef VP10_COMMON_LOOPFILTER_THREAD_H_
#define VP10_COMMON_LOOPFILTER_THREAD_H_
#include "./vpx_config.h"
#include "av1/common/loopfilter.h"
#include "aom_util/vpx_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

struct VP10Common;
struct FRAME_COUNTS;

// Loopfilter row synchronization
typedef struct VP10LfSyncData {
#if CONFIG_MULTITHREAD
  pthread_mutex_t *mutex_;
  pthread_cond_t *cond_;
#endif
  // Allocate memory to store the loop-filtered superblock index in each row.
  int *cur_sb_col;
  // The optimal sync_range for different resolution and platform should be
  // determined by testing. Currently, it is chosen to be a power-of-2 number.
  int sync_range;
  int rows;

  // Row-based parallel loopfilter data
  LFWorkerData *lfdata;
  int num_workers;
} VP10LfSync;

// Allocate memory for loopfilter row synchronization.
void vp10_loop_filter_alloc(VP10LfSync *lf_sync, struct VP10Common *cm,
                            int rows, int width, int num_workers);

// Deallocate loopfilter synchronization related mutex and data.
void vp10_loop_filter_dealloc(VP10LfSync *lf_sync);

// Multi-threaded loopfilter that uses the tile threads.
void vp10_loop_filter_frame_mt(YV12_BUFFER_CONFIG *frame, struct VP10Common *cm,
                               struct macroblockd_plane planes[MAX_MB_PLANE],
                               int frame_filter_level, int y_only,
                               int partial_frame, VPxWorker *workers,
                               int num_workers, VP10LfSync *lf_sync);

void vp10_accumulate_frame_counts(struct VP10Common *cm,
                                  struct FRAME_COUNTS *counts, int is_dec);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_COMMON_LOOPFILTER_THREAD_H_
