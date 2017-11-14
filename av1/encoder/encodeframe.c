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

#include <limits.h>
#include <math.h>
#include <stdio.h>

#include "./av1_rtcd.h"
#include "./aom_dsp_rtcd.h"
#include "./aom_config.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/binary_codes_writer.h"
#include "aom_ports/mem.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/system_state.h"

#include "av1/common/common.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/idct.h"
#include "av1/common/mv.h"
#include "av1/common/mvref_common.h"
#include "av1/common/pred_common.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconintra.h"
#include "av1/common/reconinter.h"
#include "av1/common/seg_common.h"
#include "av1/common/tile_common.h"

#include "av1/encoder/aq_complexity.h"
#include "av1/encoder/aq_cyclicrefresh.h"
#include "av1/encoder/aq_variance.h"
#include "av1/common/warped_motion.h"
#include "av1/encoder/global_motion.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/encodemv.h"
#if CONFIG_LV_MAP
#include "av1/encoder/encodetxb.h"
#endif
#include "av1/encoder/ethread.h"
#include "av1/encoder/extend.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/segmentation.h"
#include "av1/encoder/tokenize.h"
#if CONFIG_HIGHBITDEPTH
#define IF_HBD(...) __VA_ARGS__
#else
#define IF_HBD(...)
#endif  // CONFIG_HIGHBITDEPTH

static void encode_superblock(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                              ThreadData *td, TOKENEXTRA **t, RUN_TYPE dry_run,
                              int mi_row, int mi_col, BLOCK_SIZE bsize,
                              int *rate);

// This is used as a reference when computing the source variance for the
//  purposes of activity masking.
// Eventually this should be replaced by custom no-reference routines,
//  which will be faster.
static const uint8_t AV1_VAR_OFFS[MAX_SB_SIZE] = {
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
#if CONFIG_EXT_PARTITION
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128
#endif  // CONFIG_EXT_PARTITION
};

#if CONFIG_HIGHBITDEPTH
static const uint16_t AV1_HIGH_VAR_OFFS_8[MAX_SB_SIZE] = {
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
#if CONFIG_EXT_PARTITION
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128
#endif  // CONFIG_EXT_PARTITION
};

static const uint16_t AV1_HIGH_VAR_OFFS_10[MAX_SB_SIZE] = {
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
#if CONFIG_EXT_PARTITION
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4
#endif  // CONFIG_EXT_PARTITION
};

static const uint16_t AV1_HIGH_VAR_OFFS_12[MAX_SB_SIZE] = {
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16,
#if CONFIG_EXT_PARTITION
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16
#endif  // CONFIG_EXT_PARTITION
};
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_FP_MB_STATS
static const uint8_t num_16x16_blocks_wide_lookup[BLOCK_SIZES_ALL] = {
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  2,
  2,
  2,
  4,
  4,
  IF_EXT_PARTITION(4, 8, 8) 1,
  1,
  1,
  2,
  2,
  4,
  IF_EXT_PARTITION(2, 8)
};
static const uint8_t num_16x16_blocks_high_lookup[BLOCK_SIZES_ALL] = {
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  2,
  1,
  2,
  4,
  2,
  4,
  IF_EXT_PARTITION(8, 4, 8) 1,
  1,
  2,
  1,
  4,
  2,
  IF_EXT_PARTITION(8, 2)
};
#endif  // CONFIG_FP_MB_STATS

unsigned int av1_get_sby_perpixel_variance(const AV1_COMP *cpi,
                                           const struct buf_2d *ref,
                                           BLOCK_SIZE bs) {
  unsigned int sse;
  const unsigned int var =
      cpi->fn_ptr[bs].vf(ref->buf, ref->stride, AV1_VAR_OFFS, 0, &sse);
  return ROUND_POWER_OF_TWO(var, num_pels_log2_lookup[bs]);
}

#if CONFIG_HIGHBITDEPTH
unsigned int av1_high_get_sby_perpixel_variance(const AV1_COMP *cpi,
                                                const struct buf_2d *ref,
                                                BLOCK_SIZE bs, int bd) {
  unsigned int var, sse;
  switch (bd) {
    case 10:
      var =
          cpi->fn_ptr[bs].vf(ref->buf, ref->stride,
                             CONVERT_TO_BYTEPTR(AV1_HIGH_VAR_OFFS_10), 0, &sse);
      break;
    case 12:
      var =
          cpi->fn_ptr[bs].vf(ref->buf, ref->stride,
                             CONVERT_TO_BYTEPTR(AV1_HIGH_VAR_OFFS_12), 0, &sse);
      break;
    case 8:
    default:
      var =
          cpi->fn_ptr[bs].vf(ref->buf, ref->stride,
                             CONVERT_TO_BYTEPTR(AV1_HIGH_VAR_OFFS_8), 0, &sse);
      break;
  }
  return ROUND_POWER_OF_TWO(var, num_pels_log2_lookup[bs]);
}
#endif  // CONFIG_HIGHBITDEPTH

static unsigned int get_sby_perpixel_diff_variance(const AV1_COMP *const cpi,
                                                   const struct buf_2d *ref,
                                                   int mi_row, int mi_col,
                                                   BLOCK_SIZE bs) {
  unsigned int sse, var;
  uint8_t *last_y;
  const YV12_BUFFER_CONFIG *last = get_ref_frame_buffer(cpi, LAST_FRAME);

  assert(last != NULL);
  last_y =
      &last->y_buffer[mi_row * MI_SIZE * last->y_stride + mi_col * MI_SIZE];
  var = cpi->fn_ptr[bs].vf(ref->buf, ref->stride, last_y, last->y_stride, &sse);
  return ROUND_POWER_OF_TWO(var, num_pels_log2_lookup[bs]);
}

static BLOCK_SIZE get_rd_var_based_fixed_partition(AV1_COMP *cpi, MACROBLOCK *x,
                                                   int mi_row, int mi_col) {
  unsigned int var = get_sby_perpixel_diff_variance(
      cpi, &x->plane[0].src, mi_row, mi_col, BLOCK_64X64);
  if (var < 8)
    return BLOCK_64X64;
  else if (var < 128)
    return BLOCK_32X32;
  else if (var < 2048)
    return BLOCK_16X16;
  else
    return BLOCK_8X8;
}

// Lighter version of set_offsets that only sets the mode info
// pointers.
static void set_mode_info_offsets(const AV1_COMP *const cpi,
                                  MACROBLOCK *const x, MACROBLOCKD *const xd,
                                  int mi_row, int mi_col) {
  const AV1_COMMON *const cm = &cpi->common;
  const int idx_str = xd->mi_stride * mi_row + mi_col;
  xd->mi = cm->mi_grid_visible + idx_str;
  xd->mi[0] = cm->mi + idx_str;
  x->mbmi_ext = cpi->mbmi_ext_base + (mi_row * cm->mi_cols + mi_col);
}

static void set_offsets_without_segment_id(const AV1_COMP *const cpi,
                                           const TileInfo *const tile,
                                           MACROBLOCK *const x, int mi_row,
                                           int mi_col, BLOCK_SIZE bsize) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];

  set_mode_info_offsets(cpi, x, xd, mi_row, mi_col);

  set_skip_context(xd, mi_row, mi_col);
  xd->above_txfm_context =
      cm->above_txfm_context + (mi_col << TX_UNIT_WIDE_LOG2);
  xd->left_txfm_context = xd->left_txfm_context_buffer +
                          ((mi_row & MAX_MIB_MASK) << TX_UNIT_HIGH_LOG2);

  // Set up destination pointers.
  av1_setup_dst_planes(xd->plane, bsize, get_frame_new_buffer(cm), mi_row,
                       mi_col);

  // Set up limit values for MV components.
  // Mv beyond the range do not produce new/different prediction block.
  x->mv_limits.row_min =
      -(((mi_row + mi_height) * MI_SIZE) + AOM_INTERP_EXTEND);
  x->mv_limits.col_min = -(((mi_col + mi_width) * MI_SIZE) + AOM_INTERP_EXTEND);
  x->mv_limits.row_max = (cm->mi_rows - mi_row) * MI_SIZE + AOM_INTERP_EXTEND;
  x->mv_limits.col_max = (cm->mi_cols - mi_col) * MI_SIZE + AOM_INTERP_EXTEND;

  set_plane_n4(xd, mi_width, mi_height);

  // Set up distance of MB to edge of frame in 1/8th pel units.
  assert(!(mi_col & (mi_width - 1)) && !(mi_row & (mi_height - 1)));
  set_mi_row_col(xd, tile, mi_row, mi_height, mi_col, mi_width,
#if CONFIG_DEPENDENT_HORZTILES
                 cm->dependent_horz_tiles,
#endif  // CONFIG_DEPENDENT_HORZTILES
                 cm->mi_rows, cm->mi_cols);

  // Set up source buffers.
  av1_setup_src_planes(x, cpi->source, mi_row, mi_col);

  // R/D setup.
  x->rdmult = cpi->rd.RDMULT;

  // required by av1_append_sub8x8_mvs_for_idx() and av1_find_best_ref_mvs()
  xd->tile = *tile;
}

static void set_offsets(const AV1_COMP *const cpi, const TileInfo *const tile,
                        MACROBLOCK *const x, int mi_row, int mi_col,
                        BLOCK_SIZE bsize) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi;
  const struct segmentation *const seg = &cm->seg;

  set_offsets_without_segment_id(cpi, tile, x, mi_row, mi_col, bsize);

  mbmi = &xd->mi[0]->mbmi;
#if CONFIG_CFL
  xd->cfl->mi_row = mi_row;
  xd->cfl->mi_col = mi_col;
#endif

  mbmi->segment_id = 0;
#if CONFIG_Q_SEGMENTATION
  mbmi->q_segment_id = 0;
#endif

// Setup segment ID.
#if CONFIG_Q_SEGMENTATION
  if (seg->enabled || seg->q_lvls) {
#else
  if (seg->enabled) {
#endif
    if (seg->enabled && !cpi->vaq_refresh) {
      const uint8_t *const map =
          seg->update_map ? cpi->segmentation_map : cm->last_frame_seg_map;
      mbmi->segment_id = get_segment_id(cm, map, bsize, mi_row, mi_col);
    }
#if CONFIG_Q_SEGMENTATION
    if (seg->q_lvls)
      mbmi->q_segment_id =
          get_segment_id(cm, cpi->q_seg_encoding_map, bsize, mi_row, mi_col);
    av1_init_plane_quantizers(cpi, x, mbmi->segment_id, mbmi->q_segment_id);
#else
    av1_init_plane_quantizers(cpi, x, mbmi->segment_id);
#endif
  }
}

#if CONFIG_DUAL_FILTER
static void reset_intmv_filter_type(const AV1_COMMON *const cm, MACROBLOCKD *xd,
                                    MB_MODE_INFO *mbmi) {
  InterpFilter filters[2];
  InterpFilter default_filter = av1_unswitchable_filter(cm->interp_filter);

  for (int dir = 0; dir < 2; ++dir) {
    filters[dir] = ((!has_subpel_mv_component(xd->mi[0], xd, dir) &&
                     (mbmi->ref_frame[1] == NONE_FRAME ||
                      !has_subpel_mv_component(xd->mi[0], xd, dir + 2)))
                        ? default_filter
                        : av1_extract_interp_filter(mbmi->interp_filters, dir));
  }
  mbmi->interp_filters = av1_make_interp_filters(filters[0], filters[1]);
}

static void update_filter_type_count(uint8_t allow_update_cdf,
                                     FRAME_COUNTS *counts,
                                     const MACROBLOCKD *xd,
                                     const MB_MODE_INFO *mbmi) {
  int dir;
  for (dir = 0; dir < 2; ++dir) {
    if (has_subpel_mv_component(xd->mi[0], xd, dir) ||
        (mbmi->ref_frame[1] > INTRA_FRAME &&
         has_subpel_mv_component(xd->mi[0], xd, dir + 2))) {
      const int ctx = av1_get_pred_context_switchable_interp(xd, dir);
      InterpFilter filter =
          av1_extract_interp_filter(mbmi->interp_filters, dir);
      ++counts->switchable_interp[ctx][filter];
      if (allow_update_cdf)
        update_cdf(xd->tile_ctx->switchable_interp_cdf[ctx], filter,
                   SWITCHABLE_FILTERS);
    }
  }
}
#endif
static void update_global_motion_used(PREDICTION_MODE mode, BLOCK_SIZE bsize,
                                      const MB_MODE_INFO *mbmi,
                                      RD_COUNTS *rdc) {
  if (mode == GLOBALMV || mode == GLOBAL_GLOBALMV) {
    const int num_4x4s =
        num_4x4_blocks_wide_lookup[bsize] * num_4x4_blocks_high_lookup[bsize];
    int ref;
    for (ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
      rdc->global_motion_used[mbmi->ref_frame[ref]] += num_4x4s;
    }
  }
}

static void reset_tx_size(MACROBLOCKD *xd, MB_MODE_INFO *mbmi,
                          const TX_MODE tx_mode) {
  if (xd->lossless[mbmi->segment_id]) {
    mbmi->tx_size = TX_4X4;
  } else if (tx_mode != TX_MODE_SELECT) {
    mbmi->tx_size =
        tx_size_from_tx_mode(mbmi->sb_type, tx_mode, is_inter_block(mbmi));
  }
}

static void set_ref_and_pred_mvs(MACROBLOCK *const x, int_mv *const mi_pred_mv,
                                 int8_t rf_type) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;

  const int bw = xd->n8_w << MI_SIZE_LOG2;
  const int bh = xd->n8_h << MI_SIZE_LOG2;
  int ref_mv_idx = mbmi->ref_mv_idx;
  MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  CANDIDATE_MV *const curr_ref_mv_stack = mbmi_ext->ref_mv_stack[rf_type];

  if (has_second_ref(mbmi)) {
    // Special case: NEAR_NEWMV and NEW_NEARMV modes use 1 + mbmi->ref_mv_idx
    // (like NEARMV) instead
    if (mbmi->mode == NEAR_NEWMV || mbmi->mode == NEW_NEARMV) ref_mv_idx += 1;

    if (compound_ref0_mode(mbmi->mode) == NEWMV) {
      int_mv this_mv = curr_ref_mv_stack[ref_mv_idx].this_mv;
      clamp_mv_ref(&this_mv.as_mv, bw, bh, xd);
      mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0] = this_mv;
      mbmi->pred_mv[0] = this_mv;
      mi_pred_mv[0] = this_mv;
    }
    if (compound_ref1_mode(mbmi->mode) == NEWMV) {
      int_mv this_mv = curr_ref_mv_stack[ref_mv_idx].comp_mv;
      clamp_mv_ref(&this_mv.as_mv, bw, bh, xd);
      mbmi_ext->ref_mvs[mbmi->ref_frame[1]][0] = this_mv;
      mbmi->pred_mv[1] = this_mv;
      mi_pred_mv[1] = this_mv;
    }
#if CONFIG_COMPOUND_SINGLEREF
  } else if (is_inter_singleref_comp_mode(mbmi->mode)) {
    // Special case: SR_NEAR_NEWMV uses 1 + mbmi->ref_mv_idx
    // (like NEARMV) instead
    if (mbmi->mode == SR_NEAR_NEWMV) ref_mv_idx += 1;

    if (compound_ref0_mode(mbmi->mode) == NEWMV ||
        compound_ref1_mode(mbmi->mode) == NEWMV) {
      int_mv this_mv = curr_ref_mv_stack[ref_mv_idx].this_mv;
      clamp_mv_ref(&this_mv.as_mv, bw, bh, xd);
      mbmi_ext->ref_mvs[mbmi->ref_frame[0]][0] = this_mv;
      mbmi->pred_mv[0] = this_mv;
      mi_pred_mv[0] = this_mv;
    }
#endif  // CONFIG_COMPOUND_SINGLEREF
  } else {
    if (mbmi->mode == NEWMV) {
      int i;
      for (i = 0; i < 1 + has_second_ref(mbmi); ++i) {
        int_mv this_mv = (i == 0) ? curr_ref_mv_stack[ref_mv_idx].this_mv
                                  : curr_ref_mv_stack[ref_mv_idx].comp_mv;
        clamp_mv_ref(&this_mv.as_mv, bw, bh, xd);
        mbmi_ext->ref_mvs[mbmi->ref_frame[i]][0] = this_mv;
        mbmi->pred_mv[i] = this_mv;
        mi_pred_mv[i] = this_mv;
      }
    }
  }
}

static void update_state(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                         ThreadData *td, PICK_MODE_CONTEXT *ctx, int mi_row,
                         int mi_col, BLOCK_SIZE bsize, RUN_TYPE dry_run) {
  int i, x_idx, y;
  const AV1_COMMON *const cm = &cpi->common;
  RD_COUNTS *const rdc = &td->rd_counts;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  MODE_INFO *mi = &ctx->mic;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  MODE_INFO *mi_addr = xd->mi[0];
  const struct segmentation *const seg = &cm->seg;
  const int bw = mi_size_wide[mi->mbmi.sb_type];
  const int bh = mi_size_high[mi->mbmi.sb_type];
  const int mis = cm->mi_stride;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];
  int8_t rf_type;

  assert(mi->mbmi.sb_type == bsize);

  *mi_addr = *mi;
  *x->mbmi_ext = ctx->mbmi_ext;

#if CONFIG_DUAL_FILTER
  reset_intmv_filter_type(cm, xd, mbmi);
#endif

  rf_type = av1_ref_frame_type(mbmi->ref_frame);
  if (x->mbmi_ext->ref_mv_count[rf_type] > 1) {
    set_ref_and_pred_mvs(x, mi->mbmi.pred_mv, rf_type);
  }

  // If segmentation in use
  if (seg->enabled) {
    // For in frame complexity AQ copy the segment id from the segment map.
    if (cpi->oxcf.aq_mode == COMPLEXITY_AQ) {
      const uint8_t *const map =
          seg->update_map ? cpi->segmentation_map : cm->last_frame_seg_map;
      mi_addr->mbmi.segment_id = get_segment_id(cm, map, bsize, mi_row, mi_col);
      reset_tx_size(xd, &mi_addr->mbmi, cm->tx_mode);
    }
    // Else for cyclic refresh mode update the segment map, set the segment id
    // and then update the quantizer.
    if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ) {
      av1_cyclic_refresh_update_segment(cpi, &xd->mi[0]->mbmi, mi_row, mi_col,
                                        bsize, ctx->rate, ctx->dist, x->skip);
      reset_tx_size(xd, &mi_addr->mbmi, cm->tx_mode);
    }
  }

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    p[i].coeff = ctx->coeff[i];
    p[i].qcoeff = ctx->qcoeff[i];
    pd[i].dqcoeff = ctx->dqcoeff[i];
    p[i].eobs = ctx->eobs[i];
#if CONFIG_LV_MAP
    p[i].txb_entropy_ctx = ctx->txb_entropy_ctx[i];
#endif  // CONFIG_LV_MAP
  }
  for (i = 0; i < 2; ++i) pd[i].color_index_map = ctx->color_index_map[i];
#if CONFIG_MRC_TX
  xd->mrc_mask = ctx->mrc_mask;
#endif  // CONFIG_MRC_TX
  // Restore the coding context of the MB to that that was in place
  // when the mode was picked for it
  for (y = 0; y < mi_height; y++)
    for (x_idx = 0; x_idx < mi_width; x_idx++)
      if ((xd->mb_to_right_edge >> (3 + MI_SIZE_LOG2)) + mi_width > x_idx &&
          (xd->mb_to_bottom_edge >> (3 + MI_SIZE_LOG2)) + mi_height > y) {
        xd->mi[x_idx + y * mis] = mi_addr;
      }

#if !CONFIG_EXT_DELTA_Q
  if (cpi->oxcf.aq_mode > NO_AQ && cpi->oxcf.aq_mode < DELTA_AQ)
#if CONFIG_Q_SEGMENTATION
    av1_init_plane_quantizers(cpi, x, xd->mi[0]->mbmi.segment_id,
                              xd->mi[0]->mbmi.q_segment_id);
#else
    av1_init_plane_quantizers(cpi, x, xd->mi[0]->mbmi.segment_id);
#endif
#else
  if (cpi->oxcf.aq_mode)
#if CONFIG_Q_SEGMENTATION
    av1_init_plane_quantizers(cpi, x, xd->mi[0]->mbmi.segment_id,
                              xd->mi[0]->mbmi.q_segment_id);
#else
    av1_init_plane_quantizers(cpi, x, xd->mi[0]->mbmi.segment_id);
#endif
#endif

  x->skip = ctx->skip;

  for (i = 0; i < 1; ++i)
    memcpy(x->blk_skip[i], ctx->blk_skip[i],
           sizeof(uint8_t) * ctx->num_4x4_blk);

  if (dry_run) return;

#if CONFIG_INTERNAL_STATS
  {
    unsigned int *const mode_chosen_counts =
        (unsigned int *)cpi->mode_chosen_counts;  // Cast const away.
    if (frame_is_intra_only(cm)) {
      static const int kf_mode_index[] = {
        THR_DC /*DC_PRED*/,
        THR_V_PRED /*V_PRED*/,
        THR_H_PRED /*H_PRED*/,
        THR_D45_PRED /*D45_PRED*/,
        THR_D135_PRED /*D135_PRED*/,
        THR_D117_PRED /*D117_PRED*/,
        THR_D153_PRED /*D153_PRED*/,
        THR_D207_PRED /*D207_PRED*/,
        THR_D63_PRED /*D63_PRED*/,
        THR_SMOOTH,   /*SMOOTH_PRED*/
        THR_SMOOTH_V, /*SMOOTH_V_PRED*/
        THR_SMOOTH_H, /*SMOOTH_H_PRED*/
        THR_PAETH /*PAETH_PRED*/,
      };
      ++mode_chosen_counts[kf_mode_index[mbmi->mode]];
    } else {
      // Note how often each mode chosen as best
      ++mode_chosen_counts[ctx->best_mode_index];
    }
  }
#endif
  if (!frame_is_intra_only(cm)) {
    if (is_inter_block(mbmi)) {
      av1_update_mv_count(td);
      if (bsize >= BLOCK_8X8) {
        // TODO(sarahparker): global motion stats need to be handled per-tile
        // to be compatible with tile-based threading.
        update_global_motion_used(mbmi->mode, bsize, mbmi, rdc);
      } else {
        const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];
        const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];
        int idx, idy;
        for (idy = 0; idy < 2; idy += num_4x4_h) {
          for (idx = 0; idx < 2; idx += num_4x4_w) {
            const int j = idy * 2 + idx;
            update_global_motion_used(mi->bmi[j].as_mode, bsize, mbmi, rdc);
          }
        }
      }
      if (cm->interp_filter == SWITCHABLE &&
          mbmi->motion_mode != WARPED_CAUSAL &&
          !is_nontrans_global_motion(xd)) {
#if CONFIG_DUAL_FILTER
        update_filter_type_count(tile_data->allow_update_cdf, td->counts, xd,
                                 mbmi);
#else
        (void)tile_data;
        const int switchable_ctx = av1_get_pred_context_switchable_interp(xd);
        const InterpFilter filter =
            av1_extract_interp_filter(mbmi->interp_filters, 0);
        ++td->counts->switchable_interp[switchable_ctx][filter];
#endif
      }
    }

    rdc->comp_pred_diff[SINGLE_REFERENCE] += ctx->single_pred_diff;
    rdc->comp_pred_diff[COMPOUND_REFERENCE] += ctx->comp_pred_diff;
    rdc->comp_pred_diff[REFERENCE_MODE_SELECT] += ctx->hybrid_pred_diff;
  }

  const int x_mis = AOMMIN(bw, cm->mi_cols - mi_col);
  const int y_mis = AOMMIN(bh, cm->mi_rows - mi_row);
  av1_copy_frame_mvs(cm, mi, mi_row, mi_col, x_mis, y_mis);

#if CONFIG_JNT_COMP
#if !CONFIG_NEW_MULTISYMBOL
  if (has_second_ref(mbmi)) {
    const int comp_index_ctx = get_comp_index_context(cm, xd);
    ++td->counts->compound_index[comp_index_ctx][mbmi->compound_idx];
  }
#endif  // CONFIG_NEW_MULTISYMBOL
#endif  // CONFIG_JNT_COMP
}

#if NC_MODE_INFO
static void set_mode_info_b(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                            ThreadData *td, int mi_row, int mi_col,
                            BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx) {
  const TileInfo *const tile = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  set_offsets(cpi, tile, x, mi_row, mi_col, bsize);
  update_state(cpi, tile_data, td, ctx, mi_row, mi_col, bsize, 1);
}

