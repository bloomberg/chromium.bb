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

#define RESTORATION_PROC_UNIT_SIZE 64

#if CONFIG_STRIPED_LOOP_RESTORATION
// Filter tile grid offset upwards compared to the superblock grid
#define RESTORATION_TILE_OFFSET 8
#endif  // CONFIG_STRIPED_LOOP_RESTORATION

#if CONFIG_STRIPED_LOOP_RESTORATION
#define SGRPROJ_BORDER_VERT 3  // Vertical border used for Sgr
#else
#define SGRPROJ_BORDER_VERT 3  // Vertical border used for Sgr
#endif                         // CONFIG_STRIPED_LOOP_RESTORATION
#define SGRPROJ_BORDER_HORZ 3  // Horizontal border used for Sgr

#if CONFIG_STRIPED_LOOP_RESTORATION
#define WIENER_BORDER_VERT 2  // Vertical border used for Wiener
#else
#define WIENER_BORDER_VERT 3  // Vertical border used for Wiener
#endif                        // CONFIG_STRIPED_LOOP_RESTORATION
#define WIENER_HALFWIN 3
#define WIENER_BORDER_HORZ (WIENER_HALFWIN)  // Horizontal border for Wiener

// RESTORATION_BORDER_VERT determines line buffer requirement for LR.
// Should be set at the max of SGRPROJ_BORDER_VERT and WIENER_BORDER_VERT.
// Note the line buffer needed is twice the value of this macro.
#if SGRPROJ_BORDER_VERT >= WIENER_BORDER_VERT
#define RESTORATION_BORDER_VERT (SGRPROJ_BORDER_VERT)
#else
#define RESTORATION_BORDER_VERT (WIENER_BORDER_VERT)
#endif  // SGRPROJ_BORDER_VERT >= WIENER_BORDER_VERT

#if SGRPROJ_BORDER_HORZ >= WIENER_BORDER_HORZ
#define RESTORATION_BORDER_HORZ (SGRPROJ_BORDER_HORZ)
#else
#define RESTORATION_BORDER_HORZ (WIENER_BORDER_HORZ)
#endif  // SGRPROJ_BORDER_VERT >= WIENER_BORDER_VERT

// How many border pixels do we need for each processing unit?
#define RESTORATION_BORDER 3

#if CONFIG_STRIPED_LOOP_RESTORATION
// How many rows of deblocked pixels do we save above/below each processing
// stripe?
#define RESTORATION_CTX_VERT 2

// Additional pixels to the left and right in above/below buffers
// It is RESTORATION_BORDER_HORZ rounded up to get nicer buffer alignment
#define RESTORATION_EXTRA_HORZ 4
#endif  // CONFIG_STRIPED_LOOP_RESTORATION

// Pad up to 20 more (may be much less is needed)
#define RESTORATION_PADDING 20
#define RESTORATION_PROC_UNIT_PELS                             \
  ((RESTORATION_PROC_UNIT_SIZE + RESTORATION_BORDER_HORZ * 2 + \
    RESTORATION_PADDING) *                                     \
   (RESTORATION_PROC_UNIT_SIZE + RESTORATION_BORDER_VERT * 2 + \
    RESTORATION_PADDING))

#define RESTORATION_TILESIZE_MAX 256
#if CONFIG_STRIPED_LOOP_RESTORATION
#define RESTORATION_TILEPELS_HORZ_MAX \
  (RESTORATION_TILESIZE_MAX * 3 / 2 + 2 * RESTORATION_BORDER_HORZ + 16)
#define RESTORATION_TILEPELS_VERT_MAX                                \
  ((RESTORATION_TILESIZE_MAX * 3 / 2 + 2 * RESTORATION_BORDER_VERT + \
    RESTORATION_TILE_OFFSET))
#define RESTORATION_TILEPELS_MAX \
  (RESTORATION_TILEPELS_HORZ_MAX * RESTORATION_TILEPELS_VERT_MAX)
#else
#define RESTORATION_TILEPELS_MAX                                           \
  ((RESTORATION_TILESIZE_MAX * 3 / 2 + 2 * RESTORATION_BORDER_HORZ + 16) * \
   (RESTORATION_TILESIZE_MAX * 3 / 2 + 2 * RESTORATION_BORDER_VERT))
#endif  // CONFIG_STRIPED_LOOP_RESTORATION

// Two 32-bit buffers needed for the restored versions from two filters
// TODO(debargha, rupert): Refactor to not need the large tilesize to be stored
// on the decoder side.
#define SGRPROJ_TMPBUF_SIZE (RESTORATION_TILEPELS_MAX * 2 * sizeof(int32_t))

