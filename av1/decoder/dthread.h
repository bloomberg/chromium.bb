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

#ifndef VP10_DECODER_DTHREAD_H_
#define VP10_DECODER_DTHREAD_H_

#include "./vpx_config.h"
#include "aom_util/vpx_thread.h"
#include "aom/internal/vpx_codec_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

struct VP10Common;
struct VP10Decoder;

// WorkerData for the FrameWorker thread. It contains all the information of
// the worker and decode structures for decoding a frame.
typedef struct FrameWorkerData {
  struct VP10Decoder *pbi;
  const uint8_t *data;
  const uint8_t *data_end;
  size_t data_size;
  void *user_priv;
  int result;
  int worker_id;
  int received_frame;

  // scratch_buffer is used in frame parallel mode only.
  // It is used to make a copy of the compressed data.
  uint8_t *scratch_buffer;
  size_t scratch_buffer_size;

#if CONFIG_MULTITHREAD
  pthread_mutex_t stats_mutex;
  pthread_cond_t stats_cond;
#endif

  int frame_context_ready;  // Current frame's context is ready to read.
  int frame_decoded;        // Finished decoding current frame.
} FrameWorkerData;

void vp10_frameworker_lock_stats(VPxWorker *const worker);
void vp10_frameworker_unlock_stats(VPxWorker *const worker);
void vp10_frameworker_signal_stats(VPxWorker *const worker);

// Wait until ref_buf has been decoded to row in real pixel unit.
// Note: worker may already finish decoding ref_buf and release it in order to
// start decoding next frame. So need to check whether worker is still decoding
// ref_buf.
void vp10_frameworker_wait(VPxWorker *const worker, RefCntBuffer *const ref_buf,
                           int row);

// FrameWorker broadcasts its decoding progress so other workers that are
// waiting on it can resume decoding.
void vp10_frameworker_broadcast(RefCntBuffer *const buf, int row);

// Copy necessary decoding context from src worker to dst worker.
void vp10_frameworker_copy_context(VPxWorker *const dst_worker,
                                   VPxWorker *const src_worker);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_DECODER_DTHREAD_H_