static void set_mode_info_sb(const AV1_COMP *const cpi, ThreadData *td,
                             TileDataEnc *tile_data, TOKENEXTRA **tp,
                             int mi_row, int mi_col, BLOCK_SIZE bsize,
                             PC_TREE *pc_tree) {
  const AV1_COMMON *const cm = &cpi->common;
  const int hbs = mi_size_wide[bsize] / 2;
  const PARTITION_TYPE partition = pc_tree->partitioning;
  BLOCK_SIZE subsize = get_subsize(bsize, partition);
#if CONFIG_EXT_PARTITION_TYPES
  const BLOCK_SIZE bsize2 = get_subsize(bsize, PARTITION_SPLIT);
  const int quarter_step = mi_size_wide[bsize] / 4;
#endif

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  switch (partition) {
    case PARTITION_NONE:
      set_mode_info_b(cpi, tile_data, td, mi_row, mi_col, subsize,
                      &pc_tree->none);
      break;
    case PARTITION_VERT:
      set_mode_info_b(cpi, tile_data, td, mi_row, mi_col, subsize,
                      &pc_tree->vertical[0]);
      if (mi_col + hbs < cm->mi_cols) {
        set_mode_info_b(cpi, tile_data, td, mi_row, mi_col + hbs, subsize,
                        &pc_tree->vertical[1]);
      }
      break;
    case PARTITION_HORZ:
      set_mode_info_b(cpi, tile_data, td, mi_row, mi_col, subsize,
                      &pc_tree->horizontal[0]);
      if (mi_row + hbs < cm->mi_rows) {
        set_mode_info_b(cpi, tile_data, td, mi_row + hbs, mi_col, subsize,
                        &pc_tree->horizontal[1]);
      }
      break;
    case PARTITION_SPLIT:
      set_mode_info_sb(cpi, td, tile_data, tp, mi_row, mi_col, subsize,
                       pc_tree->split[0]);
      set_mode_info_sb(cpi, td, tile_data, tp, mi_row, mi_col + hbs, subsize,
                       pc_tree->split[1]);
      set_mode_info_sb(cpi, td, tile_data, tp, mi_row + hbs, mi_col, subsize,
                       pc_tree->split[2]);
      set_mode_info_sb(cpi, td, tile_data, tp, mi_row + hbs, mi_col + hbs,
                       subsize, pc_tree->split[3]);
      break;
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION_TYPES_AB
#error NC_MODE_INFO+MOTION_VAR not yet supported for new HORZ/VERT_AB partitions
#endif
    case PARTITION_HORZ_A:
      set_mode_info_b(cpi, tile_data, td, mi_row, mi_col, bsize2,
                      &pc_tree->horizontala[0]);
      set_mode_info_b(cpi, tile_data, td, mi_row, mi_col + hbs, bsize2,
                      &pc_tree->horizontala[1]);
      set_mode_info_b(cpi, tile_data, td, mi_row + hbs, mi_col, subsize,
                      &pc_tree->horizontala[2]);
      break;
    case PARTITION_HORZ_B:
      set_mode_info_b(cpi, tile_data, td, mi_row, mi_col, subsize,
                      &pc_tree->horizontalb[0]);
      set_mode_info_b(cpi, tile_data, td, mi_row + hbs, mi_col, bsize2,
                      &pc_tree->horizontalb[1]);
      set_mode_info_b(cpi, tile_data, td, mi_row + hbs, mi_col + hbs, bsize2,
                      &pc_tree->horizontalb[2]);
      break;
    case PARTITION_VERT_A:
      set_mode_info_b(cpi, tile_data, td, mi_row, mi_col, bsize2,
                      &pc_tree->verticala[0]);
      set_mode_info_b(cpi, tile_data, td, mi_row + hbs, mi_col, bsize2,
                      &pc_tree->verticala[1]);
      set_mode_info_b(cpi, tile_data, td, mi_row, mi_col + hbs, subsize,
                      &pc_tree->verticala[2]);
      break;
    case PARTITION_VERT_B:
      set_mode_info_b(cpi, tile_data, td, mi_row, mi_col, subsize,
                      &pc_tree->verticalb[0]);
      set_mode_info_b(cpi, tile_data, td, mi_row, mi_col + hbs, bsize2,
                      &pc_tree->verticalb[1]);
      set_mode_info_b(cpi, tile_data, td, mi_row + hbs, mi_col + hbs, bsize2,
                      &pc_tree->verticalb[2]);
      break;
    case PARTITION_HORZ_4:
      for (int i = 0; i < 4; ++i) {
        int this_mi_row = mi_row + i * quarter_step;
        if (i > 0 && this_mi_row >= cm->mi_rows) break;

        set_mode_info_b(cpi, tile_data, td, this_mi_row, mi_col, subsize,
                        &pc_tree->horizontal4[i]);
      }
      break;
    case PARTITION_VERT_4:
      for (int i = 0; i < 4; ++i) {
        int this_mi_col = mi_col + i * quarter_step;
        if (i > 0 && this_mi_col >= cm->mi_cols) break;

        set_mode_info_b(cpi, tile_data, td, mi_row, this_mi_col, subsize,
                        &pc_tree->vertical4[i]);
      }
      break;
#endif  // CONFIG_EXT_PARTITION_TYPES
    default: assert(0 && "Invalid partition type."); break;
  }
}
#endif

void av1_setup_src_planes(MACROBLOCK *x, const YV12_BUFFER_CONFIG *src,
                          int mi_row, int mi_col) {
  uint8_t *const buffers[3] = { src->y_buffer, src->u_buffer, src->v_buffer };
  const int widths[3] = { src->y_crop_width, src->uv_crop_width,
                          src->uv_crop_width };
  const int heights[3] = { src->y_crop_height, src->uv_crop_height,
                           src->uv_crop_height };
  const int strides[3] = { src->y_stride, src->uv_stride, src->uv_stride };
  int i;

  // Set current frame pointer.
  x->e_mbd.cur_buf = src;

  for (i = 0; i < MAX_MB_PLANE; i++)
    setup_pred_plane(&x->plane[i].src, x->e_mbd.mi[0]->mbmi.sb_type, buffers[i],
                     widths[i], heights[i], strides[i], mi_row, mi_col, NULL,
                     x->e_mbd.plane[i].subsampling_x,
                     x->e_mbd.plane[i].subsampling_y);
}

static int set_segment_rdmult(const AV1_COMP *const cpi, MACROBLOCK *const x,
#if CONFIG_Q_SEGMENTATION
                              int8_t segment_id, int8_t q_segment_id) {
#else
                              int8_t segment_id) {
#endif
  int segment_qindex;
  const AV1_COMMON *const cm = &cpi->common;
#if CONFIG_Q_SEGMENTATION
  av1_init_plane_quantizers(cpi, x, segment_id, q_segment_id);
#else
  av1_init_plane_quantizers(cpi, x, segment_id);
#endif
  aom_clear_system_state();
#if CONFIG_Q_SEGMENTATION
  segment_qindex =
      av1_get_qindex(&cm->seg, segment_id, q_segment_id, cm->base_qindex);
#else
  segment_qindex = av1_get_qindex(&cm->seg, segment_id, cm->base_qindex);
#endif
  return av1_compute_rd_mult(cpi, segment_qindex + cm->y_dc_delta_q);
}

#if CONFIG_DIST_8X8
static void dist_8x8_set_sub8x8_dst(MACROBLOCK *const x, uint8_t *dst8x8,
                                    BLOCK_SIZE bsize, int bw, int bh,
                                    int mi_row, int mi_col) {
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblockd_plane *const pd = &xd->plane[0];
  const int dst_stride = pd->dst.stride;
  uint8_t *dst = pd->dst.buf;

  assert(bsize < BLOCK_8X8);

  if (bsize < BLOCK_8X8) {
    int i, j;
#if CONFIG_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      uint16_t *dst8x8_16 = (uint16_t *)dst8x8;
      uint16_t *dst_sub8x8 = &dst8x8_16[((mi_row & 1) * 8 + (mi_col & 1)) << 2];

      for (j = 0; j < bh; ++j)
        for (i = 0; i < bw; ++i)
          dst_sub8x8[j * 8 + i] = CONVERT_TO_SHORTPTR(dst)[j * dst_stride + i];
    } else {
#endif
      uint8_t *dst_sub8x8 = &dst8x8[((mi_row & 1) * 8 + (mi_col & 1)) << 2];

      for (j = 0; j < bh; ++j)
        for (i = 0; i < bw; ++i)
          dst_sub8x8[j * 8 + i] = dst[j * dst_stride + i];
#if CONFIG_HIGHBITDEPTH
    }
#endif
  }
}
#endif  // CONFIG_DIST_8X8

static void rd_pick_sb_modes(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                             MACROBLOCK *const x, int mi_row, int mi_col,
                             RD_STATS *rd_cost,
#if CONFIG_EXT_PARTITION_TYPES
                             PARTITION_TYPE partition,
#endif
                             BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                             int64_t best_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  const AQ_MODE aq_mode = cpi->oxcf.aq_mode;
  int i, orig_rdmult;

  aom_clear_system_state();

  set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize);
  mbmi = &xd->mi[0]->mbmi;
  mbmi->sb_type = bsize;
#if CONFIG_RD_DEBUG
  mbmi->mi_row = mi_row;
  mbmi->mi_col = mi_col;
#endif
#if CONFIG_EXT_PARTITION_TYPES
  mbmi->partition = partition;
#endif

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    p[i].coeff = ctx->coeff[i];
    p[i].qcoeff = ctx->qcoeff[i];
    pd[i].dqcoeff = ctx->dqcoeff[i];
    p[i].eobs = ctx->eobs[i];
#if CONFIG_LV_MAP
    p[i].txb_entropy_ctx = ctx->txb_entropy_ctx[i];
#endif
  }

  for (i = 0; i < 2; ++i) pd[i].color_index_map = ctx->color_index_map[i];
#if CONFIG_MRC_TX
  xd->mrc_mask = ctx->mrc_mask;
#endif  // CONFIG_MRC_TX

  ctx->skippable = 0;

  // Set to zero to make sure we do not use the previous encoded frame stats
  mbmi->skip = 0;

  x->skip_chroma_rd =
      !is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                           xd->plane[1].subsampling_y);

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    x->source_variance = av1_high_get_sby_perpixel_variance(
        cpi, &x->plane[0].src, bsize, xd->bd);
  } else {
    x->source_variance =
        av1_get_sby_perpixel_variance(cpi, &x->plane[0].src, bsize);
  }
#else
  x->source_variance =
      av1_get_sby_perpixel_variance(cpi, &x->plane[0].src, bsize);
#endif  // CONFIG_HIGHBITDEPTH

  // Save rdmult before it might be changed, so it can be restored later.
  orig_rdmult = x->rdmult;

  if (aq_mode == VARIANCE_AQ) {
    if (cpi->vaq_refresh) {
      const int energy =
          bsize <= BLOCK_16X16 ? x->mb_energy : av1_block_energy(cpi, x, bsize);
      mbmi->segment_id = av1_vaq_segment_id(energy);
// Re-initialise quantiser
#if CONFIG_Q_SEGMENTATION
      av1_init_plane_quantizers(cpi, x, mbmi->segment_id, mbmi->q_segment_id);
#else
      av1_init_plane_quantizers(cpi, x, mbmi->segment_id);
#endif
    }
#if CONFIG_Q_SEGMENTATION
    x->rdmult =
        set_segment_rdmult(cpi, x, mbmi->segment_id, mbmi->q_segment_id);
  } else if (aq_mode == COMPLEXITY_AQ) {
    x->rdmult =
        set_segment_rdmult(cpi, x, mbmi->segment_id, mbmi->q_segment_id);
#else
    x->rdmult = set_segment_rdmult(cpi, x, mbmi->segment_id);
  } else if (aq_mode == COMPLEXITY_AQ) {
    x->rdmult = set_segment_rdmult(cpi, x, mbmi->segment_id);
#endif
  } else if (aq_mode == CYCLIC_REFRESH_AQ) {
    // If segment is boosted, use rdmult for that segment.
    if (cyclic_refresh_segment_id_boosted(mbmi->segment_id))
      x->rdmult = av1_cyclic_refresh_get_rdmult(cpi->cyclic_refresh);
  }

  // Find best coding mode & reconstruct the MB so it is available
  // as a predictor for MBs that follow in the SB
  if (frame_is_intra_only(cm)) {
    av1_rd_pick_intra_mode_sb(cpi, x, rd_cost, bsize, ctx, best_rd);
  } else {
    if (segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
      av1_rd_pick_inter_mode_sb_seg_skip(cpi, tile_data, x, mi_row, mi_col,
                                         rd_cost, bsize, ctx, best_rd);
    } else {
      av1_rd_pick_inter_mode_sb(cpi, tile_data, x, mi_row, mi_col, rd_cost,
                                bsize, ctx, best_rd);
    }
  }

  // Examine the resulting rate and for AQ mode 2 make a segment choice.
  if ((rd_cost->rate != INT_MAX) && (aq_mode == COMPLEXITY_AQ) &&
      (bsize >= BLOCK_16X16) &&
      (cm->frame_type == KEY_FRAME || cpi->refresh_alt_ref_frame ||
       cpi->refresh_alt2_ref_frame ||
       (cpi->refresh_golden_frame && !cpi->rc.is_src_frame_alt_ref))) {
    av1_caq_select_segment(cpi, x, bsize, mi_row, mi_col, rd_cost->rate);
  }

  x->rdmult = orig_rdmult;

  // TODO(jingning) The rate-distortion optimization flow needs to be
  // refactored to provide proper exit/return handle.
  if (rd_cost->rate == INT_MAX) rd_cost->rdcost = INT64_MAX;

  ctx->rate = rd_cost->rate;
  ctx->dist = rd_cost->dist;
}

static void update_inter_mode_stats(FRAME_COUNTS *counts, PREDICTION_MODE mode,
                                    int16_t mode_context) {
  int16_t mode_ctx = mode_context & NEWMV_CTX_MASK;
  if (mode == NEWMV) {
    ++counts->newmv_mode[mode_ctx][0];
    return;
  } else {
    ++counts->newmv_mode[mode_ctx][1];

    if (mode_context & (1 << ALL_ZERO_FLAG_OFFSET)) {
      return;
    }

    mode_ctx = (mode_context >> GLOBALMV_OFFSET) & GLOBALMV_CTX_MASK;
    if (mode == GLOBALMV) {
      ++counts->zeromv_mode[mode_ctx][0];
      return;
    } else {
      ++counts->zeromv_mode[mode_ctx][1];
      mode_ctx = (mode_context >> REFMV_OFFSET) & REFMV_CTX_MASK;

      if (mode_context & (1 << SKIP_NEARESTMV_OFFSET)) mode_ctx = 6;
      if (mode_context & (1 << SKIP_NEARMV_OFFSET)) mode_ctx = 7;
      if (mode_context & (1 << SKIP_NEARESTMV_SUB8X8_OFFSET)) mode_ctx = 8;

      ++counts->refmv_mode[mode_ctx][mode != NEARESTMV];
    }
  }
}

static void update_stats(const AV1_COMMON *const cm, TileDataEnc *tile_data,
                         ThreadData *td, int mi_row, int mi_col) {
  MACROBLOCK *x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const MODE_INFO *const mi = xd->mi[0];
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  FRAME_CONTEXT *fc = xd->tile_ctx;
  const uint8_t allow_update_cdf = tile_data->allow_update_cdf;

  // delta quant applies to both intra and inter
  const int super_block_upper_left = ((mi_row & (cm->mib_size - 1)) == 0) &&
                                     ((mi_col & (cm->mib_size - 1)) == 0);

  const int seg_ref_active =
      segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_REF_FRAME);

  if (!seg_ref_active) {
    const int skip_ctx = av1_get_skip_context(xd);
    td->counts->skip[skip_ctx][mbmi->skip]++;
#if CONFIG_NEW_MULTISYMBOL
    if (allow_update_cdf) update_cdf(fc->skip_cdfs[skip_ctx], mbmi->skip, 2);
#endif  // CONFIG_NEW_MULTISYMBOL
  }

  if (cm->delta_q_present_flag && (bsize != cm->sb_size || !mbmi->skip) &&
      super_block_upper_left) {
    const int dq = (mbmi->current_q_index - xd->prev_qindex) / cm->delta_q_res;
    const int absdq = abs(dq);
    int i;
    for (i = 0; i < AOMMIN(absdq, DELTA_Q_SMALL); ++i) {
      td->counts->delta_q[i][1]++;
    }
    if (absdq < DELTA_Q_SMALL) td->counts->delta_q[absdq][0]++;
    xd->prev_qindex = mbmi->current_q_index;
#if CONFIG_EXT_DELTA_Q
#if CONFIG_LOOPFILTER_LEVEL
    if (cm->delta_lf_present_flag) {
      if (cm->delta_lf_multi) {
        for (int lf_id = 0; lf_id < FRAME_LF_COUNT; ++lf_id) {
          const int delta_lf =
              (mbmi->curr_delta_lf[lf_id] - xd->prev_delta_lf[lf_id]) /
              cm->delta_lf_res;
          const int abs_delta_lf = abs(delta_lf);
          for (i = 0; i < AOMMIN(abs_delta_lf, DELTA_LF_SMALL); ++i) {
            td->counts->delta_lf_multi[lf_id][i][1]++;
          }
          if (abs_delta_lf < DELTA_LF_SMALL)
            td->counts->delta_lf_multi[lf_id][abs_delta_lf][0]++;
          xd->prev_delta_lf[lf_id] = mbmi->curr_delta_lf[lf_id];
        }
      } else {
        const int delta_lf =
            (mbmi->current_delta_lf_from_base - xd->prev_delta_lf_from_base) /
            cm->delta_lf_res;
        const int abs_delta_lf = abs(delta_lf);
        for (i = 0; i < AOMMIN(abs_delta_lf, DELTA_LF_SMALL); ++i) {
          td->counts->delta_lf[i][1]++;
        }
        if (abs_delta_lf < DELTA_LF_SMALL)
          td->counts->delta_lf[abs_delta_lf][0]++;
        xd->prev_delta_lf_from_base = mbmi->current_delta_lf_from_base;
      }
    }
#else
    if (cm->delta_lf_present_flag) {
      const int dlf =
          (mbmi->current_delta_lf_from_base - xd->prev_delta_lf_from_base) /
          cm->delta_lf_res;
      const int absdlf = abs(dlf);
      for (i = 0; i < AOMMIN(absdlf, DELTA_LF_SMALL); ++i) {
        td->counts->delta_lf[i][1]++;
      }
      if (absdlf < DELTA_LF_SMALL) td->counts->delta_lf[absdlf][0]++;
      xd->prev_delta_lf_from_base = mbmi->current_delta_lf_from_base;
    }
#endif  // CONFIG_LOOPFILTER_LEVEL
#endif
  }
  if (!frame_is_intra_only(cm)) {
    FRAME_COUNTS *const counts = td->counts;
    RD_COUNTS *rdc = &td->rd_counts;
    const int inter_block = is_inter_block(mbmi);
    if (!seg_ref_active) {
      counts->intra_inter[av1_get_intra_inter_context(xd)][inter_block]++;
#if CONFIG_NEW_MULTISYMBOL
      if (allow_update_cdf)
        update_cdf(fc->intra_inter_cdf[av1_get_intra_inter_context(xd)],
                   inter_block, 2);
#endif
      // If the segment reference feature is enabled we have only a single
      // reference frame allowed for the segment so exclude it from
      // the reference frame counts used to work out probabilities.
      if (inter_block) {
        const MV_REFERENCE_FRAME ref0 = mbmi->ref_frame[0];
        const MV_REFERENCE_FRAME ref1 = mbmi->ref_frame[1];

        if (cm->reference_mode == REFERENCE_MODE_SELECT) {
          if (has_second_ref(mbmi))
            // This flag is also updated for 4x4 blocks
            rdc->compound_ref_used_flag = 1;
          else
            // This flag is also updated for 4x4 blocks
            rdc->single_ref_used_flag = 1;
          if (is_comp_ref_allowed(mbmi->sb_type)) {
            counts->comp_inter[av1_get_reference_mode_context(cm, xd)]
                              [has_second_ref(mbmi)]++;
#if CONFIG_NEW_MULTISYMBOL
            if (allow_update_cdf)
              update_cdf(av1_get_reference_mode_cdf(cm, xd),
                         has_second_ref(mbmi), 2);
#endif  // CONFIG_NEW_MULTISYMBOL
          }
        }

        if (has_second_ref(mbmi)) {
#if CONFIG_EXT_COMP_REFS
          const COMP_REFERENCE_TYPE comp_ref_type = has_uni_comp_refs(mbmi)
                                                        ? UNIDIR_COMP_REFERENCE
                                                        : BIDIR_COMP_REFERENCE;
          counts->comp_ref_type[av1_get_comp_reference_type_context(xd)]
                               [comp_ref_type]++;

          if (comp_ref_type == UNIDIR_COMP_REFERENCE) {
            const int bit = (ref0 == BWDREF_FRAME);
            counts->uni_comp_ref[av1_get_pred_context_uni_comp_ref_p(xd)][0]
                                [bit]++;
            if (!bit) {
              const int bit1 = (ref1 == LAST3_FRAME || ref1 == GOLDEN_FRAME);
              counts->uni_comp_ref[av1_get_pred_context_uni_comp_ref_p1(xd)][1]
                                  [bit1]++;
              if (bit1) {
                counts->uni_comp_ref[av1_get_pred_context_uni_comp_ref_p2(xd)]
                                    [2][ref1 == GOLDEN_FRAME]++;
              }
            }
          } else {
#endif  // CONFIG_EXT_COMP_REFS
            const int bit = (ref0 == GOLDEN_FRAME || ref0 == LAST3_FRAME);

            counts->comp_ref[av1_get_pred_context_comp_ref_p(cm, xd)][0][bit]++;
            if (!bit) {
              counts->comp_ref[av1_get_pred_context_comp_ref_p1(cm, xd)][1]
                              [ref0 == LAST_FRAME]++;
            } else {
              counts->comp_ref[av1_get_pred_context_comp_ref_p2(cm, xd)][2]
                              [ref0 == GOLDEN_FRAME]++;
            }

            counts->comp_bwdref[av1_get_pred_context_comp_bwdref_p(cm, xd)][0]
                               [ref1 == ALTREF_FRAME]++;
            if (ref1 != ALTREF_FRAME)
              counts->comp_bwdref[av1_get_pred_context_comp_bwdref_p1(cm, xd)]
                                 [1][ref1 == ALTREF2_FRAME]++;
#if CONFIG_EXT_COMP_REFS
          }
#endif  // CONFIG_EXT_COMP_REFS
        } else {
          const int bit = (ref0 >= BWDREF_FRAME);

          counts->single_ref[av1_get_pred_context_single_ref_p1(xd)][0][bit]++;
          if (bit) {
            assert(ref0 <= ALTREF_FRAME);
            counts->single_ref[av1_get_pred_context_single_ref_p2(xd)][1]
                              [ref0 == ALTREF_FRAME]++;
            if (ref0 != ALTREF_FRAME)
              counts->single_ref[av1_get_pred_context_single_ref_p6(xd)][5]
                                [ref0 == ALTREF2_FRAME]++;
          } else {
            const int bit1 = !(ref0 == LAST2_FRAME || ref0 == LAST_FRAME);
            counts
                ->single_ref[av1_get_pred_context_single_ref_p3(xd)][2][bit1]++;
            if (!bit1) {
              counts->single_ref[av1_get_pred_context_single_ref_p4(xd)][3]
                                [ref0 != LAST_FRAME]++;
            } else {
              counts->single_ref[av1_get_pred_context_single_ref_p5(xd)][4]
                                [ref0 != LAST3_FRAME]++;
            }
          }
        }

#if CONFIG_COMPOUND_SINGLEREF
        if (!has_second_ref(mbmi))
          counts->comp_inter_mode[av1_get_inter_mode_context(xd)]
                                 [is_inter_singleref_comp_mode(mbmi->mode)]++;
#endif  // CONFIG_COMPOUND_SINGLEREF

        if (cm->reference_mode != COMPOUND_REFERENCE &&
            cm->allow_interintra_compound && is_interintra_allowed(mbmi)) {
          const int bsize_group = size_group_lookup[bsize];
          if (mbmi->ref_frame[1] == INTRA_FRAME) {
            counts->interintra[bsize_group][1]++;
#if CONFIG_NEW_MULTISYMBOL
            if (allow_update_cdf)
              update_cdf(fc->interintra_cdf[bsize_group], 1, 2);
#endif
            counts->interintra_mode[bsize_group][mbmi->interintra_mode]++;
            if (allow_update_cdf)
              update_cdf(fc->interintra_mode_cdf[bsize_group],
                         mbmi->interintra_mode, INTERINTRA_MODES);
            if (is_interintra_wedge_used(bsize)) {
              counts->wedge_interintra[bsize][mbmi->use_wedge_interintra]++;
#if CONFIG_NEW_MULTISYMBOL
              if (allow_update_cdf)
                update_cdf(fc->wedge_interintra_cdf[bsize],
                           mbmi->use_wedge_interintra, 2);
#endif
            }
          } else {
            counts->interintra[bsize_group][0]++;
#if CONFIG_NEW_MULTISYMBOL
            if (allow_update_cdf)
              update_cdf(fc->interintra_cdf[bsize_group], 0, 2);
#endif
          }
        }

        set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
        const MOTION_MODE motion_allowed =
            motion_mode_allowed(0, xd->global_motion, xd, mi);
        if (mbmi->ref_frame[1] != INTRA_FRAME) {
          if (motion_allowed == WARPED_CAUSAL) {
            counts->motion_mode[mbmi->sb_type][mbmi->motion_mode]++;
            if (allow_update_cdf)
              update_cdf(fc->motion_mode_cdf[mbmi->sb_type], mbmi->motion_mode,
                         MOTION_MODES);
          } else if (motion_allowed == OBMC_CAUSAL) {
            counts->obmc[mbmi->sb_type][mbmi->motion_mode == OBMC_CAUSAL]++;
#if CONFIG_NEW_MULTISYMBOL
            if (allow_update_cdf)
              update_cdf(fc->obmc_cdf[mbmi->sb_type],
                         mbmi->motion_mode == OBMC_CAUSAL, 2);
#endif
          }
        }

        if (
#if CONFIG_COMPOUND_SINGLEREF
            is_inter_anyref_comp_mode(mbmi->mode)
#else   // !CONFIG_COMPOUND_SINGLEREF
            cm->reference_mode != SINGLE_REFERENCE &&
            is_inter_compound_mode(mbmi->mode)
#endif  // CONFIG_COMPOUND_SINGLEREF
            && mbmi->motion_mode == SIMPLE_TRANSLATION) {
          if (is_interinter_compound_used(COMPOUND_WEDGE, bsize)) {
            counts
                ->compound_interinter[bsize][mbmi->interinter_compound_type]++;
            if (allow_update_cdf)
              update_cdf(fc->compound_type_cdf[bsize],
                         mbmi->interinter_compound_type, COMPOUND_TYPES);
          }
        }
      }
    }

    if (inter_block &&
        !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
      int16_t mode_ctx;
      const PREDICTION_MODE mode = mbmi->mode;
      if (has_second_ref(mbmi)) {
        mode_ctx = mbmi_ext->compound_mode_context[mbmi->ref_frame[0]];
        ++counts->inter_compound_mode[mode_ctx][INTER_COMPOUND_OFFSET(mode)];
        if (allow_update_cdf)
          update_cdf(fc->inter_compound_mode_cdf[mode_ctx],
                     INTER_COMPOUND_OFFSET(mode), INTER_COMPOUND_MODES);
#if CONFIG_COMPOUND_SINGLEREF
      } else if (is_inter_singleref_comp_mode(mode)) {
        mode_ctx = mbmi_ext->compound_mode_context[mbmi->ref_frame[0]];
        ++counts->inter_singleref_comp_mode[mode_ctx]
                                           [INTER_SINGLEREF_COMP_OFFSET(mode)];
#endif  // CONFIG_COMPOUND_SINGLEREF
      } else {
        mode_ctx = av1_mode_context_analyzer(mbmi_ext->mode_context,
                                             mbmi->ref_frame, bsize, -1);
        update_inter_mode_stats(counts, mode, mode_ctx);
      }

      int mode_allowed = (mbmi->mode == NEWMV);
      mode_allowed |= (mbmi->mode == NEW_NEWMV);
#if CONFIG_COMPOUND_SINGLEREF
      mode_allowed |= (mbmi->mode == SR_NEW_NEWMV);
#endif  // CONFIG_COMPOUND_SINGLEREF
      if (mode_allowed) {
        uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
        int idx;

        for (idx = 0; idx < 2; ++idx) {
          if (mbmi_ext->ref_mv_count[ref_frame_type] > idx + 1) {
            uint8_t drl_ctx =
                av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type], idx);
            ++counts->drl_mode[drl_ctx][mbmi->ref_mv_idx != idx];

            if (mbmi->ref_mv_idx == idx) break;
          }
        }
      }

      if (have_nearmv_in_inter_mode(mbmi->mode)) {
        uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
        int idx;

        for (idx = 1; idx < 3; ++idx) {
          if (mbmi_ext->ref_mv_count[ref_frame_type] > idx + 1) {
            uint8_t drl_ctx =
                av1_drl_ctx(mbmi_ext->ref_mv_stack[ref_frame_type], idx);
            ++counts->drl_mode[drl_ctx][mbmi->ref_mv_idx != idx - 1];

            if (mbmi->ref_mv_idx == idx - 1) break;
          }
        }
      }

