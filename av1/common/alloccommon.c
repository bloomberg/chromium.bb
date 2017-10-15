/*
 *
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
#include "aom_mem/aom_mem.h"

#include "av1/common/alloccommon.h"
#include "av1/common/blockd.h"
#include "av1/common/entropymode.h"
#include "av1/common/entropymv.h"
#include "av1/common/onyxc_int.h"

int av1_get_MBs(int width, int height) {
  const int aligned_width = ALIGN_POWER_OF_TWO(width, 3);
  const int aligned_height = ALIGN_POWER_OF_TWO(height, 3);
  const int mi_cols = aligned_width >> MI_SIZE_LOG2;
  const int mi_rows = aligned_height >> MI_SIZE_LOG2;

  const int mb_cols = (mi_cols + 2) >> 2;
  const int mb_rows = (mi_rows + 2) >> 2;
  return mb_rows * mb_cols;
}

void av1_set_mb_mi(AV1_COMMON *cm, int width, int height) {
  // Ensure that the decoded width and height are both multiples of
  // 8 luma pixels (note: this may only be a multiple of 4 chroma pixels if
  // subsampling is used).
  // This simplifies the implementation of various experiments,
  // eg. cdef, which operates on units of 8x8 luma pixels.
  const int aligned_width = ALIGN_POWER_OF_TWO(width, 3);
  const int aligned_height = ALIGN_POWER_OF_TWO(height, 3);

  cm->mi_cols = aligned_width >> MI_SIZE_LOG2;
  cm->mi_rows = aligned_height >> MI_SIZE_LOG2;
  cm->mi_stride = calc_mi_size(cm->mi_cols);

  cm->mb_cols = (cm->mi_cols + 2) >> 2;
  cm->mb_rows = (cm->mi_rows + 2) >> 2;
  cm->MBs = cm->mb_rows * cm->mb_cols;
}

#if !CONFIG_SEGMENT_PRED_LAST
static int alloc_seg_map(AV1_COMMON *cm, int rows, int cols) {
  int i;
  int seg_map_size = rows * cols;

#if CONFIG_Q_SEGMENTATION
  cm->q_seg_map = (uint8_t *)aom_calloc(seg_map_size, 1);
  if (!cm->q_seg_map) return 1;
#endif

  for (i = 0; i < NUM_PING_PONG_BUFFERS; ++i) {
    cm->seg_map_array[i] = (uint8_t *)aom_calloc(seg_map_size, 1);
    if (cm->seg_map_array[i] == NULL) return 1;
  }
  cm->seg_map_alloc_size = seg_map_size;

  // Init the index.
  cm->seg_map_idx = 0;
  cm->prev_seg_map_idx = 1;

  cm->current_frame_seg_map = cm->seg_map_array[cm->seg_map_idx];
  if (!cm->frame_parallel_decode)
    cm->last_frame_seg_map = cm->seg_map_array[cm->prev_seg_map_idx];

  return 0;
}

static void free_seg_map(AV1_COMMON *cm) {
  int i;

#if CONFIG_Q_SEGMENTATION
  aom_free(cm->q_seg_map);
  cm->q_seg_map = NULL;
#endif

  for (i = 0; i < NUM_PING_PONG_BUFFERS; ++i) {
    aom_free(cm->seg_map_array[i]);
    cm->seg_map_array[i] = NULL;
  }

  cm->current_frame_seg_map = NULL;

  if (!cm->frame_parallel_decode) {
    cm->last_frame_seg_map = NULL;
  }
  cm->seg_map_alloc_size = 0;
}
#endif

void av1_free_ref_frame_buffers(BufferPool *pool) {
  int i;

  for (i = 0; i < FRAME_BUFFERS; ++i) {
    if (pool->frame_bufs[i].ref_count > 0 &&
        pool->frame_bufs[i].raw_frame_buffer.data != NULL) {
      pool->release_fb_cb(pool->cb_priv, &pool->frame_bufs[i].raw_frame_buffer);
      pool->frame_bufs[i].ref_count = 0;
    }
    aom_free(pool->frame_bufs[i].mvs);
    pool->frame_bufs[i].mvs = NULL;
#if CONFIG_SEGMENT_PRED_LAST
    aom_free(pool->frame_bufs[i].seg_map);
    pool->frame_bufs[i].seg_map = NULL;
#endif
    aom_free_frame_buffer(&pool->frame_bufs[i].buf);
#if CONFIG_HASH_ME
    av1_hash_table_destroy(&pool->frame_bufs[i].hash_table);
#endif
  }
}

#if CONFIG_LOOP_RESTORATION
// Assumes cm->rst_info[p].restoration_unit_size is already initialized
void av1_alloc_restoration_buffers(AV1_COMMON *cm) {
  for (int p = 0; p < MAX_MB_PLANE; ++p)
    av1_alloc_restoration_struct(cm, &cm->rst_info[p], p > 0);
  aom_free(cm->rst_tmpbuf);
  CHECK_MEM_ERROR(cm, cm->rst_tmpbuf,
                  (int32_t *)aom_memalign(16, RESTORATION_TMPBUF_SIZE));

#if CONFIG_STRIPED_LOOP_RESTORATION
  // For striped loop restoration, we divide each row of tiles into "stripes",
  // of height 64 luma pixels but with an offset by RESTORATION_TILE_OFFSET
  // luma pixels to match the output from CDEF. We will need to store 2 *
  // RESTORATION_CTX_VERT lines of data for each stripe, and also need to be
  // able to quickly answer the question "Where is the <n>'th stripe for tile
  // row <m>?" To make that efficient, we generate the rst_last_stripe array.
  int num_stripes = 0;
  for (int i = 0; i < cm->tile_rows; ++i) {
#if CONFIG_MAX_TILE
    const int sb_h = cm->tile_row_start_sb[i + 1] - cm->tile_row_start_sb[i];
    const int mi_h = sb_h << MAX_MIB_SIZE_LOG2;
#else
    const int mi_h = ((i + 1) < cm->tile_rows)
                         ? cm->tile_height
                         : (cm->mi_rows - i * cm->tile_height);
#endif
    const int ext_h = RESTORATION_TILE_OFFSET + (mi_h << MI_SIZE_LOG2);
    const int tile_stripes = (ext_h + 63) / 64;
    num_stripes += tile_stripes;
    cm->rst_end_stripe[i] = num_stripes;
  }

// Now we need to allocate enough space to store the line buffers for the
// stripes
#if CONFIG_FRAME_SUPERRES
  const int frame_w = cm->superres_upscaled_width;
#else
  const int frame_w = cm->width;
#endif  // CONFIG_FRAME_SUPERRES
#if CONFIG_HIGHBITDEPTH
  const int use_highbd = cm->use_highbitdepth ? 1 : 0;
#else
  const int use_highbd = 0;
#endif
  for (int p = 0; p < MAX_MB_PLANE; ++p) {
    const int is_uv = p > 0;
    const int ss_x = is_uv && cm->subsampling_x;

    const int plane_w = ((frame_w + ss_x) >> ss_x) + 2 * RESTORATION_EXTRA_HORZ;
    const int align_bits = 5;  // align for efficiency
    const int stride = ALIGN_POWER_OF_TWO(plane_w, align_bits);

    const int buf_size = num_stripes * stride * RESTORATION_CTX_VERT
                         << use_highbd;

    RestorationStripeBoundaries *boundaries = &cm->rst_info[p].boundaries;
    aom_free(boundaries->stripe_boundary_above);
    aom_free(boundaries->stripe_boundary_below);

    CHECK_MEM_ERROR(cm, boundaries->stripe_boundary_above,
                    (uint8_t *)aom_memalign(1 << align_bits, buf_size));
    CHECK_MEM_ERROR(cm, boundaries->stripe_boundary_below,
                    (uint8_t *)aom_memalign(1 << align_bits, buf_size));

    boundaries->stripe_boundary_stride = stride;
  }
#endif  // CONFIG_STRIPED_LOOP_RESTORATION
}

void av1_free_restoration_buffers(AV1_COMMON *cm) {
  int p;
  for (p = 0; p < MAX_MB_PLANE; ++p)
    av1_free_restoration_struct(&cm->rst_info[p]);
  aom_free(cm->rst_tmpbuf);
  cm->rst_tmpbuf = NULL;
#if CONFIG_STRIPED_LOOP_RESTORATION
  for (p = 0; p < MAX_MB_PLANE; ++p) {
    RestorationStripeBoundaries *boundaries = &cm->rst_info[p].boundaries;
    aom_free(boundaries->stripe_boundary_above);
    aom_free(boundaries->stripe_boundary_below);
    boundaries->stripe_boundary_above = NULL;
    boundaries->stripe_boundary_below = NULL;
  }
#endif
}
#endif  // CONFIG_LOOP_RESTORATION

void av1_free_context_buffers(AV1_COMMON *cm) {
  int i;
  cm->free_mi(cm);
#if !CONFIG_SEGMENT_PRED_LAST
  free_seg_map(cm);
#endif
  for (i = 0; i < MAX_MB_PLANE; i++) {
    aom_free(cm->above_context[i]);
    cm->above_context[i] = NULL;
  }
  aom_free(cm->above_seg_context);
  cm->above_seg_context = NULL;
  cm->above_context_alloc_cols = 0;
  aom_free(cm->above_txfm_context);
  cm->above_txfm_context = NULL;

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    aom_free(cm->top_txfm_context[i]);
    cm->top_txfm_context[i] = NULL;
  }
}

int av1_alloc_context_buffers(AV1_COMMON *cm, int width, int height) {
  int new_mi_size;

  av1_set_mb_mi(cm, width, height);
  new_mi_size = cm->mi_stride * calc_mi_size(cm->mi_rows);
  if (cm->mi_alloc_size < new_mi_size) {
    cm->free_mi(cm);
    if (cm->alloc_mi(cm, new_mi_size)) goto fail;
  }

#if !CONFIG_SEGMENT_PRED_LAST
  if (cm->seg_map_alloc_size < cm->mi_rows * cm->mi_cols) {
    // Create the segmentation map structure and set to 0.
    free_seg_map(cm);
    if (alloc_seg_map(cm, cm->mi_rows, cm->mi_cols)) goto fail;
  }
#endif

  if (cm->above_context_alloc_cols < cm->mi_cols) {
    // TODO(geza.lore): These are bigger than they need to be.
    // cm->tile_width would be enough but it complicates indexing a
    // little elsewhere.
    const int aligned_mi_cols =
        ALIGN_POWER_OF_TWO(cm->mi_cols, MAX_MIB_SIZE_LOG2);
    int i;

    for (i = 0; i < MAX_MB_PLANE; i++) {
      aom_free(cm->above_context[i]);
      cm->above_context[i] = (ENTROPY_CONTEXT *)aom_calloc(
          aligned_mi_cols << (MI_SIZE_LOG2 - tx_size_wide_log2[0]),
          sizeof(*cm->above_context[0]));
      if (!cm->above_context[i]) goto fail;
    }

    aom_free(cm->above_seg_context);
    cm->above_seg_context = (PARTITION_CONTEXT *)aom_calloc(
        aligned_mi_cols, sizeof(*cm->above_seg_context));
    if (!cm->above_seg_context) goto fail;

    aom_free(cm->above_txfm_context);
    cm->above_txfm_context = (TXFM_CONTEXT *)aom_calloc(
        aligned_mi_cols << TX_UNIT_WIDE_LOG2, sizeof(*cm->above_txfm_context));
    if (!cm->above_txfm_context) goto fail;

    for (i = 0; i < MAX_MB_PLANE; ++i) {
      aom_free(cm->top_txfm_context[i]);
      cm->top_txfm_context[i] =
          (TXFM_CONTEXT *)aom_calloc(aligned_mi_cols << TX_UNIT_WIDE_LOG2,
                                     sizeof(*cm->top_txfm_context[0]));
      if (!cm->top_txfm_context[i]) goto fail;
    }

    cm->above_context_alloc_cols = aligned_mi_cols;
  }

  return 0;

fail:
  // clear the mi_* values to force a realloc on resync
  av1_set_mb_mi(cm, 0, 0);
  av1_free_context_buffers(cm);
  return 1;
}

void av1_remove_common(AV1_COMMON *cm) {
  av1_free_context_buffers(cm);

  aom_free(cm->fc);
  cm->fc = NULL;
  aom_free(cm->frame_contexts);
  cm->frame_contexts = NULL;
}

void av1_init_context_buffers(AV1_COMMON *cm) {
  cm->setup_mi(cm);
#if !CONFIG_SEGMENT_PRED_LAST
  if (cm->last_frame_seg_map && !cm->frame_parallel_decode)
    memset(cm->last_frame_seg_map, 0, cm->mi_rows * cm->mi_cols);
#endif
}
#if !CONFIG_SEGMENT_PRED_LAST
void av1_swap_current_and_last_seg_map(AV1_COMMON *cm) {
  // Swap indices.
  const int tmp = cm->seg_map_idx;
  cm->seg_map_idx = cm->prev_seg_map_idx;
  cm->prev_seg_map_idx = tmp;

  cm->current_frame_seg_map = cm->seg_map_array[cm->seg_map_idx];
  cm->last_frame_seg_map = cm->seg_map_array[cm->prev_seg_map_idx];
}
#endif
