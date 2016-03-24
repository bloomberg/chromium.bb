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

#ifndef VP10_COMMON_ALLOCCOMMON_H_
#define VP10_COMMON_ALLOCCOMMON_H_

#define INVALID_IDX -1  // Invalid buffer index.

#ifdef __cplusplus
extern "C" {
#endif

struct VP10Common;
struct BufferPool;

void vp10_remove_common(struct VP10Common *cm);

int vp10_alloc_context_buffers(struct VP10Common *cm, int width, int height);
void vp10_init_context_buffers(struct VP10Common *cm);
void vp10_free_context_buffers(struct VP10Common *cm);

void vp10_free_ref_frame_buffers(struct BufferPool *pool);

int vp10_alloc_state_buffers(struct VP10Common *cm, int width, int height);
void vp10_free_state_buffers(struct VP10Common *cm);

void vp10_set_mb_mi(struct VP10Common *cm, int width, int height);

void vp10_swap_current_and_last_seg_map(struct VP10Common *cm);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_COMMON_ALLOCCOMMON_H_