#if CONFIG_JNT_COMP
#if CONFIG_NEW_MULTISYMBOL
      if (has_second_ref(mbmi)) {
        const int comp_index_ctx = get_comp_index_context(cm, xd);
        ++counts->compound_index[comp_index_ctx][mbmi->compound_idx];
        update_cdf(fc->compound_index_cdf[comp_index_ctx], mbmi->compound_idx,
                   2);
      }
#endif  // CONFIG_NEW_MULTISYMBOL
#endif  // CONFIG_JNT_COMP
    }
  }
}

typedef struct {
  ENTROPY_CONTEXT a[2 * MAX_MIB_SIZE * MAX_MB_PLANE];
  ENTROPY_CONTEXT l[2 * MAX_MIB_SIZE * MAX_MB_PLANE];
  PARTITION_CONTEXT sa[MAX_MIB_SIZE];
  PARTITION_CONTEXT sl[MAX_MIB_SIZE];
  TXFM_CONTEXT *p_ta;
  TXFM_CONTEXT *p_tl;
  TXFM_CONTEXT ta[2 * MAX_MIB_SIZE];
  TXFM_CONTEXT tl[2 * MAX_MIB_SIZE];
} RD_SEARCH_MACROBLOCK_CONTEXT;

static void restore_context(MACROBLOCK *x,
                            const RD_SEARCH_MACROBLOCK_CONTEXT *ctx, int mi_row,
                            int mi_col, BLOCK_SIZE bsize) {
  MACROBLOCKD *xd = &x->e_mbd;
  int p;
  const int num_4x4_blocks_wide =
      block_size_wide[bsize] >> tx_size_wide_log2[0];
  const int num_4x4_blocks_high =
      block_size_high[bsize] >> tx_size_high_log2[0];
  int mi_width = mi_size_wide[bsize];
  int mi_height = mi_size_high[bsize];
  for (p = 0; p < MAX_MB_PLANE; p++) {
    int tx_col;
    int tx_row;
    tx_col = mi_col << (MI_SIZE_LOG2 - tx_size_wide_log2[0]);
    tx_row = (mi_row & MAX_MIB_MASK) << (MI_SIZE_LOG2 - tx_size_high_log2[0]);
    memcpy(xd->above_context[p] + (tx_col >> xd->plane[p].subsampling_x),
           ctx->a + num_4x4_blocks_wide * p,
           (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_wide) >>
               xd->plane[p].subsampling_x);
    memcpy(xd->left_context[p] + (tx_row >> xd->plane[p].subsampling_y),
           ctx->l + num_4x4_blocks_high * p,
           (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_high) >>
               xd->plane[p].subsampling_y);
  }
  memcpy(xd->above_seg_context + mi_col, ctx->sa,
         sizeof(*xd->above_seg_context) * mi_width);
  memcpy(xd->left_seg_context + (mi_row & MAX_MIB_MASK), ctx->sl,
         sizeof(xd->left_seg_context[0]) * mi_height);
  xd->above_txfm_context = ctx->p_ta;
  xd->left_txfm_context = ctx->p_tl;
  memcpy(xd->above_txfm_context, ctx->ta,
         sizeof(*xd->above_txfm_context) * (mi_width << TX_UNIT_WIDE_LOG2));
  memcpy(xd->left_txfm_context, ctx->tl,
         sizeof(*xd->left_txfm_context) * (mi_height << TX_UNIT_HIGH_LOG2));
}

static void save_context(const MACROBLOCK *x, RD_SEARCH_MACROBLOCK_CONTEXT *ctx,
                         int mi_row, int mi_col, BLOCK_SIZE bsize) {
  const MACROBLOCKD *xd = &x->e_mbd;
  int p;
  const int num_4x4_blocks_wide =
      block_size_wide[bsize] >> tx_size_wide_log2[0];
  const int num_4x4_blocks_high =
      block_size_high[bsize] >> tx_size_high_log2[0];
  int mi_width = mi_size_wide[bsize];
  int mi_height = mi_size_high[bsize];

  // buffer the above/left context information of the block in search.
  for (p = 0; p < MAX_MB_PLANE; ++p) {
    int tx_col;
    int tx_row;
    tx_col = mi_col << (MI_SIZE_LOG2 - tx_size_wide_log2[0]);
    tx_row = (mi_row & MAX_MIB_MASK) << (MI_SIZE_LOG2 - tx_size_high_log2[0]);
    memcpy(ctx->a + num_4x4_blocks_wide * p,
           xd->above_context[p] + (tx_col >> xd->plane[p].subsampling_x),
           (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_wide) >>
               xd->plane[p].subsampling_x);
    memcpy(ctx->l + num_4x4_blocks_high * p,
           xd->left_context[p] + (tx_row >> xd->plane[p].subsampling_y),
           (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_high) >>
               xd->plane[p].subsampling_y);
  }
  memcpy(ctx->sa, xd->above_seg_context + mi_col,
         sizeof(*xd->above_seg_context) * mi_width);
  memcpy(ctx->sl, xd->left_seg_context + (mi_row & MAX_MIB_MASK),
         sizeof(xd->left_seg_context[0]) * mi_height);
  memcpy(ctx->ta, xd->above_txfm_context,
         sizeof(*xd->above_txfm_context) * (mi_width << TX_UNIT_WIDE_LOG2));
  memcpy(ctx->tl, xd->left_txfm_context,
         sizeof(*xd->left_txfm_context) * (mi_height << TX_UNIT_HIGH_LOG2));
  ctx->p_ta = xd->above_txfm_context;
  ctx->p_tl = xd->left_txfm_context;
}

static void encode_b(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                     ThreadData *td, TOKENEXTRA **tp, int mi_row, int mi_col,
                     RUN_TYPE dry_run, BLOCK_SIZE bsize,
#if CONFIG_EXT_PARTITION_TYPES
                     PARTITION_TYPE partition,
#endif
                     PICK_MODE_CONTEXT *ctx, int *rate) {
  TileInfo *const tile = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
#if CONFIG_NCOBMC || CONFIG_EXT_DELTA_Q
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi;
#if CONFIG_NCOBMC
  int check_ncobmc;
#endif
#endif

  set_offsets(cpi, tile, x, mi_row, mi_col, bsize);
#if CONFIG_EXT_PARTITION_TYPES
  x->e_mbd.mi[0]->mbmi.partition = partition;
#endif
  update_state(cpi, tile_data, td, ctx, mi_row, mi_col, bsize, dry_run);
#if CONFIG_NCOBMC
  mbmi = &xd->mi[0]->mbmi;
  set_ref_ptrs(&cpi->common, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
#endif

#if CONFIG_NCOBMC
  const MOTION_MODE motion_allowed =
      motion_mode_allowed(0, xd->global_motion, xd, xd->mi[0]);
#endif

#if CONFIG_NCOBMC
  check_ncobmc = is_inter_block(mbmi) && motion_allowed >= OBMC_CAUSAL;
  if (!dry_run && check_ncobmc) {
    av1_check_ncobmc_rd(cpi, x, mi_row, mi_col);
    av1_setup_dst_planes(x->e_mbd.plane, bsize,
                         get_frame_new_buffer(&cpi->common), mi_row, mi_col);
  }
#endif

#if CONFIG_LV_MAP
  av1_set_coeff_buffer(cpi, x, mi_row, mi_col);
#endif

  encode_superblock(cpi, tile_data, td, tp, dry_run, mi_row, mi_col, bsize,
                    rate);

#if CONFIG_LV_MAP
  if (dry_run == 0)
    x->cb_offset += block_size_wide[bsize] * block_size_high[bsize];
#endif

  if (!dry_run) {
#if CONFIG_EXT_DELTA_Q
    mbmi = &xd->mi[0]->mbmi;
    if (bsize == cpi->common.sb_size && mbmi->skip == 1 &&
        cpi->common.delta_lf_present_flag) {
#if CONFIG_LOOPFILTER_LEVEL
      for (int lf_id = 0; lf_id < FRAME_LF_COUNT; ++lf_id)
        mbmi->curr_delta_lf[lf_id] = xd->prev_delta_lf[lf_id];
#endif  // CONFIG_LOOPFILTER_LEVEL
      mbmi->current_delta_lf_from_base = xd->prev_delta_lf_from_base;
    }
#endif
    update_stats(&cpi->common, tile_data, td, mi_row, mi_col);
  }
}

static void encode_sb(const AV1_COMP *const cpi, ThreadData *td,
                      TileDataEnc *tile_data, TOKENEXTRA **tp, int mi_row,
                      int mi_col, RUN_TYPE dry_run, BLOCK_SIZE bsize,
                      PC_TREE *pc_tree, int *rate) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int hbs = mi_size_wide[bsize] / 2;
#if CONFIG_EXT_PARTITION_TYPES && CONFIG_EXT_PARTITION_TYPES_AB
  const int qbs = mi_size_wide[bsize] / 4;
#endif
  const int is_partition_root = bsize >= BLOCK_8X8;
  const int ctx = is_partition_root
                      ? partition_plane_context(xd, mi_row, mi_col, bsize)
                      : -1;
  const PARTITION_TYPE partition = pc_tree->partitioning;
  const BLOCK_SIZE subsize = get_subsize(bsize, partition);
#if CONFIG_EXT_PARTITION_TYPES
  int quarter_step = mi_size_wide[bsize] / 4;
  int i;
#if !CONFIG_EXT_PARTITION_TYPES_AB
  BLOCK_SIZE bsize2 = get_subsize(bsize, PARTITION_SPLIT);
#endif  // !CONFIG_EXT_PARTITION_TYPES_AB
#endif  // CONFIG_EXT_PARTITION_TYPES

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  if (!dry_run && ctx >= 0) td->counts->partition[ctx][partition]++;

  switch (partition) {
    case PARTITION_NONE:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
#if CONFIG_EXT_PARTITION_TYPES
               partition,
#endif
               &pc_tree->none, rate);
      break;
    case PARTITION_VERT:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
#if CONFIG_EXT_PARTITION_TYPES
               partition,
#endif
               &pc_tree->vertical[0], rate);
      if (mi_col + hbs < cm->mi_cols) {
        encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs, dry_run, subsize,
#if CONFIG_EXT_PARTITION_TYPES
                 partition,
#endif
                 &pc_tree->vertical[1], rate);
      }
      break;
    case PARTITION_HORZ:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
#if CONFIG_EXT_PARTITION_TYPES
               partition,
#endif
               &pc_tree->horizontal[0], rate);
      if (mi_row + hbs < cm->mi_rows) {
        encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col, dry_run, subsize,
#if CONFIG_EXT_PARTITION_TYPES
                 partition,
#endif
                 &pc_tree->horizontal[1], rate);
      }
      break;
    case PARTITION_SPLIT:
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, dry_run, subsize,
                pc_tree->split[0], rate);
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col + hbs, dry_run, subsize,
                pc_tree->split[1], rate);
      encode_sb(cpi, td, tile_data, tp, mi_row + hbs, mi_col, dry_run, subsize,
                pc_tree->split[2], rate);
      encode_sb(cpi, td, tile_data, tp, mi_row + hbs, mi_col + hbs, dry_run,
                subsize, pc_tree->split[3], rate);
      break;

#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION_TYPES_AB
    case PARTITION_HORZ_A:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run,
               get_subsize(bsize, PARTITION_HORZ_4), partition,
               &pc_tree->horizontala[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + qbs, mi_col, dry_run,
               get_subsize(bsize, PARTITION_HORZ_4), partition,
               &pc_tree->horizontala[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col, dry_run, subsize,
               partition, &pc_tree->horizontala[2], rate);
      break;
    case PARTITION_HORZ_B:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, &pc_tree->horizontalb[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col, dry_run,
               get_subsize(bsize, PARTITION_HORZ_4), partition,
               &pc_tree->horizontalb[1], rate);
      if (mi_row + 3 * qbs < cm->mi_rows)
        encode_b(cpi, tile_data, td, tp, mi_row + 3 * qbs, mi_col, dry_run,
                 get_subsize(bsize, PARTITION_HORZ_4), partition,
                 &pc_tree->horizontalb[2], rate);
      break;
    case PARTITION_VERT_A:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run,
               get_subsize(bsize, PARTITION_VERT_4), partition,
               &pc_tree->verticala[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + qbs, dry_run,
               get_subsize(bsize, PARTITION_VERT_4), partition,
               &pc_tree->verticala[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs, dry_run, subsize,
               partition, &pc_tree->verticala[2], rate);

      break;
    case PARTITION_VERT_B:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, &pc_tree->verticalb[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs, dry_run,
               get_subsize(bsize, PARTITION_VERT_4), partition,
               &pc_tree->verticalb[1], rate);
      if (mi_col + 3 * qbs < cm->mi_cols)
        encode_b(cpi, tile_data, td, tp, mi_row, mi_col + 3 * qbs, dry_run,
                 get_subsize(bsize, PARTITION_VERT_4), partition,
                 &pc_tree->verticalb[2], rate);
      break;
#else
    case PARTITION_HORZ_A:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, bsize2,
               partition, &pc_tree->horizontala[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs, dry_run, bsize2,
               partition, &pc_tree->horizontala[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col, dry_run, subsize,
               partition, &pc_tree->horizontala[2], rate);
      break;
    case PARTITION_HORZ_B:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, &pc_tree->horizontalb[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col, dry_run, bsize2,
               partition, &pc_tree->horizontalb[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col + hbs, dry_run,
               bsize2, partition, &pc_tree->horizontalb[2], rate);
      break;
    case PARTITION_VERT_A:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, bsize2,
               partition, &pc_tree->verticala[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col, dry_run, bsize2,
               partition, &pc_tree->verticala[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs, dry_run, subsize,
               partition, &pc_tree->verticala[2], rate);

      break;
    case PARTITION_VERT_B:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, &pc_tree->verticalb[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs, dry_run, bsize2,
               partition, &pc_tree->verticalb[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col + hbs, dry_run,
               bsize2, partition, &pc_tree->verticalb[2], rate);
      break;
#endif
    case PARTITION_HORZ_4:
      for (i = 0; i < 4; ++i) {
        int this_mi_row = mi_row + i * quarter_step;
        if (i > 0 && this_mi_row >= cm->mi_rows) break;

        encode_b(cpi, tile_data, td, tp, this_mi_row, mi_col, dry_run, subsize,
                 partition, &pc_tree->horizontal4[i], rate);
      }
      break;
    case PARTITION_VERT_4:
      for (i = 0; i < 4; ++i) {
        int this_mi_col = mi_col + i * quarter_step;
        if (i > 0 && this_mi_col >= cm->mi_cols) break;

        encode_b(cpi, tile_data, td, tp, mi_row, this_mi_col, dry_run, subsize,
                 partition, &pc_tree->vertical4[i], rate);
      }
      break;
#endif  // CONFIG_EXT_PARTITION_TYPES
    default: assert(0 && "Invalid partition type."); break;
  }

#if CONFIG_EXT_PARTITION_TYPES
  update_ext_partition_context(xd, mi_row, mi_col, subsize, bsize, partition);
#else
  if (partition != PARTITION_SPLIT || bsize == BLOCK_8X8)
    update_partition_context(xd, mi_row, mi_col, subsize, bsize);
#endif  // CONFIG_EXT_PARTITION_TYPES
}

// Check to see if the given partition size is allowed for a specified number
// of mi block rows and columns remaining in the image.
// If not then return the largest allowed partition size
static BLOCK_SIZE find_partition_size(BLOCK_SIZE bsize, int rows_left,
                                      int cols_left, int *bh, int *bw) {
  if (rows_left <= 0 || cols_left <= 0) {
    return AOMMIN(bsize, BLOCK_8X8);
  } else {
    for (; bsize > 0; bsize -= 3) {
      *bh = mi_size_high[bsize];
      *bw = mi_size_wide[bsize];
      if ((*bh <= rows_left) && (*bw <= cols_left)) {
        break;
      }
    }
  }
  return bsize;
}

static void set_partial_sb_partition(const AV1_COMMON *const cm, MODE_INFO *mi,
                                     int bh_in, int bw_in,
                                     int mi_rows_remaining,
                                     int mi_cols_remaining, BLOCK_SIZE bsize,
                                     MODE_INFO **mib) {
  int bh = bh_in;
  int r, c;
  for (r = 0; r < cm->mib_size; r += bh) {
    int bw = bw_in;
    for (c = 0; c < cm->mib_size; c += bw) {
      const int index = r * cm->mi_stride + c;
      mib[index] = mi + index;
      mib[index]->mbmi.sb_type = find_partition_size(
          bsize, mi_rows_remaining - r, mi_cols_remaining - c, &bh, &bw);
    }
  }
}

// This function attempts to set all mode info entries in a given superblock
// to the same block partition size.
// However, at the bottom and right borders of the image the requested size
// may not be allowed in which case this code attempts to choose the largest
// allowable partition.
static void set_fixed_partitioning(AV1_COMP *cpi, const TileInfo *const tile,
                                   MODE_INFO **mib, int mi_row, int mi_col,
                                   BLOCK_SIZE bsize) {
  AV1_COMMON *const cm = &cpi->common;
  const int mi_rows_remaining = tile->mi_row_end - mi_row;
  const int mi_cols_remaining = tile->mi_col_end - mi_col;
  int block_row, block_col;
  MODE_INFO *const mi_upper_left = cm->mi + mi_row * cm->mi_stride + mi_col;
  int bh = mi_size_high[bsize];
  int bw = mi_size_wide[bsize];

  assert((mi_rows_remaining > 0) && (mi_cols_remaining > 0));

  // Apply the requested partition size to the SB if it is all "in image"
  if ((mi_cols_remaining >= cm->mib_size) &&
      (mi_rows_remaining >= cm->mib_size)) {
    for (block_row = 0; block_row < cm->mib_size; block_row += bh) {
      for (block_col = 0; block_col < cm->mib_size; block_col += bw) {
        int index = block_row * cm->mi_stride + block_col;
        mib[index] = mi_upper_left + index;
        mib[index]->mbmi.sb_type = bsize;
      }
    }
  } else {
    // Else this is a partial SB.
    set_partial_sb_partition(cm, mi_upper_left, bh, bw, mi_rows_remaining,
                             mi_cols_remaining, bsize, mib);
  }
}

static void rd_use_partition(AV1_COMP *cpi, ThreadData *td,
                             TileDataEnc *tile_data, MODE_INFO **mib,
                             TOKENEXTRA **tp, int mi_row, int mi_col,
                             BLOCK_SIZE bsize, int *rate, int64_t *dist,
                             int do_recon, PC_TREE *pc_tree) {
  AV1_COMMON *const cm = &cpi->common;
  TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int bs = mi_size_wide[bsize];
  const int hbs = bs / 2;
  int i;
  const int pl = (bsize >= BLOCK_8X8)
                     ? partition_plane_context(xd, mi_row, mi_col, bsize)
                     : 0;
  const PARTITION_TYPE partition =
      (bsize >= BLOCK_8X8) ? get_partition(cm, mi_row, mi_col, bsize)
                           : PARTITION_NONE;
  const BLOCK_SIZE subsize = get_subsize(bsize, partition);
  RD_SEARCH_MACROBLOCK_CONTEXT x_ctx;
  RD_STATS last_part_rdc, none_rdc, chosen_rdc;
  BLOCK_SIZE sub_subsize = BLOCK_4X4;
  int splits_below = 0;
  BLOCK_SIZE bs_type = mib[0]->mbmi.sb_type;
  int do_partition_search = 1;
  PICK_MODE_CONTEXT *ctx_none = &pc_tree->none;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  assert(num_4x4_blocks_wide_lookup[bsize] ==
         num_4x4_blocks_high_lookup[bsize]);

  av1_invalid_rd_stats(&last_part_rdc);
  av1_invalid_rd_stats(&none_rdc);
  av1_invalid_rd_stats(&chosen_rdc);

  pc_tree->partitioning = partition;

  xd->above_txfm_context =
      cm->above_txfm_context + (mi_col << TX_UNIT_WIDE_LOG2);
  xd->left_txfm_context = xd->left_txfm_context_buffer +
                          ((mi_row & MAX_MIB_MASK) << TX_UNIT_HIGH_LOG2);
  save_context(x, &x_ctx, mi_row, mi_col, bsize);

  if (bsize == BLOCK_16X16 && cpi->vaq_refresh) {
    set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize);
    x->mb_energy = av1_block_energy(cpi, x, bsize);
  }

  if (do_partition_search &&
      cpi->sf.partition_search_type == SEARCH_PARTITION &&
      cpi->sf.adjust_partitioning_from_last_frame) {
    // Check if any of the sub blocks are further split.
    if (partition == PARTITION_SPLIT && subsize > BLOCK_8X8) {
      sub_subsize = get_subsize(subsize, PARTITION_SPLIT);
      splits_below = 1;
      for (i = 0; i < 4; i++) {
        int jj = i >> 1, ii = i & 0x01;
        MODE_INFO *this_mi = mib[jj * hbs * cm->mi_stride + ii * hbs];
        if (this_mi && this_mi->mbmi.sb_type >= sub_subsize) {
          splits_below = 0;
        }
      }
    }

    // If partition is not none try none unless each of the 4 splits are split
    // even further..
    if (partition != PARTITION_NONE && !splits_below &&
        mi_row + hbs < cm->mi_rows && mi_col + hbs < cm->mi_cols) {
      pc_tree->partitioning = PARTITION_NONE;
      rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &none_rdc,
#if CONFIG_EXT_PARTITION_TYPES
                       PARTITION_NONE,
#endif
                       bsize, ctx_none, INT64_MAX);

      if (none_rdc.rate < INT_MAX) {
        none_rdc.rate += x->partition_cost[pl][PARTITION_NONE];
        none_rdc.rdcost = RDCOST(x->rdmult, none_rdc.rate, none_rdc.dist);
      }

      restore_context(x, &x_ctx, mi_row, mi_col, bsize);
      mib[0]->mbmi.sb_type = bs_type;
      pc_tree->partitioning = partition;
    }
  }

  switch (partition) {
    case PARTITION_NONE:
      rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &last_part_rdc,
#if CONFIG_EXT_PARTITION_TYPES
                       PARTITION_NONE,
#endif
                       bsize, ctx_none, INT64_MAX);
      break;
    case PARTITION_HORZ:
      rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &last_part_rdc,
#if CONFIG_EXT_PARTITION_TYPES
                       PARTITION_HORZ,
#endif
                       subsize, &pc_tree->horizontal[0], INT64_MAX);
      if (last_part_rdc.rate != INT_MAX && bsize >= BLOCK_8X8 &&
          mi_row + hbs < cm->mi_rows) {
        RD_STATS tmp_rdc;
        PICK_MODE_CONTEXT *ctx_h = &pc_tree->horizontal[0];
        av1_init_rd_stats(&tmp_rdc);
        update_state(cpi, tile_data, td, ctx_h, mi_row, mi_col, subsize, 1);
        encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, mi_row,
                          mi_col, subsize, NULL);
        rd_pick_sb_modes(cpi, tile_data, x, mi_row + hbs, mi_col, &tmp_rdc,
#if CONFIG_EXT_PARTITION_TYPES
                         PARTITION_HORZ,
#endif
                         subsize, &pc_tree->horizontal[1], INT64_MAX);
        if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
          av1_invalid_rd_stats(&last_part_rdc);
          break;
        }
        last_part_rdc.rate += tmp_rdc.rate;
        last_part_rdc.dist += tmp_rdc.dist;
        last_part_rdc.rdcost += tmp_rdc.rdcost;
      }
      break;
    case PARTITION_VERT:
      rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &last_part_rdc,
#if CONFIG_EXT_PARTITION_TYPES
                       PARTITION_VERT,
#endif
                       subsize, &pc_tree->vertical[0], INT64_MAX);
      if (last_part_rdc.rate != INT_MAX && bsize >= BLOCK_8X8 &&
          mi_col + hbs < cm->mi_cols) {
        RD_STATS tmp_rdc;
        PICK_MODE_CONTEXT *ctx_v = &pc_tree->vertical[0];
        av1_init_rd_stats(&tmp_rdc);
        update_state(cpi, tile_data, td, ctx_v, mi_row, mi_col, subsize, 1);
        encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, mi_row,
                          mi_col, subsize, NULL);
        rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col + hbs, &tmp_rdc,
#if CONFIG_EXT_PARTITION_TYPES
                         PARTITION_VERT,
#endif
                         subsize, &pc_tree->vertical[bsize > BLOCK_8X8],
                         INT64_MAX);
        if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
          av1_invalid_rd_stats(&last_part_rdc);
          break;
        }
        last_part_rdc.rate += tmp_rdc.rate;
        last_part_rdc.dist += tmp_rdc.dist;
        last_part_rdc.rdcost += tmp_rdc.rdcost;
      }
      break;
    case PARTITION_SPLIT:
      last_part_rdc.rate = 0;
      last_part_rdc.dist = 0;
      last_part_rdc.rdcost = 0;
      for (i = 0; i < 4; i++) {
        int x_idx = (i & 1) * hbs;
        int y_idx = (i >> 1) * hbs;
        int jj = i >> 1, ii = i & 0x01;
        RD_STATS tmp_rdc;
        if ((mi_row + y_idx >= cm->mi_rows) || (mi_col + x_idx >= cm->mi_cols))
          continue;

        av1_init_rd_stats(&tmp_rdc);
        rd_use_partition(cpi, td, tile_data,
                         mib + jj * hbs * cm->mi_stride + ii * hbs, tp,
                         mi_row + y_idx, mi_col + x_idx, subsize, &tmp_rdc.rate,
                         &tmp_rdc.dist, i != 3, pc_tree->split[i]);
        if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
          av1_invalid_rd_stats(&last_part_rdc);
          break;
        }
        last_part_rdc.rate += tmp_rdc.rate;
        last_part_rdc.dist += tmp_rdc.dist;
      }
      break;
#if CONFIG_EXT_PARTITION_TYPES
    case PARTITION_VERT_A:
    case PARTITION_VERT_B:
    case PARTITION_HORZ_A:
    case PARTITION_HORZ_B:
    case PARTITION_HORZ_4:
    case PARTITION_VERT_4: assert(0 && "Cannot handle extended partiton types");
