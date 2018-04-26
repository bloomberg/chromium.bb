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

#ifndef AV1_COMMON_LOOPFILTER_H_
#define AV1_COMMON_LOOPFILTER_H_

#include "aom_ports/mem.h"
#include "./aom_config.h"

#include "av1/common/blockd.h"
#include "av1/common/seg_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LOOP_FILTER 63
#define MAX_SHARPNESS 7

#define SIMD_WIDTH 16

enum lf_path {
  LF_PATH_420,
  LF_PATH_444,
  LF_PATH_SLOW,
};

#if LOOP_FILTER_BITMASK
typedef struct {
  uint64_t bits[4];
} FilterMaskY;

typedef uint64_t FilterMaskUV;

// This structure holds bit masks for all 4x4 blocks in a 64x64 region.
// Each 1 bit represents a position in which we want to apply the loop filter.
// For Y plane, 4x4 in 64x64 requires 16x16 = 256 bit, therefore we use 4
// uint64_t; For U, V plane, for 420 format, plane size is 32x32, thus we use
// a uint64_t to represent bitmask.
// Left_ entries refer to whether we apply a filter on the border to the
// left of the block.   Above_ entries refer to whether or not to apply a
// filter on the above border.
// Since each transform is accompanied by a potentially different type of
// loop filter there is a different entry in the array for each transform size.
typedef struct {
  FilterMaskY left_y[TX_SIZES];
  FilterMaskY above_y[TX_SIZES];
  FilterMaskUV left_u[TX_SIZES];
  FilterMaskUV above_u[TX_SIZES];
  FilterMaskUV left_v[TX_SIZES];
  FilterMaskUV above_v[TX_SIZES];

  // Y plane vertical edge and horizontal edge filter level
  uint8_t lfl_y_hor[MI_SIZE_64X64][MI_SIZE_64X64];
  uint8_t lfl_y_ver[MI_SIZE_64X64][MI_SIZE_64X64];

  // UV plane vertical edge and horizontal edge shares the same level
  uint8_t lfl_u[MI_SIZE_64X64 / 2][MI_SIZE_64X64 / 2];
  uint8_t lfl_v[MI_SIZE_64X64 / 2][MI_SIZE_64X64 / 2];
} LoopFilterMaskInfo;
// TODO(chengchen): remove old version of bitmask construction code once
// new bitmask is complete.

// Loopfilter bit mask per super block
#define LOOP_FILTER_MASK_NUM 4
typedef struct {
  LoopFilterMaskInfo lfm_info[LOOP_FILTER_MASK_NUM];
  int is_setup;
} LoopFilterMask;

// To determine whether to apply loop filtering at one transform block edge,
// we need information of the neighboring transform block. Specifically,
// in determining a vertical edge, we need the information of the tx block
// to its left. For a horizontal edge, we need info of the tx block above it.
// Thus, we need to record info of right column and bottom row of tx blocks.
// We record the information of the neighboring superblock, when bitmask
// building for a superblock is finished. And it will be used for next
// superblock bitmask building.
// Information includes:
// ------------------------------------------------------------
//                    MI_SIZE_64X64
// Y  tx_size above |--------------|
// Y  tx_size left  |--------------|
// UV tx_size above |--------------|
// UV tx_size left  |--------------|
// Y level above    |--------------|
// Y level left     |--------------|
// U level above    |--------------|
// U level left     |--------------|
// V level above    |--------------|
// V level left     |--------------|
// skip             |--------------|
// ------------------------------------------------------------
typedef struct {
  TX_SIZE tx_size_y_above[MI_SIZE_64X64];
  TX_SIZE tx_size_y_left[MI_SIZE_64X64];
  TX_SIZE tx_size_uv_above[MI_SIZE_64X64];
  TX_SIZE tx_size_uv_left[MI_SIZE_64X64];
  uint8_t y_level_above[MI_SIZE_64X64];
  uint8_t y_level_left[MI_SIZE_64X64];
  uint8_t u_level_above[MI_SIZE_64X64];
  uint8_t u_level_left[MI_SIZE_64X64];
  uint8_t v_level_above[MI_SIZE_64X64];
  uint8_t v_level_left[MI_SIZE_64X64];
  uint8_t skip[MI_SIZE_64X64];
} LpfSuperblockInfo;
#endif  // LOOP_FILTER_BITMASK

