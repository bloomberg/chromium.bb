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

#ifndef AV1_DECODER_DECODER_H_
#define AV1_DECODER_DECODER_H_

#include "./aom_config.h"

#include "aom/aom_codec.h"
#include "aom_dsp/bitreader.h"
#include "aom_scale/yv12config.h"
#include "aom_util/aom_thread.h"

#include "av1/common/thread_common.h"
#include "av1/common/onyxc_int.h"
#include "av1/decoder/dthread.h"

#ifdef __cplusplus
extern "C" {
#endif

// TODO(hkuang): combine this with TileWorkerData.
typedef struct TileData {
  AV1_COMMON *cm;
  aom_reader bit_reader;
  DECLARE_ALIGNED(16, MACROBLOCKD, xd);
  /* dqcoeff are shared by all the planes. So planes must be decoded serially */
  DECLARE_ALIGNED(16, tran_low_t, dqcoeff[32 * 32]);
  DECLARE_ALIGNED(16, uint8_t, color_index_map[2][64 * 64]);
} TileData;

typedef struct TileWorkerData {
  struct AV1Decoder *pbi;
  aom_reader bit_reader;
  FRAME_COUNTS counts;
  DECLARE_ALIGNED(16, MACROBLOCKD, xd);
  /* dqcoeff are shared by all the planes. So planes must be decoded serially */
  DECLARE_ALIGNED(16, tran_low_t, dqcoeff[32 * 32]);
  DECLARE_ALIGNED(16, uint8_t, color_index_map[2][64 * 64]);
  struct aom_internal_error_info error_info;
} TileWorkerData;

typedef struct AV1Decoder {
  DECLARE_ALIGNED(16, MACROBLOCKD, mb);

  DECLARE_ALIGNED(16, AV1_COMMON, common);

  int ready_for_new_data;

  int refresh_frame_flags;

  // TODO(hkuang): Combine this with cur_buf in macroblockd as they are
  // the same.
  RefCntBuffer *cur_buf;  //  Current decoding frame buffer.

  AVxWorker *frame_worker_owner;  // frame_worker that owns this pbi.
  AVxWorker lf_worker;
  AVxWorker *tile_workers;
  TileWorkerData *tile_worker_data;
  TileInfo *tile_worker_info;
  int num_tile_workers;

  TileData *tile_data;
  int total_tiles;

  AV1LfSync lf_row_sync;

  aom_decrypt_cb decrypt_cb;
  void *decrypt_state;

  int max_threads;
  int inv_tile_order;
  int need_resync;   // wait for key/intra-only frame.
  int hold_ref_buf;  // hold the reference buffer.
} AV1Decoder;

int av1_receive_compressed_data(struct AV1Decoder *pbi, size_t size,
                                 const uint8_t **dest);

int av1_get_raw_frame(struct AV1Decoder *pbi, YV12_BUFFER_CONFIG *sd);

aom_codec_err_t av1_copy_reference_dec(struct AV1Decoder *pbi,
                                        AOM_REFFRAME ref_frame_flag,
                                        YV12_BUFFER_CONFIG *sd);

aom_codec_err_t av1_set_reference_dec(AV1_COMMON *cm,
                                       AOM_REFFRAME ref_frame_flag,
                                       YV12_BUFFER_CONFIG *sd);

static INLINE uint8_t read_marker(aom_decrypt_cb decrypt_cb,
                                  void *decrypt_state, const uint8_t *data) {
  if (decrypt_cb) {
    uint8_t marker;
    decrypt_cb(decrypt_state, data, &marker, 1);
    return marker;
  }
  return *data;
}

// This function is exposed for use in tests, as well as the inlined function
// "read_marker".
aom_codec_err_t av1_parse_superframe_index(const uint8_t *data, size_t data_sz,
                                            uint32_t sizes[8], int *count,
                                            aom_decrypt_cb decrypt_cb,
                                            void *decrypt_state);

struct AV1Decoder *av1_decoder_create(BufferPool *const pool);

void av1_decoder_remove(struct AV1Decoder *pbi);

static INLINE void decrease_ref_count(int idx, RefCntBuffer *const frame_bufs,
                                      BufferPool *const pool) {
  if (idx >= 0) {
    --frame_bufs[idx].ref_count;
    // A worker may only get a free framebuffer index when calling get_free_fb.
    // But the private buffer is not set up until finish decoding header.
    // So any error happens during decoding header, the frame_bufs will not
    // have valid priv buffer.
    if (frame_bufs[idx].ref_count == 0 &&
        frame_bufs[idx].raw_frame_buffer.priv) {
      pool->release_fb_cb(pool->cb_priv, &frame_bufs[idx].raw_frame_buffer);
    }
  }
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_DECODER_DECODER_H_