#endif  //  CONFIG_EXT_PARTITION_TYPES
    default: assert(0); break;
  }

  if (last_part_rdc.rate < INT_MAX) {
    last_part_rdc.rate += x->partition_cost[pl][partition];
    last_part_rdc.rdcost =
        RDCOST(x->rdmult, last_part_rdc.rate, last_part_rdc.dist);
  }

  if (do_partition_search && cpi->sf.adjust_partitioning_from_last_frame &&
      cpi->sf.partition_search_type == SEARCH_PARTITION &&
      partition != PARTITION_SPLIT && bsize > BLOCK_8X8 &&
      (mi_row + bs < cm->mi_rows || mi_row + hbs == cm->mi_rows) &&
      (mi_col + bs < cm->mi_cols || mi_col + hbs == cm->mi_cols)) {
    BLOCK_SIZE split_subsize = get_subsize(bsize, PARTITION_SPLIT);
    chosen_rdc.rate = 0;
    chosen_rdc.dist = 0;

    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
    pc_tree->partitioning = PARTITION_SPLIT;

    // Split partition.
    for (i = 0; i < 4; i++) {
      int x_idx = (i & 1) * hbs;
      int y_idx = (i >> 1) * hbs;
      RD_STATS tmp_rdc;

      if ((mi_row + y_idx >= cm->mi_rows) || (mi_col + x_idx >= cm->mi_cols))
        continue;

      save_context(x, &x_ctx, mi_row, mi_col, bsize);
      pc_tree->split[i]->partitioning = PARTITION_NONE;
      rd_pick_sb_modes(cpi, tile_data, x, mi_row + y_idx, mi_col + x_idx,
                       &tmp_rdc,
#if CONFIG_EXT_PARTITION_TYPES
                       PARTITION_SPLIT,
#endif
                       split_subsize, &pc_tree->split[i]->none, INT64_MAX);

      restore_context(x, &x_ctx, mi_row, mi_col, bsize);
      if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
        av1_invalid_rd_stats(&chosen_rdc);
        break;
      }

      chosen_rdc.rate += tmp_rdc.rate;
      chosen_rdc.dist += tmp_rdc.dist;

      if (i != 3)
        encode_sb(cpi, td, tile_data, tp, mi_row + y_idx, mi_col + x_idx,
                  OUTPUT_ENABLED, split_subsize, pc_tree->split[i], NULL);

      chosen_rdc.rate += x->partition_cost[pl][PARTITION_NONE];
    }
    if (chosen_rdc.rate < INT_MAX) {
      chosen_rdc.rate += x->partition_cost[pl][PARTITION_SPLIT];
      chosen_rdc.rdcost = RDCOST(x->rdmult, chosen_rdc.rate, chosen_rdc.dist);
    }
  }

  // If last_part is better set the partitioning to that.
  if (last_part_rdc.rdcost < chosen_rdc.rdcost) {
    mib[0]->mbmi.sb_type = bsize;
    if (bsize >= BLOCK_8X8) pc_tree->partitioning = partition;
    chosen_rdc = last_part_rdc;
  }
  // If none was better set the partitioning to that.
  if (none_rdc.rdcost < chosen_rdc.rdcost) {
    if (bsize >= BLOCK_8X8) pc_tree->partitioning = PARTITION_NONE;
    chosen_rdc = none_rdc;
  }

  restore_context(x, &x_ctx, mi_row, mi_col, bsize);

  // We must have chosen a partitioning and encoding or we'll fail later on.
  // No other opportunities for success.
  if (bsize == cm->sb_size)
    assert(chosen_rdc.rate < INT_MAX && chosen_rdc.dist < INT64_MAX);

  if (do_recon) {
    if (bsize == cm->sb_size) {
      // NOTE: To get estimate for rate due to the tokens, use:
      // int rate_coeffs = 0;
      // encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_COSTCOEFFS,
      //           bsize, pc_tree, &rate_coeffs);
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, OUTPUT_ENABLED, bsize,
                pc_tree, NULL);
    } else {
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_NORMAL, bsize,
                pc_tree, NULL);
    }
  }

  *rate = chosen_rdc.rate;
  *dist = chosen_rdc.dist;
}

/* clang-format off */
static const BLOCK_SIZE min_partition_size[BLOCK_SIZES_ALL] = {
  BLOCK_2X2,   BLOCK_2X2,   BLOCK_2X2,    //    2x2,    2x4,     4x2
                            BLOCK_4X4,    //                     4x4
  BLOCK_4X4,   BLOCK_4X4,   BLOCK_4X4,    //    4x8,    8x4,     8x8
  BLOCK_4X4,   BLOCK_4X4,   BLOCK_8X8,    //   8x16,   16x8,   16x16
  BLOCK_8X8,   BLOCK_8X8,   BLOCK_16X16,  //  16x32,  32x16,   32x32
  BLOCK_16X16, BLOCK_16X16, BLOCK_16X16,  //  32x64,  64x32,   64x64
#if CONFIG_EXT_PARTITION
  BLOCK_16X16, BLOCK_16X16, BLOCK_16X16,  // 64x128, 128x64, 128x128
#endif  // CONFIG_EXT_PARTITION
  BLOCK_4X4,   BLOCK_4X4,   BLOCK_8X8,    //   4x16,   16x4,    8x32
  BLOCK_8X8,   BLOCK_16X16, BLOCK_16X16,  //   32x8,  16x64,   64x16
#if CONFIG_EXT_PARTITION
  BLOCK_16X16, BLOCK_16X16                // 32x128, 128x32
#endif  // CONFIG_EXT_PARTITION
};

static const BLOCK_SIZE max_partition_size[BLOCK_SIZES_ALL] = {
  BLOCK_4X4,     BLOCK_4X4,       BLOCK_4X4,    //    2x2,    2x4,     4x2
                                  BLOCK_8X8,    //                     4x4
  BLOCK_16X16,   BLOCK_16X16,   BLOCK_16X16,    //    4x8,    8x4,     8x8
  BLOCK_32X32,   BLOCK_32X32,   BLOCK_32X32,    //   8x16,   16x8,   16x16
  BLOCK_64X64,   BLOCK_64X64,   BLOCK_64X64,    //  16x32,  32x16,   32x32
  BLOCK_LARGEST, BLOCK_LARGEST, BLOCK_LARGEST,  //  32x64,  64x32,   64x64
#if CONFIG_EXT_PARTITION
  BLOCK_LARGEST, BLOCK_LARGEST, BLOCK_LARGEST,  // 64x128, 128x64, 128x128
#endif  // CONFIG_EXT_PARTITION
  BLOCK_16X16,   BLOCK_16X16,   BLOCK_32X32,    //   4x16,   16x4,    8x32
  BLOCK_32X32,   BLOCK_LARGEST, BLOCK_LARGEST,  //   32x8,  16x64,   64x16
#if CONFIG_EXT_PARTITION
  BLOCK_LARGEST, BLOCK_LARGEST                  // 32x128, 128x32
#endif  // CONFIG_EXT_PARTITION
};

// Next square block size less or equal than current block size.
static const BLOCK_SIZE next_square_size[BLOCK_SIZES_ALL] = {
  BLOCK_2X2,   BLOCK_2X2,     BLOCK_2X2,    //    2x2,    2x4,     4x2
                              BLOCK_4X4,    //                     4x4
  BLOCK_4X4,   BLOCK_4X4,     BLOCK_8X8,    //    4x8,    8x4,     8x8
  BLOCK_8X8,   BLOCK_8X8,     BLOCK_16X16,  //   8x16,   16x8,   16x16
  BLOCK_16X16, BLOCK_16X16,   BLOCK_32X32,  //  16x32,  32x16,   32x32
  BLOCK_32X32, BLOCK_32X32,   BLOCK_64X64,  //  32x64,  64x32,   64x64
#if CONFIG_EXT_PARTITION
  BLOCK_64X64, BLOCK_64X64, BLOCK_128X128,  // 64x128, 128x64, 128x128
#endif  // CONFIG_EXT_PARTITION
  BLOCK_4X4,   BLOCK_4X4,   BLOCK_8X8,      //   4x16,   16x4,    8x32
  BLOCK_8X8,   BLOCK_16X16, BLOCK_16X16,    //   32x8,  16x64,   64x16
#if CONFIG_EXT_PARTITION
  BLOCK_32X32, BLOCK_32X32                  // 32x128, 128x32
#endif  // CONFIG_EXT_PARTITION
};
/* clang-format on */

// Look at all the mode_info entries for blocks that are part of this
// partition and find the min and max values for sb_type.
// At the moment this is designed to work on a superblock but could be
// adjusted to use a size parameter.
//
// The min and max are assumed to have been initialized prior to calling this
// function so repeat calls can accumulate a min and max of more than one
// superblock.
static void get_sb_partition_size_range(const AV1_COMMON *const cm,
                                        MACROBLOCKD *xd, MODE_INFO **mib,
                                        BLOCK_SIZE *min_block_size,
                                        BLOCK_SIZE *max_block_size) {
  int i, j;
  int index = 0;

  // Check the sb_type for each block that belongs to this region.
  for (i = 0; i < cm->mib_size; ++i) {
    for (j = 0; j < cm->mib_size; ++j) {
      MODE_INFO *mi = mib[index + j];
      BLOCK_SIZE sb_type = mi ? mi->mbmi.sb_type : BLOCK_4X4;
      *min_block_size = AOMMIN(*min_block_size, sb_type);
      *max_block_size = AOMMAX(*max_block_size, sb_type);
    }
    index += xd->mi_stride;
  }
}

// Look at neighboring blocks and set a min and max partition size based on
// what they chose.
static void rd_auto_partition_range(AV1_COMP *cpi, const TileInfo *const tile,
                                    MACROBLOCKD *const xd, int mi_row,
                                    int mi_col, BLOCK_SIZE *min_block_size,
                                    BLOCK_SIZE *max_block_size) {
  AV1_COMMON *const cm = &cpi->common;
  MODE_INFO **mi = xd->mi;
  const int left_in_image = xd->left_available && mi[-1];
  const int above_in_image = xd->up_available && mi[-xd->mi_stride];
  const int mi_rows_remaining = tile->mi_row_end - mi_row;
  const int mi_cols_remaining = tile->mi_col_end - mi_col;
  int bh, bw;
  BLOCK_SIZE min_size = BLOCK_4X4;
  BLOCK_SIZE max_size = BLOCK_LARGEST;

  // Trap case where we do not have a prediction.
  if (left_in_image || above_in_image || cm->frame_type != KEY_FRAME) {
    // Default "min to max" and "max to min"
    min_size = BLOCK_LARGEST;
    max_size = BLOCK_4X4;

    // NOTE: each call to get_sb_partition_size_range() uses the previous
    // passed in values for min and max as a starting point.
    // Find the min and max partition used in previous frame at this location
    if (cm->frame_type != KEY_FRAME) {
      MODE_INFO **prev_mi =
          &cm->prev_mi_grid_visible[mi_row * xd->mi_stride + mi_col];
      get_sb_partition_size_range(cm, xd, prev_mi, &min_size, &max_size);
    }
    // Find the min and max partition sizes used in the left superblock
    if (left_in_image) {
      MODE_INFO **left_sb_mi = &mi[-cm->mib_size];
      get_sb_partition_size_range(cm, xd, left_sb_mi, &min_size, &max_size);
    }
    // Find the min and max partition sizes used in the above suprblock.
    if (above_in_image) {
      MODE_INFO **above_sb_mi = &mi[-xd->mi_stride * cm->mib_size];
      get_sb_partition_size_range(cm, xd, above_sb_mi, &min_size, &max_size);
    }

    // Adjust observed min and max for "relaxed" auto partition case.
    if (cpi->sf.auto_min_max_partition_size == RELAXED_NEIGHBORING_MIN_MAX) {
      min_size = min_partition_size[min_size];
      max_size = max_partition_size[max_size];
    }
  }

  // Check border cases where max and min from neighbors may not be legal.
  max_size = find_partition_size(max_size, mi_rows_remaining, mi_cols_remaining,
                                 &bh, &bw);
  min_size = AOMMIN(min_size, max_size);

  // Test for blocks at the edge of the active image.
  // This may be the actual edge of the image or where there are formatting
  // bars.
  if (av1_active_edge_sb(cpi, mi_row, mi_col)) {
    min_size = BLOCK_4X4;
  } else {
    min_size = AOMMIN(cpi->sf.rd_auto_partition_min_limit, min_size);
  }

  // When use_square_partition_only is true, make sure at least one square
  // partition is allowed by selecting the next smaller square size as
  // *min_block_size.
  if (cpi->sf.use_square_partition_only) {
    min_size = AOMMIN(min_size, next_square_size[max_size]);
  }

  *min_block_size = AOMMIN(min_size, cm->sb_size);
  *max_block_size = AOMMIN(max_size, cm->sb_size);
}

// TODO(jingning) refactor functions setting partition search range
static void set_partition_range(const AV1_COMMON *const cm,
                                const MACROBLOCKD *const xd, int mi_row,
                                int mi_col, BLOCK_SIZE bsize,
                                BLOCK_SIZE *const min_bs,
                                BLOCK_SIZE *const max_bs) {
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];
  int idx, idy;

  const int idx_str = cm->mi_stride * mi_row + mi_col;
  MODE_INFO **const prev_mi = &cm->prev_mi_grid_visible[idx_str];
  BLOCK_SIZE min_size = cm->sb_size;  // default values
  BLOCK_SIZE max_size = BLOCK_4X4;

  if (prev_mi) {
    for (idy = 0; idy < mi_height; ++idy) {
      for (idx = 0; idx < mi_width; ++idx) {
        const MODE_INFO *const mi = prev_mi[idy * cm->mi_stride + idx];
        const BLOCK_SIZE bs = mi ? mi->mbmi.sb_type : bsize;
        min_size = AOMMIN(min_size, bs);
        max_size = AOMMAX(max_size, bs);
      }
    }
  }

  if (xd->left_available) {
    for (idy = 0; idy < mi_height; ++idy) {
      const MODE_INFO *const mi = xd->mi[idy * cm->mi_stride - 1];
      const BLOCK_SIZE bs = mi ? mi->mbmi.sb_type : bsize;
      min_size = AOMMIN(min_size, bs);
      max_size = AOMMAX(max_size, bs);
    }
  }

  if (xd->up_available) {
    for (idx = 0; idx < mi_width; ++idx) {
      const MODE_INFO *const mi = xd->mi[idx - cm->mi_stride];
      const BLOCK_SIZE bs = mi ? mi->mbmi.sb_type : bsize;
      min_size = AOMMIN(min_size, bs);
      max_size = AOMMAX(max_size, bs);
    }
  }

  if (min_size == max_size) {
    min_size = min_partition_size[min_size];
    max_size = max_partition_size[max_size];
  }

  *min_bs = AOMMIN(min_size, cm->sb_size);
  *max_bs = AOMMIN(max_size, cm->sb_size);
}

static INLINE void store_pred_mv(MACROBLOCK *x, PICK_MODE_CONTEXT *ctx) {
  memcpy(ctx->pred_mv, x->pred_mv, sizeof(x->pred_mv));
}

static INLINE void load_pred_mv(MACROBLOCK *x, PICK_MODE_CONTEXT *ctx) {
  memcpy(x->pred_mv, ctx->pred_mv, sizeof(x->pred_mv));
}

#if CONFIG_FP_MB_STATS
const int qindex_skip_threshold_lookup[BLOCK_SIZES] = {
  0,
  10,
  10,
  30,
  40,
  40,
  60,
  80,
  80,
  90,
  100,
  100,
  120,
#if CONFIG_EXT_PARTITION
  // TODO(debargha): What are the correct numbers here?
  130,
  130,
  150
#endif  // CONFIG_EXT_PARTITION
};
const int qindex_split_threshold_lookup[BLOCK_SIZES] = {
  0,
  3,
  3,
  7,
  15,
  15,
  30,
  40,
  40,
  60,
  80,
  80,
  120,
#if CONFIG_EXT_PARTITION
  // TODO(debargha): What are the correct numbers here?
  160,
  160,
  240
#endif  // CONFIG_EXT_PARTITION
};
const int complexity_16x16_blocks_threshold[BLOCK_SIZES] = {
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  4,
  4,
  6,
#if CONFIG_EXT_PARTITION
  // TODO(debargha): What are the correct numbers here?
  8,
  8,
  10
#endif  // CONFIG_EXT_PARTITION
};

typedef enum {
  MV_ZERO = 0,
  MV_LEFT = 1,
  MV_UP = 2,
  MV_RIGHT = 3,
  MV_DOWN = 4,
  MV_INVALID
} MOTION_DIRECTION;

static INLINE MOTION_DIRECTION get_motion_direction_fp(uint8_t fp_byte) {
  if (fp_byte & FPMB_MOTION_ZERO_MASK) {
    return MV_ZERO;
  } else if (fp_byte & FPMB_MOTION_LEFT_MASK) {
    return MV_LEFT;
  } else if (fp_byte & FPMB_MOTION_RIGHT_MASK) {
    return MV_RIGHT;
  } else if (fp_byte & FPMB_MOTION_UP_MASK) {
    return MV_UP;
  } else {
    return MV_DOWN;
  }
}

static INLINE int get_motion_inconsistency(MOTION_DIRECTION this_mv,
                                           MOTION_DIRECTION that_mv) {
  if (this_mv == that_mv) {
    return 0;
  } else {
    return abs(this_mv - that_mv) == 2 ? 2 : 1;
  }
}
#endif

#if CONFIG_EXT_PARTITION_TYPES
// Try searching for an encoding for the given subblock. Returns zero if the
// rdcost is already too high (to tell the caller not to bother searching for
// encodings of further subblocks)
static int rd_try_subblock(const AV1_COMP *const cpi, ThreadData *td,
                           TileDataEnc *tile_data, TOKENEXTRA **tp,
                           int is_first, int is_last, int mi_row, int mi_col,
                           BLOCK_SIZE subsize, RD_STATS *best_rdc,
                           RD_STATS *sum_rdc, RD_STATS *this_rdc,
                           PARTITION_TYPE partition,
                           PICK_MODE_CONTEXT *prev_ctx,
                           PICK_MODE_CONTEXT *this_ctx) {
#define RTS_X_RATE_NOCOEF_ARG
#define RTS_MAX_RDCOST best_rdc->rdcost

  MACROBLOCK *const x = &td->mb;

  if (cpi->sf.adaptive_motion_search) load_pred_mv(x, prev_ctx);

  // On the first time around, write the rd stats straight to sum_rdc. Also, we
  // should treat sum_rdc as containing zeros (even if it doesn't) to avoid
  // having to zero it at the start.
  if (is_first) this_rdc = sum_rdc;
  const int64_t spent_rdcost = is_first ? 0 : sum_rdc->rdcost;
  const int64_t rdcost_remaining = best_rdc->rdcost - spent_rdcost;

  rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, this_rdc,
                   RTS_X_RATE_NOCOEF_ARG partition, subsize, this_ctx,
                   rdcost_remaining);

  if (!is_first) {
    if (this_rdc->rate == INT_MAX) {
      sum_rdc->rdcost = INT64_MAX;
    } else {
      sum_rdc->rate += this_rdc->rate;
      sum_rdc->dist += this_rdc->dist;
      sum_rdc->rdcost += this_rdc->rdcost;
    }
  }

  if (sum_rdc->rdcost >= RTS_MAX_RDCOST) return 0;

  if (!is_last) {
    update_state(cpi, tile_data, td, this_ctx, mi_row, mi_col, subsize, 1);
    encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, mi_row, mi_col,
                      subsize, NULL);
  }

  return 1;

#undef RTS_X_RATE_NOCOEF_ARG
#undef RTS_MAX_RDCOST
}

static void rd_test_partition3(const AV1_COMP *const cpi, ThreadData *td,
                               TileDataEnc *tile_data, TOKENEXTRA **tp,
                               PC_TREE *pc_tree, RD_STATS *best_rdc,
                               PICK_MODE_CONTEXT ctxs[3],
                               PICK_MODE_CONTEXT *ctx, int mi_row, int mi_col,
                               BLOCK_SIZE bsize, PARTITION_TYPE partition,
                               int mi_row0, int mi_col0, BLOCK_SIZE subsize0,
                               int mi_row1, int mi_col1, BLOCK_SIZE subsize1,
                               int mi_row2, int mi_col2, BLOCK_SIZE subsize2) {
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  RD_STATS sum_rdc, this_rdc;
#if CONFIG_EXT_PARTITION_TYPES_AB
  const AV1_COMMON *const cm = &cpi->common;
#endif
#define RTP_STX_TRY_ARGS

  if (!rd_try_subblock(cpi, td, tile_data, tp, 1, 0, mi_row0, mi_col0, subsize0,
                       best_rdc, &sum_rdc, &this_rdc,
                       RTP_STX_TRY_ARGS partition, ctx, &ctxs[0]))
    return;

  if (!rd_try_subblock(cpi, td, tile_data, tp, 0, 0, mi_row1, mi_col1, subsize1,
                       best_rdc, &sum_rdc, &this_rdc,
                       RTP_STX_TRY_ARGS partition, &ctxs[0], &ctxs[1]))
    return;

// With the new layout of mixed partitions for PARTITION_HORZ_B and
// PARTITION_VERT_B, the last subblock might start past halfway through the
// main block, so we might signal it even though the subblock lies strictly
// outside the image. In that case, we won't spend any bits coding it and the
// difference (obviously) doesn't contribute to the error.
#if CONFIG_EXT_PARTITION_TYPES_AB
  const int try_block2 = mi_row2 < cm->mi_rows && mi_col2 < cm->mi_cols;
#else
  const int try_block2 = 1;
#endif
  if (try_block2 &&
      !rd_try_subblock(cpi, td, tile_data, tp, 0, 1, mi_row2, mi_col2, subsize2,
                       best_rdc, &sum_rdc, &this_rdc,
                       RTP_STX_TRY_ARGS partition, &ctxs[1], &ctxs[2]))
    return;

  if (sum_rdc.rdcost >= best_rdc->rdcost) return;

  int pl = partition_plane_context(xd, mi_row, mi_col, bsize);
  sum_rdc.rate += x->partition_cost[pl][partition];
  sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);

  if (sum_rdc.rdcost >= best_rdc->rdcost) return;

  *best_rdc = sum_rdc;
  pc_tree->partitioning = partition;

#undef RTP_STX_TRY_ARGS
}
#endif  // CONFIG_EXT_PARTITION_TYPES

#if CONFIG_DIST_8X8
static int64_t dist_8x8_yuv(const AV1_COMP *const cpi, MACROBLOCK *const x,
                            uint8_t *y_src_8x8) {
  MACROBLOCKD *const xd = &x->e_mbd;
  int64_t dist_8x8, dist_8x8_uv, total_dist;
  const int src_stride = x->plane[0].src.stride;
  uint8_t *decoded_8x8;
  int plane;

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    decoded_8x8 = CONVERT_TO_BYTEPTR(x->decoded_8x8);
  else
#endif
    decoded_8x8 = (uint8_t *)x->decoded_8x8;

  dist_8x8 = av1_dist_8x8(cpi, x, y_src_8x8, src_stride, decoded_8x8, 8,
                          BLOCK_8X8, 8, 8, 8, 8, x->qindex)
             << 4;

  // Compute chroma distortion for a luma 8x8 block
  dist_8x8_uv = 0;

  for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
    const int src_stride_uv = x->plane[plane].src.stride;
    const int dst_stride_uv = xd->plane[plane].dst.stride;
    // uv buff pointers now (i.e. the last sub8x8 block) is the same
    // to those at the first sub8x8 block because
    // uv buff pointer is set only once at first sub8x8 block in a 8x8.
    uint8_t *src_uv = x->plane[plane].src.buf;
    uint8_t *dst_uv = xd->plane[plane].dst.buf;
    unsigned sse;
    const BLOCK_SIZE plane_bsize =
        AOMMAX(BLOCK_4X4, get_plane_block_size(BLOCK_8X8, &xd->plane[plane]));
    cpi->fn_ptr[plane_bsize].vf(src_uv, src_stride_uv, dst_uv, dst_stride_uv,
                                &sse);
    dist_8x8_uv += (int64_t)sse << 4;
  }

  return total_dist = dist_8x8 + dist_8x8_uv;
}
#endif  // CONFIG_DIST_8X8

// TODO(jingning,jimbankoski,rbultje): properly skip partition types that are
// unlikely to be selected depending on previous rate-distortion optimization
// results, for encoding speed-up.
static void rd_pick_partition(const AV1_COMP *const cpi, ThreadData *td,
                              TileDataEnc *tile_data, TOKENEXTRA **tp,
                              int mi_row, int mi_col, BLOCK_SIZE bsize,
                              RD_STATS *rd_cost, int64_t best_rd,
                              PC_TREE *pc_tree, int64_t *none_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int mi_step = mi_size_wide[bsize] / 2;
  RD_SEARCH_MACROBLOCK_CONTEXT x_ctx;
  const TOKENEXTRA *const tp_orig = *tp;
  PICK_MODE_CONTEXT *ctx_none = &pc_tree->none;
  int tmp_partition_cost[PARTITION_TYPES];
  BLOCK_SIZE subsize;
  RD_STATS this_rdc, sum_rdc, best_rdc;
  const int bsize_at_least_8x8 = (bsize >= BLOCK_8X8);
  int do_square_split = bsize_at_least_8x8;
  const int pl = bsize_at_least_8x8
                     ? partition_plane_context(xd, mi_row, mi_col, bsize)
                     : 0;
  const int *partition_cost =
      pl >= 0 ? x->partition_cost[pl] : x->partition_cost[0];

  int do_rectangular_split = 1;
#if CONFIG_EXT_PARTITION_TYPES
  int64_t split_rd[4] = { 0, 0, 0, 0 };
  int64_t horz_rd[4] = { 0, 0 };
  int64_t vert_rd[4] = { 0, 0 };
#endif  // CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION_TYPES && !CONFIG_EXT_PARTITION_TYPES_AB
  BLOCK_SIZE bsize2 = get_subsize(bsize, PARTITION_SPLIT);
#endif

  // Override skipping rectangular partition operations for edge blocks
  const int has_rows = (mi_row + mi_step < cm->mi_rows);
  const int has_cols = (mi_col + mi_step < cm->mi_cols);
  const int xss = x->e_mbd.plane[1].subsampling_x;
  const int yss = x->e_mbd.plane[1].subsampling_y;

  BLOCK_SIZE min_size = x->min_partition_size;
  BLOCK_SIZE max_size = x->max_partition_size;

  if (none_rd) *none_rd = 0;

#if CONFIG_FP_MB_STATS
  unsigned int src_diff_var = UINT_MAX;
  int none_complexity = 0;
#endif

  int partition_none_allowed = has_rows && has_cols;
  int partition_horz_allowed = has_cols && yss <= xss && bsize_at_least_8x8;
  int partition_vert_allowed = has_rows && xss <= yss && bsize_at_least_8x8;

  (void)*tp_orig;

  if (!(has_rows && has_cols)) {
    tmp_partition_cost[PARTITION_NONE] = INT_MAX;

    if (has_cols) {
      tmp_partition_cost[PARTITION_VERT] = INT_MAX;
      tmp_partition_cost[PARTITION_HORZ] =
          av1_cost_bit(cm->fc->partition_prob[pl][PARTITION_HORZ], 0);
      tmp_partition_cost[PARTITION_SPLIT] =
          av1_cost_bit(cm->fc->partition_prob[pl][PARTITION_HORZ], 1);
    } else if (has_rows) {
      tmp_partition_cost[PARTITION_HORZ] = INT_MAX;
      tmp_partition_cost[PARTITION_VERT] =
          av1_cost_bit(cm->fc->partition_prob[pl][PARTITION_VERT], 0);
      tmp_partition_cost[PARTITION_SPLIT] =
          av1_cost_bit(cm->fc->partition_prob[pl][PARTITION_VERT], 1);
    } else {
      tmp_partition_cost[PARTITION_HORZ] = INT_MAX;
      tmp_partition_cost[PARTITION_VERT] = INT_MAX;
      tmp_partition_cost[PARTITION_SPLIT] = 0;
    }

    partition_cost = tmp_partition_cost;
  }

#ifndef NDEBUG
  // Nothing should rely on the default value of this array (which is just
  // leftover from encoding the previous block. Setting it to magic number
  // when debugging.
  memset(x->blk_skip[0], 234, sizeof(x->blk_skip[0]));
#endif  // NDEBUG

  assert(mi_size_wide[bsize] == mi_size_high[bsize]);

  av1_init_rd_stats(&this_rdc);
  av1_init_rd_stats(&sum_rdc);
  av1_invalid_rd_stats(&best_rdc);
  best_rdc.rdcost = best_rd;

  set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize);

  if (bsize == BLOCK_16X16 && cpi->vaq_refresh)
    x->mb_energy = av1_block_energy(cpi, x, bsize);

  if (cpi->sf.cb_partition_search && bsize == BLOCK_16X16) {
    const int cb_partition_search_ctrl =
        ((pc_tree->index == 0 || pc_tree->index == 3) +
         get_chessboard_index(cm->current_video_frame)) &
        0x1;

    if (cb_partition_search_ctrl && bsize > min_size && bsize < max_size)
      set_partition_range(cm, xd, mi_row, mi_col, bsize, &min_size, &max_size);
  }

  // Determine partition types in search according to the speed features.
  // The threshold set here has to be of square block size.
  if (cpi->sf.auto_min_max_partition_size) {
    const int no_partition_allowed = (bsize <= max_size && bsize >= min_size);
    // Note: Further partitioning is NOT allowed when bsize == min_size already.
    const int partition_allowed = (bsize <= max_size && bsize > min_size);
    partition_none_allowed &= no_partition_allowed;
    partition_horz_allowed &= partition_allowed || !has_rows;
    partition_vert_allowed &= partition_allowed || !has_cols;
    do_square_split &= bsize > min_size;
  }
  if (cpi->sf.use_square_partition_only) {
    partition_horz_allowed &= !has_rows;
    partition_vert_allowed &= !has_cols;
  }

  xd->above_txfm_context =
      cm->above_txfm_context + (mi_col << TX_UNIT_WIDE_LOG2);
  xd->left_txfm_context = xd->left_txfm_context_buffer +
                          ((mi_row & MAX_MIB_MASK) << TX_UNIT_HIGH_LOG2);
  save_context(x, &x_ctx, mi_row, mi_col, bsize);

