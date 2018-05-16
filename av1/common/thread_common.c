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
#include "av1/common/av1_loopfilter.h"
#include "av1/common/entropymode.h"
#include "av1/common/thread_common.h"
#include "av1/common/reconinter.h"

// Set up nsync by width.
static INLINE int get_sync_range(int width) {
  // nsync numbers are picked by testing. For example, for 4k
  // video, using 4 gives best performance.
  if (width < 640)
    return 1;
  else if (width <= 1280)
    return 2;
  else if (width <= 4096)
    return 4;
  else
    return 8;
}

// Allocate memory for lf row synchronization
static void loop_filter_alloc(AV1LfSync *lf_sync, AV1_COMMON *cm, int rows,
                              int width, int num_workers) {
  lf_sync->rows = rows;
#if CONFIG_MULTITHREAD
  {
    int i, j;

    for (j = 0; j < MAX_MB_PLANE; j++) {
      CHECK_MEM_ERROR(cm, lf_sync->mutex_[j],
                      aom_malloc(sizeof(*(lf_sync->mutex_[j])) * rows));
      if (lf_sync->mutex_[j]) {
        for (i = 0; i < rows; ++i) {
          pthread_mutex_init(&lf_sync->mutex_[j][i], NULL);
        }
      }

      CHECK_MEM_ERROR(cm, lf_sync->cond_[j],
                      aom_malloc(sizeof(*(lf_sync->cond_[j])) * rows));
      if (lf_sync->cond_[j]) {
        for (i = 0; i < rows; ++i) {
          pthread_cond_init(&lf_sync->cond_[j][i], NULL);
        }
      }
    }

    CHECK_MEM_ERROR(cm, lf_sync->job_mutex,
                    aom_malloc(sizeof(*(lf_sync->job_mutex))));
    if (lf_sync->job_mutex) {
      pthread_mutex_init(lf_sync->job_mutex, NULL);
    }
  }
#endif  // CONFIG_MULTITHREAD
  CHECK_MEM_ERROR(cm, lf_sync->lfdata,
                  aom_malloc(num_workers * sizeof(*(lf_sync->lfdata))));
  lf_sync->num_workers = num_workers;

  for (int j = 0; j < MAX_MB_PLANE; j++) {
    CHECK_MEM_ERROR(cm, lf_sync->cur_sb_col[j],
                    aom_malloc(sizeof(*(lf_sync->cur_sb_col[j])) * rows));
  }
  CHECK_MEM_ERROR(
      cm, lf_sync->job_queue,
      aom_malloc(sizeof(*(lf_sync->job_queue)) * rows * MAX_MB_PLANE * 2));
  // Set up nsync.
  lf_sync->sync_range = get_sync_range(width);
}

// Deallocate lf synchronization related mutex and data
void av1_loop_filter_dealloc(AV1LfSync *lf_sync) {
  if (lf_sync != NULL) {
    int j;
#if CONFIG_MULTITHREAD
    int i;
    for (j = 0; j < MAX_MB_PLANE; j++) {
      if (lf_sync->mutex_[j] != NULL) {
        for (i = 0; i < lf_sync->rows; ++i) {
          pthread_mutex_destroy(&lf_sync->mutex_[j][i]);
        }
        aom_free(lf_sync->mutex_[j]);
      }
      if (lf_sync->cond_[j] != NULL) {
        for (i = 0; i < lf_sync->rows; ++i) {
          pthread_cond_destroy(&lf_sync->cond_[j][i]);
        }
        aom_free(lf_sync->cond_[j]);
      }
    }
    if (lf_sync->job_mutex != NULL) {
      pthread_mutex_destroy(lf_sync->job_mutex);
      aom_free(lf_sync->job_mutex);
    }
#endif  // CONFIG_MULTITHREAD
    aom_free(lf_sync->lfdata);
    for (j = 0; j < MAX_MB_PLANE; j++) {
      aom_free(lf_sync->cur_sb_col[j]);
    }

    aom_free(lf_sync->job_queue);
    // clear the structure as the source of this call may be a resize in which
    // case this call will be followed by an _alloc() which may fail.
    av1_zero(*lf_sync);
  }
}

static void loop_filter_data_reset(LFWorkerData *lf_data,
                                   YV12_BUFFER_CONFIG *frame_buffer,
                                   struct AV1Common *cm, MACROBLOCKD *xd) {
  struct macroblockd_plane *pd = xd->plane;
  lf_data->frame_buffer = frame_buffer;
  lf_data->cm = cm;
  lf_data->xd = xd;
  for (int i = 0; i < MAX_MB_PLANE; i++) {
    memcpy(&lf_data->planes[i].dst, &pd[i].dst, sizeof(lf_data->planes[i].dst));
    lf_data->planes[i].subsampling_x = pd[i].subsampling_x;
    lf_data->planes[i].subsampling_y = pd[i].subsampling_y;
  }
}