#define SGRPROJ_EXTBUF_SIZE (0)
#define SGRPROJ_PARAMS_BITS 4
#define SGRPROJ_PARAMS (1 << SGRPROJ_PARAMS_BITS)

// Precision bits for projection
#define SGRPROJ_PRJ_BITS 7
// Restoration precision bits generated higher than source before projection
#define SGRPROJ_RST_BITS 4
// Internal precision bits for core selfguided_restoration
#define SGRPROJ_SGR_BITS 8
#define SGRPROJ_SGR (1 << SGRPROJ_SGR_BITS)

#define SGRPROJ_PRJ_MIN0 (-(1 << SGRPROJ_PRJ_BITS) * 3 / 4)
#define SGRPROJ_PRJ_MAX0 (SGRPROJ_PRJ_MIN0 + (1 << SGRPROJ_PRJ_BITS) - 1)
#define SGRPROJ_PRJ_MIN1 (-(1 << SGRPROJ_PRJ_BITS) / 4)
#define SGRPROJ_PRJ_MAX1 (SGRPROJ_PRJ_MIN1 + (1 << SGRPROJ_PRJ_BITS) - 1)

#define SGRPROJ_PRJ_SUBEXP_K 4

#define SGRPROJ_BITS (SGRPROJ_PRJ_BITS * 2 + SGRPROJ_PARAMS_BITS)

#define MAX_RADIUS 2  // Only 1, 2, 3 allowed
#define MAX_EPS 80    // Max value of eps
#define MAX_NELEM ((2 * MAX_RADIUS + 1) * (2 * MAX_RADIUS + 1))
#define SGRPROJ_MTABLE_BITS 20
#define SGRPROJ_RECIP_BITS 12

#define WIENER_HALFWIN1 (WIENER_HALFWIN + 1)
#define WIENER_WIN (2 * WIENER_HALFWIN + 1)
#define WIENER_WIN2 ((WIENER_WIN) * (WIENER_WIN))
#define WIENER_TMPBUF_SIZE (0)
#define WIENER_EXTBUF_SIZE (0)

// If WIENER_WIN_CHROMA == WIENER_WIN - 2, that implies 5x5 filters are used for
// chroma. To use 7x7 for chroma set WIENER_WIN_CHROMA to WIENER_WIN.
#define WIENER_WIN_CHROMA (WIENER_WIN - 2)

#define WIENER_FILT_PREC_BITS 7
#define WIENER_FILT_STEP (1 << WIENER_FILT_PREC_BITS)

// Whether to use high intermediate precision filtering
#define USE_WIENER_HIGH_INTERMEDIATE_PRECISION 1

// Central values for the taps
#define WIENER_FILT_TAP0_MIDV (3)
#define WIENER_FILT_TAP1_MIDV (-7)
#define WIENER_FILT_TAP2_MIDV (15)
#define WIENER_FILT_TAP3_MIDV                           \
  (WIENER_FILT_STEP -                                   \
   2 * (WIENER_FILT_TAP0_MIDV + WIENER_FILT_TAP1_MIDV + \
        WIENER_FILT_TAP2_MIDV))

#define WIENER_FILT_TAP0_BITS 4
#define WIENER_FILT_TAP1_BITS 5
#define WIENER_FILT_TAP2_BITS 6

#define WIENER_FILT_BITS \
  ((WIENER_FILT_TAP0_BITS + WIENER_FILT_TAP1_BITS + WIENER_FILT_TAP2_BITS) * 2)

#define WIENER_FILT_TAP0_MINV \
  (WIENER_FILT_TAP0_MIDV - (1 << WIENER_FILT_TAP0_BITS) / 2)
#define WIENER_FILT_TAP1_MINV \
  (WIENER_FILT_TAP1_MIDV - (1 << WIENER_FILT_TAP1_BITS) / 2)
#define WIENER_FILT_TAP2_MINV \
  (WIENER_FILT_TAP2_MIDV - (1 << WIENER_FILT_TAP2_BITS) / 2)

#define WIENER_FILT_TAP0_MAXV \
  (WIENER_FILT_TAP0_MIDV - 1 + (1 << WIENER_FILT_TAP0_BITS) / 2)
#define WIENER_FILT_TAP1_MAXV \
  (WIENER_FILT_TAP1_MIDV - 1 + (1 << WIENER_FILT_TAP1_BITS) / 2)
#define WIENER_FILT_TAP2_MAXV \
  (WIENER_FILT_TAP2_MIDV - 1 + (1 << WIENER_FILT_TAP2_BITS) / 2)

#define WIENER_FILT_TAP0_SUBEXP_K 1
#define WIENER_FILT_TAP1_SUBEXP_K 2
#define WIENER_FILT_TAP2_SUBEXP_K 3