#if CONFIG_FP_MB_STATS
  if (cpi->use_fp_mb_stats) {
    set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize);
    src_diff_var = get_sby_perpixel_diff_variance(cpi, &x->plane[0].src, mi_row,
                                                  mi_col, bsize);
  }

  // Decide whether we shall split directly and skip searching NONE by using
  // the first pass block statistics
  if (cpi->use_fp_mb_stats && bsize >= BLOCK_32X32 && do_square_split &&
      partition_none_allowed && src_diff_var > 4 &&
      cm->base_qindex < qindex_split_threshold_lookup[bsize]) {
    int mb_row = mi_row >> 1;
    int mb_col = mi_col >> 1;
    int mb_row_end =
        AOMMIN(mb_row + num_16x16_blocks_high_lookup[bsize], cm->mb_rows);
    int mb_col_end =
        AOMMIN(mb_col + num_16x16_blocks_wide_lookup[bsize], cm->mb_cols);
    int r, c;

    // compute a complexity measure, basically measure inconsistency of motion
    // vectors obtained from the first pass in the current block
    for (r = mb_row; r < mb_row_end; r++) {
      for (c = mb_col; c < mb_col_end; c++) {
        const int mb_index = r * cm->mb_cols + c;

        MOTION_DIRECTION this_mv;
        MOTION_DIRECTION right_mv;
        MOTION_DIRECTION bottom_mv;

        this_mv =
            get_motion_direction_fp(cpi->twopass.this_frame_mb_stats[mb_index]);

        // to its right
        if (c != mb_col_end - 1) {
          right_mv = get_motion_direction_fp(
              cpi->twopass.this_frame_mb_stats[mb_index + 1]);
          none_complexity += get_motion_inconsistency(this_mv, right_mv);
        }

        // to its bottom
        if (r != mb_row_end - 1) {
          bottom_mv = get_motion_direction_fp(
              cpi->twopass.this_frame_mb_stats[mb_index + cm->mb_cols]);
          none_complexity += get_motion_inconsistency(this_mv, bottom_mv);
        }

        // do not count its left and top neighbors to avoid double counting
      }
    }

    if (none_complexity > complexity_16x16_blocks_threshold[bsize]) {
      partition_none_allowed = 0;
    }
  }
#endif

  // PARTITION_NONE
  if (partition_none_allowed) {
    rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &this_rdc,
#if CONFIG_EXT_PARTITION_TYPES
                     PARTITION_NONE,
#endif
                     bsize, ctx_none, best_rdc.rdcost);
    if (none_rd) *none_rd = this_rdc.rdcost;
    if (this_rdc.rate != INT_MAX) {
      if (bsize_at_least_8x8) {
        const int pt_cost = partition_cost[PARTITION_NONE] < INT_MAX
                                ? partition_cost[PARTITION_NONE]
                                : 0;
        this_rdc.rate += pt_cost;
        this_rdc.rdcost = RDCOST(x->rdmult, this_rdc.rate, this_rdc.dist);
      }

      if (this_rdc.rdcost < best_rdc.rdcost) {
        // Adjust dist breakout threshold according to the partition size.
        const int64_t dist_breakout_thr =
            cpi->sf.partition_search_breakout_dist_thr >>
            ((2 * (MAX_SB_SIZE_LOG2 - 2)) -
             (b_width_log2_lookup[bsize] + b_height_log2_lookup[bsize]));
        const int rate_breakout_thr =
            cpi->sf.partition_search_breakout_rate_thr *
            num_pels_log2_lookup[bsize];

        best_rdc = this_rdc;
        if (bsize_at_least_8x8) pc_tree->partitioning = PARTITION_NONE;

        // If all y, u, v transform blocks in this partition are skippable, and
        // the dist & rate are within the thresholds, the partition search is
        // terminated for current branch of the partition search tree.
        // The dist & rate thresholds are set to 0 at speed 0 to disable the
        // early termination at that speed.
        if (!x->e_mbd.lossless[xd->mi[0]->mbmi.segment_id] &&
            (ctx_none->skippable && best_rdc.dist < dist_breakout_thr &&
             best_rdc.rate < rate_breakout_thr)) {
          do_square_split = 0;
          do_rectangular_split = 0;
        }

#if CONFIG_FP_MB_STATS
        // Check if every 16x16 first pass block statistics has zero
        // motion and the corresponding first pass residue is small enough.
        // If that is the case, check the difference variance between the
        // current frame and the last frame. If the variance is small enough,
        // stop further splitting in RD optimization
        if (cpi->use_fp_mb_stats && do_square_split &&
            cm->base_qindex > qindex_skip_threshold_lookup[bsize]) {
          int mb_row = mi_row >> 1;
          int mb_col = mi_col >> 1;
          int mb_row_end =
              AOMMIN(mb_row + num_16x16_blocks_high_lookup[bsize], cm->mb_rows);
          int mb_col_end =
              AOMMIN(mb_col + num_16x16_blocks_wide_lookup[bsize], cm->mb_cols);
          int r, c;

          int skip = 1;
          for (r = mb_row; r < mb_row_end; r++) {
            for (c = mb_col; c < mb_col_end; c++) {
              const int mb_index = r * cm->mb_cols + c;
              if (!(cpi->twopass.this_frame_mb_stats[mb_index] &
                    FPMB_MOTION_ZERO_MASK) ||
                  !(cpi->twopass.this_frame_mb_stats[mb_index] &
                    FPMB_ERROR_SMALL_MASK)) {
                skip = 0;
                break;
              }
            }
            if (skip == 0) {
              break;
            }
          }
          if (skip) {
            if (src_diff_var == UINT_MAX) {
              set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize);
              src_diff_var = get_sby_perpixel_diff_variance(
                  cpi, &x->plane[0].src, mi_row, mi_col, bsize);
            }
            if (src_diff_var < 8) {
              do_square_split = 0;
              do_rectangular_split = 0;
            }
          }
        }
#endif
      }
    }

    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
  }

  // store estimated motion vector
  if (cpi->sf.adaptive_motion_search) store_pred_mv(x, ctx_none);

  int64_t temp_best_rdcost = best_rdc.rdcost;

  // PARTITION_SPLIT
  // TODO(jingning): use the motion vectors given by the above search as
  // the starting point of motion search in the following partition type check.
  if (do_square_split) {
    int reached_last_index = 0;
    subsize = get_subsize(bsize, PARTITION_SPLIT);
    int idx;
    for (idx = 0; idx < 4 && sum_rdc.rdcost < temp_best_rdcost; ++idx) {
      const int x_idx = (idx & 1) * mi_step;
      const int y_idx = (idx >> 1) * mi_step;

      if (mi_row + y_idx >= cm->mi_rows || mi_col + x_idx >= cm->mi_cols)
        continue;

      if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_none);

      pc_tree->split[idx]->index = idx;
#if CONFIG_EXT_PARTITION_TYPES
      int64_t *p_split_rd = &split_rd[idx];
#else
      int64_t *p_split_rd = NULL;
#endif  // CONFIG_EXT_PARTITION_TYPES
      rd_pick_partition(cpi, td, tile_data, tp, mi_row + y_idx, mi_col + x_idx,
                        subsize, &this_rdc, temp_best_rdcost - sum_rdc.rdcost,
                        pc_tree->split[idx], p_split_rd);

      if (this_rdc.rate == INT_MAX) {
        sum_rdc.rdcost = INT64_MAX;
        break;
      } else {
        sum_rdc.rate += this_rdc.rate;
        sum_rdc.dist += this_rdc.dist;
        sum_rdc.rdcost += this_rdc.rdcost;
      }
    }
    reached_last_index = (idx == 4);

#if CONFIG_DIST_8X8
    if (x->using_dist_8x8 && reached_last_index &&
        sum_rdc.rdcost != INT64_MAX && bsize == BLOCK_8X8) {
      const int src_stride = x->plane[0].src.stride;
      int64_t dist_8x8;
      dist_8x8 = dist_8x8_yuv(cpi, x, x->plane[0].src.buf - 4 * src_stride - 4);
      if (x->tune_metric == AOM_TUNE_PSNR) assert(sum_rdc.dist == dist_8x8);
      sum_rdc.dist = dist_8x8;
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
    }
#endif  // CONFIG_DIST_8X8

    if (reached_last_index && sum_rdc.rdcost < best_rdc.rdcost) {
      sum_rdc.rate += partition_cost[PARTITION_SPLIT];
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);

      if (sum_rdc.rdcost < best_rdc.rdcost) {
        best_rdc = sum_rdc;
        temp_best_rdcost = best_rdc.rdcost;
        pc_tree->partitioning = PARTITION_SPLIT;
      }
    } else if (cpi->sf.less_rectangular_check) {
      // skip rectangular partition test when larger block size
      // gives better rd cost
      do_rectangular_split &= !partition_none_allowed;
    }

    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
  }  // if (do_split)

  // PARTITION_HORZ
  if (partition_horz_allowed &&
      (do_rectangular_split || av1_active_h_edge(cpi, mi_row, mi_step))) {
    subsize = get_subsize(bsize, PARTITION_HORZ);
    if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_none);
    if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
        partition_none_allowed)
      pc_tree->horizontal[0].pred_interp_filter =
          av1_extract_interp_filter(ctx_none->mic.mbmi.interp_filters, 0);

    rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &sum_rdc,
#if CONFIG_EXT_PARTITION_TYPES
                     PARTITION_HORZ,
#endif
                     subsize, &pc_tree->horizontal[0], best_rdc.rdcost);
#if CONFIG_EXT_PARTITION_TYPES
    horz_rd[0] = sum_rdc.rdcost;
#endif  // CONFIG_EXT_PARTITION_TYPES

    if (sum_rdc.rdcost < temp_best_rdcost && has_rows) {
      PICK_MODE_CONTEXT *ctx_h = &pc_tree->horizontal[0];
      update_state(cpi, tile_data, td, ctx_h, mi_row, mi_col, subsize, 1);
      encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, mi_row, mi_col,
                        subsize, NULL);

      if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_h);

      if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
          partition_none_allowed)
        pc_tree->horizontal[1].pred_interp_filter =
            av1_extract_interp_filter(ctx_h->mic.mbmi.interp_filters, 0);

      rd_pick_sb_modes(cpi, tile_data, x, mi_row + mi_step, mi_col, &this_rdc,
#if CONFIG_EXT_PARTITION_TYPES
                       PARTITION_HORZ,
#endif
                       subsize, &pc_tree->horizontal[1],
                       best_rdc.rdcost - sum_rdc.rdcost);
#if CONFIG_EXT_PARTITION_TYPES
      horz_rd[1] = this_rdc.rdcost;
#endif  // CONFIG_EXT_PARTITION_TYPES

#if CONFIG_DIST_8X8
      if (x->using_dist_8x8 && this_rdc.rate != INT_MAX && bsize == BLOCK_8X8) {
        update_state(cpi, tile_data, td, &pc_tree->horizontal[1],
                     mi_row + mi_step, mi_col, subsize, DRY_RUN_NORMAL);
        encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL,
                          mi_row + mi_step, mi_col, subsize, NULL);
      }
#endif  // CONFIG_DIST_8X8

      if (this_rdc.rate == INT_MAX) {
        sum_rdc.rdcost = INT64_MAX;
      } else {
        sum_rdc.rate += this_rdc.rate;
        sum_rdc.dist += this_rdc.dist;
        sum_rdc.rdcost += this_rdc.rdcost;
      }
#if CONFIG_DIST_8X8
      if (x->using_dist_8x8 && sum_rdc.rdcost != INT64_MAX &&
          bsize == BLOCK_8X8) {
        const int src_stride = x->plane[0].src.stride;
        int64_t dist_8x8;
        dist_8x8 = dist_8x8_yuv(cpi, x, x->plane[0].src.buf - 4 * src_stride);
        if (x->tune_metric == AOM_TUNE_PSNR) assert(sum_rdc.dist == dist_8x8);
        sum_rdc.dist = dist_8x8;
        sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
      }
#endif  // CONFIG_DIST_8X8
    }

    if (sum_rdc.rdcost < best_rdc.rdcost) {
      sum_rdc.rate += partition_cost[PARTITION_HORZ];
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
      if (sum_rdc.rdcost < best_rdc.rdcost) {
        best_rdc = sum_rdc;
        pc_tree->partitioning = PARTITION_HORZ;
      }
    }

    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
  }

  // PARTITION_VERT
  if (partition_vert_allowed &&
      (do_rectangular_split || av1_active_v_edge(cpi, mi_col, mi_step))) {
    subsize = get_subsize(bsize, PARTITION_VERT);

    if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_none);

    if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
        partition_none_allowed)
      pc_tree->vertical[0].pred_interp_filter =
          av1_extract_interp_filter(ctx_none->mic.mbmi.interp_filters, 0);

    rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &sum_rdc,
#if CONFIG_EXT_PARTITION_TYPES
                     PARTITION_VERT,
#endif
                     subsize, &pc_tree->vertical[0], best_rdc.rdcost);
#if CONFIG_EXT_PARTITION_TYPES
    vert_rd[0] = sum_rdc.rdcost;
#endif  // CONFIG_EXT_PARTITION_TYPES
    const int64_t vert_max_rdcost = best_rdc.rdcost;
    if (sum_rdc.rdcost < vert_max_rdcost && has_cols) {
      update_state(cpi, tile_data, td, &pc_tree->vertical[0], mi_row, mi_col,
                   subsize, 1);
      encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, mi_row, mi_col,
                        subsize, NULL);

      if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_none);

      if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
          partition_none_allowed)
        pc_tree->vertical[1].pred_interp_filter =
            av1_extract_interp_filter(ctx_none->mic.mbmi.interp_filters, 0);

      rd_pick_sb_modes(cpi, tile_data, x, mi_row, mi_col + mi_step, &this_rdc,
#if CONFIG_EXT_PARTITION_TYPES
                       PARTITION_VERT,
#endif
                       subsize, &pc_tree->vertical[1],
                       best_rdc.rdcost - sum_rdc.rdcost);
#if CONFIG_EXT_PARTITION_TYPES
      vert_rd[1] = this_rdc.rdcost;
#endif  // CONFIG_EXT_PARTITION_TYPES

#if CONFIG_DIST_8X8
      if (x->using_dist_8x8 && this_rdc.rate != INT_MAX && bsize == BLOCK_8X8) {
        update_state(cpi, tile_data, td, &pc_tree->vertical[1], mi_row,
                     mi_col + mi_step, subsize, DRY_RUN_NORMAL);
        encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, mi_row,
                          mi_col + mi_step, subsize, NULL);
      }
#endif  // CONFIG_DIST_8X8

      if (this_rdc.rate == INT_MAX) {
        sum_rdc.rdcost = INT64_MAX;
      } else {
        sum_rdc.rate += this_rdc.rate;
        sum_rdc.dist += this_rdc.dist;
        sum_rdc.rdcost += this_rdc.rdcost;
      }
#if CONFIG_DIST_8X8
      if (x->using_dist_8x8 && sum_rdc.rdcost != INT64_MAX &&
          bsize == BLOCK_8X8) {
        int64_t dist_8x8;
        dist_8x8 = dist_8x8_yuv(cpi, x, x->plane[0].src.buf - 4);
        if (x->tune_metric == AOM_TUNE_PSNR) assert(sum_rdc.dist == dist_8x8);
        sum_rdc.dist = dist_8x8;
        sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
      }
#endif  // CONFIG_DIST_8X8
    }

    if (sum_rdc.rdcost < best_rdc.rdcost) {
      sum_rdc.rate += partition_cost[PARTITION_VERT];
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
      if (sum_rdc.rdcost < best_rdc.rdcost) {
        best_rdc = sum_rdc;
        pc_tree->partitioning = PARTITION_VERT;
      }
    }

    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
  }

#if CONFIG_EXT_PARTITION_TYPES
  const int ext_partition_allowed =
      do_rectangular_split && bsize > BLOCK_8X8 && partition_none_allowed;

  // horz4_partition_allowed and vert4_partition_allowed encode the requirement
  // that we don't choose a block size that wouldn't be allowed by this
  // subsampling (stored in the xss and yss variables).
  //
  // We definitely can't allow (say) a 16x4 block if yss > xss because it would
  // subsample to 16x2, which doesn't have an enum. Also, there's no BLOCK_8X2
  // or BLOCK_2X8, so we can't do 4:1 or 1:4 partitions for BLOCK_16X16 if there
  // is any subsampling.
  int horz4_partition_allowed = ext_partition_allowed && partition_horz_allowed;
  int vert4_partition_allowed = ext_partition_allowed && partition_vert_allowed;

#if CONFIG_EXT_PARTITION_TYPES_AB
  // The alternative AB partitions are allowed iff the corresponding 4:1
  // partitions are allowed.
  int horzab_partition_allowed = horz4_partition_allowed;
  int vertab_partition_allowed = vert4_partition_allowed;
#else
  // The standard AB partitions are allowed whenever ext-partition-types are
  // allowed
  int horzab_partition_allowed = ext_partition_allowed;
  int vertab_partition_allowed = ext_partition_allowed;

  if (cpi->sf.prune_ext_partition_types_search) {
    horzab_partition_allowed &= (pc_tree->partitioning == PARTITION_HORZ ||
                                 pc_tree->partitioning == PARTITION_SPLIT);
    vertab_partition_allowed &= (pc_tree->partitioning == PARTITION_VERT ||
                                 pc_tree->partitioning == PARTITION_SPLIT);
    horz_rd[0] = (horz_rd[0] < INT64_MAX ? horz_rd[0] : 0);
    horz_rd[1] = (horz_rd[1] < INT64_MAX ? horz_rd[1] : 0);
    vert_rd[0] = (vert_rd[0] < INT64_MAX ? vert_rd[0] : 0);
    vert_rd[1] = (vert_rd[1] < INT64_MAX ? vert_rd[1] : 0);
    split_rd[0] = (split_rd[0] < INT64_MAX ? split_rd[0] : 0);
    split_rd[1] = (split_rd[1] < INT64_MAX ? split_rd[1] : 0);
    split_rd[2] = (split_rd[2] < INT64_MAX ? split_rd[2] : 0);
    split_rd[3] = (split_rd[3] < INT64_MAX ? split_rd[3] : 0);
  }
  int horza_partition_allowed = horzab_partition_allowed;
  int horzb_partition_allowed = horzab_partition_allowed;
  if (cpi->sf.prune_ext_partition_types_search) {
    const int64_t horz_a_rd = horz_rd[1] + split_rd[0] + split_rd[1];
    const int64_t horz_b_rd = horz_rd[0] + split_rd[2] + split_rd[3];
    horza_partition_allowed &= (horz_a_rd / 16 * 15 < best_rdc.rdcost);
    horzb_partition_allowed &= (horz_b_rd / 16 * 15 < best_rdc.rdcost);
  }
#endif  // CONFIG_EXT_PARTITION_TYPES_AB

// PARTITION_HORZ_A
#if CONFIG_EXT_PARTITION_TYPES_AB
  if (partition_horz_allowed && horzab_partition_allowed) {
    rd_test_partition3(
        cpi, td, tile_data, tp, pc_tree, &best_rdc, pc_tree->horizontala,
        ctx_none, mi_row, mi_col, bsize, PARTITION_HORZ_A, mi_row, mi_col,
        get_subsize(bsize, PARTITION_HORZ_4), mi_row + mi_step / 2, mi_col,
        get_subsize(bsize, PARTITION_HORZ_4), mi_row + mi_step, mi_col,
        get_subsize(bsize, PARTITION_HORZ));
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
  }
#else
  if (partition_horz_allowed && horza_partition_allowed) {
    subsize = get_subsize(bsize, PARTITION_HORZ_A);
    rd_test_partition3(cpi, td, tile_data, tp, pc_tree, &best_rdc,
                       pc_tree->horizontala, ctx_none, mi_row, mi_col, bsize,
                       PARTITION_HORZ_A, mi_row, mi_col, bsize2, mi_row,
                       mi_col + mi_step, bsize2, mi_row + mi_step, mi_col,
                       subsize);
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
  }
#endif
// PARTITION_HORZ_B
#if CONFIG_EXT_PARTITION_TYPES_AB
  if (partition_horz_allowed && horzab_partition_allowed) {
    rd_test_partition3(
        cpi, td, tile_data, tp, pc_tree, &best_rdc, pc_tree->horizontalb,
        ctx_none, mi_row, mi_col, bsize, PARTITION_HORZ_B, mi_row, mi_col,
        get_subsize(bsize, PARTITION_HORZ), mi_row + mi_step, mi_col,
        get_subsize(bsize, PARTITION_HORZ_4), mi_row + 3 * mi_step / 2, mi_col,
        get_subsize(bsize, PARTITION_HORZ_4));
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
  }
  (void)vert_rd;
  (void)horz_rd;
  (void)split_rd;
#else
  if (partition_horz_allowed && horzb_partition_allowed) {
    subsize = get_subsize(bsize, PARTITION_HORZ_B);
    rd_test_partition3(cpi, td, tile_data, tp, pc_tree, &best_rdc,
                       pc_tree->horizontalb, ctx_none, mi_row, mi_col, bsize,
                       PARTITION_HORZ_B, mi_row, mi_col, subsize,
                       mi_row + mi_step, mi_col, bsize2, mi_row + mi_step,
                       mi_col + mi_step, bsize2);
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
  }

  int verta_partition_allowed = vertab_partition_allowed;
  int vertb_partition_allowed = vertab_partition_allowed;
  if (cpi->sf.prune_ext_partition_types_search) {
    const int64_t vert_a_rd = vert_rd[1] + split_rd[0] + split_rd[2];
    const int64_t vert_b_rd = vert_rd[0] + split_rd[1] + split_rd[3];
    verta_partition_allowed &= (vert_a_rd / 16 * 15 < best_rdc.rdcost);
    vertb_partition_allowed &= (vert_b_rd / 16 * 15 < best_rdc.rdcost);
  }
#endif  // CONFIG_EXT_PARTITION_TYPES_AB

// PARTITION_VERT_A
#if CONFIG_EXT_PARTITION_TYPES_AB
  if (partition_vert_allowed && vertab_partition_allowed) {
    rd_test_partition3(
        cpi, td, tile_data, tp, pc_tree, &best_rdc, pc_tree->verticala,
        ctx_none, mi_row, mi_col, bsize, PARTITION_VERT_A, mi_row, mi_col,
        get_subsize(bsize, PARTITION_VERT_4), mi_row, mi_col + mi_step / 2,
        get_subsize(bsize, PARTITION_VERT_4), mi_row, mi_col + mi_step,
        get_subsize(bsize, PARTITION_VERT));
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
  }
#else
  if (partition_vert_allowed && verta_partition_allowed) {
    subsize = get_subsize(bsize, PARTITION_VERT_A);
    rd_test_partition3(cpi, td, tile_data, tp, pc_tree, &best_rdc,
                       pc_tree->verticala, ctx_none, mi_row, mi_col, bsize,
                       PARTITION_VERT_A, mi_row, mi_col, bsize2,
                       mi_row + mi_step, mi_col, bsize2, mi_row,
                       mi_col + mi_step, subsize);
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
  }
#endif
// PARTITION_VERT_B
#if CONFIG_EXT_PARTITION_TYPES_AB
  if (partition_vert_allowed && vertab_partition_allowed) {
    rd_test_partition3(
        cpi, td, tile_data, tp, pc_tree, &best_rdc, pc_tree->verticalb,
        ctx_none, mi_row, mi_col, bsize, PARTITION_VERT_B, mi_row, mi_col,
        get_subsize(bsize, PARTITION_VERT), mi_row, mi_col + mi_step,
        get_subsize(bsize, PARTITION_VERT_4), mi_row, mi_col + 3 * mi_step / 2,
        get_subsize(bsize, PARTITION_VERT_4));
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
  }