static INLINE void sync_read(AV1LfSync *const lf_sync, int r, int c,
                             int plane) {
#if CONFIG_MULTITHREAD
  const int nsync = lf_sync->sync_range;

  if (r && !(c & (nsync - 1))) {
    pthread_mutex_t *const mutex = &lf_sync->mutex_[plane][r - 1];
    pthread_mutex_lock(mutex);

    while (c > lf_sync->cur_sb_col[plane][r - 1] - nsync) {
      pthread_cond_wait(&lf_sync->cond_[plane][r - 1], mutex);
    }
    pthread_mutex_unlock(mutex);
  }
#else
  (void)lf_sync;
  (void)r;
  (void)c;
  (void)plane;
#endif  // CONFIG_MULTITHREAD
}

static INLINE void sync_write(AV1LfSync *const lf_sync, int r, int c,
                              const int sb_cols, int plane) {
#if CONFIG_MULTITHREAD
  const int nsync = lf_sync->sync_range;
  int cur;
  // Only signal when there are enough filtered SB for next row to run.
  int sig = 1;

  if (c < sb_cols - 1) {
    cur = c;
    if (c % nsync) sig = 0;
  } else {
    cur = sb_cols + nsync;
  }

  if (sig) {
    pthread_mutex_lock(&lf_sync->mutex_[plane][r]);

    lf_sync->cur_sb_col[plane][r] = cur;

    pthread_cond_broadcast(&lf_sync->cond_[plane][r]);
    pthread_mutex_unlock(&lf_sync->mutex_[plane][r]);
  }
#else
  (void)lf_sync;
  (void)r;
  (void)c;
  (void)sb_cols;
  (void)plane;
#endif  // CONFIG_MULTITHREAD
}

static void enqueue_lf_jobs(AV1LfSync *lf_sync, AV1_COMMON *cm, int start,
                            int stop, int plane_start, int plane_end) {
  int mi_row, plane, dir;
  AV1LfMTInfo *lf_job_queue = lf_sync->job_queue;
  lf_sync->jobs_enqueued = 0;
  lf_sync->jobs_dequeued = 0;

  for (dir = 0; dir < 2; dir++) {
    for (plane = plane_start; plane < plane_end; plane++) {
      if (plane == 0 && !(cm->lf.filter_level[0]) && !(cm->lf.filter_level[1]))
        break;
      else if (plane == 1 && !(cm->lf.filter_level_u))
        continue;
      else if (plane == 2 && !(cm->lf.filter_level_v))
        continue;
      for (mi_row = start; mi_row < stop; mi_row += MAX_MIB_SIZE) {
        lf_job_queue->mi_row = mi_row;
        lf_job_queue->plane = plane;
        lf_job_queue->dir = dir;
        lf_job_queue++;
        lf_sync->jobs_enqueued++;
      }
    }
  }
}

AV1LfMTInfo *get_lf_job_info(AV1LfSync *lf_sync) {
  AV1LfMTInfo *cur_job_info = NULL;

#if CONFIG_MULTITHREAD
  pthread_mutex_lock(lf_sync->job_mutex);

  if (lf_sync->jobs_dequeued < lf_sync->jobs_enqueued) {
    cur_job_info = lf_sync->job_queue + lf_sync->jobs_dequeued;
    lf_sync->jobs_dequeued++;
  }

  pthread_mutex_unlock(lf_sync->job_mutex);
#else
  (void)lf_sync;
#endif

  return cur_job_info;
}

