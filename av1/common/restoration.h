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

#ifndef AV1_COMMON_RESTORATION_H_
#define AV1_COMMON_RESTORATION_H_

#include "aom_ports/mem.h"
#include "./aom_config.h"

#include "av1/common/blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLIP(x, lo, hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))
#define RINT(x) ((x) < 0 ? (int)((x)-0.5) : (int)((x) + 0.5))

#define RESTORATION_TILESIZE_SML 128
#define RESTORATION_TILESIZE_BIG 256
#define RESTORATION_TILEPELS_MAX \
  (RESTORATION_TILESIZE_BIG * RESTORATION_TILESIZE_BIG * 9 / 4)

#define DOMAINTXFMRF_PARAMS_BITS 6
#define DOMAINTXFMRF_PARAMS (1 << DOMAINTXFMRF_PARAMS_BITS)
#define DOMAINTXFMRF_SIGMA_SCALEBITS 4
#define DOMAINTXFMRF_SIGMA_SCALE (1 << DOMAINTXFMRF_SIGMA_SCALEBITS)
#define DOMAINTXFMRF_ITERS 3
#define DOMAINTXFMRF_VTABLE_PRECBITS 8
#define DOMAINTXFMRF_VTABLE_PREC (1 << DOMAINTXFMRF_VTABLE_PRECBITS)
#define DOMAINTXFMRF_MULT \
  sqrt(((1 << (DOMAINTXFMRF_ITERS * 2)) - 1) * 2.0 / 3.0)
// A single 32 bit buffer needed for the filter
#define DOMAINTXFMRF_TMPBUF_SIZE (RESTORATION_TILEPELS_MAX * sizeof(int32_t))
#define DOMAINTXFMRF_BITS (DOMAINTXFMRF_PARAMS_BITS)

// 6 highprecision 64-bit buffers needed for the filter:
// 1 for the degraded frame, 2 for the restored versions and
// 3 for each restoration operation
// TODO(debargha): Explore if we can use 32-bit buffers
#define SGRPROJ_TMPBUF_SIZE (RESTORATION_TILEPELS_MAX * 6 * sizeof(int64_t))
#define SGRPROJ_PARAMS_BITS 3
#define SGRPROJ_PARAMS (1 << SGRPROJ_PARAMS_BITS)

// Precision bits for projection
#define SGRPROJ_PRJ_BITS 7
// Restoration precision bits generated higher than source before projection
#define SGRPROJ_RST_BITS 4
// Internal precision bits for core selfguided_restoration
#define SGRPROJ_SGR_BITS 8
#define SGRPROJ_SGR (1 << SGRPROJ_SGR_BITS)

#define SGRPROJ_PRJ_MIN0 (-(1 << SGRPROJ_PRJ_BITS) / 4)
#define SGRPROJ_PRJ_MAX0 (SGRPROJ_PRJ_MIN0 + (1 << SGRPROJ_PRJ_BITS) - 1)
#define SGRPROJ_PRJ_MIN1 (-(1 << SGRPROJ_PRJ_BITS) / 4)
#define SGRPROJ_PRJ_MAX1 (SGRPROJ_PRJ_MIN1 + (1 << SGRPROJ_PRJ_BITS) - 1)

#define SGRPROJ_BITS (SGRPROJ_PRJ_BITS * 2 + SGRPROJ_PARAMS_BITS)

// Max of SGRPROJ_TMPBUF_SIZE and DOMAINTXFMRF_TMPBUF_SIZE
#define RESTORATION_TMPBUF_SIZE (SGRPROJ_TMPBUF_SIZE)

#define RESTORATION_HALFWIN 3
#define RESTORATION_HALFWIN1 (RESTORATION_HALFWIN + 1)
#define RESTORATION_WIN (2 * RESTORATION_HALFWIN + 1)
#define RESTORATION_WIN2 ((RESTORATION_WIN) * (RESTORATION_WIN))

#define RESTORATION_FILT_BITS 7
#define RESTORATION_FILT_STEP (1 << RESTORATION_FILT_BITS)

#define WIENER_FILT_TAP0_MINV (-5)
#define WIENER_FILT_TAP1_MINV (-23)
#define WIENER_FILT_TAP2_MINV (-16)

#define WIENER_FILT_TAP0_BITS 4
#define WIENER_FILT_TAP1_BITS 5
#define WIENER_FILT_TAP2_BITS 6

#define WIENER_FILT_BITS \
  ((WIENER_FILT_TAP0_BITS + WIENER_FILT_TAP1_BITS + WIENER_FILT_TAP2_BITS) * 2)

#define WIENER_FILT_TAP0_MAXV \
  (WIENER_FILT_TAP0_MINV - 1 + (1 << WIENER_FILT_TAP0_BITS))
#define WIENER_FILT_TAP1_MAXV \
  (WIENER_FILT_TAP1_MINV - 1 + (1 << WIENER_FILT_TAP1_BITS))
#define WIENER_FILT_TAP2_MAXV \
  (WIENER_FILT_TAP2_MINV - 1 + (1 << WIENER_FILT_TAP2_BITS))

typedef struct {
  int level;
  int vfilter[RESTORATION_WIN], hfilter[RESTORATION_WIN];
} WienerInfo;

typedef struct {
  int r1;
  int e1;
  int r2;
  int e2;
} sgr_params_type;

typedef struct {
  int level;
  int ep;
  int xqd[2];
} SgrprojInfo;