#else
  if (partition_vert_allowed && vertb_partition_allowed) {
    subsize = get_subsize(bsize, PARTITION_VERT_B);
    rd_test_partition3(cpi, td, tile_data, tp, pc_tree, &best_rdc,
                       pc_tree->verticalb, ctx_none, mi_row, mi_col, bsize,
                       PARTITION_VERT_B, mi_row, mi_col, subsize, mi_row,
                       mi_col + mi_step, bsize2, mi_row + mi_step,
                       mi_col + mi_step, bsize2);
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
  }
#endif

  // PARTITION_HORZ_4
  // TODO(david.barker): For this and PARTITION_VERT_4,
  // * Add support for BLOCK_16X16 once we support 2x8 and 8x2 blocks for the
  //   chroma plane
  if (cpi->sf.prune_ext_partition_types_search) {
    horz4_partition_allowed &= (pc_tree->partitioning == PARTITION_HORZ ||
                                pc_tree->partitioning == PARTITION_HORZ_A ||
                                pc_tree->partitioning == PARTITION_HORZ_B ||
                                pc_tree->partitioning == PARTITION_SPLIT ||
                                pc_tree->partitioning == PARTITION_NONE);
  }
  if (horz4_partition_allowed && has_rows &&
      (do_rectangular_split || av1_active_h_edge(cpi, mi_row, mi_step))) {
    const int quarter_step = mi_size_high[bsize] / 4;
    PICK_MODE_CONTEXT *ctx_prev = ctx_none;

    subsize = get_subsize(bsize, PARTITION_HORZ_4);

    for (int i = 0; i < 4; ++i) {
      int this_mi_row = mi_row + i * quarter_step;

      if (i > 0 && this_mi_row >= cm->mi_rows) break;

      PICK_MODE_CONTEXT *ctx_this = &pc_tree->horizontal4[i];

      if (!rd_try_subblock(cpi, td, tile_data, tp, (i == 0), (i == 3),
                           this_mi_row, mi_col, subsize, &best_rdc, &sum_rdc,
                           &this_rdc, PARTITION_HORZ_4, ctx_prev, ctx_this))
        break;

      ctx_prev = ctx_this;
    }

    if (sum_rdc.rdcost < best_rdc.rdcost) {
      sum_rdc.rate += partition_cost[PARTITION_HORZ_4];
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
      if (sum_rdc.rdcost < best_rdc.rdcost) {
        best_rdc = sum_rdc;
        pc_tree->partitioning = PARTITION_HORZ_4;
      }
    }
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
  }
  // PARTITION_VERT_4
  if (cpi->sf.prune_ext_partition_types_search) {
    vert4_partition_allowed &= (pc_tree->partitioning == PARTITION_VERT ||
                                pc_tree->partitioning == PARTITION_VERT_A ||
                                pc_tree->partitioning == PARTITION_VERT_B ||
                                pc_tree->partitioning == PARTITION_SPLIT ||
                                pc_tree->partitioning == PARTITION_NONE);
  }
  if (vert4_partition_allowed && has_cols &&
      (do_rectangular_split || av1_active_v_edge(cpi, mi_row, mi_step))) {
    const int quarter_step = mi_size_wide[bsize] / 4;
    PICK_MODE_CONTEXT *ctx_prev = ctx_none;

    subsize = get_subsize(bsize, PARTITION_VERT_4);

    for (int i = 0; i < 4; ++i) {
      int this_mi_col = mi_col + i * quarter_step;

      if (i > 0 && this_mi_col >= cm->mi_cols) break;

      PICK_MODE_CONTEXT *ctx_this = &pc_tree->vertical4[i];

      if (!rd_try_subblock(cpi, td, tile_data, tp, (i == 0), (i == 3), mi_row,
                           this_mi_col, subsize, &best_rdc, &sum_rdc, &this_rdc,
                           PARTITION_VERT_4, ctx_prev, ctx_this))
        break;

      ctx_prev = ctx_this;
    }

    if (sum_rdc.rdcost < best_rdc.rdcost) {
      sum_rdc.rate += partition_cost[PARTITION_VERT_4];
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
      if (sum_rdc.rdcost < best_rdc.rdcost) {
        best_rdc = sum_rdc;
        pc_tree->partitioning = PARTITION_VERT_4;
      }
    }
    restore_context(x, &x_ctx, mi_row, mi_col, bsize);
  }
#endif  // CONFIG_EXT_PARTITION_TYPES

  // TODO(jbb): This code added so that we avoid static analysis
  // warning related to the fact that best_rd isn't used after this
  // point.  This code should be refactored so that the duplicate
  // checks occur in some sub function and thus are used...
  (void)best_rd;
  *rd_cost = best_rdc;

  if (best_rdc.rate < INT_MAX && best_rdc.dist < INT64_MAX &&
      pc_tree->index != 3) {
    if (bsize == cm->sb_size) {
#if NC_MODE_INFO
      set_mode_info_sb(cpi, td, tile_data, tp, mi_row, mi_col, bsize, pc_tree);
#endif

#if CONFIG_LV_MAP
      x->cb_offset = 0;
#endif

      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, OUTPUT_ENABLED, bsize,
                pc_tree, NULL);
    } else {
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_NORMAL, bsize,
                pc_tree, NULL);
    }
  }

#if CONFIG_DIST_8X8
  if (x->using_dist_8x8 && best_rdc.rate < INT_MAX &&
      best_rdc.dist < INT64_MAX && bsize == BLOCK_4X4 && pc_tree->index == 3) {
    encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_NORMAL, bsize,
              pc_tree, NULL);
  }
#endif  // CONFIG_DIST_8X8

  if (bsize == cm->sb_size) {
#if !CONFIG_LV_MAP
    assert(tp_orig < *tp || (tp_orig == *tp && xd->mi[0]->mbmi.skip));
#endif
    assert(best_rdc.rate < INT_MAX);
    assert(best_rdc.dist < INT64_MAX);
  } else {
    assert(tp_orig == *tp);
  }
}

static void encode_rd_sb_row(AV1_COMP *cpi, ThreadData *td,
                             TileDataEnc *tile_data, int mi_row,
                             TOKENEXTRA **tp) {
  AV1_COMMON *const cm = &cpi->common;
  const TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  SPEED_FEATURES *const sf = &cpi->sf;
  int mi_col;
#if CONFIG_EXT_PARTITION
  const int leaf_nodes = 256;
#else
  const int leaf_nodes = 64;
#endif  // CONFIG_EXT_PARTITION

  // Initialize the left context for the new SB row
  av1_zero_left_context(xd);

  // Reset delta for every tile
  if (cm->delta_q_present_flag)
    if (mi_row == tile_info->mi_row_start) xd->prev_qindex = cm->base_qindex;
#if CONFIG_EXT_DELTA_Q
  if (cm->delta_lf_present_flag) {
#if CONFIG_LOOPFILTER_LEVEL
    if (mi_row == tile_info->mi_row_start)
      for (int lf_id = 0; lf_id < FRAME_LF_COUNT; ++lf_id)
        xd->prev_delta_lf[lf_id] = 0;
#endif  // CONFIG_LOOPFILTER_LEVEL
    if (mi_row == tile_info->mi_row_start) xd->prev_delta_lf_from_base = 0;
  }
#endif

  // Code each SB in the row
  for (mi_col = tile_info->mi_col_start; mi_col < tile_info->mi_col_end;
       mi_col += cm->mib_size) {
    const struct segmentation *const seg = &cm->seg;
    int dummy_rate;
    int64_t dummy_dist;
    RD_STATS dummy_rdc;
    int i;
    int seg_skip = 0;

    const int idx_str = cm->mi_stride * mi_row + mi_col;
    MODE_INFO **mi = cm->mi_grid_visible + idx_str;
    PC_TREE *const pc_root = td->pc_root[cm->mib_size_log2 - MIN_MIB_SIZE_LOG2];

#if CONFIG_LV_MAP
    av1_fill_coeff_costs(&td->mb, xd->tile_ctx);
#else
    av1_fill_token_costs_from_cdf(x->token_head_costs,
                                  x->e_mbd.tile_ctx->coef_head_cdfs);
    av1_fill_token_costs_from_cdf(x->token_tail_costs,
                                  x->e_mbd.tile_ctx->coef_tail_cdfs);
#endif
    av1_fill_mode_rates(cm, x, xd->tile_ctx);

    if (sf->adaptive_pred_interp_filter) {
      for (i = 0; i < leaf_nodes; ++i) {
        td->pc_tree[i].vertical[0].pred_interp_filter = SWITCHABLE;
        td->pc_tree[i].vertical[1].pred_interp_filter = SWITCHABLE;
        td->pc_tree[i].horizontal[0].pred_interp_filter = SWITCHABLE;
        td->pc_tree[i].horizontal[1].pred_interp_filter = SWITCHABLE;
      }
    }

    x->tx_rd_record.num = x->tx_rd_record.index_start = 0;
    av1_zero(x->pred_mv);
    pc_root->index = 0;

    if (seg->enabled) {
      const uint8_t *const map =
          seg->update_map ? cpi->segmentation_map : cm->last_frame_seg_map;
      int segment_id = get_segment_id(cm, map, cm->sb_size, mi_row, mi_col);
      seg_skip = segfeature_active(seg, segment_id, SEG_LVL_SKIP);
    }
#if CONFIG_AMVR
    xd->cur_frame_force_integer_mv = cm->cur_frame_force_integer_mv;
#endif

    if (cm->delta_q_present_flag) {
      // Test mode for delta quantization
      int sb_row = mi_row >> 3;
      int sb_col = mi_col >> 3;
      int sb_stride = (cm->width + MAX_SB_SIZE - 1) >> MAX_SB_SIZE_LOG2;
      int index = ((sb_row * sb_stride + sb_col + 8) & 31) - 16;

      // Ensure divisibility of delta_qindex by delta_q_res
      int offset_qindex = (index < 0 ? -index - 8 : index - 8);
      int qmask = ~(cm->delta_q_res - 1);
      int current_qindex = clamp(cm->base_qindex + offset_qindex,
                                 cm->delta_q_res, 256 - cm->delta_q_res);

      current_qindex =
          ((current_qindex - cm->base_qindex + cm->delta_q_res / 2) & qmask) +
          cm->base_qindex;
      assert(current_qindex > 0);

      xd->delta_qindex = current_qindex - cm->base_qindex;
      set_offsets(cpi, tile_info, x, mi_row, mi_col, cm->sb_size);
      xd->mi[0]->mbmi.current_q_index = current_qindex;
#if !CONFIG_EXT_DELTA_Q
      xd->mi[0]->mbmi.segment_id = 0;
#if CONFIG_Q_SEGMENTATION
      xd->mi[0]->mbmi.q_segment_id = 0;
#endif
#endif  // CONFIG_EXT_DELTA_Q
#if CONFIG_Q_SEGMENTATION
      av1_init_plane_quantizers(cpi, x, xd->mi[0]->mbmi.segment_id,
                                xd->mi[0]->mbmi.q_segment_id);
#else
      av1_init_plane_quantizers(cpi, x, xd->mi[0]->mbmi.segment_id);
#endif
#if CONFIG_EXT_DELTA_Q
      if (cpi->oxcf.deltaq_mode == DELTA_Q_LF) {
        int j, k;
        int lfmask = ~(cm->delta_lf_res - 1);
        int current_delta_lf_from_base = offset_qindex / 2;
        current_delta_lf_from_base =
            ((current_delta_lf_from_base + cm->delta_lf_res / 2) & lfmask);

        // pre-set the delta lf for loop filter. Note that this value is set
        // before mi is assigned for each block in current superblock
        for (j = 0; j < AOMMIN(cm->mib_size, cm->mi_rows - mi_row); j++) {
          for (k = 0; k < AOMMIN(cm->mib_size, cm->mi_cols - mi_col); k++) {
            cm->mi[(mi_row + j) * cm->mi_stride + (mi_col + k)]
                .mbmi.current_delta_lf_from_base = clamp(
                current_delta_lf_from_base, -MAX_LOOP_FILTER, MAX_LOOP_FILTER);
#if CONFIG_LOOPFILTER_LEVEL
            for (int lf_id = 0; lf_id < FRAME_LF_COUNT; ++lf_id) {
              cm->mi[(mi_row + j) * cm->mi_stride + (mi_col + k)]
                  .mbmi.curr_delta_lf[lf_id] =
                  clamp(current_delta_lf_from_base, -MAX_LOOP_FILTER,
                        MAX_LOOP_FILTER);
            }
#endif  // CONFIG_LOOPFILTER_LEVEL
          }
        }
      }
#endif  // CONFIG_EXT_DELTA_Q
    }

    x->source_variance = UINT_MAX;
    if (sf->partition_search_type == FIXED_PARTITION || seg_skip) {
      BLOCK_SIZE bsize;
      set_offsets(cpi, tile_info, x, mi_row, mi_col, cm->sb_size);
      bsize = seg_skip ? cm->sb_size : sf->always_this_block_size;
      set_fixed_partitioning(cpi, tile_info, mi, mi_row, mi_col, bsize);
      rd_use_partition(cpi, td, tile_data, mi, tp, mi_row, mi_col, cm->sb_size,
                       &dummy_rate, &dummy_dist, 1, pc_root);
    } else if (cpi->partition_search_skippable_frame) {
      BLOCK_SIZE bsize;
      set_offsets(cpi, tile_info, x, mi_row, mi_col, cm->sb_size);
      bsize = get_rd_var_based_fixed_partition(cpi, x, mi_row, mi_col);
      set_fixed_partitioning(cpi, tile_info, mi, mi_row, mi_col, bsize);
      rd_use_partition(cpi, td, tile_data, mi, tp, mi_row, mi_col, cm->sb_size,
                       &dummy_rate, &dummy_dist, 1, pc_root);
    } else {
      // If required set upper and lower partition size limits
      if (sf->auto_min_max_partition_size) {
        set_offsets(cpi, tile_info, x, mi_row, mi_col, cm->sb_size);
        rd_auto_partition_range(cpi, tile_info, xd, mi_row, mi_col,
                                &x->min_partition_size, &x->max_partition_size);
      }
      rd_pick_partition(cpi, td, tile_data, tp, mi_row, mi_col, cm->sb_size,
                        &dummy_rdc, INT64_MAX, pc_root, NULL);
    }
#if CONFIG_LPF_SB
    if (USE_LOOP_FILTER_SUPERBLOCK) {
      // apply deblocking filtering right after each superblock is encoded.
      const int guess_filter_lvl = FAKE_FILTER_LEVEL;
      av1_loop_filter_frame(get_frame_new_buffer(cm), cm, xd, guess_filter_lvl,
                            0, 1, mi_row, mi_col);
    }
#endif  // CONFIG_LPF_SB
  }
}

static void init_encode_frame_mb_context(AV1_COMP *cpi) {
  MACROBLOCK *const x = &cpi->td.mb;
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;

  // Copy data over into macro block data structures.
  av1_setup_src_planes(x, cpi->source, 0, 0);

  av1_setup_block_planes(xd, cm->subsampling_x, cm->subsampling_y);
}

#if !CONFIG_REF_ADAPT && !CONFIG_JNT_COMP
static int check_dual_ref_flags(AV1_COMP *cpi) {
  const int ref_flags = cpi->ref_frame_flags;

  if (segfeature_active(&cpi->common.seg, 1, SEG_LVL_REF_FRAME)) {
    return 0;
  } else {
    return (!!(ref_flags & AOM_GOLD_FLAG) + !!(ref_flags & AOM_LAST_FLAG) +
            !!(ref_flags & AOM_LAST2_FLAG) + !!(ref_flags & AOM_LAST3_FLAG) +
            !!(ref_flags & AOM_BWD_FLAG) + !!(ref_flags & AOM_ALT2_FLAG) +
            !!(ref_flags & AOM_ALT_FLAG)) >= 2;
  }
}
#endif  // !CONFIG_REF_ADAPT

static MV_REFERENCE_FRAME get_frame_type(const AV1_COMP *cpi) {
  if (frame_is_intra_only(&cpi->common)) return INTRA_FRAME;
  // We will not update the golden frame with an internal overlay frame
  else if ((cpi->rc.is_src_frame_alt_ref && cpi->refresh_golden_frame) ||
           cpi->rc.is_src_frame_ext_arf)
    return ALTREF_FRAME;
  else if (cpi->refresh_golden_frame || cpi->refresh_alt2_ref_frame ||
           cpi->refresh_alt_ref_frame)
    return GOLDEN_FRAME;
  else
    // TODO(zoeliu): To investigate whether a frame_type other than
    // INTRA/ALTREF/GOLDEN/LAST needs to be specified seperately.
    return LAST_FRAME;
}

#if CONFIG_SIMPLIFY_TX_MODE
static TX_MODE select_tx_mode(const AV1_COMP *cpi) {
  if (cpi->common.all_lossless) return ONLY_4X4;
#if CONFIG_VAR_TX_NO_TX_MODE
  return TX_MODE_SELECT;
#else
  if (cpi->sf.tx_size_search_method == USE_LARGESTALL)
    return TX_MODE_LARGEST;
  else if (cpi->sf.tx_size_search_method == USE_FULL_RD ||
           cpi->sf.tx_size_search_method == USE_FAST_RD)
    return TX_MODE_SELECT;
  else
    return cpi->common.tx_mode;
#endif  // CONFIG_VAR_TX_NO_TX_MODE
}
#else
static TX_MODE select_tx_mode(const AV1_COMP *cpi) {
  if (cpi->common.all_lossless) return ONLY_4X4;
#if CONFIG_VAR_TX_NO_TX_MODE
  return TX_MODE_SELECT;
#else
  if (cpi->sf.tx_size_search_method == USE_LARGESTALL)
    return ALLOW_32X32 + CONFIG_TX64X64;
  else if (cpi->sf.tx_size_search_method == USE_FULL_RD ||
           cpi->sf.tx_size_search_method == USE_FAST_RD)
    return TX_MODE_SELECT;
  else
    return cpi->common.tx_mode;
#endif  // CONFIG_VAR_TX_NO_TX_MODE
}
#endif  // CONFIG_SIMPLIFY_TX_MODE

void av1_init_tile_data(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;
  int tile_col, tile_row;
  TOKENEXTRA *pre_tok = cpi->tile_tok[0][0];
  unsigned int tile_tok = 0;

  if (cpi->tile_data == NULL || cpi->allocated_tiles < tile_cols * tile_rows) {
    if (cpi->tile_data != NULL) aom_free(cpi->tile_data);
    CHECK_MEM_ERROR(
        cm, cpi->tile_data,
        aom_memalign(32, tile_cols * tile_rows * sizeof(*cpi->tile_data)));
    cpi->allocated_tiles = tile_cols * tile_rows;

    for (tile_row = 0; tile_row < tile_rows; ++tile_row)
      for (tile_col = 0; tile_col < tile_cols; ++tile_col) {
        TileDataEnc *const tile_data =
            &cpi->tile_data[tile_row * tile_cols + tile_col];
        int i, j;
        for (i = 0; i < BLOCK_SIZES_ALL; ++i) {
          for (j = 0; j < MAX_MODES; ++j) {
            tile_data->thresh_freq_fact[i][j] = 32;
            tile_data->mode_map[i][j] = j;
          }
        }
      }
  }

  for (tile_row = 0; tile_row < tile_rows; ++tile_row) {
    for (tile_col = 0; tile_col < tile_cols; ++tile_col) {
      TileDataEnc *const tile_data =
          &cpi->tile_data[tile_row * tile_cols + tile_col];
      TileInfo *const tile_info = &tile_data->tile_info;
      av1_tile_init(tile_info, cm, tile_row, tile_col);

      cpi->tile_tok[tile_row][tile_col] = pre_tok + tile_tok;
      pre_tok = cpi->tile_tok[tile_row][tile_col];
      tile_tok = allocated_tokens(*tile_info, cm->mib_size_log2 + MI_SIZE_LOG2,
                                  av1_num_planes(cm));

#if CONFIG_EXT_TILE
      tile_data->allow_update_cdf = !cm->large_scale_tile;
#else
      tile_data->allow_update_cdf = 1;
#endif  // CONFIG_EXT_TILE
    }
  }
}

void av1_encode_tile(AV1_COMP *cpi, ThreadData *td, int tile_row,
                     int tile_col) {
  AV1_COMMON *const cm = &cpi->common;
  TileDataEnc *const this_tile =
      &cpi->tile_data[tile_row * cm->tile_cols + tile_col];
  const TileInfo *const tile_info = &this_tile->tile_info;
  TOKENEXTRA *tok = cpi->tile_tok[tile_row][tile_col];
  int mi_row;

#if CONFIG_DEPENDENT_HORZTILES
  if ((!cm->dependent_horz_tiles) || (tile_row == 0) ||
      tile_info->tg_horz_boundary) {
    av1_zero_above_context(cm, tile_info->mi_col_start, tile_info->mi_col_end);
  }
#else
  av1_zero_above_context(cm, tile_info->mi_col_start, tile_info->mi_col_end);
#endif

  // Set up pointers to per thread motion search counters.
  this_tile->m_search_count = 0;   // Count of motion search hits.
  this_tile->ex_search_count = 0;  // Exhaustive mesh search hits.
  td->mb.m_search_count_ptr = &this_tile->m_search_count;
  td->mb.ex_search_count_ptr = &this_tile->ex_search_count;
  this_tile->tctx = *cm->fc;
  td->mb.e_mbd.tile_ctx = &this_tile->tctx;

#if CONFIG_CFL
  MACROBLOCKD *const xd = &td->mb.e_mbd;
  xd->cfl = &this_tile->cfl;
  cfl_init(xd->cfl, cm);
#endif

#if CONFIG_LOOPFILTERING_ACROSS_TILES
  if (!cm->loop_filter_across_tiles_enabled)
    av1_setup_across_tile_boundary_info(cm, tile_info);
#endif

  av1_crc_calculator_init(&td->mb.tx_rd_record.crc_calculator, 24, 0x5D6DCB);

#if CONFIG_INTRABC
  td->intrabc_used_this_tile = 0;
#endif  // CONFIG_INTRABC

  for (mi_row = tile_info->mi_row_start; mi_row < tile_info->mi_row_end;
       mi_row += cm->mib_size) {
    encode_rd_sb_row(cpi, td, this_tile, mi_row, &tok);
  }

#if CONFIG_INTRABC
  cpi->intrabc_used |= td->intrabc_used_this_tile;
#endif  // CONFIG_INTRABC

  cpi->tok_count[tile_row][tile_col] =
      (unsigned int)(tok - cpi->tile_tok[tile_row][tile_col]);
  assert(cpi->tok_count[tile_row][tile_col] <=
         allocated_tokens(*tile_info, cm->mib_size_log2 + MI_SIZE_LOG2,
                          av1_num_planes(cm)));
}

static void encode_tiles(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  int tile_col, tile_row;

  av1_init_tile_data(cpi);

  for (tile_row = 0; tile_row < cm->tile_rows; ++tile_row)
    for (tile_col = 0; tile_col < cm->tile_cols; ++tile_col)
      av1_encode_tile(cpi, &cpi->td, tile_row, tile_col);
}

#if CONFIG_FP_MB_STATS
static int input_fpmb_stats(FIRSTPASS_MB_STATS *firstpass_mb_stats,
                            AV1_COMMON *cm, uint8_t **this_frame_mb_stats) {
  uint8_t *mb_stats_in = firstpass_mb_stats->mb_stats_start +
                         cm->current_video_frame * cm->MBs * sizeof(uint8_t);

  if (mb_stats_in > firstpass_mb_stats->mb_stats_end) return EOF;

  *this_frame_mb_stats = mb_stats_in;

  return 1;
}
#endif

#define GLOBAL_TRANS_TYPES_ENC 3  // highest motion model to search
static int gm_get_params_cost(const WarpedMotionParams *gm,
                              const WarpedMotionParams *ref_gm, int allow_hp) {
  assert(gm->wmtype < GLOBAL_TRANS_TYPES);
  int params_cost = 0;
  int trans_bits, trans_prec_diff;
  switch (gm->wmtype) {
    case AFFINE:
    case ROTZOOM:
      params_cost += aom_count_signed_primitive_refsubexpfin(
          GM_ALPHA_MAX + 1, SUBEXPFIN_K,
          (ref_gm->wmmat[2] >> GM_ALPHA_PREC_DIFF) - (1 << GM_ALPHA_PREC_BITS),
          (gm->wmmat[2] >> GM_ALPHA_PREC_DIFF) - (1 << GM_ALPHA_PREC_BITS));
      params_cost += aom_count_signed_primitive_refsubexpfin(
          GM_ALPHA_MAX + 1, SUBEXPFIN_K,
          (ref_gm->wmmat[3] >> GM_ALPHA_PREC_DIFF),
          (gm->wmmat[3] >> GM_ALPHA_PREC_DIFF));
      if (gm->wmtype >= AFFINE) {
        params_cost += aom_count_signed_primitive_refsubexpfin(
            GM_ALPHA_MAX + 1, SUBEXPFIN_K,
            (ref_gm->wmmat[4] >> GM_ALPHA_PREC_DIFF),
            (gm->wmmat[4] >> GM_ALPHA_PREC_DIFF));
        params_cost += aom_count_signed_primitive_refsubexpfin(
            GM_ALPHA_MAX + 1, SUBEXPFIN_K,
            (ref_gm->wmmat[5] >> GM_ALPHA_PREC_DIFF) -
                (1 << GM_ALPHA_PREC_BITS),
            (gm->wmmat[5] >> GM_ALPHA_PREC_DIFF) - (1 << GM_ALPHA_PREC_BITS));
      }
      AOM_FALLTHROUGH_INTENDED;
    case TRANSLATION:
      trans_bits = (gm->wmtype == TRANSLATION)
                       ? GM_ABS_TRANS_ONLY_BITS - !allow_hp
                       : GM_ABS_TRANS_BITS;
      trans_prec_diff = (gm->wmtype == TRANSLATION)
                            ? GM_TRANS_ONLY_PREC_DIFF + !allow_hp
                            : GM_TRANS_PREC_DIFF;
      params_cost += aom_count_signed_primitive_refsubexpfin(
          (1 << trans_bits) + 1, SUBEXPFIN_K,
          (ref_gm->wmmat[0] >> trans_prec_diff),
          (gm->wmmat[0] >> trans_prec_diff));
      params_cost += aom_count_signed_primitive_refsubexpfin(
          (1 << trans_bits) + 1, SUBEXPFIN_K,
          (ref_gm->wmmat[1] >> trans_prec_diff),
          (gm->wmmat[1] >> trans_prec_diff));
      AOM_FALLTHROUGH_INTENDED;
    case IDENTITY: break;
    default: assert(0);
  }
  return (params_cost << AV1_PROB_COST_SHIFT);
}

static int do_gm_search_logic(SPEED_FEATURES *const sf, int num_refs_using_gm,
                              int frame) {
  (void)num_refs_using_gm;
  (void)frame;
  switch (sf->gm_search_type) {
    case GM_FULL_SEARCH: return 1;
    case GM_REDUCED_REF_SEARCH:
      return !(frame == LAST2_FRAME || frame == LAST3_FRAME);
    case GM_DISABLE_SEARCH: return 0;
    default: assert(0);
  }
  return 1;
}