// Implement row loopfiltering for each thread.
static INLINE void thread_loop_filter_rows(
    const YV12_BUFFER_CONFIG *const frame_buffer, AV1_COMMON *const cm,
    struct macroblockd_plane *planes, MACROBLOCKD *xd,
    AV1LfSync *const lf_sync) {
  const int sb_cols =
      ALIGN_POWER_OF_TWO(cm->mi_cols, MAX_MIB_SIZE_LOG2) >> MAX_MIB_SIZE_LOG2;
  int mi_row, mi_col, plane, dir;
  int r, c;

  while (1) {
    AV1LfMTInfo *cur_job_info = get_lf_job_info(lf_sync);

    if (cur_job_info != NULL) {
      mi_row = cur_job_info->mi_row;
      plane = cur_job_info->plane;
      dir = cur_job_info->dir;
      r = mi_row >> MAX_MIB_SIZE_LOG2;

#if LOOP_FILTER_BITMASK
      enum lf_path path = av1_get_loop_filter_path(plane, planes);
#endif

      if (dir == 0) {
        for (mi_col = 0; mi_col < cm->mi_cols; mi_col += MAX_MIB_SIZE) {
          c = mi_col >> MAX_MIB_SIZE_LOG2;

          av1_setup_dst_planes(planes, cm->seq_params.sb_size, frame_buffer,
                               mi_row, mi_col, plane, plane + 1);

#if LOOP_FILTER_BITMASK
          LoopFilterMask *lf_mask =
              av1_get_loop_filter_mask(cm, mi_row, mi_col);
          av1_setup_bitmask(cm, mi_row, mi_col, plane,
                            planes[plane].subsampling_x,
                            planes[plane].subsampling_y, lf_mask);
          av1_loop_filter_block_plane_vert(cm, planes, plane, mi_row, mi_col,
                                           path, lf_mask);
#else
          av1_filter_block_plane_vert(cm, xd, plane, &planes[plane], mi_row,
                                      mi_col);
#endif
          sync_write(lf_sync, r, c, sb_cols, plane);
        }
      } else if (dir == 1) {
        for (mi_col = 0; mi_col < cm->mi_cols; mi_col += MAX_MIB_SIZE) {
          c = mi_col >> MAX_MIB_SIZE_LOG2;

          // Wait for vertical edge filtering of the top-right block to be
          // completed
          sync_read(lf_sync, r, c, plane);

          // Wait for vertical edge filtering of the right block to be
          // completed
          sync_read(lf_sync, r + 1, c, plane);

          av1_setup_dst_planes(planes, cm->seq_params.sb_size, frame_buffer,
                               mi_row, mi_col, plane, plane + 1);
#if LOOP_FILTER_BITMASK
          LoopFilterMask *lf_mask =
              av1_get_loop_filter_mask(cm, mi_row, mi_col);
          av1_loop_filter_block_plane_horz(cm, planes, plane, mi_row, mi_col,
                                           path, lf_mask);
#else
          av1_filter_block_plane_horz(cm, xd, plane, &planes[plane], mi_row,
                                      mi_col);
#endif
        }
      }
    } else {
      break;
    }
  }
}

// Row-based multi-threaded loopfilter hook
static int loop_filter_row_worker(AV1LfSync *const lf_sync,
                                  LFWorkerData *const lf_data) {
  thread_loop_filter_rows(lf_data->frame_buffer, lf_data->cm, lf_data->planes,
                          lf_data->xd, lf_sync);
  return 1;
}

static void loop_filter_rows_mt(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                                MACROBLOCKD *xd, int start, int stop,
                                int plane_start, int plane_end,
                                AVxWorker *workers, int nworkers,
                                AV1LfSync *lf_sync) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  // Number of superblock rows and cols
  const int sb_rows =
      ALIGN_POWER_OF_TWO(cm->mi_rows, MAX_MIB_SIZE_LOG2) >> MAX_MIB_SIZE_LOG2;
  const int num_workers = nworkers;
  int i;

  if (!lf_sync->sync_range || sb_rows != lf_sync->rows ||
      num_workers > lf_sync->num_workers) {
    av1_loop_filter_dealloc(lf_sync);
    loop_filter_alloc(lf_sync, cm, sb_rows, cm->width, num_workers);
  }

  // Initialize cur_sb_col to -1 for all SB rows.
  for (i = 0; i < MAX_MB_PLANE; i++) {
    memset(lf_sync->cur_sb_col[i], -1,
           sizeof(*(lf_sync->cur_sb_col[i])) * sb_rows);
  }

  enqueue_lf_jobs(lf_sync, cm, start, stop, plane_start, plane_end);

  // Set up loopfilter thread data.
  for (i = 0; i < num_workers; ++i) {
    AVxWorker *const worker = &workers[i];
    LFWorkerData *const lf_data = &lf_sync->lfdata[i];

    worker->hook = (AVxWorkerHook)loop_filter_row_worker;
    worker->data1 = lf_sync;
    worker->data2 = lf_data;

    // Loopfilter data
    loop_filter_data_reset(lf_data, frame, cm, xd);

    // Start loopfiltering
    if (i == num_workers - 1) {
      winterface->execute(worker);
    } else {
      winterface->launch(worker);
    }
  }

  // Wait till all rows are finished
  for (i = 0; i < num_workers; ++i) {
    winterface->sync(&workers[i]);
  }
}

void av1_loop_filter_frame_mt(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                              MACROBLOCKD *xd, int plane_start, int plane_end,
                              int partial_frame, AVxWorker *workers,
                              int num_workers, AV1LfSync *lf_sync) {
  int start_mi_row, end_mi_row, mi_rows_to_filter;

  start_mi_row = 0;
  mi_rows_to_filter = cm->mi_rows;
  if (partial_frame && cm->mi_rows > 8) {
    start_mi_row = cm->mi_rows >> 1;
    start_mi_row &= 0xfffffff8;
    mi_rows_to_filter = AOMMAX(cm->mi_rows / 8, 8);
  }
  end_mi_row = start_mi_row + mi_rows_to_filter;
  av1_loop_filter_frame_init(cm, plane_start, plane_end);

  loop_filter_rows_mt(frame, cm, xd, start_mi_row, end_mi_row, plane_start,
                      plane_end, workers, num_workers, lf_sync);
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