typedef struct {
  int level;
  int sigma_r;
} DomaintxfmrfInfo;

typedef struct {
  RestorationType frame_restoration_type;
  RestorationType *restoration_type;
  // Wiener filter
  WienerInfo *wiener_info;
  // Selfguided proj filter
  SgrprojInfo *sgrproj_info;
  // Domain transform filter
  DomaintxfmrfInfo *domaintxfmrf_info;
} RestorationInfo;

typedef struct {
  RestorationInfo *rsi;
  int keyframe;
  int subsampling_x;
  int subsampling_y;
  int ntiles;
  int tile_width, tile_height;
  int nhtiles, nvtiles;
  uint8_t *tmpbuf;
} RestorationInternal;

static INLINE int get_rest_tilesize(int width, int height) {
  if (width * height <= 352 * 288)
    return RESTORATION_TILESIZE_SML;
  else
    return RESTORATION_TILESIZE_BIG;
}

static INLINE int av1_get_rest_ntiles(int width, int height, int *tile_width,
                                      int *tile_height, int *nhtiles,
                                      int *nvtiles) {
  int nhtiles_, nvtiles_;
  int tile_width_, tile_height_;
  int tilesize = get_rest_tilesize(width, height);
  tile_width_ = (tilesize < 0) ? width : AOMMIN(tilesize, width);
  tile_height_ = (tilesize < 0) ? height : AOMMIN(tilesize, height);
  nhtiles_ = (width + (tile_width_ >> 1)) / tile_width_;
  nvtiles_ = (height + (tile_height_ >> 1)) / tile_height_;
  if (tile_width) *tile_width = tile_width_;
  if (tile_height) *tile_height = tile_height_;
  if (nhtiles) *nhtiles = nhtiles_;
  if (nvtiles) *nvtiles = nvtiles_;
  return (nhtiles_ * nvtiles_);
}

static INLINE void av1_get_rest_tile_limits(
    int tile_idx, int subtile_idx, int subtile_bits, int nhtiles, int nvtiles,
    int tile_width, int tile_height, int im_width, int im_height, int clamp_h,
    int clamp_v, int *h_start, int *h_end, int *v_start, int *v_end) {
  const int htile_idx = tile_idx % nhtiles;
  const int vtile_idx = tile_idx / nhtiles;
  *h_start = htile_idx * tile_width;
  *v_start = vtile_idx * tile_height;
  *h_end = (htile_idx < nhtiles - 1) ? *h_start + tile_width : im_width;
  *v_end = (vtile_idx < nvtiles - 1) ? *v_start + tile_height : im_height;
  if (subtile_bits) {
    const int num_subtiles_1d = (1 << subtile_bits);
    const int subtile_width = (*h_end - *h_start) >> subtile_bits;
    const int subtile_height = (*v_end - *v_start) >> subtile_bits;
    const int subtile_idx_h = subtile_idx & (num_subtiles_1d - 1);
    const int subtile_idx_v = subtile_idx >> subtile_bits;
    *h_start += subtile_idx_h * subtile_width;
    *v_start += subtile_idx_v * subtile_height;
    *h_end = subtile_idx_h == num_subtiles_1d - 1 ? *h_end
                                                  : *h_start + subtile_width;
    *v_end = subtile_idx_v == num_subtiles_1d - 1 ? *v_end
                                                  : *v_start + subtile_height;
  }
  if (clamp_h) {
    *h_start = AOMMAX(*h_start, RESTORATION_HALFWIN);
    *h_end = AOMMIN(*h_end, im_width - RESTORATION_HALFWIN);
  }
  if (clamp_v) {
    *v_start = AOMMAX(*v_start, RESTORATION_HALFWIN);
    *v_end = AOMMIN(*v_end, im_height - RESTORATION_HALFWIN);
  }
}

extern const sgr_params_type sgr_params[SGRPROJ_PARAMS];

int av1_alloc_restoration_struct(RestorationInfo *rst_info, int width,
                                 int height);
void av1_free_restoration_struct(RestorationInfo *rst_info);

void av1_selfguided_restoration(int64_t *dgd, int width, int height, int stride,
                                int bit_depth, int r, int eps, void *tmpbuf);
void av1_domaintxfmrf_restoration(uint8_t *dgd, int width, int height,
                                  int stride, int param, uint8_t *dst,
                                  int dst_stride, int32_t *tmpbuf);
#if CONFIG_AOM_HIGHBITDEPTH
void av1_domaintxfmrf_restoration_highbd(uint16_t *dgd, int width, int height,
                                         int stride, int param, int bit_depth,
                                         uint16_t *dst, int dst_stride,
                                         int32_t *tmpbuf);
#endif  // CONFIG_AOM_HIGHBITDEPTH
void decode_xq(int *xqd, int *xq);
void av1_loop_restoration_init(RestorationInternal *rst, RestorationInfo *rsi,
                               int kf, int width, int height);
void av1_loop_restoration_frame(YV12_BUFFER_CONFIG *frame, struct AV1Common *cm,
                                RestorationInfo *rsi, int y_only,
                                int partial_frame, YV12_BUFFER_CONFIG *dst);
void av1_loop_restoration_rows(YV12_BUFFER_CONFIG *frame, struct AV1Common *cm,
                               int start_mi_row, int end_mi_row, int y_only,
                               YV12_BUFFER_CONFIG *dst);
void av1_loop_restoration_precal();
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_RESTORATION_H_