// Estimate if the source frame is screen content, based on the portion of
// blocks that have no more than 4 (experimentally selected) luma colors.
static int is_screen_content(const uint8_t *src,
#if CONFIG_HIGHBITDEPTH
                             int use_hbd, int bd,
#endif  // CONFIG_HIGHBITDEPTH
                             int stride, int width, int height) {
  assert(src != NULL);
  int counts = 0;
  const int blk_w = 16;
  const int blk_h = 16;
  const int limit = 4;
  for (int r = 0; r + blk_h <= height; r += blk_h) {
    for (int c = 0; c + blk_w <= width; c += blk_w) {
      const int n_colors =
#if CONFIG_HIGHBITDEPTH
          use_hbd ? av1_count_colors_highbd(src + r * stride + c, stride, blk_w,
                                            blk_h, bd)
                  :
#endif  // CONFIG_HIGHBITDEPTH
                  av1_count_colors(src + r * stride + c, stride, blk_w, blk_h);
      if (n_colors > 1 && n_colors <= limit) counts++;
    }
  }
  // The threshold is 10%.
  return counts * blk_h * blk_w * 10 > width * height;
}

#if CONFIG_FRAME_MARKER
static int refs_are_one_sided(const AV1_COMMON *cm) {
  int one_sided_refs = 1;
  if (cm->cur_frame->lst_frame_offset > cm->frame_offset ||
      cm->cur_frame->lst2_frame_offset > cm->frame_offset ||
      cm->cur_frame->lst3_frame_offset > cm->frame_offset ||
      cm->cur_frame->gld_frame_offset > cm->frame_offset ||
      cm->cur_frame->bwd_frame_offset > cm->frame_offset ||
      cm->cur_frame->alt2_frame_offset > cm->frame_offset ||
      cm->cur_frame->alt_frame_offset > cm->frame_offset)
    one_sided_refs = 0;
  return one_sided_refs;
}

// Enforce the number of references for each arbitrary frame limited to
// (INTER_REFS_PER_FRAME - 1)
static void enforce_max_ref_frames(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  static const int flag_list[TOTAL_REFS_PER_FRAME] = { 0,
                                                       AOM_LAST_FLAG,
                                                       AOM_LAST2_FLAG,
                                                       AOM_LAST3_FLAG,
                                                       AOM_GOLD_FLAG,
                                                       AOM_BWD_FLAG,
                                                       AOM_ALT2_FLAG,
                                                       AOM_ALT_FLAG };
  MV_REFERENCE_FRAME ref_frame;
  int total_valid_refs = 0;

  (void)flag_list;

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    if (cpi->ref_frame_flags & flag_list[ref_frame]) total_valid_refs++;
  }

  // NOTE(zoeliu): When all the possible reference frames are availble, we
  // reduce the number of reference frames by 1, following the rules of:
  // (1) Retain GOLDEN_FARME/ALTEF_FRAME;
  // (2) Check the earliest 2 remaining reference frames, and remove the one
  //     with the lower quality factor, otherwise if both have been coded at
  //     the same quality level, remove the earliest reference frame.

  if (total_valid_refs == INTER_REFS_PER_FRAME) {
    unsigned int min_ref_offset = UINT_MAX;
    unsigned int second_min_ref_offset = UINT_MAX;
    MV_REFERENCE_FRAME earliest_ref_frames[2] = { LAST3_FRAME, LAST2_FRAME };
    int earliest_buf_idxes[2] = { 0 };

    // Locate the earliest two reference frames except GOLDEN/ALTREF.
    for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
      // Retain GOLDEN/ALTERF
      if (ref_frame == GOLDEN_FRAME || ref_frame == ALTREF_FRAME) continue;

      const int buf_idx = cm->frame_refs[ref_frame - LAST_FRAME].idx;
      if (buf_idx >= 0) {
        const unsigned int ref_offset =
            cm->buffer_pool->frame_bufs[buf_idx].cur_frame_offset;

        if (min_ref_offset == UINT_MAX) {
          min_ref_offset = ref_offset;
          earliest_ref_frames[0] = ref_frame;
          earliest_buf_idxes[0] = buf_idx;
        } else {
          if (ref_offset < min_ref_offset) {
            second_min_ref_offset = min_ref_offset;
            earliest_ref_frames[1] = earliest_ref_frames[0];
            earliest_buf_idxes[1] = earliest_buf_idxes[0];

            min_ref_offset = ref_offset;
            earliest_ref_frames[0] = ref_frame;
            earliest_buf_idxes[0] = buf_idx;
          } else if (ref_offset < second_min_ref_offset) {
            second_min_ref_offset = ref_offset;
            earliest_ref_frames[1] = ref_frame;
            earliest_buf_idxes[1] = buf_idx;
          }
        }
      }
    }
    // Check the coding quality factors of the two earliest reference frames.
    RATE_FACTOR_LEVEL ref_rf_level[2];
    double ref_rf_deltas[2];
    for (int i = 0; i < 2; ++i) {
      ref_rf_level[i] = cpi->frame_rf_level[earliest_buf_idxes[i]];
      ref_rf_deltas[i] = rate_factor_deltas[ref_rf_level[i]];
    }
    (void)ref_rf_level;
    (void)ref_rf_deltas;

#define USE_RF_LEVEL_TO_ENFORCE 1
#if USE_RF_LEVEL_TO_ENFORCE
    // If both earliest two reference frames are coded using the same rate-
    // factor, disable the earliest reference frame; Otherwise disable the
    // reference frame that uses a lower rate-factor delta.
    const MV_REFERENCE_FRAME ref_frame_to_disable =
        (ref_rf_deltas[0] <= ref_rf_deltas[1]) ? earliest_ref_frames[0]
                                               : earliest_ref_frames[1];
#else
    // Always disable the earliest reference frame
    const MV_REFERENCE_FRAME ref_frame_to_disable = earliest_ref_frames[0];
#endif  // USE_RF_LEVEL_TO_ENFORCE
#undef USE_RF_LEVEL_TO_ENFORCE

#if 0
    printf("===Enforce: Frame_offset=%d, earliest refs: %d-%d(%d,%4.2f) and "
           "%d-%d(%d,%4.2f), ref_frame_to_disable=%d\n",
           cm->frame_offset, earliest_ref_frames[0], min_ref_offset,
           ref_rf_level[0], ref_rf_deltas[0], earliest_ref_frames[1],
           second_min_ref_offset, ref_rf_level[1], ref_rf_deltas[1],
           ref_frame_to_disable);
#endif  // 0

    switch (ref_frame_to_disable) {
      case LAST_FRAME: cpi->ref_frame_flags &= ~AOM_LAST_FLAG; break;
      case LAST2_FRAME: cpi->ref_frame_flags &= ~AOM_LAST2_FLAG; break;
      case LAST3_FRAME: cpi->ref_frame_flags &= ~AOM_LAST3_FLAG; break;
      case BWDREF_FRAME: cpi->ref_frame_flags &= ~AOM_BWD_FLAG; break;
      case ALTREF2_FRAME: cpi->ref_frame_flags &= ~AOM_ALT2_FLAG; break;
      default: break;
    }
  }
}
#endif  // CONFIG_FRAME_MARKER

static void encode_frame_internal(AV1_COMP *cpi) {
  ThreadData *const td = &cpi->td;
  MACROBLOCK *const x = &td->mb;
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  RD_COUNTS *const rdc = &cpi->td.rd_counts;
  int i;
  const int last_fb_buf_idx = get_ref_frame_buf_idx(cpi, LAST_FRAME);

#if CONFIG_ADAPT_SCAN
  av1_deliver_eob_threshold(cm, xd);
#endif

  x->min_partition_size = AOMMIN(x->min_partition_size, cm->sb_size);
  x->max_partition_size = AOMMIN(x->max_partition_size, cm->sb_size);
#if CONFIG_DIST_8X8
  x->using_dist_8x8 = cpi->oxcf.using_dist_8x8;
  x->tune_metric = cpi->oxcf.tuning;
#endif
  cm->setup_mi(cm);

  xd->mi = cm->mi_grid_visible;
  xd->mi[0] = cm->mi;

  av1_zero(*td->counts);
  av1_zero(rdc->comp_pred_diff);

  if (frame_is_intra_only(cm)) {
    cm->allow_screen_content_tools =
        cpi->oxcf.content == AOM_CONTENT_SCREEN ||
        is_screen_content(cpi->source->y_buffer,
#if CONFIG_HIGHBITDEPTH
                          cpi->source->flags & YV12_FLAG_HIGHBITDEPTH, xd->bd,
#endif  // CONFIG_HIGHBITDEPTH
                          cpi->source->y_stride, cpi->source->y_width,
                          cpi->source->y_height);
  }

#if CONFIG_INTRABC
  // Allow intrabc when screen content tools are enabled.
  cm->allow_intrabc = cm->allow_screen_content_tools;
  // Reset the flag.
  cpi->intrabc_used = 0;
#endif  // CONFIG_INTRABC

#if CONFIG_HASH_ME
  if (cpi->oxcf.pass != 1 && cpi->common.allow_screen_content_tools) {
    // add to hash table
    const int pic_width = cpi->source->y_crop_width;
    const int pic_height = cpi->source->y_crop_height;
    uint32_t *block_hash_values[2][2];
    int8_t *is_block_same[2][3];
    int k, j;

    for (k = 0; k < 2; k++) {
      for (j = 0; j < 2; j++) {
        CHECK_MEM_ERROR(cm, block_hash_values[k][j],
                        aom_malloc(sizeof(uint32_t) * pic_width * pic_height));
      }

      for (j = 0; j < 3; j++) {
        CHECK_MEM_ERROR(cm, is_block_same[k][j],
                        aom_malloc(sizeof(int8_t) * pic_width * pic_height));
      }
    }

    av1_hash_table_create(&cm->cur_frame->hash_table);
    av1_generate_block_2x2_hash_value(cpi->source, block_hash_values[0],
                                      is_block_same[0]);
    av1_generate_block_hash_value(cpi->source, 4, block_hash_values[0],
                                  block_hash_values[1], is_block_same[0],
                                  is_block_same[1]);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[1], is_block_same[1][2],
        pic_width, pic_height, 4);
    av1_generate_block_hash_value(cpi->source, 8, block_hash_values[1],
                                  block_hash_values[0], is_block_same[1],
                                  is_block_same[0]);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[0], is_block_same[0][2],
        pic_width, pic_height, 8);
    av1_generate_block_hash_value(cpi->source, 16, block_hash_values[0],
                                  block_hash_values[1], is_block_same[0],
                                  is_block_same[1]);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[1], is_block_same[1][2],
        pic_width, pic_height, 16);
    av1_generate_block_hash_value(cpi->source, 32, block_hash_values[1],
                                  block_hash_values[0], is_block_same[1],
                                  is_block_same[0]);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[0], is_block_same[0][2],
        pic_width, pic_height, 32);
    av1_generate_block_hash_value(cpi->source, 64, block_hash_values[0],
                                  block_hash_values[1], is_block_same[0],
                                  is_block_same[1]);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[1], is_block_same[1][2],
        pic_width, pic_height, 64);

    av1_generate_block_hash_value(cpi->source, 128, block_hash_values[1],
                                  block_hash_values[0], is_block_same[1],
                                  is_block_same[0]);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[0], is_block_same[0][2],
        pic_width, pic_height, 128);

    for (k = 0; k < 2; k++) {
      for (j = 0; j < 2; j++) {
        aom_free(block_hash_values[k][j]);
      }

      for (j = 0; j < 3; j++) {
        aom_free(is_block_same[k][j]);
      }
    }
  }
#endif

  av1_zero(rdc->global_motion_used);
  av1_zero(cpi->gmparams_cost);
  if (cpi->common.frame_type == INTER_FRAME && cpi->source &&
      !cpi->global_motion_search_done) {
    YV12_BUFFER_CONFIG *ref_buf[TOTAL_REFS_PER_FRAME];
    int frame;
    double params_by_motion[RANSAC_NUM_MOTIONS * (MAX_PARAMDIM - 1)];
    const double *params_this_motion;
    int inliers_by_motion[RANSAC_NUM_MOTIONS];
    WarpedMotionParams tmp_wm_params;
    static const double kIdentityParams[MAX_PARAMDIM - 1] = {
      0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0
    };
    int num_refs_using_gm = 0;

    for (frame = LAST_FRAME; frame <= ALTREF_FRAME; ++frame) {
      ref_buf[frame] = get_ref_frame_buffer(cpi, frame);
      int pframe;
      cm->global_motion[frame] = default_warp_params;
      const WarpedMotionParams *ref_params =
          cm->error_resilient_mode ? &default_warp_params
                                   : &cm->prev_frame->global_motion[frame];
      // check for duplicate buffer
      for (pframe = LAST_FRAME; pframe < frame; ++pframe) {
        if (ref_buf[frame] == ref_buf[pframe]) break;
      }
      if (pframe < frame) {
        memcpy(&cm->global_motion[frame], &cm->global_motion[pframe],
               sizeof(WarpedMotionParams));
      } else if (ref_buf[frame] &&
                 ref_buf[frame]->y_crop_width == cpi->source->y_crop_width &&
                 ref_buf[frame]->y_crop_height == cpi->source->y_crop_height &&
                 do_gm_search_logic(&cpi->sf, num_refs_using_gm, frame)) {
        TransformationType model;
        const int64_t ref_frame_error = av1_frame_error(
#if CONFIG_HIGHBITDEPTH
            xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH, xd->bd,
#endif  // CONFIG_HIGHBITDEPTH
            ref_buf[frame]->y_buffer, ref_buf[frame]->y_stride,
            cpi->source->y_buffer, cpi->source->y_width, cpi->source->y_height,
            cpi->source->y_stride);

        if (ref_frame_error == 0) continue;

        aom_clear_system_state();
        for (model = ROTZOOM; model < GLOBAL_TRANS_TYPES_ENC; ++model) {
          int64_t best_warp_error = INT64_MAX;
          // Initially set all params to identity.
          for (i = 0; i < RANSAC_NUM_MOTIONS; ++i) {
            memcpy(params_by_motion + (MAX_PARAMDIM - 1) * i, kIdentityParams,
                   (MAX_PARAMDIM - 1) * sizeof(*params_by_motion));
          }

          compute_global_motion_feature_based(
              model, cpi->source, ref_buf[frame],
#if CONFIG_HIGHBITDEPTH
              cpi->common.bit_depth,
#endif  // CONFIG_HIGHBITDEPTH
              inliers_by_motion, params_by_motion, RANSAC_NUM_MOTIONS);

          for (i = 0; i < RANSAC_NUM_MOTIONS; ++i) {
            if (inliers_by_motion[i] == 0) continue;

            params_this_motion = params_by_motion + (MAX_PARAMDIM - 1) * i;
            convert_model_to_params(params_this_motion, &tmp_wm_params);

            if (tmp_wm_params.wmtype != IDENTITY) {
              const int64_t warp_error = refine_integerized_param(
                  &tmp_wm_params, tmp_wm_params.wmtype,
#if CONFIG_HIGHBITDEPTH
                  xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH, xd->bd,
#endif  // CONFIG_HIGHBITDEPTH
                  ref_buf[frame]->y_buffer, ref_buf[frame]->y_width,
                  ref_buf[frame]->y_height, ref_buf[frame]->y_stride,
                  cpi->source->y_buffer, cpi->source->y_width,
                  cpi->source->y_height, cpi->source->y_stride, 5,
                  best_warp_error);
              if (warp_error < best_warp_error) {
                best_warp_error = warp_error;
                // Save the wm_params modified by refine_integerized_param()
                // rather than motion index to avoid rerunning refine() below.
                memcpy(&(cm->global_motion[frame]), &tmp_wm_params,
                       sizeof(WarpedMotionParams));
              }
            }
          }
          if (cm->global_motion[frame].wmtype <= AFFINE)
            if (!get_shear_params(&cm->global_motion[frame]))
              cm->global_motion[frame] = default_warp_params;

          if (cm->global_motion[frame].wmtype == TRANSLATION) {
            cm->global_motion[frame].wmmat[0] =
                convert_to_trans_prec(cm->allow_high_precision_mv,
                                      cm->global_motion[frame].wmmat[0]) *
                GM_TRANS_ONLY_DECODE_FACTOR;
            cm->global_motion[frame].wmmat[1] =
                convert_to_trans_prec(cm->allow_high_precision_mv,
                                      cm->global_motion[frame].wmmat[1]) *
                GM_TRANS_ONLY_DECODE_FACTOR;
          }

          // If the best error advantage found doesn't meet the threshold for
          // this motion type, revert to IDENTITY.
          if (!is_enough_erroradvantage(
                  (double)best_warp_error / ref_frame_error,
                  gm_get_params_cost(&cm->global_motion[frame], ref_params,
                                     cm->allow_high_precision_mv))) {
            cm->global_motion[frame] = default_warp_params;
          }
          if (cm->global_motion[frame].wmtype != IDENTITY) break;
        }
        aom_clear_system_state();
      }
      if (cm->global_motion[frame].wmtype != IDENTITY) num_refs_using_gm++;
      cpi->gmparams_cost[frame] =
          gm_get_params_cost(&cm->global_motion[frame], ref_params,
                             cm->allow_high_precision_mv) +
          cpi->gmtype_cost[cm->global_motion[frame].wmtype] -
          cpi->gmtype_cost[IDENTITY];
    }
    cpi->global_motion_search_done = 1;
  }
  memcpy(cm->cur_frame->global_motion, cm->global_motion,
         TOTAL_REFS_PER_FRAME * sizeof(WarpedMotionParams));

  for (i = 0; i < MAX_SEGMENTS; ++i) {
    const int qindex = cm->seg.enabled
#if CONFIG_Q_SEGMENTATION
                           ? av1_get_qindex(&cm->seg, i, i, cm->base_qindex)
#else
                           ? av1_get_qindex(&cm->seg, i, cm->base_qindex)
#endif
                           : cm->base_qindex;
    xd->lossless[i] = qindex == 0 && cm->y_dc_delta_q == 0 &&
                      cm->u_dc_delta_q == 0 && cm->u_ac_delta_q == 0 &&
                      cm->v_dc_delta_q == 0 && cm->v_ac_delta_q == 0;
    xd->qindex[i] = qindex;
  }
  cm->all_lossless = all_lossless(cm, xd);
  if (!cm->seg.enabled && xd->lossless[0]) x->optimize = 0;

  cm->tx_mode = select_tx_mode(cpi);

  // Fix delta q resolution for the moment
  cm->delta_q_res = DEFAULT_DELTA_Q_RES;
// Set delta_q_present_flag before it is used for the first time
#if CONFIG_EXT_DELTA_Q
  cm->delta_lf_res = DEFAULT_DELTA_LF_RES;
  // update delta_q_present_flag and delta_lf_present_flag based on base_qindex
  cm->delta_q_present_flag &= cm->base_qindex > 0;
  cm->delta_lf_present_flag &= cm->base_qindex > 0;
#else
  cm->delta_q_present_flag =
      cpi->oxcf.aq_mode == DELTA_AQ && cm->base_qindex > 0;
#endif  // CONFIG_EXT_DELTA_Q

  av1_frame_init_quantizer(cpi);

  av1_initialize_rd_consts(cpi);
  av1_initialize_me_consts(cpi, x, cm->base_qindex);
  init_encode_frame_mb_context(cpi);

  // NOTE(zoeliu): As cm->prev_frame can take neither a frame of
  //               show_exisiting_frame=1, nor can it take a frame not used as
  //               a reference, it is probable that by the time it is being
  //               referred to, the frame buffer it originally points to may
  //               already get expired and have been reassigned to the current
  //               newly coded frame. Hence, we need to check whether this is
  //               the case, and if yes, we have 2 choices:
  //               (1) Simply disable the use of previous frame mvs; or
  //               (2) Have cm->prev_frame point to one reference frame buffer,
  //                   e.g. LAST_FRAME.
  if (!enc_is_ref_frame_buf(cpi, cm->prev_frame)) {
    // Reassign the LAST_FRAME buffer to cm->prev_frame.
    cm->prev_frame = last_fb_buf_idx != INVALID_IDX
                         ? &cm->buffer_pool->frame_bufs[last_fb_buf_idx]
                         : NULL;
  }

#if CONFIG_TEMPMV_SIGNALING
  cm->use_prev_frame_mvs &= frame_can_use_prev_frame_mvs(cm);
#else
  if (cm->prev_frame) {
    cm->use_prev_frame_mvs = !cm->error_resilient_mode &&
#if CONFIG_FRAME_SUPERRES
                             cm->width == cm->last_width &&
                             cm->height == cm->last_height &&
#else
                             cm->width == cm->prev_frame->buf.y_crop_width &&
                             cm->height == cm->prev_frame->buf.y_crop_height &&
#endif  // CONFIG_FRAME_SUPERRES
                             !cm->intra_only && cm->last_show_frame;
  } else {
    cm->use_prev_frame_mvs = 0;
  }
#endif  // CONFIG_TEMPMV_SIGNALING

  // Special case: set prev_mi to NULL when the previous mode info
  // context cannot be used.
  cm->prev_mi =
      cm->use_prev_frame_mvs ? cm->prev_mip + cm->mi_stride + 1 : NULL;

  x->txb_split_count = 0;
  av1_zero(x->blk_skip_drl);

#if CONFIG_MFMV
  av1_setup_motion_field(cm);
#endif  // CONFIG_MFMV

#if CONFIG_FRAME_MARKER
  cpi->all_one_sided_refs = refs_are_one_sided(cm);
#endif  // CONFIG_FRAME_MARKER

  {
    struct aom_usec_timer emr_timer;
    aom_usec_timer_start(&emr_timer);

#if CONFIG_FP_MB_STATS
    if (cpi->use_fp_mb_stats) {
      input_fpmb_stats(&cpi->twopass.firstpass_mb_stats, cm,
                       &cpi->twopass.this_frame_mb_stats);
    }
#endif

    av1_setup_frame_boundary_info(cm);

    // If allowed, encoding tiles in parallel with one thread handling one tile.
    // TODO(geza.lore): The multi-threaded encoder is not safe with more than
    // 1 tile rows, as it uses the single above_context et al arrays from
    // cpi->common
    if (AOMMIN(cpi->oxcf.max_threads, cm->tile_cols) > 1 && cm->tile_rows == 1)
      av1_encode_tiles_mt(cpi);
    else
      encode_tiles(cpi);

    aom_usec_timer_mark(&emr_timer);
    cpi->time_encode_sb_row += aom_usec_timer_elapsed(&emr_timer);
  }

#if CONFIG_INTRABC
  // If intrabc is allowed but never selected, reset the allow_intrabc flag.
  if (cm->allow_intrabc && !cpi->intrabc_used) cm->allow_intrabc = 0;
#endif  // CONFIG_INTRABC

#if 0
  // Keep record of the total distortion this time around for future use
  cpi->last_frame_distortion = cpi->frame_distortion;
#endif
}

static void make_consistent_compound_tools(AV1_COMMON *cm) {
  (void)cm;
  if (frame_is_intra_only(cm) || cm->reference_mode == COMPOUND_REFERENCE)
    cm->allow_interintra_compound = 0;
#if CONFIG_COMPOUND_SINGLEREF
  if (frame_is_intra_only(cm))
#else   // !CONFIG_COMPOUND_SINGLEREF
  if (frame_is_intra_only(cm) || cm->reference_mode == SINGLE_REFERENCE)
#endif  // CONFIG_COMPOUND_SINGLEREF
    cm->allow_masked_compound = 0;
}