// Max of SGRPROJ_TMPBUF_SIZE, DOMAINTXFMRF_TMPBUF_SIZE, WIENER_TMPBUF_SIZE
#define RESTORATION_TMPBUF_SIZE (SGRPROJ_TMPBUF_SIZE)

// Max of SGRPROJ_EXTBUF_SIZE, WIENER_EXTBUF_SIZE
#define RESTORATION_EXTBUF_SIZE (WIENER_EXTBUF_SIZE)

// Check the assumptions of the existing code
#if SUBPEL_TAPS != WIENER_WIN + 1
#error "Wiener filter currently only works if SUBPEL_TAPS == WIENER_WIN + 1"
#endif
#if WIENER_FILT_PREC_BITS != 7
#error "Wiener filter currently only works if WIENER_FILT_PREC_BITS == 7"
#endif

typedef struct {
  int r1;
  int e1;
  int r2;
  int e2;
} sgr_params_type;

typedef struct {
  RestorationType restoration_type;
  WienerInfo wiener_info;
  SgrprojInfo sgrproj_info;
} RestorationUnitInfo;

#if CONFIG_STRIPED_LOOP_RESTORATION
// A restoration line buffer needs space for two lines plus a horizontal filter
// margin of RESTORATION_EXTRA_HORZ on each side.
#define RESTORATION_LINEBUFFER_WIDTH \
  (RESTORATION_TILESIZE_MAX * 3 / 2 + 2 * RESTORATION_EXTRA_HORZ)

typedef struct {
  // Temporary buffers to save/restore 3 lines above/below the restoration
  // stripe.
  uint16_t tmp_save_above[RESTORATION_BORDER][RESTORATION_LINEBUFFER_WIDTH];
  uint16_t tmp_save_below[RESTORATION_BORDER][RESTORATION_LINEBUFFER_WIDTH];
#if CONFIG_LOOPFILTERING_ACROSS_TILES
  // Column buffers, for storing 3 pixels at the left/right of each tile
  // when loopfiltering across tiles is disabled.
  //
  // Note: These arrays only need to store the pixels immediately left/right
  // of each processing unit; the corner pixels (top-left, etc.) are always
  // stored into the above/below arrays.
  uint16_t tmp_save_left[RESTORATION_BORDER][RESTORATION_PROC_UNIT_SIZE];
  uint16_t tmp_save_right[RESTORATION_BORDER][RESTORATION_PROC_UNIT_SIZE];
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
} RestorationLineBuffers;

typedef struct {
  uint8_t *stripe_boundary_above;
  uint8_t *stripe_boundary_below;
  int stripe_boundary_stride;
} RestorationStripeBoundaries;
#endif  // CONFIG_STRIPED_LOOP_RESTORATION

typedef struct {
  RestorationType frame_restoration_type;
  int restoration_unit_size;

  // Fields below here are allocated and initialised by
  // av1_alloc_restoration_struct. (horz_)units_per_tile give the number of
  // restoration units in (one row of) the largest tile in the frame. The data
  // in unit_info is laid out with units_per_tile entries for each tile, which
  // have stride horz_units_per_tile.
  //
  // Even if there are tiles of different sizes, the data in unit_info is laid
  // out as if all tiles are of full size.
  int units_per_tile;
  int vert_units_per_tile, horz_units_per_tile;
  RestorationUnitInfo *unit_info;
#if CONFIG_STRIPED_LOOP_RESTORATION
  RestorationStripeBoundaries boundaries;
#endif  // CONFIG_STRIPED_LOOP_RESTORATION
} RestorationInfo;

static INLINE void set_default_sgrproj(SgrprojInfo *sgrproj_info) {
  sgrproj_info->xqd[0] = (SGRPROJ_PRJ_MIN0 + SGRPROJ_PRJ_MAX0) / 2;
  sgrproj_info->xqd[1] = (SGRPROJ_PRJ_MIN1 + SGRPROJ_PRJ_MAX1) / 2;
}

static INLINE void set_default_wiener(WienerInfo *wiener_info) {
  wiener_info->vfilter[0] = wiener_info->hfilter[0] = WIENER_FILT_TAP0_MIDV;
  wiener_info->vfilter[1] = wiener_info->hfilter[1] = WIENER_FILT_TAP1_MIDV;
  wiener_info->vfilter[2] = wiener_info->hfilter[2] = WIENER_FILT_TAP2_MIDV;
  wiener_info->vfilter[WIENER_HALFWIN] = wiener_info->hfilter[WIENER_HALFWIN] =
      -2 *
      (WIENER_FILT_TAP2_MIDV + WIENER_FILT_TAP1_MIDV + WIENER_FILT_TAP0_MIDV);
  wiener_info->vfilter[4] = wiener_info->hfilter[4] = WIENER_FILT_TAP2_MIDV;
  wiener_info->vfilter[5] = wiener_info->hfilter[5] = WIENER_FILT_TAP1_MIDV;
  wiener_info->vfilter[6] = wiener_info->hfilter[6] = WIENER_FILT_TAP0_MIDV;
}