struct loopfilter {
  int filter_level[2];
  int filter_level_u;
  int filter_level_v;

  int sharpness_level;

  uint8_t mode_ref_delta_enabled;
  uint8_t mode_ref_delta_update;

  // 0 = Intra, Last, Last2+Last3,
  // GF, BRF, ARF2, ARF
  int8_t ref_deltas[REF_FRAMES];

  // 0 = ZERO_MV, MV
  int8_t mode_deltas[MAX_MODE_LF_DELTAS];

  int combine_vert_horz_lf;

#if LOOP_FILTER_BITMASK
  LoopFilterMask *lfm;
  size_t lfm_num;
  int lfm_stride;
  LpfSuperblockInfo neighbor_sb_lpf_info;
#endif  // LOOP_FILTER_BITMASK
};

// Need to align this structure so when it is declared and
// passed it can be loaded into vector registers.
typedef struct {
  DECLARE_ALIGNED(SIMD_WIDTH, uint8_t, mblim[SIMD_WIDTH]);
  DECLARE_ALIGNED(SIMD_WIDTH, uint8_t, lim[SIMD_WIDTH]);
  DECLARE_ALIGNED(SIMD_WIDTH, uint8_t, hev_thr[SIMD_WIDTH]);
} loop_filter_thresh;

typedef struct {
  loop_filter_thresh lfthr[MAX_LOOP_FILTER + 1];
  uint8_t lvl[MAX_MB_PLANE][MAX_SEGMENTS][2][REF_FRAMES][MAX_MODE_LF_DELTAS];
} loop_filter_info_n;

/* assorted loopfilter functions which get used elsewhere */
struct AV1Common;
struct macroblockd;
struct AV1LfSyncData;

void av1_loop_filter_init(struct AV1Common *cm);

void av1_loop_filter_frame_init(struct AV1Common *cm, int plane_start,
                                int plane_end);

void av1_loop_filter_frame(YV12_BUFFER_CONFIG *frame, struct AV1Common *cm,
                           struct macroblockd *mbd, int plane_start,
                           int plane_end, int partial_frame);

void av1_filter_block_plane_vert(const struct AV1Common *const cm,
                                 const MACROBLOCKD *const xd, const int plane,
                                 const MACROBLOCKD_PLANE *const plane_ptr,
                                 const uint32_t mi_row, const uint32_t mi_col);

void av1_filter_block_plane_horz(const struct AV1Common *const cm,
                                 const MACROBLOCKD *const xd, const int plane,
                                 const MACROBLOCKD_PLANE *const plane_ptr,
                                 const uint32_t mi_row, const uint32_t mi_col);

typedef struct LoopFilterWorkerData {
  YV12_BUFFER_CONFIG *frame_buffer;
  struct AV1Common *cm;
  struct macroblockd_plane planes[MAX_MB_PLANE];
  // TODO(Ranjit): When the filter functions are modified to use xd->lossless
  // add lossless as a member here.
  MACROBLOCKD *xd;
} LFWorkerData;

#if LOOP_FILTER_BITMASK
INLINE enum lf_path av1_get_loop_filter_path(
    int plane, struct macroblockd_plane pd[MAX_MB_PLANE]);

LoopFilterMask *av1_get_loop_filter_mask(struct AV1Common *const cm, int mi_row,
                                         int mi_col);

void av1_setup_bitmask(struct AV1Common *const cm, int mi_row, int mi_col,
                       int plane, int subsampling_x, int subsampling_y,
                       LoopFilterMask *lfm);

void av1_loop_filter_block_plane_vert(struct AV1Common *const cm,
                                      struct macroblockd_plane *pd, int pl,
                                      int mi_row, int mi_col, enum lf_path path,
                                      LoopFilterMask *lf_mask);

void av1_loop_filter_block_plane_horz(struct AV1Common *const cm,
                                      struct macroblockd_plane *pd, int pl,
                                      int mi_row, int mi_col, enum lf_path path,
                                      LoopFilterMask *lf_mask);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_LOOPFILTER_H_