void av1_encode_frame(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  // Indicates whether or not to use a default reduced set for ext-tx
  // rather than the potential full set of 16 transforms
  cm->reduced_tx_set_used = 0;
#if CONFIG_ADAPT_SCAN
#if CONFIG_EXT_TILE
  if (cm->large_scale_tile)
    cm->use_adapt_scan = 0;
  else
#endif  // CONFIG_EXT_TILE
    cm->use_adapt_scan = 1;
  // TODO(angiebird): call av1_init_scan_order only when use_adapt_scan
  // switches from 1 to 0
  if (cm->use_adapt_scan == 0) av1_init_scan_order(cm);
#endif

#if CONFIG_FRAME_MARKER
  if (cm->show_frame == 0) {
    int arf_offset = AOMMIN(
        (MAX_GF_INTERVAL - 1),
        cpi->twopass.gf_group.arf_src_offset[cpi->twopass.gf_group.index]);
    int brf_offset =
        cpi->twopass.gf_group.brf_src_offset[cpi->twopass.gf_group.index];
    arf_offset = AOMMIN((MAX_GF_INTERVAL - 1), arf_offset + brf_offset);
    cm->frame_offset = cm->current_video_frame + arf_offset;
  } else {
    cm->frame_offset = cm->current_video_frame;
  }
  av1_setup_frame_buf_refs(cm);
  if (cpi->sf.selective_ref_frame >= 2) enforce_max_ref_frames(cpi);
#if CONFIG_FRAME_SIGN_BIAS
  av1_setup_frame_sign_bias(cm);
#endif  // CONFIG_FRAME_SIGN_BIAS
#endif  // CONFIG_FRAME_MARKER

  // In the longer term the encoder should be generalized to match the
  // decoder such that we allow compound where one of the 3 buffers has a
  // different sign bias and that buffer is then the fixed ref. However, this
  // requires further work in the rd loop. For now the only supported encoder
  // side behavior is where the ALT ref buffer has opposite sign bias to
  // the other two.
  if (!frame_is_intra_only(cm)) {
    cpi->allow_comp_inter_inter = 1;
    cm->comp_fwd_ref[0] = LAST_FRAME;
    cm->comp_fwd_ref[1] = LAST2_FRAME;
    cm->comp_fwd_ref[2] = LAST3_FRAME;
    cm->comp_fwd_ref[3] = GOLDEN_FRAME;
    cm->comp_bwd_ref[0] = BWDREF_FRAME;
    cm->comp_bwd_ref[1] = ALTREF2_FRAME;
    cm->comp_bwd_ref[2] = ALTREF_FRAME;
  } else {
    cpi->allow_comp_inter_inter = 0;
  }

  if (cpi->sf.frame_parameter_update) {
    int i;
    RD_OPT *const rd_opt = &cpi->rd;
    FRAME_COUNTS *counts = cpi->td.counts;
    RD_COUNTS *const rdc = &cpi->td.rd_counts;

    // This code does a single RD pass over the whole frame assuming
    // either compound, single or hybrid prediction as per whatever has
    // worked best for that type of frame in the past.
    // It also predicts whether another coding mode would have worked
    // better than this coding mode. If that is the case, it remembers
    // that for subsequent frames.
    // It does the same analysis for transform size selection also.
    //
    // TODO(zoeliu): To investigate whether a frame_type other than
    // INTRA/ALTREF/GOLDEN/LAST needs to be specified seperately.
    const MV_REFERENCE_FRAME frame_type = get_frame_type(cpi);
    int64_t *const mode_thrs = rd_opt->prediction_type_threshes[frame_type];
    const int is_alt_ref = frame_type == ALTREF_FRAME;

/* prediction (compound, single or hybrid) mode selection */
#if CONFIG_REF_ADAPT
    // NOTE(zoeliu): "is_alt_ref" is true only for OVERLAY/INTNL_OVERLAY frames
    if (is_alt_ref || !cpi->allow_comp_inter_inter)
      cm->reference_mode = SINGLE_REFERENCE;
    else
      cm->reference_mode = REFERENCE_MODE_SELECT;
#else
#if CONFIG_BGSPRITE
    (void)is_alt_ref;
    if (!cpi->allow_comp_inter_inter)
#else
    if (is_alt_ref || !cpi->allow_comp_inter_inter)
#endif  // CONFIG_BGSPRITE
      cm->reference_mode = SINGLE_REFERENCE;
#if !CONFIG_JNT_COMP
    else if (mode_thrs[COMPOUND_REFERENCE] > mode_thrs[SINGLE_REFERENCE] &&
             mode_thrs[COMPOUND_REFERENCE] > mode_thrs[REFERENCE_MODE_SELECT] &&
             check_dual_ref_flags(cpi) && cpi->static_mb_pct == 100)
      cm->reference_mode = COMPOUND_REFERENCE;
    else if (mode_thrs[SINGLE_REFERENCE] > mode_thrs[REFERENCE_MODE_SELECT])
      cm->reference_mode = SINGLE_REFERENCE;
#endif  // CONFIG_JNT_COMP
    else
      cm->reference_mode = REFERENCE_MODE_SELECT;
#endif  // CONFIG_REF_ADAPT

#if CONFIG_DUAL_FILTER
    cm->interp_filter = SWITCHABLE;
#endif

    make_consistent_compound_tools(cm);

    rdc->single_ref_used_flag = 0;
    rdc->compound_ref_used_flag = 0;

    encode_frame_internal(cpi);

    for (i = 0; i < REFERENCE_MODES; ++i)
      mode_thrs[i] = (mode_thrs[i] + rdc->comp_pred_diff[i] / cm->MBs) / 2;

    if (cm->reference_mode == REFERENCE_MODE_SELECT) {
      // Use a flag that includes 4x4 blocks
      if (rdc->compound_ref_used_flag == 0) {
        cm->reference_mode = SINGLE_REFERENCE;
        av1_zero(counts->comp_inter);
#if !CONFIG_REF_ADAPT
        // Use a flag that includes 4x4 blocks
      } else if (rdc->single_ref_used_flag == 0) {
        cm->reference_mode = COMPOUND_REFERENCE;
        av1_zero(counts->comp_inter);
#endif  // !CONFIG_REF_ADAPT
      }
    }
    make_consistent_compound_tools(cm);

    if (cm->tx_mode == TX_MODE_SELECT && cpi->td.mb.txb_split_count == 0)
#if CONFIG_SIMPLIFY_TX_MODE
      cm->tx_mode = TX_MODE_LARGEST;
#else
      cm->tx_mode = ALLOW_32X32 + CONFIG_TX64X64;
#endif  // CONFIG_SIMPLIFY_TX_MODE
  } else {
    make_consistent_compound_tools(cm);
    encode_frame_internal(cpi);
  }
}

static void sum_intra_stats(FRAME_COUNTS *counts, MACROBLOCKD *xd,
                            const MODE_INFO *mi, const MODE_INFO *above_mi,
                            const MODE_INFO *left_mi, const int intraonly,
                            const int mi_row, const int mi_col,
                            uint8_t allow_update_cdf) {
  FRAME_CONTEXT *fc = xd->tile_ctx;
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const PREDICTION_MODE y_mode = mbmi->mode;
  const UV_PREDICTION_MODE uv_mode = mbmi->uv_mode;
  (void)counts;
  const BLOCK_SIZE bsize = mbmi->sb_type;

  if (intraonly) {
#if CONFIG_ENTROPY_STATS
    const PREDICTION_MODE above = av1_above_block_mode(mi, above_mi, 0);
    const PREDICTION_MODE left = av1_left_block_mode(mi, left_mi, 0);
#if CONFIG_KF_CTX
    int above_ctx = intra_mode_context[above];
    int left_ctx = intra_mode_context[left];
    ++counts->kf_y_mode[above_ctx][left_ctx][y_mode];
#else
    ++counts->kf_y_mode[above][left][y_mode];
#endif
#endif  // CONFIG_ENTROPY_STATS
    if (allow_update_cdf)
      update_cdf(get_y_mode_cdf(fc, mi, above_mi, left_mi, 0), y_mode,
                 INTRA_MODES);
  } else {
#if CONFIG_ENTROPY_STATS
    ++counts->y_mode[size_group_lookup[bsize]][y_mode];
#endif  // CONFIG_ENTROPY_STATS
    if (allow_update_cdf)
      update_cdf(fc->y_mode_cdf[size_group_lookup[bsize]], y_mode, INTRA_MODES);
  }

#if CONFIG_FILTER_INTRA
  if (mbmi->mode == DC_PRED && mbmi->palette_mode_info.palette_size[0] == 0 &&
      av1_filter_intra_allowed_txsize(mbmi->tx_size)) {
    const int use_filter_intra_mode =
        mbmi->filter_intra_mode_info.use_filter_intra_mode[0];
#if CONFIG_ENTROPY_STATS
    ++counts->filter_intra_mode[0][mbmi->filter_intra_mode_info
                                       .filter_intra_mode[0]];
    ++counts->filter_intra_tx[mbmi->tx_size][use_filter_intra_mode];
#endif  // CONFIG_ENTROPY_STATS
    if (allow_update_cdf) {
      update_cdf(fc->filter_intra_mode_cdf[0],
                 mbmi->filter_intra_mode_info.filter_intra_mode[0],
                 FILTER_INTRA_MODES);
      update_cdf(fc->filter_intra_cdfs[mbmi->tx_size], use_filter_intra_mode,
                 2);
    }
  }
#endif  // CONFIG_FILTER_INTRA
#if CONFIG_EXT_INTRA && CONFIG_EXT_INTRA_MOD
  if (av1_is_directional_mode(mbmi->mode, bsize) &&
      av1_use_angle_delta(bsize)) {
#if CONFIG_ENTROPY_STATS
    ++counts->angle_delta[mbmi->mode - V_PRED]
                         [mbmi->angle_delta[0] + MAX_ANGLE_DELTA];
#endif
    if (allow_update_cdf)
      update_cdf(fc->angle_delta_cdf[mbmi->mode - V_PRED],
                 mbmi->angle_delta[0] + MAX_ANGLE_DELTA,
                 2 * MAX_ANGLE_DELTA + 1);
  }
#endif  // CONFIG_EXT_INTRA && CONFIG_EXT_INTRA_MOD

  if (!is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                           xd->plane[1].subsampling_y))
    return;
#if CONFIG_EXT_INTRA && CONFIG_EXT_INTRA_MOD
  if (av1_is_directional_mode(get_uv_mode(mbmi->uv_mode), bsize) &&
      av1_use_angle_delta(bsize)) {
#if CONFIG_ENTROPY_STATS
    ++counts->angle_delta[mbmi->uv_mode - V_PRED]
                         [mbmi->angle_delta[1] + MAX_ANGLE_DELTA];
#endif
    if (allow_update_cdf)
      update_cdf(fc->angle_delta_cdf[mbmi->uv_mode - V_PRED],
                 mbmi->angle_delta[1] + MAX_ANGLE_DELTA,
                 2 * MAX_ANGLE_DELTA + 1);
  }
#endif  // CONFIG_EXT_INTRA && CONFIG_EXT_INTRA_MOD
#if CONFIG_ENTROPY_STATS
  ++counts->uv_mode[y_mode][uv_mode];
#endif  // CONFIG_ENTROPY_STATS
  if (allow_update_cdf)
    update_cdf(fc->uv_mode_cdf[y_mode], uv_mode, UV_INTRA_MODES);
}

#if CONFIG_NEW_MULTISYMBOL
// TODO(anybody) We can add stats accumulation here to train entropy models for
// palette modes
static void update_palette_cdf(MACROBLOCKD *xd, const MODE_INFO *mi) {
  FRAME_CONTEXT *fc = xd->tile_ctx;
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;

  assert(bsize >= BLOCK_8X8 && bsize <= BLOCK_LARGEST);
  const int block_palette_idx = bsize - BLOCK_8X8;

  if (mbmi->mode == DC_PRED) {
    const int n = pmi->palette_size[0];
    int palette_y_mode_ctx = 0;
    if (above_mi) {
      palette_y_mode_ctx +=
          (above_mi->mbmi.palette_mode_info.palette_size[0] > 0);
    }
    if (left_mi) {
      palette_y_mode_ctx +=
          (left_mi->mbmi.palette_mode_info.palette_size[0] > 0);
    }
    update_cdf(fc->palette_y_mode_cdf[block_palette_idx][palette_y_mode_ctx],
               n > 0, 2);
  }

  if (mbmi->uv_mode == UV_DC_PRED) {
    const int n = pmi->palette_size[1];
    const int palette_uv_mode_ctx = (pmi->palette_size[0] > 0);
    update_cdf(fc->palette_uv_mode_cdf[palette_uv_mode_ctx], n > 0, 2);
  }
}
#endif

static void update_txfm_count(MACROBLOCK *x, MACROBLOCKD *xd,
                              FRAME_COUNTS *counts, TX_SIZE tx_size, int depth,
                              int blk_row, int blk_col,
                              uint8_t allow_update_cdf) {
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int tx_row = blk_row >> 1;
  const int tx_col = blk_col >> 1;
  const int max_blocks_high = max_block_high(xd, mbmi->sb_type, 0);
  const int max_blocks_wide = max_block_wide(xd, mbmi->sb_type, 0);
  int ctx = txfm_partition_context(xd->above_txfm_context + blk_col,
                                   xd->left_txfm_context + blk_row,
                                   mbmi->sb_type, tx_size);
  const TX_SIZE plane_tx_size = mbmi->inter_tx_size[tx_row][tx_col];

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;
  assert(tx_size > TX_4X4);

  if (depth == MAX_VARTX_DEPTH) {
    // Don't add to counts in this case
    mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
    return;
  }

  if (tx_size == plane_tx_size) {
    ++counts->txfm_partition[ctx][0];
#if CONFIG_NEW_MULTISYMBOL
    if (allow_update_cdf)
      update_cdf(xd->tile_ctx->txfm_partition_cdf[ctx], 0, 2);
#endif
    mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];

    ++counts->txfm_partition[ctx][1];
#if CONFIG_NEW_MULTISYMBOL
    if (allow_update_cdf)
      update_cdf(xd->tile_ctx->txfm_partition_cdf[ctx], 1, 2);
#endif
    ++x->txb_split_count;

    if (sub_txs == TX_4X4) {
      mbmi->inter_tx_size[tx_row][tx_col] = TX_4X4;
      mbmi->tx_size = TX_4X4;
      txfm_partition_update(xd->above_txfm_context + blk_col,
                            xd->left_txfm_context + blk_row, TX_4X4, tx_size);
      return;
    }

    for (int row = 0; row < tx_size_high_unit[tx_size]; row += bsh) {
      for (int col = 0; col < tx_size_wide_unit[tx_size]; col += bsw) {
        int offsetr = row;
        int offsetc = col;

        update_txfm_count(x, xd, counts, sub_txs, depth + 1, blk_row + offsetr,
                          blk_col + offsetc, allow_update_cdf);
      }
    }
  }
}

static void tx_partition_count_update(const AV1_COMMON *const cm, MACROBLOCK *x,
                                      BLOCK_SIZE plane_bsize, int mi_row,
                                      int mi_col, FRAME_COUNTS *td_counts,
                                      uint8_t allow_update_cdf) {
  MACROBLOCKD *xd = &x->e_mbd;
  const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
  const int mi_height = block_size_high[plane_bsize] >> tx_size_wide_log2[0];
  TX_SIZE max_tx_size = get_vartx_max_txsize(&xd->mi[0]->mbmi, plane_bsize, 0);
  const int bh = tx_size_high_unit[max_tx_size];
  const int bw = tx_size_wide_unit[max_tx_size];
  int idx, idy;

  xd->above_txfm_context =
      cm->above_txfm_context + (mi_col << TX_UNIT_WIDE_LOG2);
  xd->left_txfm_context = xd->left_txfm_context_buffer +
                          ((mi_row & MAX_MIB_MASK) << TX_UNIT_HIGH_LOG2);

  for (idy = 0; idy < mi_height; idy += bh)
    for (idx = 0; idx < mi_width; idx += bw)
      update_txfm_count(x, xd, td_counts, max_tx_size, 0, idy, idx,
                        allow_update_cdf);
}

static void set_txfm_context(MACROBLOCKD *xd, TX_SIZE tx_size, int blk_row,
                             int blk_col) {
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int tx_row = blk_row >> 1;
  const int tx_col = blk_col >> 1;
  const int max_blocks_high = max_block_high(xd, mbmi->sb_type, 0);
  const int max_blocks_wide = max_block_wide(xd, mbmi->sb_type, 0);
  const TX_SIZE plane_tx_size = mbmi->inter_tx_size[tx_row][tx_col];

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  if (tx_size == plane_tx_size) {
    mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);

  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsl = tx_size_wide_unit[sub_txs];
    int i;

    if (tx_size == TX_8X8) {
      mbmi->inter_tx_size[tx_row][tx_col] = TX_4X4;
      mbmi->tx_size = TX_4X4;
      txfm_partition_update(xd->above_txfm_context + blk_col,
                            xd->left_txfm_context + blk_row, TX_4X4, tx_size);
      return;
    }

    assert(bsl > 0);
    for (i = 0; i < 4; ++i) {
      int offsetr = (i >> 1) * bsl;
      int offsetc = (i & 0x01) * bsl;
      set_txfm_context(xd, sub_txs, blk_row + offsetr, blk_col + offsetc);
    }
  }
}

static void tx_partition_set_contexts(const AV1_COMMON *const cm,
                                      MACROBLOCKD *xd, BLOCK_SIZE plane_bsize,
                                      int mi_row, int mi_col) {
  const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
  const int mi_height = block_size_high[plane_bsize] >> tx_size_high_log2[0];
  TX_SIZE max_tx_size = get_vartx_max_txsize(&xd->mi[0]->mbmi, plane_bsize, 0);
  const int bh = tx_size_high_unit[max_tx_size];
  const int bw = tx_size_wide_unit[max_tx_size];
  int idx, idy;

  xd->above_txfm_context =
      cm->above_txfm_context + (mi_col << TX_UNIT_WIDE_LOG2);
  xd->left_txfm_context = xd->left_txfm_context_buffer +
                          ((mi_row & MAX_MIB_MASK) << TX_UNIT_HIGH_LOG2);

  for (idy = 0; idy < mi_height; idy += bh)
    for (idx = 0; idx < mi_width; idx += bw)
      set_txfm_context(xd, max_tx_size, idy, idx);
}

void av1_update_tx_type_count(const AV1_COMMON *cm, MACROBLOCKD *xd,
#if CONFIG_TXK_SEL
                              int blk_row, int blk_col, int block, int plane,
#endif
                              BLOCK_SIZE bsize, TX_SIZE tx_size,
                              FRAME_COUNTS *counts, uint8_t allow_update_cdf) {
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  int is_inter = is_inter_block(mbmi);
  FRAME_CONTEXT *fc = xd->tile_ctx;
#if !CONFIG_ENTROPY_STATS
  (void)counts;
#endif  // !CONFIG_ENTROPY_STATS

#if !CONFIG_TXK_SEL
  TX_TYPE tx_type = mbmi->tx_type;
#else
  (void)blk_row;
  (void)blk_col;
  // Only y plane's tx_type is updated
  if (plane > 0) return;
  TX_TYPE tx_type =
      av1_get_tx_type(PLANE_TYPE_Y, xd, blk_row, blk_col, block, tx_size);
#endif
  if (get_ext_tx_types(tx_size, bsize, is_inter, cm->reduced_tx_set_used) > 1 &&
      cm->base_qindex > 0 && !mbmi->skip &&
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    const int eset =
        get_ext_tx_set(tx_size, bsize, is_inter, cm->reduced_tx_set_used);
    if (eset > 0) {
      const TxSetType tx_set_type = get_ext_tx_set_type(
          tx_size, bsize, is_inter, cm->reduced_tx_set_used);
      if (is_inter) {
        if (allow_update_cdf)
          update_cdf(fc->inter_ext_tx_cdf[eset][txsize_sqr_map[tx_size]],
                     av1_ext_tx_ind[tx_set_type][tx_type],
                     av1_num_ext_tx_set[tx_set_type]);
#if CONFIG_ENTROPY_STATS
        ++counts->inter_ext_tx[eset][txsize_sqr_map[tx_size]][tx_type];
#endif  // CONFIG_ENTROPY_STATS
      } else {
#if CONFIG_FILTER_INTRA
        PREDICTION_MODE intra_dir;
        if (mbmi->filter_intra_mode_info.use_filter_intra_mode[0])
          intra_dir = fimode_to_intradir[mbmi->filter_intra_mode_info
                                             .filter_intra_mode[0]];
        else
          intra_dir = mbmi->mode;
#if CONFIG_ENTROPY_STATS
        ++counts
              ->intra_ext_tx[eset][txsize_sqr_map[tx_size]][intra_dir][tx_type];
#endif  // CONFIG_ENTROPY_STATS
        if (allow_update_cdf)
          update_cdf(
              fc->intra_ext_tx_cdf[eset][txsize_sqr_map[tx_size]][intra_dir],
              av1_ext_tx_ind[tx_set_type][tx_type],
              av1_num_ext_tx_set[tx_set_type]);
#else
#if CONFIG_ENTROPY_STATS
        ++counts->intra_ext_tx[eset][txsize_sqr_map[tx_size]][mbmi->mode]
                              [tx_type];
#endif  // CONFIG_ENTROPY_STATS
        if (allow_update_cdf)
          update_cdf(
              fc->intra_ext_tx_cdf[eset][txsize_sqr_map[tx_size]][mbmi->mode],
              av1_ext_tx_ind[tx_set_type][tx_type],
              av1_num_ext_tx_set[tx_set_type]);
#endif
      }
    }
  }
}

static void encode_superblock(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                              ThreadData *td, TOKENEXTRA **t, RUN_TYPE dry_run,
                              int mi_row, int mi_col, BLOCK_SIZE bsize,
                              int *rate) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO **mi_8x8 = xd->mi;
  MODE_INFO *mi = mi_8x8[0];
  MB_MODE_INFO *mbmi = &mi->mbmi;
  const int seg_skip =
      segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP);
  const int mis = cm->mi_stride;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];
  const int is_inter = is_inter_block(mbmi);
  const BLOCK_SIZE block_size = bsize;
  const int num_planes = av1_num_planes(cm);

  if (!is_inter) {
#if CONFIG_CFL
    xd->cfl->store_y = 1;
#endif  // CONFIG_CFL
    mbmi->skip = 1;
    for (int plane = 0; plane < num_planes; ++plane) {
      av1_encode_intra_block_plane((AV1_COMMON *)cm, x, block_size, plane, 1,
                                   mi_row, mi_col);
    }
#if CONFIG_CFL
    xd->cfl->store_y = 0;
#endif  // CONFIG_CFL
    if (!dry_run) {
      sum_intra_stats(td->counts, xd, mi, xd->above_mi, xd->left_mi,
                      frame_is_intra_only(cm), mi_row, mi_col,
                      tile_data->allow_update_cdf);
#if CONFIG_NEW_MULTISYMBOL
      if (av1_allow_palette(cm->allow_screen_content_tools, bsize) &&
          tile_data->allow_update_cdf)
        update_palette_cdf(xd, mi);
#endif
    }

    if (bsize >= BLOCK_8X8) {
      for (int plane = 0; plane < AOMMIN(2, num_planes); ++plane) {
        if (mbmi->palette_mode_info.palette_size[plane] > 0) {
          if (!dry_run)
            av1_tokenize_color_map(x, plane, 0, t, bsize, mbmi->tx_size,
                                   PALETTE_MAP);
          else if (dry_run == DRY_RUN_COSTCOEFFS)
            rate += av1_cost_color_map(x, plane, 0, bsize, mbmi->tx_size,
                                       PALETTE_MAP);
        }
      }
    }

    mbmi->min_tx_size = get_min_tx_size(mbmi->tx_size);
#if CONFIG_LV_MAP
    av1_update_txb_context(cpi, td, dry_run, block_size, rate, mi_row, mi_col,
                           tile_data->allow_update_cdf);
#else   // CONFIG_LV_MAP
    av1_tokenize_sb(cpi, td, t, dry_run, block_size, rate, mi_row, mi_col,
                    tile_data->allow_update_cdf);
#endif  // CONFIG_LV_MAP
  } else {
    int ref;
    const int is_compound = has_second_ref(mbmi);

    set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
    for (ref = 0; ref < 1 + is_compound; ++ref) {
      YV12_BUFFER_CONFIG *cfg = get_ref_frame_buffer(cpi, mbmi->ref_frame[ref]);
#if CONFIG_INTRABC
      assert(IMPLIES(!is_intrabc_block(mbmi), cfg));
#else
      assert(cfg != NULL);
#endif  // !CONFIG_INTRABC
      av1_setup_pre_planes(xd, ref, cfg, mi_row, mi_col,
                           &xd->block_refs[ref]->sf);
    }
#if CONFIG_COMPOUND_SINGLEREF
    // Single ref compound mode
    if (!is_compound && is_inter_singleref_comp_mode(mbmi->mode)) {
      xd->block_refs[1] = xd->block_refs[0];
      YV12_BUFFER_CONFIG *cfg = get_ref_frame_buffer(cpi, mbmi->ref_frame[0]);
#if CONFIG_INTRABC
      assert(IMPLIES(!is_intrabc_block(mbmi), cfg));
#else
      assert(cfg != NULL);
#endif  // !CONFIG_INTRABC
      av1_setup_pre_planes(xd, 1, cfg, mi_row, mi_col, &xd->block_refs[1]->sf);
    }
#endif  // CONFIG_COMPOUND_SINGLEREF

    av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL, block_size);

    if (mbmi->motion_mode == OBMC_CAUSAL) {
#if CONFIG_NCOBMC
      if (dry_run == OUTPUT_ENABLED)
        av1_build_ncobmc_inter_predictors_sb(cm, xd, mi_row, mi_col);
      else
#endif
        av1_build_obmc_inter_predictors_sb(cm, xd, mi_row, mi_col);
    }

    av1_encode_sb((AV1_COMMON *)cm, x, block_size, mi_row, mi_col);
    if (mbmi->skip) mbmi->min_tx_size = get_min_tx_size(mbmi->tx_size);
    av1_tokenize_sb_vartx(cpi, td, t, dry_run, mi_row, mi_col, block_size, rate,
                          tile_data->allow_update_cdf);
  }

#if CONFIG_DIST_8X8
  if (x->using_dist_8x8 && bsize < BLOCK_8X8) {
    dist_8x8_set_sub8x8_dst(x, (uint8_t *)x->decoded_8x8, bsize,
                            block_size_wide[bsize], block_size_high[bsize],
                            mi_row, mi_col);
  }
#endif  // CONFIG_DIST_8X8

  if (!dry_run) {
#if CONFIG_INTRABC
    if (av1_allow_intrabc(bsize, cm))
      if (is_intrabc_block(mbmi)) td->intrabc_used_this_tile = 1;
#endif  // CONFIG_INTRABC
    TX_SIZE tx_size =
        is_inter && !mbmi->skip ? mbmi->min_tx_size : mbmi->tx_size;
    if (cm->tx_mode == TX_MODE_SELECT && !xd->lossless[mbmi->segment_id] &&
        mbmi->sb_type > BLOCK_4X4 && !(is_inter && (mbmi->skip || seg_skip))) {
      if (is_inter) {
        tx_partition_count_update(cm, x, bsize, mi_row, mi_col, td->counts,
                                  tile_data->allow_update_cdf);
      } else {
        if (tx_size != get_max_rect_tx_size(bsize, 0)) ++x->txb_split_count;
      }
      assert(IMPLIES(is_rect_tx(tx_size), is_rect_tx_allowed(xd, mbmi)));
    } else {
      int i, j;
      TX_SIZE intra_tx_size;
      // The new intra coding scheme requires no change of transform size
      if (is_inter) {
        if (xd->lossless[mbmi->segment_id]) {
          intra_tx_size = TX_4X4;
        } else {
          intra_tx_size = tx_size_from_tx_mode(bsize, cm->tx_mode, 1);
        }
      } else {
        intra_tx_size = tx_size;
      }
      ++td->counts->tx_size_implied[max_txsize_lookup[bsize]]
                                   [txsize_sqr_up_map[tx_size]];

      for (j = 0; j < mi_height; j++)
        for (i = 0; i < mi_width; i++)
          if (mi_col + i < cm->mi_cols && mi_row + j < cm->mi_rows)
            mi_8x8[mis * j + i]->mbmi.tx_size = intra_tx_size;

      mbmi->min_tx_size = get_min_tx_size(intra_tx_size);
      if (intra_tx_size != get_max_rect_tx_size(bsize, is_inter))
        ++x->txb_split_count;
    }

#if !CONFIG_TXK_SEL
    av1_update_tx_type_count(cm, xd, bsize, tx_size, td->counts,
                             tile_data->allow_update_cdf);
#endif
  }

  if (cm->tx_mode == TX_MODE_SELECT && mbmi->sb_type > BLOCK_4X4 && is_inter &&
      !(mbmi->skip || seg_skip) && !xd->lossless[mbmi->segment_id]) {
    if (dry_run) tx_partition_set_contexts(cm, xd, bsize, mi_row, mi_col);
  } else {
    TX_SIZE tx_size = mbmi->tx_size;
    // The new intra coding scheme requires no change of transform size
    if (is_inter) {
      if (xd->lossless[mbmi->segment_id]) {
        tx_size = TX_4X4;
      } else {
        tx_size = tx_size_from_tx_mode(bsize, cm->tx_mode, is_inter);
      }
    } else {
      tx_size = (bsize > BLOCK_4X4) ? tx_size : TX_4X4;
    }
    mbmi->tx_size = tx_size;
    set_txfm_ctxs(tx_size, xd->n8_w, xd->n8_h, (mbmi->skip || seg_skip), xd);
  }
#if CONFIG_CFL
  CFL_CTX *const cfl = xd->cfl;
  if (is_inter_block(mbmi) &&
      !is_chroma_reference(mi_row, mi_col, bsize, cfl->subsampling_x,
                           cfl->subsampling_y)) {
    cfl_store_block(xd, mbmi->sb_type, mbmi->tx_size);
  }
#endif  // CONFIG_CFL
}