typedef struct { int h_start, h_end, v_start, v_end; } RestorationTileLimits;

extern const sgr_params_type sgr_params[SGRPROJ_PARAMS];
extern int sgrproj_mtable[MAX_EPS][MAX_NELEM];
extern const int32_t x_by_xplus1[256];
extern const int32_t one_by_x[MAX_NELEM];

void av1_alloc_restoration_struct(struct AV1Common *cm, RestorationInfo *rsi,
                                  int is_uv);
void av1_free_restoration_struct(RestorationInfo *rst_info);

void extend_frame(uint8_t *data, int width, int height, int stride,
                  int border_horz, int border_vert, int highbd);
void decode_xq(const int *xqd, int *xq);

// Filter a single loop restoration unit.
//
// limits is the limits of the unit. rui gives the mode to use for this unit
// and its coefficients. If striped loop restoration is enabled, rsb contains
// deblocked pixels to use for stripe boundaries; rlbs is just some space to
// use as a scratch buffer. tile_rect gives the limits of the tile containing
// this unit. tile_stripe0 is the index of the first stripe in this tile.
//
// ss_x and ss_y are flags which should be 1 if this is a plane with
// horizontal/vertical subsampling, respectively. highbd is a flag which should
// be 1 in high bit depth mode, in which case bit_depth is the bit depth.
//
// data8 is the frame data (pointing at the top-left corner of the frame, not
// the restoration unit) and stride is its stride. dst8 is the buffer where the
// results will be written and has stride dst_stride. Like data8, dst8 should
// point at the top-left corner of the frame.
//
// Finally tmpbuf is a scratch buffer used by the sgrproj filter which should
// be at least SGRPROJ_TMPBUF_SIZE big.
void av1_loop_restoration_filter_unit(
    const RestorationTileLimits *limits, const RestorationUnitInfo *rui,
#if CONFIG_STRIPED_LOOP_RESTORATION
    const RestorationStripeBoundaries *rsb, RestorationLineBuffers *rlbs,
    const AV1PixelRect *tile_rect, int tile_stripe0,
#if CONFIG_LOOPFILTERING_ACROSS_TILES
    int loop_filter_across_tiles_enabled,
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
#endif  // CONFIG_STRIPED_LOOP_RESTORATION
    int ss_x, int ss_y, int highbd, int bit_depth, uint8_t *data8, int stride,
    uint8_t *dst8, int dst_stride, int32_t *tmpbuf);

void av1_loop_restoration_filter_frame(YV12_BUFFER_CONFIG *frame,
                                       struct AV1Common *cm,
                                       RestorationInfo *rsi,
                                       int components_pattern,
                                       YV12_BUFFER_CONFIG *dst);
void av1_loop_restoration_precal();

typedef void (*rest_unit_visitor_t)(const RestorationTileLimits *limits,
                                    const AV1PixelRect *tile_rect,
                                    int rest_unit_idx, void *priv);

typedef void (*rest_tile_start_visitor_t)(int tile_row, int tile_col,
                                          void *priv);

// Call on_rest_unit for each loop restoration unit in the frame. At the start
// of each tile, call on_tile.
void av1_foreach_rest_unit_in_frame(const struct AV1Common *cm, int plane,
                                    rest_tile_start_visitor_t on_tile,
                                    rest_unit_visitor_t on_rest_unit,
                                    void *priv);

// Return 1 iff the block at mi_row, mi_col with size bsize is a
// top-level superblock containing the top-left corner of at least one
// loop restoration tile.
//
// If the block is a top-level superblock, the function writes to
// *rcol0, *rcol1, *rrow0, *rrow1. The rectangle of restoration unit
// indices given by [*rcol0, *rcol1) x [*rrow0, *rrow1) are relative
// to the current tile, whose starting index is returned as
// *tile_tl_idx.
int av1_loop_restoration_corners_in_sb(const struct AV1Common *cm, int plane,
                                       int mi_row, int mi_col, BLOCK_SIZE bsize,
                                       int *rcol0, int *rcol1, int *rrow0,
                                       int *rrow1, int *tile_tl_idx);

void av1_loop_restoration_save_boundary_lines(const YV12_BUFFER_CONFIG *frame,
                                              struct AV1Common *cm,
                                              int after_cdef);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_RESTORATION_H_
