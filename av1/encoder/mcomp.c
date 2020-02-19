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

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "aom_ports/system_state.h"

#include "av1/common/common.h"
#include "av1/common/mvref_common.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/reconinter.h"

#include "av1/encoder/encoder.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/partition_strategy.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/reconinter_enc.h"

// #define NEW_DIAMOND_SEARCH

// TODO(any): Adaptively adjust the regularization strength based on image size
// and motion activity instead of using hard-coded values. It seems like we
// roughly half the lambda for each increase in resolution
// These are multiplier used to perform regularization in motion compensation
// when x->mv_cost_type is set to MV_COST_L1.
// LOWRES
#define SSE_LAMBDA_LOWRES 2   // Used by mv_cost_err_fn
#define SAD_LAMBDA_LOWRES 32  // Used by mvsad_err_cost during full pixel search
// MIDRES
#define SSE_LAMBDA_MIDRES 0   // Used by mv_cost_err_fn
#define SAD_LAMBDA_MIDRES 15  // Used by mvsad_err_cost during full pixel search
// HDRES
#define SSE_LAMBDA_HDRES 1  // Used by mv_cost_err_fn
#define SAD_LAMBDA_HDRES 8  // Used by mvsad_err_cost during full pixel search

void av1_make_default_subpel_ms_params(
    SUBPEL_MOTION_SEARCH_PARAMS *ms_params, const struct AV1_COMP *cpi,
    const MACROBLOCK *x, BLOCK_SIZE bsize, const MV *ref_mv,
    const int *cost_list, const uint8_t *second_pred, const uint8_t *mask,
    int mask_stride, int invert_mask, int do_reset_fractional_mv) {
  const AV1_COMMON *cm = &cpi->common;
  // High level params
  ms_params->allow_hp = cm->allow_high_precision_mv;
  ms_params->forced_stop = cpi->sf.mv_sf.subpel_force_stop;
  ms_params->iters_per_step = cpi->sf.mv_sf.subpel_iters_per_step;
  ms_params->cost_list = cond_cost_list_const(cpi, cost_list);
  ms_params->do_reset_fractional_mv = do_reset_fractional_mv;

  // Mvcost params
  ms_params->mv_cost_params.ref_mv = ref_mv;
  ms_params->mv_cost_params.error_per_bit = x->errorperbit;
  ms_params->mv_cost_params.mvjcost = x->nmv_vec_cost;
  ms_params->mv_cost_params.mvcost[0] = x->mv_cost_stack[0];
  ms_params->mv_cost_params.mvcost[1] = x->mv_cost_stack[1];
  ms_params->mv_cost_params.mv_cost_type = x->mv_cost_type;

  // Subpel variance params
  ms_params->var_params.vfp = &cpi->fn_ptr[bsize];
  ms_params->var_params.subpel_search_type =
      cpi->sf.mv_sf.use_accurate_subpel_search;
  ms_params->var_params.second_pred = second_pred;
  ms_params->var_params.mask = mask;
  ms_params->var_params.mask_stride = mask_stride;
  ms_params->var_params.invert_mask = invert_mask;
  ms_params->var_params.w = block_size_wide[bsize];
  ms_params->var_params.h = block_size_high[bsize];
}

static INLINE int get_offset_from_mv(const FULLPEL_MV *mv, int stride) {
  return mv->row * stride + mv->col;
}

static INLINE const uint8_t *get_buf_from_mv(const struct buf_2d *buf,
                                             const FULLPEL_MV *mv) {
  return &buf->buf[get_offset_from_mv(mv, buf->stride)];
}

void av1_set_mv_search_range(FullMvLimits *mv_limits, const MV *mv) {
  int col_min =
      GET_MV_RAWPEL(mv->col) - MAX_FULL_PEL_VAL + (mv->col & 7 ? 1 : 0);
  int row_min =
      GET_MV_RAWPEL(mv->row) - MAX_FULL_PEL_VAL + (mv->row & 7 ? 1 : 0);
  int col_max = GET_MV_RAWPEL(mv->col) + MAX_FULL_PEL_VAL;
  int row_max = GET_MV_RAWPEL(mv->row) + MAX_FULL_PEL_VAL;

  col_min = AOMMAX(col_min, GET_MV_RAWPEL(MV_LOW) + 1);
  row_min = AOMMAX(row_min, GET_MV_RAWPEL(MV_LOW) + 1);
  col_max = AOMMIN(col_max, GET_MV_RAWPEL(MV_UPP) - 1);
  row_max = AOMMIN(row_max, GET_MV_RAWPEL(MV_UPP) - 1);

  // Get intersection of UMV window and valid MV window to reduce # of checks
  // in diamond search.
  if (mv_limits->col_min < col_min) mv_limits->col_min = col_min;
  if (mv_limits->col_max > col_max) mv_limits->col_max = col_max;
  if (mv_limits->row_min < row_min) mv_limits->row_min = row_min;
  if (mv_limits->row_max > row_max) mv_limits->row_max = row_max;
}

int av1_init_search_range(int size) {
  int sr = 0;
  // Minimum search size no matter what the passed in value.
  size = AOMMAX(16, size);

  while ((size << sr) < MAX_FULL_PEL_VAL) sr++;

  sr = AOMMIN(sr, MAX_MVSEARCH_STEPS - 2);
  return sr;
}

// Returns the rate of encoding the current motion vector based on the
// joint_cost and comp_cost. joint_costs covers the cost of transmitting
// JOINT_MV, and comp_cost covers the cost of transmitting the actual motion
// vector.
static INLINE int mv_cost(const MV *mv, const int *joint_cost,
                          const int *const comp_cost[2]) {
  return joint_cost[av1_get_mv_joint(mv)] + comp_cost[0][mv->row] +
         comp_cost[1][mv->col];
}

#define CONVERT_TO_CONST_MVCOST(ptr) ((const int *const *)(ptr))
// Returns the cost of encoding the motion vector diff := *mv - *ref. The cost
// is defined as the rate required to encode diff * weight, rounded to the
// nearest 2 ** 7.
// This is NOT used during motion compensation.
int av1_mv_bit_cost(const MV *mv, const MV *ref_mv, const int *mvjcost,
                    int *mvcost[2], int weight) {
  const MV diff = { mv->row - ref_mv->row, mv->col - ref_mv->col };
  return ROUND_POWER_OF_TWO(
      mv_cost(&diff, mvjcost, CONVERT_TO_CONST_MVCOST(mvcost)) * weight, 7);
}

// Returns the cost of using the current mv during the motion search. This is
// used when var is used as the error metric.
#define PIXEL_TRANSFORM_ERROR_SCALE 4
static int mv_err_cost(const MV *mv, const MV *ref_mv, const int *mvjcost,
                       const int *const mvcost[2], int error_per_bit,
                       MV_COST_TYPE mv_cost_type) {
  const MV diff = { mv->row - ref_mv->row, mv->col - ref_mv->col };
  const MV abs_diff = { abs(diff.row), abs(diff.col) };

  switch (mv_cost_type) {
    case MV_COST_ENTROPY:
      if (mvcost) {
        return (int)ROUND_POWER_OF_TWO_64(
            (int64_t)mv_cost(&diff, mvjcost, mvcost) * error_per_bit,
            RDDIV_BITS + AV1_PROB_COST_SHIFT - RD_EPB_SHIFT +
                PIXEL_TRANSFORM_ERROR_SCALE);
      }
      return 0;
    case MV_COST_L1_LOWRES:
      return (SSE_LAMBDA_LOWRES * (abs_diff.row + abs_diff.col)) >> 3;
    case MV_COST_L1_MIDRES:
      return (SSE_LAMBDA_MIDRES * (abs_diff.row + abs_diff.col)) >> 3;
    case MV_COST_L1_HDRES:
      return (SSE_LAMBDA_HDRES * (abs_diff.row + abs_diff.col)) >> 3;
    case MV_COST_NONE: return 0;
    default: assert(0 && "Invalid rd_cost_type"); return 0;
  }
}

static INLINE int mv_err_cost_(const MV *mv,
                               const MV_COST_PARAMS *mv_cost_params) {
  return mv_err_cost(mv, mv_cost_params->ref_mv, mv_cost_params->mvjcost,
                     mv_cost_params->mvcost, mv_cost_params->error_per_bit,
                     mv_cost_params->mv_cost_type);
}

// Returns the cost of using the current mv during the motion search. This is
// only used during full pixel motion search when sad is used as the error
// metric.
static int mvsad_err_cost(const MACROBLOCK *x, const FULLPEL_MV *mv,
                          const FULLPEL_MV *ref_mv, int sad_per_bit) {
  const MV diff = { GET_MV_SUBPEL(mv->row - ref_mv->row),
                    GET_MV_SUBPEL(mv->col - ref_mv->col) };
  const MV_COST_TYPE mv_cost_type = x->mv_cost_type;

  switch (mv_cost_type) {
    case MV_COST_ENTROPY:
      return ROUND_POWER_OF_TWO(
          (unsigned)mv_cost(&diff, x->nmv_vec_cost,
                            CONVERT_TO_CONST_MVCOST(x->mv_cost_stack)) *
              sad_per_bit,
          AV1_PROB_COST_SHIFT);
    case MV_COST_L1_LOWRES:
      return (SAD_LAMBDA_LOWRES * (abs(diff.row) + abs(diff.col))) >> 3;
    case MV_COST_L1_MIDRES:
      return (SAD_LAMBDA_MIDRES * (abs(diff.row) + abs(diff.col))) >> 3;
    case MV_COST_L1_HDRES:
      return (SAD_LAMBDA_HDRES * (abs(diff.row) + abs(diff.col))) >> 3;
    case MV_COST_NONE: return 0;
    default: assert(0 && "Invalid rd_cost_type"); return 0;
  }
}

void av1_init_dsmotion_compensation(search_site_config *cfg, int stride) {
  int ss_count = 0;
  int stage_index = MAX_MVSEARCH_STEPS - 1;

  cfg->ss[stage_index][0].mv.col = cfg->ss[stage_index][0].mv.row = 0;
  cfg->ss[stage_index][0].offset = 0;
  cfg->stride = stride;

  for (int radius = MAX_FIRST_STEP; radius > 0; radius /= 2) {
    int num_search_pts = 8;

    const FULLPEL_MV ss_mvs[13] = {
      { 0, 0 },           { -radius, 0 },      { radius, 0 },
      { 0, -radius },     { 0, radius },       { -radius, -radius },
      { radius, radius }, { -radius, radius }, { radius, -radius },
    };

    int i;
    for (i = 0; i <= num_search_pts; ++i) {
      search_site *const ss = &cfg->ss[stage_index][i];
      ss->mv = ss_mvs[i];
      ss->offset = get_offset_from_mv(&ss->mv, stride);
    }
    cfg->searches_per_step[stage_index] = num_search_pts;
    cfg->radius[stage_index] = radius;
    --stage_index;
    ++ss_count;
  }
  cfg->ss_count = ss_count;
}

void av1_init_motion_fpf(search_site_config *cfg, int stride) {
  int ss_count = 0;
  int stage_index = MAX_MVSEARCH_STEPS - 1;

  cfg->ss[stage_index][0].mv.col = cfg->ss[stage_index][0].mv.row = 0;
  cfg->ss[stage_index][0].offset = 0;
  cfg->stride = stride;

  for (int radius = MAX_FIRST_STEP; radius > 0; radius /= 2) {
    // Generate offsets for 8 search sites per step.
    int tan_radius = AOMMAX((int)(0.41 * radius), 1);
    int num_search_pts = 12;
    if (radius == 1) num_search_pts = 8;

    const FULLPEL_MV ss_mvs[13] = {
      { 0, 0 },
      { -radius, 0 },
      { radius, 0 },
      { 0, -radius },
      { 0, radius },
      { -radius, -tan_radius },
      { radius, tan_radius },
      { -tan_radius, radius },
      { tan_radius, -radius },
      { -radius, tan_radius },
      { radius, -tan_radius },
      { tan_radius, radius },
      { -tan_radius, -radius },
    };

    int i;
    for (i = 0; i <= num_search_pts; ++i) {
      search_site *const ss = &cfg->ss[stage_index][i];
      ss->mv = ss_mvs[i];
      ss->offset = get_offset_from_mv(&ss->mv, stride);
    }
    cfg->searches_per_step[stage_index] = num_search_pts;
    cfg->radius[stage_index] = radius;
    --stage_index;
    ++ss_count;
  }
  cfg->ss_count = ss_count;
}

void av1_init3smotion_compensation(search_site_config *cfg, int stride) {
  int ss_count = 0;
  int stage_index = 0;
  cfg->stride = stride;
  int radius = 1;
  for (stage_index = 0; stage_index < 15; ++stage_index) {
    int tan_radius = AOMMAX((int)(0.41 * radius), 1);
    int num_search_pts = 12;
    if (radius <= 5) {
      tan_radius = radius;
      num_search_pts = 8;
    }
    const FULLPEL_MV ss_mvs[13] = {
      { 0, 0 },
      { -radius, 0 },
      { radius, 0 },
      { 0, -radius },
      { 0, radius },
      { -radius, -tan_radius },
      { radius, tan_radius },
      { -tan_radius, radius },
      { tan_radius, -radius },
      { -radius, tan_radius },
      { radius, -tan_radius },
      { tan_radius, radius },
      { -tan_radius, -radius },
    };

    for (int i = 0; i <= num_search_pts; ++i) {
      search_site *const ss = &cfg->ss[stage_index][i];
      ss->mv = ss_mvs[i];
      ss->offset = get_offset_from_mv(&ss->mv, stride);
    }
    cfg->searches_per_step[stage_index] = num_search_pts;
    cfg->radius[stage_index] = radius;
    ++ss_count;
    if (stage_index < 12)
      radius = (int)AOMMAX((radius * 1.5 + 0.5), radius + 1);
  }
  cfg->ss_count = ss_count;
}

/*
 * To avoid the penalty for crossing cache-line read, preload the reference
 * area in a small buffer, which is aligned to make sure there won't be crossing
 * cache-line read while reading from this buffer. This reduced the cpu
 * cycles spent on reading ref data in sub-pixel filter functions.
 * TODO: Currently, since sub-pixel search range here is -3 ~ 3, copy 22 rows x
 * 32 cols area that is enough for 16x16 macroblock. Later, for SPLITMV, we
 * could reduce the area.
 */

// convert motion vector component to offset for sv[a]f calc
static INLINE int sp(int x) { return x & 7; }

static INLINE const uint8_t *pre(const uint8_t *buf, int stride, int r, int c) {
  const int offset = (r >> 3) * stride + (c >> 3);
  return buf + offset;
}

#define UNPACK_VAR_PARAMS(var_params)                     \
  const aom_variance_fn_ptr_t *vfp = (var_params)->vfp;   \
  const SUBPEL_SEARCH_TYPE subpel_search_type =           \
      (var_params)->subpel_search_type;                   \
  const uint8_t *second_pred = (var_params)->second_pred; \
  const uint8_t *mask = (var_params)->mask;               \
  const int mask_stride = (var_params)->mask_stride;      \
  const int invert_mask = (var_params)->invert_mask;      \
  const int w = (var_params)->w;                          \
  const int h = (var_params)->h;

static INLINE int estimated_pref_error(
    const MV *this_mv, const uint8_t *src, const int src_stride,
    const uint8_t *ref, int ref_stride,
    const SUBPEL_SEARCH_VAR_PARAMS *var_params, unsigned int *sse) {
  UNPACK_VAR_PARAMS(var_params);
  (void)subpel_search_type;
  (void)w;
  (void)h;
  const int r = this_mv->row;
  const int c = this_mv->col;
  if (second_pred == NULL) {
    return vfp->svf(pre(ref, ref_stride, r, c), ref_stride, sp(c), sp(r), src,
                    src_stride, sse);
  } else if (mask) {
    return vfp->msvf(pre(ref, ref_stride, r, c), ref_stride, sp(c), sp(r), src,
                     src_stride, second_pred, mask, mask_stride, invert_mask,
                     sse);
  } else {
    return vfp->svaf(pre(ref, ref_stride, r, c), ref_stride, sp(c), sp(r), src,
                     src_stride, sse, second_pred);
  }
}

static int upsampled_pref_error(MACROBLOCKD *xd, const AV1_COMMON *cm,
                                const MV *this_mv, const uint8_t *src,
                                int src_stride, const uint8_t *ref,
                                int ref_stride,
                                const SUBPEL_SEARCH_VAR_PARAMS *var_params,
                                unsigned int *sse) {
  UNPACK_VAR_PARAMS(var_params);
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  ref = pre(ref, ref_stride, this_mv->row, this_mv->col);
  const int subpel_x_q3 = sp(this_mv->col);
  const int subpel_y_q3 = sp(this_mv->row);
  unsigned int besterr;
#if CONFIG_AV1_HIGHBITDEPTH
  if (is_cur_buf_hbd(xd)) {
    DECLARE_ALIGNED(16, uint16_t, pred16[MAX_SB_SQUARE]);
    uint8_t *pred8 = CONVERT_TO_BYTEPTR(pred16);
    if (second_pred != NULL) {
      if (mask) {
        aom_highbd_comp_mask_upsampled_pred(
            xd, cm, mi_row, mi_col, this_mv, pred8, second_pred, w, h,
            subpel_x_q3, subpel_y_q3, ref, ref_stride, mask, mask_stride,
            invert_mask, xd->bd, subpel_search_type);
      } else {
        aom_highbd_comp_avg_upsampled_pred(
            xd, cm, mi_row, mi_col, this_mv, pred8, second_pred, w, h,
            subpel_x_q3, subpel_y_q3, ref, ref_stride, xd->bd,
            subpel_search_type);
      }
    } else {
      aom_highbd_upsampled_pred(xd, cm, mi_row, mi_col, this_mv, pred8, w, h,
                                subpel_x_q3, subpel_y_q3, ref, ref_stride,
                                xd->bd, subpel_search_type);
    }
    besterr = vfp->vf(pred8, w, src, src_stride, sse);
  } else {
    DECLARE_ALIGNED(16, uint8_t, pred[MAX_SB_SQUARE]);
    if (second_pred != NULL) {
      if (mask) {
        aom_comp_mask_upsampled_pred(
            xd, cm, mi_row, mi_col, this_mv, pred, second_pred, w, h,
            subpel_x_q3, subpel_y_q3, ref, ref_stride, mask, mask_stride,
            invert_mask, subpel_search_type);
      } else {
        aom_comp_avg_upsampled_pred(xd, cm, mi_row, mi_col, this_mv, pred,
                                    second_pred, w, h, subpel_x_q3, subpel_y_q3,
                                    ref, ref_stride, subpel_search_type);
      }
    } else {
      aom_upsampled_pred(xd, cm, mi_row, mi_col, this_mv, pred, w, h,
                         subpel_x_q3, subpel_y_q3, ref, ref_stride,
                         subpel_search_type);
    }

    besterr = vfp->vf(pred, w, src, src_stride, sse);
  }
#else
  DECLARE_ALIGNED(16, uint8_t, pred[MAX_SB_SQUARE]);
  if (second_pred != NULL) {
    if (mask) {
      aom_comp_mask_upsampled_pred(xd, cm, mi_row, mi_col, this_mv, pred,
                                   second_pred, w, h, subpel_x_q3, subpel_y_q3,
                                   ref, ref_stride, mask, mask_stride,
                                   invert_mask, subpel_search_type);
    } else {
      aom_comp_avg_upsampled_pred(xd, cm, mi_row, mi_col, this_mv, pred,
                                  second_pred, w, h, subpel_x_q3, subpel_y_q3,
                                  ref, ref_stride, subpel_search_type);
    }
  } else {
    aom_upsampled_pred(xd, cm, mi_row, mi_col, this_mv, pred, w, h, subpel_x_q3,
                       subpel_y_q3, ref, ref_stride, subpel_search_type);
  }

  besterr = vfp->vf(pred, w, src, src_stride, sse);
#endif
  return besterr;
}

static INLINE unsigned int check_better_fast(
    const MV *this_mv, MV *best_mv, const SubpelMvLimits *mv_limits,
    const uint8_t *const src, const int src_stride, const uint8_t *const ref,
    int ref_stride, const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *besterr,
    unsigned int *sse1, int *distortion) {
  unsigned int cost;
  if (av1_is_subpelmv_in_range(mv_limits, *this_mv)) {
    unsigned int sse;
    int thismse = estimated_pref_error(this_mv, src, src_stride, ref,
                                       ref_stride, var_params, &sse);
    cost = mv_err_cost_(this_mv, mv_cost_params);
    cost += thismse;

    if (cost < *besterr) {
      *besterr = cost;
      *best_mv = *this_mv;
      *distortion = thismse;
      *sse1 = sse;
    }
  } else {
    cost = INT_MAX;
  }
  return cost;
}

static AOM_FORCE_INLINE unsigned int check_better(
    MACROBLOCKD *xd, const AV1_COMMON *cm, const MV *this_mv, MV *best_mv,
    const SubpelMvLimits *mv_limits, const uint8_t *const src,
    const int src_stride, const uint8_t *const ref, int ref_stride,
    const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *besterr,
    unsigned int *sse1, int *distortion, int *is_better) {
  unsigned int cost;
  if (av1_is_subpelmv_in_range(mv_limits, *this_mv)) {
    unsigned int sse;
    int thismse;
    if (var_params->subpel_search_type != USE_2_TAPS_ORIG) {
      thismse = upsampled_pref_error(xd, cm, this_mv, src, src_stride, ref,
                                     ref_stride, var_params, &sse);
    } else {
      thismse = estimated_pref_error(this_mv, src, src_stride, ref, ref_stride,
                                     var_params, &sse);
    }
    cost = mv_err_cost_(this_mv, mv_cost_params);
    cost += thismse;
    if (cost < *besterr) {
      *besterr = cost;
      *best_mv = *this_mv;
      *distortion = thismse;
      *sse1 = sse;
      *is_better |= 1;
    }
  } else {
    cost = INT_MAX;
  }
  return cost;
}

static AOM_FORCE_INLINE int first_level_check_fast(
    const MV *this_mv, MV *best_mv, int hstep, const SubpelMvLimits *mv_limits,
    const uint8_t *const src, const int src_stride, const uint8_t *const ref,
    int ref_stride, const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *besterr,
    unsigned int *sse1, int *distortion) {
  // Check the four cardinal directions
  const MV left_mv = { this_mv->row, this_mv->col - hstep };
  const unsigned int left = check_better_fast(
      &left_mv, best_mv, mv_limits, src, src_stride, ref, ref_stride,
      var_params, mv_cost_params, besterr, sse1, distortion);

  const MV right_mv = { this_mv->row, this_mv->col + hstep };
  const unsigned int right = check_better_fast(
      &right_mv, best_mv, mv_limits, src, src_stride, ref, ref_stride,
      var_params, mv_cost_params, besterr, sse1, distortion);

  const MV top_mv = { this_mv->row - hstep, this_mv->col };
  const unsigned int up = check_better_fast(
      &top_mv, best_mv, mv_limits, src, src_stride, ref, ref_stride, var_params,
      mv_cost_params, besterr, sse1, distortion);

  const MV bottom_mv = { this_mv->row + hstep, this_mv->col };
  const unsigned int down = check_better_fast(
      &bottom_mv, best_mv, mv_limits, src, src_stride, ref, ref_stride,
      var_params, mv_cost_params, besterr, sse1, distortion);

  // Check the diagonal direction with the best mv
  const int whichdir = (left < right ? 0 : 1) + (up < down ? 0 : 2);
  switch (whichdir) {
    case 0: {
      const MV top_left_mv = { this_mv->row - hstep, this_mv->col - hstep };
      check_better_fast(&top_left_mv, best_mv, mv_limits, src, src_stride, ref,
                        ref_stride, var_params, mv_cost_params, besterr, sse1,
                        distortion);
      break;
    }
    case 1: {
      const MV top_right_mv = { this_mv->row - hstep, this_mv->col + hstep };
      check_better_fast(&top_right_mv, best_mv, mv_limits, src, src_stride, ref,
                        ref_stride, var_params, mv_cost_params, besterr, sse1,
                        distortion);
      break;
    }
    case 2: {
      const MV bottom_left_mv = { this_mv->row + hstep, this_mv->col - hstep };
      check_better_fast(&bottom_left_mv, best_mv, mv_limits, src, src_stride,
                        ref, ref_stride, var_params, mv_cost_params, besterr,
                        sse1, distortion);
      break;
    }
    case 3: {
      const MV bottom_right_mv = { this_mv->row + hstep, this_mv->col + hstep };
      check_better_fast(&bottom_right_mv, best_mv, mv_limits, src, src_stride,
                        ref, ref_stride, var_params, mv_cost_params, besterr,
                        sse1, distortion);
      break;
    }
  }
  return whichdir;
}

static AOM_FORCE_INLINE void second_level_check_fast(
    const MV *this_mv, MV *best_mv, int hstep, const SubpelMvLimits *mv_limits,
    const uint8_t *const src, const int src_stride, const uint8_t *const ref,
    int ref_stride, const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *besterr,
    unsigned int *sse1, int *distortion, int whichdir) {
  const int tr = this_mv->row;
  const int tc = this_mv->col;
  const int br = best_mv->row;
  const int bc = best_mv->col;
  if (tr != br && tc != bc) {
    const int kr = br - tr;
    const int kc = bc - tc;

    const MV chess_mv_1 = { tr + kr, tc + 2 * kc };
    check_better_fast(&chess_mv_1, best_mv, mv_limits, src, src_stride, ref,
                      ref_stride, var_params, mv_cost_params, besterr, sse1,
                      distortion);

    const MV chess_mv_2 = { tr + 2 * kr, tc + kc };
    check_better_fast(&chess_mv_2, best_mv, mv_limits, src, src_stride, ref,
                      ref_stride, var_params, mv_cost_params, besterr, sse1,
                      distortion);
  } else if (tr == br && tc != bc) {
    const int kc = bc - tc;
    const MV bottom_long_mv = { tr + hstep, tc + 2 * kc };
    check_better_fast(&bottom_long_mv, best_mv, mv_limits, src, src_stride, ref,
                      ref_stride, var_params, mv_cost_params, besterr, sse1,
                      distortion);
    const MV top_long_mv = { tr - hstep, tc + 2 * kc };
    check_better_fast(&top_long_mv, best_mv, mv_limits, src, src_stride, ref,
                      ref_stride, var_params, mv_cost_params, besterr, sse1,
                      distortion);

    switch (whichdir) {
      case 0:
      case 1: {
        const MV bottom_mv = { tr + hstep, tc + kc };
        check_better_fast(&bottom_mv, best_mv, mv_limits, src, src_stride, ref,
                          ref_stride, var_params, mv_cost_params, besterr, sse1,
                          distortion);
        break;
      }
      case 2:
      case 3: {
        const MV top_mv = { tr - hstep, tc + kc };
        check_better_fast(&top_mv, best_mv, mv_limits, src, src_stride, ref,
                          ref_stride, var_params, mv_cost_params, besterr, sse1,
                          distortion);
        break;
      }
    }
  } else if (tr != br && tc == bc) {
    const int kr = br - tr;
    const MV right_long_mv = { tr + 2 * kr, tc + hstep };
    check_better_fast(&right_long_mv, best_mv, mv_limits, src, src_stride, ref,
                      ref_stride, var_params, mv_cost_params, besterr, sse1,
                      distortion);
    const MV left_long_mv = { tr + 2 * kr, tc - hstep };
    check_better_fast(&left_long_mv, best_mv, mv_limits, src, src_stride, ref,
                      ref_stride, var_params, mv_cost_params, besterr, sse1,
                      distortion);

    switch (whichdir) {
      case 0:
      case 2: {
        const MV right_mv = { tr + kr, tc + hstep };
        check_better_fast(&right_mv, best_mv, mv_limits, src, src_stride, ref,
                          ref_stride, var_params, mv_cost_params, besterr, sse1,
                          distortion);
        break;
      }
      case 1:
      case 3: {
        const MV left_mv = { tr + kr, tc - hstep };
        check_better_fast(&left_mv, best_mv, mv_limits, src, src_stride, ref,
                          ref_stride, var_params, mv_cost_params, besterr, sse1,
                          distortion);
      }
    }
  }
}

static AOM_FORCE_INLINE void two_level_checks_fast(
    const MV *this_mv, MV *best_mv, int hstep, const SubpelMvLimits *mv_limits,
    const uint8_t *const src, const int src_stride, const uint8_t *const ref,
    int ref_stride, const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *besterr,
    unsigned int *sse1, int *distortion, int iters) {
  unsigned int whichdir = first_level_check_fast(
      this_mv, best_mv, hstep, mv_limits, src, src_stride, ref, ref_stride,
      var_params, mv_cost_params, besterr, sse1, distortion);
  if (iters > 1) {
    second_level_check_fast(this_mv, best_mv, hstep, mv_limits, src, src_stride,
                            ref, ref_stride, var_params, mv_cost_params,
                            besterr, sse1, distortion, whichdir);
  }
}

#define CHECK_BETTER(v, r, c)                                            \
  {                                                                      \
    const MV this_mv = { (r), (c) };                                     \
    (v) = check_better_fast(&this_mv, bestmv, &mv_limits, src_address,   \
                            src_stride, y, y_stride, var_params,         \
                            mv_cost_params, &besterr, sse1, distortion); \
  }

#define CHECK_BETTER0(v, r, c) CHECK_BETTER(v, r, c)

/* checks if (r, c) has better score than previous best */
#define CHECK_BETTER1(v, r, c)                                            \
  (v) = check_better(xd, cm, (r), (c), &br, &bc, &mv_limits, src_address, \
                     src_stride, y, y_stride, var_params, mv_cost_params, \
                     &besterr, sse1, distortion);

// TODO(yunqingwang): SECOND_LEVEL_CHECKS_BEST was a rewrote of
// SECOND_LEVEL_CHECKS, and SECOND_LEVEL_CHECKS should be rewritten
// later in the same way.
#define SECOND_LEVEL_CHECKS_BEST(k)                \
  {                                                \
    unsigned int second;                           \
    int br0 = br;                                  \
    int bc0 = bc;                                  \
    assert(tr == br || tc == bc);                  \
    if (tr == br && tc != bc) {                    \
      kc = bc - tc;                                \
    } else if (tr != br && tc == bc) {             \
      kr = br - tr;                                \
    }                                              \
    CHECK_BETTER##k(second, br0 + kr, bc0);        \
    CHECK_BETTER##k(second, br0, bc0 + kc);        \
    if (br0 != br || bc0 != bc) {                  \
      CHECK_BETTER##k(second, br0 + kr, bc0 + kc); \
    }                                              \
    (void)second;                                  \
  }

static unsigned int setup_center_error(
    const MACROBLOCKD *xd, const MV *bestmv, const uint8_t *const src,
    const int src_stride, const uint8_t *y, int y_stride,
    const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *sse1, int *distortion) {
  UNPACK_VAR_PARAMS(var_params);
  (void)subpel_search_type;
  unsigned int besterr;
  y = pre(y, y_stride, bestmv->row, bestmv->col);

  if (second_pred != NULL) {
#if CONFIG_AV1_HIGHBITDEPTH
    if (is_cur_buf_hbd(xd)) {
      DECLARE_ALIGNED(16, uint16_t, comp_pred16[MAX_SB_SQUARE]);
      uint8_t *comp_pred = CONVERT_TO_BYTEPTR(comp_pred16);
      if (mask) {
        aom_highbd_comp_mask_pred(comp_pred, second_pred, w, h, y, y_stride,
                                  mask, mask_stride, invert_mask);
      } else {
        aom_highbd_comp_avg_pred(comp_pred, second_pred, w, h, y, y_stride);
      }
      besterr = vfp->vf(comp_pred, w, src, src_stride, sse1);
    } else {
      DECLARE_ALIGNED(16, uint8_t, comp_pred[MAX_SB_SQUARE]);
      if (mask) {
        aom_comp_mask_pred(comp_pred, second_pred, w, h, y, y_stride, mask,
                           mask_stride, invert_mask);
      } else {
        aom_comp_avg_pred(comp_pred, second_pred, w, h, y, y_stride);
      }
      besterr = vfp->vf(comp_pred, w, src, src_stride, sse1);
    }
#else
    (void)xd;
    DECLARE_ALIGNED(16, uint8_t, comp_pred[MAX_SB_SQUARE]);
    if (mask) {
      aom_comp_mask_pred(comp_pred, second_pred, w, h, y, y_stride, mask,
                         mask_stride, invert_mask);
    } else {
      aom_comp_avg_pred(comp_pred, second_pred, w, h, y, y_stride);
    }
    besterr = vfp->vf(comp_pred, w, src, src_stride, sse1);
#endif
  } else {
    besterr = vfp->vf(y, y_stride, src, src_stride, sse1);
  }
  *distortion = besterr;
  besterr += mv_err_cost_(bestmv, mv_cost_params);
  return besterr;
}

static INLINE int divide_and_round(int n, int d) {
  return ((n < 0) ^ (d < 0)) ? ((n - d / 2) / d) : ((n + d / 2) / d);
}

static INLINE int is_cost_list_wellbehaved(const int *cost_list) {
  return cost_list[0] < cost_list[1] && cost_list[0] < cost_list[2] &&
         cost_list[0] < cost_list[3] && cost_list[0] < cost_list[4];
}

// Returns surface minima estimate at given precision in 1/2^n bits.
// Assume a model for the cost surface: S = A(x - x0)^2 + B(y - y0)^2 + C
// For a given set of costs S0, S1, S2, S3, S4 at points
// (y, x) = (0, 0), (0, -1), (1, 0), (0, 1) and (-1, 0) respectively,
// the solution for the location of the minima (x0, y0) is given by:
// x0 = 1/2 (S1 - S3)/(S1 + S3 - 2*S0),
// y0 = 1/2 (S4 - S2)/(S4 + S2 - 2*S0).
// The code below is an integerized version of that.
static AOM_INLINE void get_cost_surf_min(const int *cost_list, int *ir, int *ic,
                                         int bits) {
  *ic = divide_and_round((cost_list[1] - cost_list[3]) * (1 << (bits - 1)),
                         (cost_list[1] - 2 * cost_list[0] + cost_list[3]));
  *ir = divide_and_round((cost_list[4] - cost_list[2]) * (1 << (bits - 1)),
                         (cost_list[4] - 2 * cost_list[0] + cost_list[2]));
}

int av1_find_best_sub_pixel_tree_pruned_evenmore(
    MACROBLOCK *x, const AV1_COMMON *const cm,
    const SUBPEL_MOTION_SEARCH_PARAMS *ms_params, int *distortion,
    unsigned int *sse1) {
  const int allow_hp = ms_params->allow_hp;
  const int forced_stop = ms_params->forced_stop;
  const int iters_per_step = ms_params->iters_per_step;
  const int do_reset_fractional_mv = ms_params->do_reset_fractional_mv;
  const int *cost_list = ms_params->cost_list;
  const MV_COST_PARAMS *mv_cost_params = &ms_params->mv_cost_params;
  const SUBPEL_SEARCH_VAR_PARAMS *var_params = &ms_params->var_params;
  const MV *ref_mv = mv_cost_params->ref_mv;
  const SUBPEL_SEARCH_TYPE subpel_search_type =
      ms_params->var_params.subpel_search_type;

  const uint8_t *const src_address = x->plane[0].src.buf;
  const int src_stride = x->plane[0].src.stride;
  const MACROBLOCKD *xd = &x->e_mbd;
  unsigned int besterr = INT_MAX;
  const unsigned int halfiters = iters_per_step;
  const unsigned int quarteriters = iters_per_step;
  const unsigned int eighthiters = iters_per_step;
  const uint8_t *const y = xd->plane[0].pre[0].buf;
  const int y_stride = xd->plane[0].pre[0].stride;

  convert_fullmv_to_mv(&x->best_mv);
  MV *bestmv = &x->best_mv.as_mv;
  MV start_mv = *bestmv;

  int hstep = 4;

  SubpelMvLimits mv_limits;
  av1_set_subpel_mv_search_range(&mv_limits, &x->mv_limits,
                                 mv_cost_params->ref_mv);

  besterr = setup_center_error(xd, bestmv, src_address, src_stride, y, y_stride,
                               var_params, mv_cost_params, sse1, distortion);
  (void)halfiters;
  (void)quarteriters;
  (void)eighthiters;
  (void)allow_hp;
  (void)forced_stop;
  (void)hstep;
  (void)cm;
  (void)do_reset_fractional_mv;
  (void)ref_mv;
  (void)subpel_search_type;

  if (cost_list && cost_list[0] != INT_MAX && cost_list[1] != INT_MAX &&
      cost_list[2] != INT_MAX && cost_list[3] != INT_MAX &&
      cost_list[4] != INT_MAX && is_cost_list_wellbehaved(cost_list)) {
    int ir, ic;
    get_cost_surf_min(cost_list, &ir, &ic, 2);
    if (ir != 0 || ic != 0) {
      const MV this_mv = { start_mv.row + 2 * ir, start_mv.col + 2 * ic };
      check_better_fast(&this_mv, bestmv, &mv_limits, src_address, src_stride,
                        y, y_stride, var_params, mv_cost_params, &besterr, sse1,
                        distortion);
    }
  } else {
    two_level_checks_fast(&start_mv, bestmv, hstep, &mv_limits, src_address,
                          src_stride, y, y_stride, var_params, mv_cost_params,
                          &besterr, sse1, distortion, halfiters);

    // Each subsequent iteration checks at least one point in common with
    // the last iteration could be 2 ( if diag selected) 1/4 pel
    if (forced_stop != HALF_PEL) {
      hstep >>= 1;
      start_mv = *bestmv;
      two_level_checks_fast(&start_mv, bestmv, hstep, &mv_limits, src_address,
                            src_stride, y, y_stride, var_params, mv_cost_params,
                            &besterr, sse1, distortion, quarteriters);
    }
  }

  if (allow_hp && forced_stop == EIGHTH_PEL) {
    hstep >>= 1;
    start_mv = *bestmv;
    two_level_checks_fast(&start_mv, bestmv, hstep, &mv_limits, src_address,
                          src_stride, y, y_stride, var_params, mv_cost_params,
                          &besterr, sse1, distortion, eighthiters);
  }

  return besterr;
}

int av1_find_best_sub_pixel_tree_pruned_more(
    MACROBLOCK *x, const AV1_COMMON *const cm,
    const SUBPEL_MOTION_SEARCH_PARAMS *ms_params, int *distortion,
    unsigned int *sse1) {
  const int allow_hp = ms_params->allow_hp;
  const int forced_stop = ms_params->forced_stop;
  const int iters_per_step = ms_params->iters_per_step;
  const int *cost_list = ms_params->cost_list;
  const MV_COST_PARAMS *mv_cost_params = &ms_params->mv_cost_params;
  const SUBPEL_SEARCH_VAR_PARAMS *var_params = &ms_params->var_params;

  const uint8_t *const src_address = x->plane[0].src.buf;
  const int src_stride = x->plane[0].src.stride;
  const MACROBLOCKD *xd = &x->e_mbd;
  unsigned int besterr = INT_MAX;
  const unsigned int halfiters = iters_per_step;
  const unsigned int quarteriters = iters_per_step;
  const unsigned int eighthiters = iters_per_step;
  const uint8_t *const y = xd->plane[0].pre[0].buf;
  const int y_stride = xd->plane[0].pre[0].stride;

  convert_fullmv_to_mv(&x->best_mv);
  MV *bestmv = &x->best_mv.as_mv;
  MV start_mv = *bestmv;

  int hstep = 4;

  SubpelMvLimits mv_limits;
  av1_set_subpel_mv_search_range(&mv_limits, &x->mv_limits,
                                 mv_cost_params->ref_mv);

  (void)cm;

  besterr = setup_center_error(xd, bestmv, src_address, src_stride, y, y_stride,
                               var_params, mv_cost_params, sse1, distortion);
  if (cost_list && cost_list[0] != INT_MAX && cost_list[1] != INT_MAX &&
      cost_list[2] != INT_MAX && cost_list[3] != INT_MAX &&
      cost_list[4] != INT_MAX && is_cost_list_wellbehaved(cost_list)) {
    int ir, ic;
    get_cost_surf_min(cost_list, &ir, &ic, 1);
    if (ir != 0 || ic != 0) {
      const MV this_mv = { start_mv.row + ir * hstep,
                           start_mv.col + ic * hstep };
      check_better_fast(&this_mv, bestmv, &mv_limits, src_address, src_stride,
                        y, y_stride, var_params, mv_cost_params, &besterr, sse1,
                        distortion);
    }
  } else {
    two_level_checks_fast(&start_mv, bestmv, hstep, &mv_limits, src_address,
                          src_stride, y, y_stride, var_params, mv_cost_params,
                          &besterr, sse1, distortion, halfiters);
  }

  // Each subsequent iteration checks at least one point in common with
  // the last iteration could be 2 ( if diag selected) 1/4 pel
  if (forced_stop != HALF_PEL) {
    hstep >>= 1;
    start_mv = *bestmv;
    two_level_checks_fast(&start_mv, bestmv, hstep, &mv_limits, src_address,
                          src_stride, y, y_stride, var_params, mv_cost_params,
                          &besterr, sse1, distortion, quarteriters);
  }

  if (allow_hp && forced_stop == EIGHTH_PEL) {
    hstep >>= 1;
    start_mv = *bestmv;
    two_level_checks_fast(&start_mv, bestmv, hstep, &mv_limits, src_address,
                          src_stride, y, y_stride, var_params, mv_cost_params,
                          &besterr, sse1, distortion, eighthiters);
  }

  return besterr;
}

int av1_find_best_sub_pixel_tree_pruned(
    MACROBLOCK *x, const AV1_COMMON *const cm,
    const SUBPEL_MOTION_SEARCH_PARAMS *ms_params, int *distortion,
    unsigned int *sse1) {
  const int allow_hp = ms_params->allow_hp;
  const int forced_stop = ms_params->forced_stop;
  const int iters_per_step = ms_params->iters_per_step;
  const int *cost_list = ms_params->cost_list;
  const MV_COST_PARAMS *mv_cost_params = &ms_params->mv_cost_params;
  const SUBPEL_SEARCH_VAR_PARAMS *var_params = &ms_params->var_params;

  const uint8_t *const src_address = x->plane[0].src.buf;
  const int src_stride = x->plane[0].src.stride;
  const MACROBLOCKD *xd = &x->e_mbd;
  unsigned int besterr = INT_MAX;
  const unsigned int halfiters = iters_per_step;
  const unsigned int quarteriters = iters_per_step;
  const unsigned int eighthiters = iters_per_step;
  const uint8_t *const y = xd->plane[0].pre[0].buf;
  const int y_stride = xd->plane[0].pre[0].stride;

  convert_fullmv_to_mv(&x->best_mv);
  MV *bestmv = &x->best_mv.as_mv;
  MV start_mv = *bestmv;

  int hstep = 4;

  SubpelMvLimits mv_limits;
  av1_set_subpel_mv_search_range(&mv_limits, &x->mv_limits,
                                 mv_cost_params->ref_mv);
  (void)cm;

  besterr = setup_center_error(xd, bestmv, src_address, src_stride, y, y_stride,
                               var_params, mv_cost_params, sse1, distortion);
  if (cost_list && cost_list[0] != INT_MAX && cost_list[1] != INT_MAX &&
      cost_list[2] != INT_MAX && cost_list[3] != INT_MAX &&
      cost_list[4] != INT_MAX) {
    const unsigned int whichdir = (cost_list[1] < cost_list[3] ? 0 : 1) +
                                  (cost_list[2] < cost_list[4] ? 0 : 2);

    const MV left_mv = { start_mv.row, start_mv.col - hstep };
    const MV right_mv = { start_mv.row, start_mv.col + hstep };
    const MV bottom_mv = { start_mv.row + hstep, start_mv.col };
    const MV top_mv = { start_mv.row - hstep, start_mv.col };

    const MV bottom_left_mv = { start_mv.row + hstep, start_mv.col - hstep };
    const MV bottom_right_mv = { start_mv.row + hstep, start_mv.col + hstep };
    const MV top_left_mv = { start_mv.row - hstep, start_mv.col - hstep };
    const MV top_right_mv = { start_mv.row - hstep, start_mv.col + hstep };

    switch (whichdir) {
      case 0:  // bottom left quadrant
        check_better_fast(&left_mv, bestmv, &mv_limits, src_address, src_stride,
                          y, y_stride, var_params, mv_cost_params, &besterr,
                          sse1, distortion);
        check_better_fast(&bottom_mv, bestmv, &mv_limits, src_address,
                          src_stride, y, y_stride, var_params, mv_cost_params,
                          &besterr, sse1, distortion);
        check_better_fast(&bottom_left_mv, bestmv, &mv_limits, src_address,
                          src_stride, y, y_stride, var_params, mv_cost_params,
                          &besterr, sse1, distortion);
        break;
      case 1:  // bottom right quadrant
        check_better_fast(&right_mv, bestmv, &mv_limits, src_address,
                          src_stride, y, y_stride, var_params, mv_cost_params,
                          &besterr, sse1, distortion);
        check_better_fast(&bottom_mv, bestmv, &mv_limits, src_address,
                          src_stride, y, y_stride, var_params, mv_cost_params,
                          &besterr, sse1, distortion);
        check_better_fast(&bottom_right_mv, bestmv, &mv_limits, src_address,
                          src_stride, y, y_stride, var_params, mv_cost_params,
                          &besterr, sse1, distortion);
        break;
      case 2:  // top left quadrant
        check_better_fast(&left_mv, bestmv, &mv_limits, src_address, src_stride,
                          y, y_stride, var_params, mv_cost_params, &besterr,
                          sse1, distortion);
        check_better_fast(&top_mv, bestmv, &mv_limits, src_address, src_stride,
                          y, y_stride, var_params, mv_cost_params, &besterr,
                          sse1, distortion);
        check_better_fast(&top_left_mv, bestmv, &mv_limits, src_address,
                          src_stride, y, y_stride, var_params, mv_cost_params,
                          &besterr, sse1, distortion);
        break;
      case 3:  // top right quadrant
        check_better_fast(&right_mv, bestmv, &mv_limits, src_address,
                          src_stride, y, y_stride, var_params, mv_cost_params,
                          &besterr, sse1, distortion);
        check_better_fast(&top_mv, bestmv, &mv_limits, src_address, src_stride,
                          y, y_stride, var_params, mv_cost_params, &besterr,
                          sse1, distortion);
        check_better_fast(&top_right_mv, bestmv, &mv_limits, src_address,
                          src_stride, y, y_stride, var_params, mv_cost_params,
                          &besterr, sse1, distortion);
        break;
    }
  } else {
    two_level_checks_fast(&start_mv, bestmv, hstep, &mv_limits, src_address,
                          src_stride, y, y_stride, var_params, mv_cost_params,
                          &besterr, sse1, distortion, halfiters);
  }

  // Each subsequent iteration checks at least one point in common with
  // the last iteration could be 2 ( if diag selected) 1/4 pel
  if (forced_stop != HALF_PEL) {
    hstep >>= 1;
    start_mv = *bestmv;
    two_level_checks_fast(&start_mv, bestmv, hstep, &mv_limits, src_address,
                          src_stride, y, y_stride, var_params, mv_cost_params,
                          &besterr, sse1, distortion, quarteriters);
  }

  if (allow_hp && forced_stop == EIGHTH_PEL) {
    hstep >>= 1;
    start_mv = *bestmv;
    two_level_checks_fast(&start_mv, bestmv, hstep, &mv_limits, src_address,
                          src_stride, y, y_stride, var_params, mv_cost_params,
                          &besterr, sse1, distortion, eighthiters);
  }

  return besterr;
}

/* clang-format off */
static const MV search_step_table[12] = {
  // left, right, up, down
  { 0, -4 }, { 0, 4 }, { -4, 0 }, { 4, 0 },
  { 0, -2 }, { 0, 2 }, { -2, 0 }, { 2, 0 },
  { 0, -1 }, { 0, 1 }, { -1, 0 }, { 1, 0 }
};
/* clang-format on */

static unsigned int upsampled_setup_center_error(
    MACROBLOCKD *xd, const AV1_COMMON *const cm, const MV *bestmv,
    const uint8_t *const src, const int src_stride, const uint8_t *const y,
    int y_stride, const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *sse1, int *distortion) {
  unsigned int besterr = upsampled_pref_error(xd, cm, bestmv, src, src_stride,
                                              y, y_stride, var_params, sse1);
  *distortion = besterr;
  besterr += mv_err_cost_(bestmv, mv_cost_params);
  return besterr;
}

static AOM_FORCE_INLINE void second_level_check_v2(
    MACROBLOCKD *xd, const AV1_COMMON *const cm, const MV *diag_mv, MV *best_mv,
    int kr, int kc, const SubpelMvLimits *mv_limits, const uint8_t *const src,
    const int src_stride, const uint8_t *const ref, int ref_stride,
    const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *besterr,
    unsigned int *sse1, int *distortion) {
  const MV center_mv = *best_mv;

  assert(diag_mv->row == best_mv->row || diag_mv->col == best_mv->col);
  if (best_mv->row == diag_mv->row && best_mv->col != diag_mv->col) {
    kc = best_mv->col - diag_mv->col;
  } else if (best_mv->row != diag_mv->row && best_mv->col == diag_mv->col) {
    kr = best_mv->row - diag_mv->row;
  }

  const MV row_bias_mv = { center_mv.row + kr, center_mv.col };
  const MV col_bias_mv = { center_mv.row, center_mv.col + kc };
  const MV diag_bias_mv = { center_mv.row + kr, center_mv.col + kc };
  int has_better_mv = 0;

  check_better(xd, cm, &row_bias_mv, best_mv, mv_limits, src, src_stride, ref,
               ref_stride, var_params, mv_cost_params, besterr, sse1,
               distortion, &has_better_mv);
  check_better(xd, cm, &col_bias_mv, best_mv, mv_limits, src, src_stride, ref,
               ref_stride, var_params, mv_cost_params, besterr, sse1,
               distortion, &has_better_mv);

  // Do an additional search if the second iteration gives a better mv
  if (has_better_mv) {
    int dummy = 0;
    check_better(xd, cm, &diag_bias_mv, best_mv, mv_limits, src, src_stride,
                 ref, ref_stride, var_params, mv_cost_params, besterr, sse1,
                 distortion, &dummy);
  }
}

int av1_find_best_sub_pixel_tree(MACROBLOCK *x, const AV1_COMMON *const cm,
                                 const SUBPEL_MOTION_SEARCH_PARAMS *ms_params,
                                 int *distortion, unsigned int *sse1) {
  const int allow_hp = ms_params->allow_hp;
  const int forced_stop = ms_params->forced_stop;
  const int iters_per_step = ms_params->iters_per_step;
  const int do_reset_fractional_mv = ms_params->do_reset_fractional_mv;
  const int *cost_list = ms_params->cost_list;
  const MV_COST_PARAMS *mv_cost_params = &ms_params->mv_cost_params;
  const SUBPEL_SEARCH_VAR_PARAMS *var_params = &ms_params->var_params;
  const MV *ref_mv = mv_cost_params->ref_mv;
  const SUBPEL_SEARCH_TYPE subpel_search_type =
      ms_params->var_params.subpel_search_type;

  const uint8_t *const src_address = x->plane[0].src.buf;
  const int src_stride = x->plane[0].src.stride;
  MACROBLOCKD *xd = &x->e_mbd;
  unsigned int besterr = INT_MAX;
  const int y_stride = xd->plane[0].pre[0].stride;

  const uint8_t *const y = xd->plane[0].pre[0].buf;
  convert_fullmv_to_mv(&x->best_mv);
  MV *bestmv = &x->best_mv.as_mv;

  int hstep = 4;
  int iter, round = FULL_PEL - forced_stop;
  const MV *search_step = search_step_table;
  int best_idx = -1;
  unsigned int cost_array[5];
  int kr, kc;
  SubpelMvLimits mv_limits;

  av1_set_subpel_mv_search_range(&mv_limits, &x->mv_limits, ref_mv);

  if (!allow_hp)
    if (round == 3) round = 2;

  if (subpel_search_type != USE_2_TAPS_ORIG)
    besterr = upsampled_setup_center_error(xd, cm, bestmv, src_address,
                                           src_stride, y, y_stride, var_params,
                                           mv_cost_params, sse1, distortion);
  else
    besterr =
        setup_center_error(xd, bestmv, src_address, src_stride, y, y_stride,
                           var_params, mv_cost_params, sse1, distortion);

  (void)cost_list;  // to silence compiler warning

  if (do_reset_fractional_mv) {
    av1_set_fractional_mv(x->fractional_best_mv);
  }

  MV iter_center_mv = *bestmv;
  for (iter = 0; iter < round; ++iter) {
    if (x->fractional_best_mv[iter].as_mv.row == iter_center_mv.row &&
        x->fractional_best_mv[iter].as_mv.col == iter_center_mv.col)
      return INT_MAX;

    x->fractional_best_mv[iter].as_mv = iter_center_mv;

    MV best_iter_mv = { INT16_MAX, INT16_MAX };

    // Check vertical and horizontal sub-pixel positions.
    for (int idx = 0; idx < 4; ++idx) {
      const MV this_mv = { iter_center_mv.row + search_step[idx].row,
                           iter_center_mv.col + search_step[idx].col };

      int is_better = 0;
      cost_array[idx] =
          check_better(xd, cm, &this_mv, &best_iter_mv, &mv_limits, src_address,
                       src_stride, y, y_stride, var_params, mv_cost_params,
                       &besterr, sse1, distortion, &is_better);
      if (is_better) {
        best_idx = idx;
      }
    }

    // Check diagonal sub-pixel position
    kc = (cost_array[0] <= cost_array[1] ? -hstep : hstep);
    kr = (cost_array[2] <= cost_array[3] ? -hstep : hstep);

    const MV diag_mv = { iter_center_mv.row + kr, iter_center_mv.col + kc };
    int is_better = 0;

    cost_array[4] =
        check_better(xd, cm, &diag_mv, &best_iter_mv, &mv_limits, src_address,
                     src_stride, y, y_stride, var_params, mv_cost_params,
                     &besterr, sse1, distortion, &is_better);
    if (is_better) {
      best_idx = 4;
    }

    if (best_idx != -1) {
      iter_center_mv = best_iter_mv;

      if (iters_per_step > 1) {
        second_level_check_v2(xd, cm, &diag_mv, &iter_center_mv, kr, kc,
                              &mv_limits, src_address, src_stride, y, y_stride,
                              var_params, mv_cost_params, &besterr, sse1,
                              distortion);
      }
    }

    search_step += 4;
    hstep >>= 1;
    best_idx = -1;
  }

  *bestmv = iter_center_mv;

  return besterr;
}

#undef PRE
#undef CHECK_BETTER

unsigned int av1_compute_motion_cost(const AV1_COMP *cpi, MACROBLOCK *const x,
                                     BLOCK_SIZE bsize, const MV *this_mv) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  const uint8_t *const src = x->plane[0].src.buf;
  const int src_stride = x->plane[0].src.stride;
  uint8_t *const dst = xd->plane[0].dst.buf;
  const int dst_stride = xd->plane[0].dst.stride;
  const aom_variance_fn_ptr_t *vfp = &cpi->fn_ptr[bsize];
  const int_mv ref_mv = av1_get_ref_mv(x, 0);
  unsigned int mse;
  unsigned int sse;
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  const MV_COST_TYPE mv_cost_type = x->mv_cost_type;

  av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, NULL, bsize,
                                AOM_PLANE_Y, AOM_PLANE_Y);
  mse = vfp->vf(dst, dst_stride, src, src_stride, &sse);
  mse += mv_err_cost(this_mv, &ref_mv.as_mv, x->nmv_vec_cost,
                     CONVERT_TO_CONST_MVCOST(x->mv_cost_stack), x->errorperbit,
                     mv_cost_type);
  return mse;
}

// Refine MV in a small range
unsigned int av1_refine_warped_mv(const AV1_COMP *cpi, MACROBLOCK *const x,
                                  BLOCK_SIZE bsize, int *pts0, int *pts_inref0,
                                  int total_samples) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const MV neighbors[8] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 },
                            { 0, -2 }, { 2, 0 }, { 0, 2 }, { -2, 0 } };
  const int_mv ref_mv = av1_get_ref_mv(x, 0);
  int16_t br = mbmi->mv[0].as_mv.row;
  int16_t bc = mbmi->mv[0].as_mv.col;
  int16_t *tr = &mbmi->mv[0].as_mv.row;
  int16_t *tc = &mbmi->mv[0].as_mv.col;
  WarpedMotionParams best_wm_params = mbmi->wm_params;
  int best_num_proj_ref = mbmi->num_proj_ref;
  unsigned int bestmse;
  SubpelMvLimits mv_limits;

  const int start = cm->allow_high_precision_mv ? 0 : 4;
  int ite;

  av1_set_subpel_mv_search_range(&mv_limits, &x->mv_limits, &ref_mv.as_mv);

  // Calculate the center position's error
  assert(av1_is_subpelmv_in_range(&mv_limits, mbmi->mv[0].as_mv));
  bestmse = av1_compute_motion_cost(cpi, x, bsize, &mbmi->mv[0].as_mv);

  // MV search
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  for (ite = 0; ite < 2; ++ite) {
    int best_idx = -1;
    int idx;

    for (idx = start; idx < start + 4; ++idx) {
      unsigned int thismse;

      *tr = br + neighbors[idx].row;
      *tc = bc + neighbors[idx].col;

      MV this_mv = { *tr, *tc };
      if (av1_is_subpelmv_in_range(&mv_limits, this_mv)) {
        int pts[SAMPLES_ARRAY_SIZE], pts_inref[SAMPLES_ARRAY_SIZE];

        memcpy(pts, pts0, total_samples * 2 * sizeof(*pts0));
        memcpy(pts_inref, pts_inref0, total_samples * 2 * sizeof(*pts_inref0));
        if (total_samples > 1)
          mbmi->num_proj_ref =
              av1_selectSamples(&this_mv, pts, pts_inref, total_samples, bsize);

        if (!av1_find_projection(mbmi->num_proj_ref, pts, pts_inref, bsize, *tr,
                                 *tc, &mbmi->wm_params, mi_row, mi_col)) {
          thismse = av1_compute_motion_cost(cpi, x, bsize, &this_mv);

          if (thismse < bestmse) {
            best_idx = idx;
            best_wm_params = mbmi->wm_params;
            best_num_proj_ref = mbmi->num_proj_ref;
            bestmse = thismse;
          }
        }
      }
    }

    if (best_idx == -1) break;

    if (best_idx >= 0) {
      br += neighbors[best_idx].row;
      bc += neighbors[best_idx].col;
    }
  }

  *tr = br;
  *tc = bc;
  mbmi->wm_params = best_wm_params;
  mbmi->num_proj_ref = best_num_proj_ref;
  return bestmse;
}

static INLINE int check_bounds(const FullMvLimits *mv_limits, int row, int col,
                               int range) {
  return ((row - range) >= mv_limits->row_min) &
         ((row + range) <= mv_limits->row_max) &
         ((col - range) >= mv_limits->col_min) &
         ((col + range) <= mv_limits->col_max);
}

#define CHECK_BETTER                                                       \
  {                                                                        \
    if (thissad < bestsad) {                                               \
      if (use_mvcost)                                                      \
        thissad += mvsad_err_cost(x, &this_mv, &full_ref_mv, sad_per_bit); \
      if (thissad < bestsad) {                                             \
        bestsad = thissad;                                                 \
        best_site = i;                                                     \
      }                                                                    \
    }                                                                      \
  }

#define MAX_PATTERN_SCALES 11
#define MAX_PATTERN_CANDIDATES 8  // max number of candidates per scale
#define PATTERN_CANDIDATES_REF 3  // number of refinement candidates

// Calculate and return a sad+mvcost list around an integer best pel.
static INLINE void calc_int_cost_list(const MACROBLOCK *x,
                                      const MV *const ref_mv, int sadpb,
                                      const aom_variance_fn_ptr_t *fn_ptr,
                                      const FULLPEL_MV *best_mv,
                                      int *cost_list) {
  static const MV neighbors[4] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &x->e_mbd.plane[0].pre[0];
  const FULLPEL_MV full_ref_mv = get_fullmv_from_mv(ref_mv);
  const int br = best_mv->row;
  const int bc = best_mv->col;
  const MV_COST_TYPE mv_cost_type = x->mv_cost_type;
  unsigned int sse;

  cost_list[0] =
      fn_ptr->vf(what->buf, what->stride, get_buf_from_mv(in_what, best_mv),
                 in_what->stride, &sse) +
      mvsad_err_cost(x, best_mv, &full_ref_mv, sadpb);
  if (check_bounds(&x->mv_limits, br, bc, 1)) {
    for (int i = 0; i < 4; i++) {
      const FULLPEL_MV neighbor_mv = { br + neighbors[i].row,
                                       bc + neighbors[i].col };
      const MV sub_neighbor_mv = get_mv_from_fullmv(&neighbor_mv);
      cost_list[i + 1] = fn_ptr->vf(what->buf, what->stride,
                                    get_buf_from_mv(in_what, &neighbor_mv),
                                    in_what->stride, &sse) +
                         mv_err_cost(&sub_neighbor_mv, ref_mv, x->nmv_vec_cost,
                                     CONVERT_TO_CONST_MVCOST(x->mv_cost_stack),
                                     x->errorperbit, mv_cost_type);
    }
  } else {
    for (int i = 0; i < 4; i++) {
      const FULLPEL_MV neighbor_mv = { br + neighbors[i].row,
                                       bc + neighbors[i].col };
      if (!av1_is_fullmv_in_range(&x->mv_limits, neighbor_mv)) {
        cost_list[i + 1] = INT_MAX;
      } else {
        const MV sub_neighbor_mv = get_mv_from_fullmv(&neighbor_mv);
        cost_list[i + 1] =
            fn_ptr->vf(what->buf, what->stride,
                       get_buf_from_mv(in_what, &neighbor_mv), in_what->stride,
                       &sse) +
            mv_err_cost(&sub_neighbor_mv, ref_mv, x->nmv_vec_cost,
                        CONVERT_TO_CONST_MVCOST(x->mv_cost_stack),
                        x->errorperbit, mv_cost_type);
      }
    }
  }
}

static INLINE void calc_int_sad_list(const MACROBLOCK *x,
                                     const MV *const ref_mv, int sadpb,
                                     const aom_variance_fn_ptr_t *fn_ptr,
                                     const FULLPEL_MV *best_mv, int *cost_list,
                                     const int bestsad) {
  static const MV neighbors[4] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &x->e_mbd.plane[0].pre[0];
  const int br = best_mv->row;
  const int bc = best_mv->col;
  const FULLPEL_MV full_ref_mv = get_fullmv_from_mv(ref_mv);

  if (cost_list[0] == INT_MAX) {
    cost_list[0] = bestsad;
    if (check_bounds(&x->mv_limits, br, bc, 1)) {
      for (int i = 0; i < 4; i++) {
        const FULLPEL_MV this_mv = { br + neighbors[i].row,
                                     bc + neighbors[i].col };
        cost_list[i + 1] =
            fn_ptr->sdf(what->buf, what->stride,
                        get_buf_from_mv(in_what, &this_mv), in_what->stride);
      }
    } else {
      for (int i = 0; i < 4; i++) {
        const FULLPEL_MV this_mv = { br + neighbors[i].row,
                                     bc + neighbors[i].col };
        if (!av1_is_fullmv_in_range(&x->mv_limits, this_mv))
          cost_list[i + 1] = INT_MAX;
        else
          cost_list[i + 1] =
              fn_ptr->sdf(what->buf, what->stride,
                          get_buf_from_mv(in_what, &this_mv), in_what->stride);
      }
    }
  } else {
    if (x->mv_cost_type != MV_COST_NONE) {
      for (int i = 0; i < 4; i++) {
        const FULLPEL_MV this_mv = { br + neighbors[i].row,
                                     bc + neighbors[i].col };
        if (cost_list[i + 1] != INT_MAX) {
          cost_list[i + 1] += mvsad_err_cost(x, &this_mv, &full_ref_mv, sadpb);
        }
      }
    }
  }
}

// Generic pattern search function that searches over multiple scales.
// Each scale can have a different number of candidates and shape of
// candidates as indicated in the num_candidates and candidates arrays
// passed into this function
static int pattern_search(
    MACROBLOCK *x, FULLPEL_MV *start_mv, int search_param, int sad_per_bit,
    int do_init_search, int *cost_list, const aom_variance_fn_ptr_t *vfp,
    const MV *ref_mv, const int num_candidates[MAX_PATTERN_SCALES],
    const MV candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES]) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  static const int search_param_to_steps[MAX_MVSEARCH_STEPS] = {
    10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
  };
  int i, s, t;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const int last_is_4 = num_candidates[0] == 4;
  int br, bc;
  int bestsad = INT_MAX;
  int thissad;
  int k = -1;
  const FULLPEL_MV full_ref_mv = get_fullmv_from_mv(ref_mv);
  const int use_mvcost = x->mv_cost_type != MV_COST_NONE;
  assert(search_param < MAX_MVSEARCH_STEPS);
  int best_init_s = search_param_to_steps[search_param];
  // adjust ref_mv to make sure it is within MV range
  clamp_fullmv(start_mv, &x->mv_limits);
  br = start_mv->row;
  bc = start_mv->col;
  if (cost_list != NULL) {
    cost_list[0] = cost_list[1] = cost_list[2] = cost_list[3] = cost_list[4] =
        INT_MAX;
  }

  // Work out the start point for the search
  bestsad = vfp->sdf(what->buf, what->stride,
                     get_buf_from_mv(in_what, start_mv), in_what->stride) +
            mvsad_err_cost(x, start_mv, &full_ref_mv, sad_per_bit);

  // Search all possible scales up to the search param around the center point
  // pick the scale of the point that is best as the starting scale of
  // further steps around it.
  if (do_init_search) {
    s = best_init_s;
    best_init_s = -1;
    for (t = 0; t <= s; ++t) {
      int best_site = -1;
      if (check_bounds(&x->mv_limits, br, bc, 1 << t)) {
        for (i = 0; i < num_candidates[t]; i++) {
          const FULLPEL_MV this_mv = { br + candidates[t][i].row,
                                       bc + candidates[t][i].col };
          thissad =
              vfp->sdf(what->buf, what->stride,
                       get_buf_from_mv(in_what, &this_mv), in_what->stride);
          CHECK_BETTER
        }
      } else {
        for (i = 0; i < num_candidates[t]; i++) {
          const FULLPEL_MV this_mv = { br + candidates[t][i].row,
                                       bc + candidates[t][i].col };
          if (!av1_is_fullmv_in_range(&x->mv_limits, this_mv)) continue;
          thissad =
              vfp->sdf(what->buf, what->stride,
                       get_buf_from_mv(in_what, &this_mv), in_what->stride);
          CHECK_BETTER
        }
      }
      if (best_site == -1) {
        continue;
      } else {
        best_init_s = t;
        k = best_site;
      }
    }
    if (best_init_s != -1) {
      br += candidates[best_init_s][k].row;
      bc += candidates[best_init_s][k].col;
    }
  }

  // If the center point is still the best, just skip this and move to
  // the refinement step.
  if (best_init_s != -1) {
    const int last_s = (last_is_4 && cost_list != NULL);
    int best_site = -1;
    s = best_init_s;

    for (; s >= last_s; s--) {
      // No need to search all points the 1st time if initial search was used
      if (!do_init_search || s != best_init_s) {
        if (check_bounds(&x->mv_limits, br, bc, 1 << s)) {
          for (i = 0; i < num_candidates[s]; i++) {
            const FULLPEL_MV this_mv = { br + candidates[s][i].row,
                                         bc + candidates[s][i].col };
            thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < num_candidates[s]; i++) {
            const FULLPEL_MV this_mv = { br + candidates[s][i].row,
                                         bc + candidates[s][i].col };
            if (!av1_is_fullmv_in_range(&x->mv_limits, this_mv)) continue;
            thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        }

        if (best_site == -1) {
          continue;
        } else {
          br += candidates[s][best_site].row;
          bc += candidates[s][best_site].col;
          k = best_site;
        }
      }

      do {
        int next_chkpts_indices[PATTERN_CANDIDATES_REF];
        best_site = -1;
        next_chkpts_indices[0] = (k == 0) ? num_candidates[s] - 1 : k - 1;
        next_chkpts_indices[1] = k;
        next_chkpts_indices[2] = (k == num_candidates[s] - 1) ? 0 : k + 1;

        if (check_bounds(&x->mv_limits, br, bc, 1 << s)) {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const FULLPEL_MV this_mv = {
              br + candidates[s][next_chkpts_indices[i]].row,
              bc + candidates[s][next_chkpts_indices[i]].col
            };
            thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const FULLPEL_MV this_mv = {
              br + candidates[s][next_chkpts_indices[i]].row,
              bc + candidates[s][next_chkpts_indices[i]].col
            };
            if (!av1_is_fullmv_in_range(&x->mv_limits, this_mv)) continue;
            thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        }

        if (best_site != -1) {
          k = next_chkpts_indices[best_site];
          br += candidates[s][k].row;
          bc += candidates[s][k].col;
        }
      } while (best_site != -1);
    }

    // Note: If we enter the if below, then cost_list must be non-NULL.
    if (s == 0) {
      cost_list[0] = bestsad;
      if (!do_init_search || s != best_init_s) {
        if (check_bounds(&x->mv_limits, br, bc, 1 << s)) {
          for (i = 0; i < num_candidates[s]; i++) {
            const FULLPEL_MV this_mv = { br + candidates[s][i].row,
                                         bc + candidates[s][i].col };
            cost_list[i + 1] = thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < num_candidates[s]; i++) {
            const FULLPEL_MV this_mv = { br + candidates[s][i].row,
                                         bc + candidates[s][i].col };
            if (!av1_is_fullmv_in_range(&x->mv_limits, this_mv)) continue;
            cost_list[i + 1] = thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        }

        if (best_site != -1) {
          br += candidates[s][best_site].row;
          bc += candidates[s][best_site].col;
          k = best_site;
        }
      }
      while (best_site != -1) {
        int next_chkpts_indices[PATTERN_CANDIDATES_REF];
        best_site = -1;
        next_chkpts_indices[0] = (k == 0) ? num_candidates[s] - 1 : k - 1;
        next_chkpts_indices[1] = k;
        next_chkpts_indices[2] = (k == num_candidates[s] - 1) ? 0 : k + 1;
        cost_list[1] = cost_list[2] = cost_list[3] = cost_list[4] = INT_MAX;
        cost_list[((k + 2) % 4) + 1] = cost_list[0];
        cost_list[0] = bestsad;

        if (check_bounds(&x->mv_limits, br, bc, 1 << s)) {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const FULLPEL_MV this_mv = {
              br + candidates[s][next_chkpts_indices[i]].row,
              bc + candidates[s][next_chkpts_indices[i]].col
            };
            cost_list[next_chkpts_indices[i] + 1] = thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        } else {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const FULLPEL_MV this_mv = {
              br + candidates[s][next_chkpts_indices[i]].row,
              bc + candidates[s][next_chkpts_indices[i]].col
            };
            if (!av1_is_fullmv_in_range(&x->mv_limits, this_mv)) {
              cost_list[next_chkpts_indices[i] + 1] = INT_MAX;
              continue;
            }
            cost_list[next_chkpts_indices[i] + 1] = thissad =
                vfp->sdf(what->buf, what->stride,
                         get_buf_from_mv(in_what, &this_mv), in_what->stride);
            CHECK_BETTER
          }
        }

        if (best_site != -1) {
          k = next_chkpts_indices[best_site];
          br += candidates[s][k].row;
          bc += candidates[s][k].col;
        }
      }
    }
  }

  // Returns the one-away integer pel cost/sad around the best as follows:
  // cost_list[0]: cost/sad at the best integer pel
  // cost_list[1]: cost/sad at delta {0, -1} (left)   from the best integer pel
  // cost_list[2]: cost/sad at delta { 1, 0} (bottom) from the best integer pel
  // cost_list[3]: cost/sad at delta { 0, 1} (right)  from the best integer pel
  // cost_list[4]: cost/sad at delta {-1, 0} (top)    from the best integer pel
  if (cost_list) {
    const FULLPEL_MV best_int_mv = { br, bc };
    if (last_is_4) {
      calc_int_sad_list(x, ref_mv, sad_per_bit, vfp, &best_int_mv, cost_list,
                        bestsad);
    } else {
      calc_int_cost_list(x, ref_mv, sad_per_bit, vfp, &best_int_mv, cost_list);
    }
  }
  x->best_mv.as_mv.row = br;
  x->best_mv.as_mv.col = bc;
  return bestsad;
}

int av1_get_mvpred_sse(const MACROBLOCK *x, const FULLPEL_MV *best_mv,
                       const MV *ref_mv, const aom_variance_fn_ptr_t *vfp) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const MV mv = get_mv_from_fullmv(best_mv);
  const MV_COST_TYPE mv_cost_type = x->mv_cost_type;
  unsigned int sse, var;

  var = vfp->vf(what->buf, what->stride, get_buf_from_mv(in_what, best_mv),
                in_what->stride, &sse);
  (void)var;

  return sse + mv_err_cost(&mv, ref_mv, x->nmv_vec_cost,
                           CONVERT_TO_CONST_MVCOST(x->mv_cost_stack),
                           x->errorperbit, mv_cost_type);
}

int av1_get_mvpred_var(const MACROBLOCK *x, const FULLPEL_MV *best_mv,
                       const MV *ref_mv, const aom_variance_fn_ptr_t *vfp) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  const MV mv = get_mv_from_fullmv(best_mv);
  const MV_COST_TYPE mv_cost_type = x->mv_cost_type;
  unsigned int sse, var;

  var = vfp->vf(what->buf, what->stride, get_buf_from_mv(in_what, best_mv),
                in_what->stride, &sse);

  return var + mv_err_cost(&mv, ref_mv, x->nmv_vec_cost,
                           CONVERT_TO_CONST_MVCOST(x->mv_cost_stack),
                           x->errorperbit, mv_cost_type);
}

int av1_get_mvpred_av_var(const MACROBLOCK *x, const FULLPEL_MV *best_mv,
                          const MV *ref_mv, const uint8_t *second_pred,
                          const aom_variance_fn_ptr_t *vfp,
                          const struct buf_2d *src, const struct buf_2d *pre) {
  const struct buf_2d *const what = src;
  const struct buf_2d *const in_what = pre;
  const MV mv = get_mv_from_fullmv(best_mv);
  const MV_COST_TYPE mv_cost_type = x->mv_cost_type;
  unsigned int unused;

  return vfp->svaf(get_buf_from_mv(in_what, best_mv), in_what->stride, 0, 0,
                   what->buf, what->stride, &unused, second_pred) +
         mv_err_cost(&mv, ref_mv, x->nmv_vec_cost,
                     CONVERT_TO_CONST_MVCOST(x->mv_cost_stack), x->errorperbit,
                     mv_cost_type);
}

int av1_get_mvpred_mask_var(const MACROBLOCK *x, const FULLPEL_MV *best_mv,
                            const MV *ref_mv, const uint8_t *second_pred,
                            const uint8_t *mask, int mask_stride,
                            int invert_mask, const aom_variance_fn_ptr_t *vfp,
                            const struct buf_2d *src,
                            const struct buf_2d *pre) {
  const struct buf_2d *const what = src;
  const struct buf_2d *const in_what = pre;
  const MV mv = get_mv_from_fullmv(best_mv);
  const MV_COST_TYPE mv_cost_type = x->mv_cost_type;
  unsigned int unused;

  return vfp->msvf(what->buf, what->stride, 0, 0,
                   get_buf_from_mv(in_what, best_mv), in_what->stride,
                   second_pred, mask, mask_stride, invert_mask, &unused) +
         mv_err_cost(&mv, ref_mv, x->nmv_vec_cost,
                     CONVERT_TO_CONST_MVCOST(x->mv_cost_stack), x->errorperbit,
                     mv_cost_type);
}

// For the following foo_search, the input arguments are:
// x: The struct used to hold a bunch of random configs.
// start_mv: where we are starting our motion search
// search_param: how many steps to skip in our motion search. For example,
//   a value 3 suggests that 3 search steps have already taken place prior to
//   this function call, so we jump directly to step 4 of the search process
// sad_per_bit: a multiplier used to convert rate to sad cost
// do_init_search: if on, do an initial search of all possible scales around the
//   start_mv, and then pick the best scale.
// cond_list: used to hold the cost around the best full mv so we can use it to
//   speed up subpel search later.
// vfp: a function pointer to the simd function so we can compute the cost
//   efficiently
// ref_mf: the reference mv used to compute the mv cost
int av1_hex_search(MACROBLOCK *x, FULLPEL_MV *start_mv, int search_param,
                   int sad_per_bit, int do_init_search, int *cost_list,
                   const aom_variance_fn_ptr_t *vfp, const MV *ref_mv) {
  // First scale has 8-closest points, the rest have 6 points in hex shape
  // at increasing scales
  static const int hex_num_candidates[MAX_PATTERN_SCALES] = { 8, 6, 6, 6, 6, 6,
                                                              6, 6, 6, 6, 6 };
  // Note that the largest candidate step at each scale is 2^scale
  /* clang-format off */
  static const MV hex_candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES] = {
    { { -1, -1 }, { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 },
      { -1, 0 } },
    { { -1, -2 }, { 1, -2 }, { 2, 0 }, { 1, 2 }, { -1, 2 }, { -2, 0 } },
    { { -2, -4 }, { 2, -4 }, { 4, 0 }, { 2, 4 }, { -2, 4 }, { -4, 0 } },
    { { -4, -8 }, { 4, -8 }, { 8, 0 }, { 4, 8 }, { -4, 8 }, { -8, 0 } },
    { { -8, -16 }, { 8, -16 }, { 16, 0 }, { 8, 16 }, { -8, 16 }, { -16, 0 } },
    { { -16, -32 }, { 16, -32 }, { 32, 0 }, { 16, 32 }, { -16, 32 },
      { -32, 0 } },
    { { -32, -64 }, { 32, -64 }, { 64, 0 }, { 32, 64 }, { -32, 64 },
      { -64, 0 } },
    { { -64, -128 }, { 64, -128 }, { 128, 0 }, { 64, 128 }, { -64, 128 },
      { -128, 0 } },
    { { -128, -256 }, { 128, -256 }, { 256, 0 }, { 128, 256 }, { -128, 256 },
      { -256, 0 } },
    { { -256, -512 }, { 256, -512 }, { 512, 0 }, { 256, 512 }, { -256, 512 },
      { -512, 0 } },
    { { -512, -1024 }, { 512, -1024 }, { 1024, 0 }, { 512, 1024 },
      { -512, 1024 }, { -1024, 0 } },
  };
  /* clang-format on */
  return pattern_search(x, start_mv, search_param, sad_per_bit, do_init_search,
                        cost_list, vfp, ref_mv, hex_num_candidates,
                        hex_candidates);
}

static int bigdia_search(MACROBLOCK *x, FULLPEL_MV *start_mv, int search_param,
                         int sad_per_bit, int do_init_search, int *cost_list,
                         const aom_variance_fn_ptr_t *vfp, const MV *ref_mv) {
  // First scale has 4-closest points, the rest have 8 points in diamond
  // shape at increasing scales
  static const int bigdia_num_candidates[MAX_PATTERN_SCALES] = {
    4, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  };
  // Note that the largest candidate step at each scale is 2^scale
  /* clang-format off */
  static const MV
      bigdia_candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES] = {
        { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } },
        { { -1, -1 }, { 0, -2 }, { 1, -1 }, { 2, 0 }, { 1, 1 }, { 0, 2 },
          { -1, 1 }, { -2, 0 } },
        { { -2, -2 }, { 0, -4 }, { 2, -2 }, { 4, 0 }, { 2, 2 }, { 0, 4 },
          { -2, 2 }, { -4, 0 } },
        { { -4, -4 }, { 0, -8 }, { 4, -4 }, { 8, 0 }, { 4, 4 }, { 0, 8 },
          { -4, 4 }, { -8, 0 } },
        { { -8, -8 }, { 0, -16 }, { 8, -8 }, { 16, 0 }, { 8, 8 }, { 0, 16 },
          { -8, 8 }, { -16, 0 } },
        { { -16, -16 }, { 0, -32 }, { 16, -16 }, { 32, 0 }, { 16, 16 },
          { 0, 32 }, { -16, 16 }, { -32, 0 } },
        { { -32, -32 }, { 0, -64 }, { 32, -32 }, { 64, 0 }, { 32, 32 },
          { 0, 64 }, { -32, 32 }, { -64, 0 } },
        { { -64, -64 }, { 0, -128 }, { 64, -64 }, { 128, 0 }, { 64, 64 },
          { 0, 128 }, { -64, 64 }, { -128, 0 } },
        { { -128, -128 }, { 0, -256 }, { 128, -128 }, { 256, 0 }, { 128, 128 },
          { 0, 256 }, { -128, 128 }, { -256, 0 } },
        { { -256, -256 }, { 0, -512 }, { 256, -256 }, { 512, 0 }, { 256, 256 },
          { 0, 512 }, { -256, 256 }, { -512, 0 } },
        { { -512, -512 }, { 0, -1024 }, { 512, -512 }, { 1024, 0 },
          { 512, 512 }, { 0, 1024 }, { -512, 512 }, { -1024, 0 } },
      };
  /* clang-format on */
  return pattern_search(x, start_mv, search_param, sad_per_bit, do_init_search,
                        cost_list, vfp, ref_mv, bigdia_num_candidates,
                        bigdia_candidates);
}

static int square_search(MACROBLOCK *x, FULLPEL_MV *start_mv, int search_param,
                         int sad_per_bit, int do_init_search, int *cost_list,
                         const aom_variance_fn_ptr_t *vfp, const MV *ref_mv) {
  // All scales have 8 closest points in square shape
  static const int square_num_candidates[MAX_PATTERN_SCALES] = {
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  };
  // Note that the largest candidate step at each scale is 2^scale
  /* clang-format off */
  static const MV
      square_candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES] = {
        { { -1, -1 }, { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 },
          { -1, 1 }, { -1, 0 } },
        { { -2, -2 }, { 0, -2 }, { 2, -2 }, { 2, 0 }, { 2, 2 }, { 0, 2 },
          { -2, 2 }, { -2, 0 } },
        { { -4, -4 }, { 0, -4 }, { 4, -4 }, { 4, 0 }, { 4, 4 }, { 0, 4 },
          { -4, 4 }, { -4, 0 } },
        { { -8, -8 }, { 0, -8 }, { 8, -8 }, { 8, 0 }, { 8, 8 }, { 0, 8 },
          { -8, 8 }, { -8, 0 } },
        { { -16, -16 }, { 0, -16 }, { 16, -16 }, { 16, 0 }, { 16, 16 },
          { 0, 16 }, { -16, 16 }, { -16, 0 } },
        { { -32, -32 }, { 0, -32 }, { 32, -32 }, { 32, 0 }, { 32, 32 },
          { 0, 32 }, { -32, 32 }, { -32, 0 } },
        { { -64, -64 }, { 0, -64 }, { 64, -64 }, { 64, 0 }, { 64, 64 },
          { 0, 64 }, { -64, 64 }, { -64, 0 } },
        { { -128, -128 }, { 0, -128 }, { 128, -128 }, { 128, 0 }, { 128, 128 },
          { 0, 128 }, { -128, 128 }, { -128, 0 } },
        { { -256, -256 }, { 0, -256 }, { 256, -256 }, { 256, 0 }, { 256, 256 },
          { 0, 256 }, { -256, 256 }, { -256, 0 } },
        { { -512, -512 }, { 0, -512 }, { 512, -512 }, { 512, 0 }, { 512, 512 },
          { 0, 512 }, { -512, 512 }, { -512, 0 } },
        { { -1024, -1024 }, { 0, -1024 }, { 1024, -1024 }, { 1024, 0 },
          { 1024, 1024 }, { 0, 1024 }, { -1024, 1024 }, { -1024, 0 } },
      };
  /* clang-format on */
  return pattern_search(x, start_mv, search_param, sad_per_bit, do_init_search,
                        cost_list, vfp, ref_mv, square_num_candidates,
                        square_candidates);
}

static int fast_hex_search(MACROBLOCK *x, FULLPEL_MV *start_mv,
                           int search_param, int sad_per_bit,
                           int do_init_search,  // must be zero for fast_hex
                           int *cost_list, const aom_variance_fn_ptr_t *vfp,
                           const MV *ref_mv) {
  return av1_hex_search(x, start_mv,
                        AOMMAX(MAX_MVSEARCH_STEPS - 2, search_param),
                        sad_per_bit, do_init_search, cost_list, vfp, ref_mv);
}

static int fast_dia_search(MACROBLOCK *x, FULLPEL_MV *start_mv,
                           int search_param, int sad_per_bit,
                           int do_init_search, int *cost_list,
                           const aom_variance_fn_ptr_t *vfp, const MV *ref_mv) {
  return bigdia_search(x, start_mv,
                       AOMMAX(MAX_MVSEARCH_STEPS - 2, search_param),
                       sad_per_bit, do_init_search, cost_list, vfp, ref_mv);
}

#undef CHECK_BETTER

// Exhaustive motion search around a given centre position with a given
// step size.
static int exhuastive_mesh_search(MACROBLOCK *x, FULLPEL_MV *ref_mv,
                                  FULLPEL_MV *best_mv, int range, int step,
                                  int sad_per_bit,
                                  const aom_variance_fn_ptr_t *fn_ptr,
                                  const FULLPEL_MV *start_mv) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const what = &x->plane[0].src;
  const struct buf_2d *const in_what = &xd->plane[0].pre[0];
  FULLPEL_MV start_mv_ = *start_mv;
  unsigned int best_sad = INT_MAX;
  int r, c, i;
  int start_col, end_col, start_row, end_row;
  int col_step = (step > 1) ? step : 4;

  assert(step >= 1);

  clamp_fullmv(&start_mv_, &x->mv_limits);
  *best_mv = start_mv_;
  best_sad =
      fn_ptr->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &start_mv_),
                  in_what->stride) +
      mvsad_err_cost(x, &start_mv_, ref_mv, sad_per_bit);
  start_row = AOMMAX(-range, x->mv_limits.row_min - start_mv_.row);
  start_col = AOMMAX(-range, x->mv_limits.col_min - start_mv_.col);
  end_row = AOMMIN(range, x->mv_limits.row_max - start_mv_.row);
  end_col = AOMMIN(range, x->mv_limits.col_max - start_mv_.col);

  for (r = start_row; r <= end_row; r += step) {
    for (c = start_col; c <= end_col; c += col_step) {
      // Step > 1 means we are not checking every location in this pass.
      if (step > 1) {
        const FULLPEL_MV mv = { start_mv_.row + r, start_mv_.col + c };
        unsigned int sad =
            fn_ptr->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &mv),
                        in_what->stride);
        if (sad < best_sad) {
          sad += mvsad_err_cost(x, &mv, ref_mv, sad_per_bit);
          if (sad < best_sad) {
            best_sad = sad;
            x->second_best_mv.as_fullmv = *best_mv;
            *best_mv = mv;
          }
        }
      } else {
        // 4 sads in a single call if we are checking every location
        if (c + 3 <= end_col) {
          unsigned int sads[4];
          const uint8_t *addrs[4];
          for (i = 0; i < 4; ++i) {
            const FULLPEL_MV mv = { start_mv_.row + r, start_mv_.col + c + i };
            addrs[i] = get_buf_from_mv(in_what, &mv);
          }
          fn_ptr->sdx4df(what->buf, what->stride, addrs, in_what->stride, sads);

          for (i = 0; i < 4; ++i) {
            if (sads[i] < best_sad) {
              const FULLPEL_MV mv = { start_mv_.row + r,
                                      start_mv_.col + c + i };
              const unsigned int sad =
                  sads[i] + mvsad_err_cost(x, &mv, ref_mv, sad_per_bit);
              if (sad < best_sad) {
                best_sad = sad;
                x->second_best_mv.as_fullmv = *best_mv;
                *best_mv = mv;
              }
            }
          }
        } else {
          for (i = 0; i < end_col - c; ++i) {
            const FULLPEL_MV mv = { start_mv_.row + r, start_mv_.col + c + i };
            unsigned int sad =
                fn_ptr->sdf(what->buf, what->stride,
                            get_buf_from_mv(in_what, &mv), in_what->stride);
            if (sad < best_sad) {
              sad += mvsad_err_cost(x, &mv, ref_mv, sad_per_bit);
              if (sad < best_sad) {
                best_sad = sad;
                x->second_best_mv.as_fullmv = *best_mv;
                *best_mv = mv;
              }
            }
          }
        }
      }
    }
  }

  return best_sad;
}

int av1_diamond_search_sad_c(MACROBLOCK *x, const search_site_config *cfg,
                             FULLPEL_MV *start_mv, FULLPEL_MV *best_mv,
                             int search_param, int sad_per_bit, int *num00,
                             const aom_variance_fn_ptr_t *fn_ptr,
                             const MV *ref_mv, uint8_t *second_pred,
                             uint8_t *mask, int mask_stride, int inv_mask) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  uint8_t *what = x->plane[0].src.buf;
  const int what_stride = x->plane[0].src.stride;
  const uint8_t *in_what;
  const int in_what_stride = xd->plane[0].pre[0].stride;
  const uint8_t *best_address;

  unsigned int bestsad = INT_MAX;
  int best_site = 0;
  int is_off_center = 0;

  // search_param determines the length of the initial step and hence the number
  // of iterations.
  const int tot_steps = cfg->ss_count - search_param;

  const FULLPEL_MV full_ref_mv = get_fullmv_from_mv(ref_mv);
  clamp_fullmv(start_mv, &x->mv_limits);
  *num00 = 0;
  best_mv->row = start_mv->row;
  best_mv->col = start_mv->col;

  // Work out the start point for the search
  in_what = get_buf_from_mv(&xd->plane[0].pre[0], start_mv);
  best_address = in_what;

  // Check the starting position
  // TODO(jingning): unify the parameter interface for the following
  // computation modes.
  if (mask)
    bestsad = fn_ptr->msdf(what, what_stride, in_what, in_what_stride,
                           second_pred, mask, mask_stride, inv_mask);
  else if (second_pred)
    bestsad =
        fn_ptr->sdaf(what, what_stride, in_what, in_what_stride, second_pred);
  else
    bestsad = fn_ptr->sdf(what, what_stride, in_what, in_what_stride);

  bestsad += mvsad_err_cost(x, best_mv, &full_ref_mv, sad_per_bit);

  int next_step_size = tot_steps > 2 ? cfg->radius[tot_steps - 2] : 1;
  for (int step = tot_steps - 1; step >= 0; --step) {
    const search_site *ss = cfg->ss[step];
    best_site = 0;
    if (step > 0) next_step_size = cfg->radius[step - 1];

    int all_in = 1, j;
    // Trap illegal vectors
    all_in &= best_mv->row + ss[1].mv.row >= x->mv_limits.row_min;
    all_in &= best_mv->row + ss[2].mv.row <= x->mv_limits.row_max;
    all_in &= best_mv->col + ss[3].mv.col >= x->mv_limits.col_min;
    all_in &= best_mv->col + ss[4].mv.col <= x->mv_limits.col_max;

    // TODO(anyone): Implement 4 points search for msdf&sdaf
    if (all_in && !mask && !second_pred) {
      for (int idx = 1; idx <= cfg->searches_per_step[step]; idx += 4) {
        unsigned char const *block_offset[4];
        unsigned int sads[4];

        for (j = 0; j < 4; j++)
          block_offset[j] = ss[idx + j].offset + best_address;

        fn_ptr->sdx4df(what, what_stride, block_offset, in_what_stride, sads);
        for (j = 0; j < 4; j++) {
          if (sads[j] < bestsad) {
            const FULLPEL_MV this_mv = { best_mv->row + ss[idx + j].mv.row,
                                         best_mv->col + ss[idx + j].mv.col };
            unsigned int thissad =
                sads[j] +
                mvsad_err_cost(x, &this_mv, &full_ref_mv, sad_per_bit);
            if (thissad < bestsad) {
              bestsad = thissad;
              best_site = idx + j;
            }
          }
        }
      }
    } else {
      for (int idx = 1; idx <= cfg->searches_per_step[step]; idx++) {
        const FULLPEL_MV this_mv = { best_mv->row + ss[idx].mv.row,
                                     best_mv->col + ss[idx].mv.col };

        if (av1_is_fullmv_in_range(&x->mv_limits, this_mv)) {
          const uint8_t *const check_here = ss[idx].offset + best_address;
          unsigned int thissad;

          if (mask)
            thissad =
                fn_ptr->msdf(what, what_stride, check_here, in_what_stride,
                             second_pred, mask, mask_stride, inv_mask);
          else if (second_pred)
            thissad = fn_ptr->sdaf(what, what_stride, check_here,
                                   in_what_stride, second_pred);
          else
            thissad =
                fn_ptr->sdf(what, what_stride, check_here, in_what_stride);

          if (thissad < bestsad) {
            thissad += mvsad_err_cost(x, &this_mv, &full_ref_mv, sad_per_bit);
            if (thissad < bestsad) {
              bestsad = thissad;
              best_site = idx;
            }
          }
        }
      }
    }

    if (best_site != 0) {
      x->second_best_mv.as_fullmv = *best_mv;
      best_mv->row += ss[best_site].mv.row;
      best_mv->col += ss[best_site].mv.col;
      best_address += ss[best_site].offset;
      is_off_center = 1;
    }

    if (is_off_center == 0) (*num00)++;

    if (best_site == 0) {
      while (next_step_size == cfg->radius[step] && step > 2) {
        ++(*num00);
        --step;
        next_step_size = cfg->radius[step - 1];
      }
    }
  }

  return bestsad;
}

/* do_refine: If last step (1-away) of n-step search doesn't pick the center
              point as the best match, we will do a final 1-away diamond
              refining search  */
static int full_pixel_diamond(MACROBLOCK *x, FULLPEL_MV *start_mv,
                              int step_param, int sadpb, int *cost_list,
                              const aom_variance_fn_ptr_t *fn_ptr,
                              const MV *ref_mv, const search_site_config *cfg,
                              uint8_t *second_pred, uint8_t *mask,
                              int mask_stride, int inv_mask) {
  FULLPEL_MV best_mv;
  int thissme, n, num00 = 0;
  int bestsme = av1_diamond_search_sad_c(x, cfg, start_mv, &best_mv, step_param,
                                         sadpb, &n, fn_ptr, ref_mv, second_pred,
                                         mask, mask_stride, inv_mask);

  if (bestsme < INT_MAX) {
    if (mask)
      bestsme = av1_get_mvpred_mask_var(
          x, &best_mv, ref_mv, second_pred, mask, mask_stride, inv_mask, fn_ptr,
          &x->plane[0].src, &x->e_mbd.plane[0].pre[0]);
    else if (second_pred)
      bestsme =
          av1_get_mvpred_av_var(x, &best_mv, ref_mv, second_pred, fn_ptr,
                                &x->plane[0].src, &x->e_mbd.plane[0].pre[0]);
    else
      bestsme = av1_get_mvpred_var(x, &best_mv, ref_mv, fn_ptr);
  }

  x->best_mv.as_fullmv = best_mv;

  // If there won't be more n-step search, check to see if refining search is
  // needed.
  const int further_steps = cfg->ss_count - 1 - step_param;
  while (n < further_steps) {
    ++n;

    if (num00) {
      num00--;
    } else {
      thissme = av1_diamond_search_sad_c(
          x, cfg, start_mv, &best_mv, step_param + n, sadpb, &num00, fn_ptr,
          ref_mv, second_pred, mask, mask_stride, inv_mask);

      if (thissme < INT_MAX) {
        if (mask)
          thissme = av1_get_mvpred_mask_var(
              x, &best_mv, ref_mv, second_pred, mask, mask_stride, inv_mask,
              fn_ptr, &x->plane[0].src, &x->e_mbd.plane[0].pre[0]);
        else if (second_pred)
          thissme = av1_get_mvpred_av_var(x, &best_mv, ref_mv, second_pred,
                                          fn_ptr, &x->plane[0].src,
                                          &x->e_mbd.plane[0].pre[0]);
        else
          thissme = av1_get_mvpred_var(x, &best_mv, ref_mv, fn_ptr);
      }

      if (thissme < bestsme) {
        bestsme = thissme;
        x->best_mv.as_fullmv = best_mv;
      }
    }
  }

  // Return cost list.
  if (cost_list) {
    calc_int_cost_list(x, ref_mv, sadpb, fn_ptr, &x->best_mv.as_fullmv,
                       cost_list);
  }
  return bestsme;
}

#define MIN_RANGE 7
#define MAX_RANGE 256
#define MIN_INTERVAL 1
// Runs an limited range exhaustive mesh search using a pattern set
// according to the encode speed profile.
static int full_pixel_exhaustive(
    MACROBLOCK *x, const FULLPEL_MV *start_mv, int sadpb, int *cost_list,
    const aom_variance_fn_ptr_t *fn_ptr, const MV *ref_mv, FULLPEL_MV *best_mv,
    const struct MESH_PATTERN *const mesh_patterns) {
  FULLPEL_MV full_ref_mv = get_fullmv_from_mv(ref_mv);
  int bestsme;
  int i;
  int interval = mesh_patterns[0].interval;
  int range = mesh_patterns[0].range;
  int baseline_interval_divisor;

  *best_mv = *start_mv;

  // Trap illegal values for interval and range for this function.
  if ((range < MIN_RANGE) || (range > MAX_RANGE) || (interval < MIN_INTERVAL) ||
      (interval > range))
    return INT_MAX;

  baseline_interval_divisor = range / interval;

  // Check size of proposed first range against magnitude of the centre
  // value used as a starting point.
  range = AOMMAX(range, (5 * AOMMAX(abs(best_mv->row), abs(best_mv->col))) / 4);
  range = AOMMIN(range, MAX_RANGE);
  interval = AOMMAX(interval, range / baseline_interval_divisor);

  // initial search
  bestsme = exhuastive_mesh_search(x, &full_ref_mv, best_mv, range, interval,
                                   sadpb, fn_ptr, best_mv);

  if ((interval > MIN_INTERVAL) && (range > MIN_RANGE)) {
    // Progressive searches with range and step size decreasing each time
    // till we reach a step size of 1. Then break out.
    for (i = 1; i < MAX_MESH_STEP; ++i) {
      // First pass with coarser step and longer range
      bestsme = exhuastive_mesh_search(
          x, &full_ref_mv, best_mv, mesh_patterns[i].range,
          mesh_patterns[i].interval, sadpb, fn_ptr, best_mv);

      if (mesh_patterns[i].interval == 1) break;
    }
  }

  if (bestsme < INT_MAX)
    bestsme = av1_get_mvpred_var(x, best_mv, ref_mv, fn_ptr);

  // Return cost list.
  if (cost_list) {
    calc_int_cost_list(x, ref_mv, sadpb, fn_ptr, best_mv, cost_list);
  }
  return bestsme;
}

// This function is called when we do joint motion search in comp_inter_inter
// mode, or when searching for one component of an ext-inter compound mode.
int av1_refining_search_8p_c(MACROBLOCK *x, int error_per_bit, int search_range,
                             const aom_variance_fn_ptr_t *fn_ptr,
                             const uint8_t *mask, int mask_stride,
                             int invert_mask, const MV *ref_mv,
                             const uint8_t *second_pred,
                             const struct buf_2d *src,
                             const struct buf_2d *pre) {
  static const search_neighbors neighbors[8] = {
    { { -1, 0 }, -1 * SEARCH_GRID_STRIDE_8P + 0 },
    { { 0, -1 }, 0 * SEARCH_GRID_STRIDE_8P - 1 },
    { { 0, 1 }, 0 * SEARCH_GRID_STRIDE_8P + 1 },
    { { 1, 0 }, 1 * SEARCH_GRID_STRIDE_8P + 0 },
    { { -1, -1 }, -1 * SEARCH_GRID_STRIDE_8P - 1 },
    { { 1, -1 }, 1 * SEARCH_GRID_STRIDE_8P - 1 },
    { { -1, 1 }, -1 * SEARCH_GRID_STRIDE_8P + 1 },
    { { 1, 1 }, 1 * SEARCH_GRID_STRIDE_8P + 1 }
  };
  const struct buf_2d *const what = src;
  const struct buf_2d *const in_what = pre;
  const FULLPEL_MV full_ref_mv = get_fullmv_from_mv(ref_mv);
  FULLPEL_MV *best_mv = &x->best_mv.as_fullmv;
  unsigned int best_sad = INT_MAX;
  int i, j;
  uint8_t do_refine_search_grid[SEARCH_GRID_STRIDE_8P *
                                SEARCH_GRID_STRIDE_8P] = { 0 };
  int grid_center = SEARCH_GRID_CENTER_8P;
  int grid_coord = grid_center;

  clamp_fullmv(best_mv, &x->mv_limits);
  if (mask) {
    best_sad = fn_ptr->msdf(what->buf, what->stride,
                            get_buf_from_mv(in_what, best_mv), in_what->stride,
                            second_pred, mask, mask_stride, invert_mask) +
               mvsad_err_cost(x, best_mv, &full_ref_mv, error_per_bit);
  } else {
    best_sad =
        fn_ptr->sdaf(what->buf, what->stride, get_buf_from_mv(in_what, best_mv),
                     in_what->stride, second_pred) +
        mvsad_err_cost(x, best_mv, &full_ref_mv, error_per_bit);
  }

  do_refine_search_grid[grid_coord] = 1;

  for (i = 0; i < search_range; ++i) {
    int best_site = -1;

    for (j = 0; j < 8; ++j) {
      grid_coord = grid_center + neighbors[j].coord_offset;
      if (do_refine_search_grid[grid_coord] == 1) {
        continue;
      }
      const FULLPEL_MV mv = { best_mv->row + neighbors[j].coord.row,
                              best_mv->col + neighbors[j].coord.col };

      do_refine_search_grid[grid_coord] = 1;
      if (av1_is_fullmv_in_range(&x->mv_limits, mv)) {
        unsigned int sad;
        if (mask) {
          sad = fn_ptr->msdf(what->buf, what->stride,
                             get_buf_from_mv(in_what, &mv), in_what->stride,
                             second_pred, mask, mask_stride, invert_mask);
        } else {
          sad = fn_ptr->sdaf(what->buf, what->stride,
                             get_buf_from_mv(in_what, &mv), in_what->stride,
                             second_pred);
        }
        if (sad < best_sad) {
          sad += mvsad_err_cost(x, &mv, &full_ref_mv, error_per_bit);
          if (sad < best_sad) {
            best_sad = sad;
            best_site = j;
          }
        }
      }
    }

    if (best_site == -1) {
      break;
    } else {
      best_mv->row += neighbors[best_site].coord.row;
      best_mv->col += neighbors[best_site].coord.col;
      grid_center += neighbors[best_site].coord_offset;
    }
  }
  return best_sad;
}

static int vector_match(int16_t *ref, int16_t *src, int bwl) {
  int best_sad = INT_MAX;
  int this_sad;
  int d;
  int center, offset = 0;
  int bw = 4 << bwl;  // redundant variable, to be changed in the experiments.
  for (d = 0; d <= bw; d += 16) {
    this_sad = aom_vector_var(&ref[d], src, bwl);
    if (this_sad < best_sad) {
      best_sad = this_sad;
      offset = d;
    }
  }
  center = offset;

  for (d = -8; d <= 8; d += 16) {
    int this_pos = offset + d;
    // check limit
    if (this_pos < 0 || this_pos > bw) continue;
    this_sad = aom_vector_var(&ref[this_pos], src, bwl);
    if (this_sad < best_sad) {
      best_sad = this_sad;
      center = this_pos;
    }
  }
  offset = center;

  for (d = -4; d <= 4; d += 8) {
    int this_pos = offset + d;
    // check limit
    if (this_pos < 0 || this_pos > bw) continue;
    this_sad = aom_vector_var(&ref[this_pos], src, bwl);
    if (this_sad < best_sad) {
      best_sad = this_sad;
      center = this_pos;
    }
  }
  offset = center;

  for (d = -2; d <= 2; d += 4) {
    int this_pos = offset + d;
    // check limit
    if (this_pos < 0 || this_pos > bw) continue;
    this_sad = aom_vector_var(&ref[this_pos], src, bwl);
    if (this_sad < best_sad) {
      best_sad = this_sad;
      center = this_pos;
    }
  }
  offset = center;

  for (d = -1; d <= 1; d += 2) {
    int this_pos = offset + d;
    // check limit
    if (this_pos < 0 || this_pos > bw) continue;
    this_sad = aom_vector_var(&ref[this_pos], src, bwl);
    if (this_sad < best_sad) {
      best_sad = this_sad;
      center = this_pos;
    }
  }

  return (center - (bw >> 1));
}

static const MV search_pos[4] = {
  { -1, 0 },
  { 0, -1 },
  { 0, 1 },
  { 1, 0 },
};

unsigned int av1_int_pro_motion_estimation(const AV1_COMP *cpi, MACROBLOCK *x,
                                           BLOCK_SIZE bsize, int mi_row,
                                           int mi_col, const MV *ref_mv) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mi = xd->mi[0];
  struct buf_2d backup_yv12[MAX_MB_PLANE] = { { 0, 0, 0, 0, 0 } };
  DECLARE_ALIGNED(16, int16_t, hbuf[256]);
  DECLARE_ALIGNED(16, int16_t, vbuf[256]);
  DECLARE_ALIGNED(16, int16_t, src_hbuf[128]);
  DECLARE_ALIGNED(16, int16_t, src_vbuf[128]);
  int idx;
  const int bw = 4 << mi_size_wide_log2[bsize];
  const int bh = 4 << mi_size_high_log2[bsize];
  const int search_width = bw << 1;
  const int search_height = bh << 1;
  const int src_stride = x->plane[0].src.stride;
  const int ref_stride = xd->plane[0].pre[0].stride;
  uint8_t const *ref_buf, *src_buf;
  int_mv *best_int_mv = &xd->mi[0]->mv[0];
  unsigned int best_sad, tmp_sad, this_sad[4];
  const int norm_factor = 3 + (bw >> 5);
  const YV12_BUFFER_CONFIG *scaled_ref_frame =
      av1_get_scaled_ref_frame(cpi, mi->ref_frame[0]);

  if (scaled_ref_frame) {
    int i;
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // motion search code to be used without additional modifications.
    for (i = 0; i < MAX_MB_PLANE; i++) backup_yv12[i] = xd->plane[i].pre[0];
    av1_setup_pre_planes(xd, 0, scaled_ref_frame, mi_row, mi_col, NULL,
                         MAX_MB_PLANE);
  }

  if (xd->bd != 8) {
    unsigned int sad;
    best_int_mv->as_fullmv = kZeroFullMv;
    sad = cpi->fn_ptr[bsize].sdf(x->plane[0].src.buf, src_stride,
                                 xd->plane[0].pre[0].buf, ref_stride);

    if (scaled_ref_frame) {
      int i;
      for (i = 0; i < MAX_MB_PLANE; i++) xd->plane[i].pre[0] = backup_yv12[i];
    }
    return sad;
  }

  // Set up prediction 1-D reference set
  ref_buf = xd->plane[0].pre[0].buf - (bw >> 1);
  for (idx = 0; idx < search_width; idx += 16) {
    aom_int_pro_row(&hbuf[idx], ref_buf, ref_stride, bh);
    ref_buf += 16;
  }

  ref_buf = xd->plane[0].pre[0].buf - (bh >> 1) * ref_stride;
  for (idx = 0; idx < search_height; ++idx) {
    vbuf[idx] = aom_int_pro_col(ref_buf, bw) >> norm_factor;
    ref_buf += ref_stride;
  }

  // Set up src 1-D reference set
  for (idx = 0; idx < bw; idx += 16) {
    src_buf = x->plane[0].src.buf + idx;
    aom_int_pro_row(&src_hbuf[idx], src_buf, src_stride, bh);
  }

  src_buf = x->plane[0].src.buf;
  for (idx = 0; idx < bh; ++idx) {
    src_vbuf[idx] = aom_int_pro_col(src_buf, bw) >> norm_factor;
    src_buf += src_stride;
  }

  // Find the best match per 1-D search
  best_int_mv->as_fullmv.col =
      vector_match(hbuf, src_hbuf, mi_size_wide_log2[bsize]);
  best_int_mv->as_fullmv.row =
      vector_match(vbuf, src_vbuf, mi_size_high_log2[bsize]);

  FULLPEL_MV this_mv = best_int_mv->as_fullmv;
  src_buf = x->plane[0].src.buf;
  ref_buf = get_buf_from_mv(&xd->plane[0].pre[0], &this_mv);
  best_sad = cpi->fn_ptr[bsize].sdf(src_buf, src_stride, ref_buf, ref_stride);

  {
    const uint8_t *const pos[4] = {
      ref_buf - ref_stride,
      ref_buf - 1,
      ref_buf + 1,
      ref_buf + ref_stride,
    };

    cpi->fn_ptr[bsize].sdx4df(src_buf, src_stride, pos, ref_stride, this_sad);
  }

  for (idx = 0; idx < 4; ++idx) {
    if (this_sad[idx] < best_sad) {
      best_sad = this_sad[idx];
      best_int_mv->as_fullmv.row = search_pos[idx].row + this_mv.row;
      best_int_mv->as_fullmv.col = search_pos[idx].col + this_mv.col;
    }
  }

  if (this_sad[0] < this_sad[3])
    this_mv.row -= 1;
  else
    this_mv.row += 1;

  if (this_sad[1] < this_sad[2])
    this_mv.col -= 1;
  else
    this_mv.col += 1;

  ref_buf = get_buf_from_mv(&xd->plane[0].pre[0], &this_mv);

  tmp_sad = cpi->fn_ptr[bsize].sdf(src_buf, src_stride, ref_buf, ref_stride);
  if (best_sad > tmp_sad) {
    best_int_mv->as_fullmv = this_mv;
    best_sad = tmp_sad;
  }

  convert_fullmv_to_mv(best_int_mv);

  SubpelMvLimits subpel_mv_limits;
  av1_set_subpel_mv_search_range(&subpel_mv_limits, &x->mv_limits, ref_mv);
  clamp_mv(&best_int_mv->as_mv, &subpel_mv_limits);

  if (scaled_ref_frame) {
    int i;
    for (i = 0; i < MAX_MB_PLANE; i++) xd->plane[i].pre[0] = backup_yv12[i];
  }

  return best_sad;
}

int av1_full_pixel_search(const AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
                          FULLPEL_MV *start_mv, int step_param, int method,
                          int run_mesh_search, int error_per_bit,
                          int *cost_list, const MV *ref_mv, int var_max, int rd,
                          int x_pos, int y_pos, int intra,
                          const search_site_config *cfg,
                          int use_intrabc_mesh_pattern) {
  const SPEED_FEATURES *const sf = &cpi->sf;
  const aom_variance_fn_ptr_t *fn_ptr = &cpi->fn_ptr[bsize];
  int var = 0;

  if (cost_list) {
    cost_list[0] = INT_MAX;
    cost_list[1] = INT_MAX;
    cost_list[2] = INT_MAX;
    cost_list[3] = INT_MAX;
    cost_list[4] = INT_MAX;
  }

  switch (method) {
    case FAST_DIAMOND:
      var = fast_dia_search(x, start_mv, step_param, error_per_bit, 0,
                            cost_list, fn_ptr, ref_mv);
      break;
    case FAST_HEX:
      var = fast_hex_search(x, start_mv, step_param, error_per_bit, 0,
                            cost_list, fn_ptr, ref_mv);
      break;
    case HEX:
      var = av1_hex_search(x, start_mv, step_param, error_per_bit, 1, cost_list,
                           fn_ptr, ref_mv);
      break;
    case SQUARE:
      var = square_search(x, start_mv, step_param, error_per_bit, 1, cost_list,
                          fn_ptr, ref_mv);
      break;
    case BIGDIA:
      var = bigdia_search(x, start_mv, step_param, error_per_bit, 1, cost_list,
                          fn_ptr, ref_mv);
      break;
    case NSTEP:
    case DIAMOND:
      var =
          full_pixel_diamond(x, start_mv, step_param, error_per_bit, cost_list,
                             fn_ptr, ref_mv, cfg, NULL, NULL, 0, 0);
      break;
    default: assert(0 && "Invalid search method.");
  }

  // Should we allow a follow on exhaustive search?
  if (!run_mesh_search && method == NSTEP) {
    int exhuastive_thr = sf->mv_sf.exhaustive_searches_thresh;
    exhuastive_thr >>=
        10 - (mi_size_wide_log2[bsize] + mi_size_high_log2[bsize]);
    // Threshold variance for an exhaustive full search.
    if (var > exhuastive_thr) run_mesh_search = 1;
  }

  // TODO(yunqing): the following is used to reduce mesh search in temporal
  // filtering. Can extend it to intrabc.
  if (!use_intrabc_mesh_pattern && sf->mv_sf.prune_mesh_search) {
    const int full_pel_mv_diff =
        AOMMAX(abs(start_mv->row - x->best_mv.as_fullmv.row),
               abs(start_mv->col - x->best_mv.as_fullmv.col));
    if (full_pel_mv_diff <= 4) {
      run_mesh_search = 0;
    }
  }

  if (run_mesh_search) {
    int var_ex;
    FULLPEL_MV tmp_mv_ex;
    // Pick the mesh pattern for exhaustive search based on the toolset (intraBC
    // or non-intraBC)
    const MESH_PATTERN *const mesh_patterns =
        use_intrabc_mesh_pattern ? sf->mv_sf.intrabc_mesh_patterns
                                 : sf->mv_sf.mesh_patterns;
    var_ex = full_pixel_exhaustive(x, &x->best_mv.as_fullmv, error_per_bit,
                                   cost_list, fn_ptr, ref_mv, &tmp_mv_ex,
                                   mesh_patterns);
    if (var_ex < var) {
      var = var_ex;
      x->best_mv.as_fullmv = tmp_mv_ex;
    }
  }

  if (method != NSTEP && rd && var < var_max)
    var = av1_get_mvpred_var(x, &x->best_mv.as_fullmv, ref_mv, fn_ptr);

  // Use hash-me for intrablock copy
  do {
    if (!intra || !av1_use_hash_me(cpi)) break;

    // already single ME
    // get block size and original buffer of current block
    const int block_height = block_size_high[bsize];
    const int block_width = block_size_wide[bsize];
    if (block_height == block_width && x_pos >= 0 && y_pos >= 0) {
      if (block_width == 4 || block_width == 8 || block_width == 16 ||
          block_width == 32 || block_width == 64 || block_width == 128) {
        uint8_t *what = x->plane[0].src.buf;
        const int what_stride = x->plane[0].src.stride;
        uint32_t hash_value1, hash_value2;
        FULLPEL_MV best_hash_mv;
        int best_hash_cost = INT_MAX;

        // for the hashMap
        hash_table *ref_frame_hash = &cpi->common.cur_frame->hash_table;

        av1_get_block_hash_value(what, what_stride, block_width, &hash_value1,
                                 &hash_value2, is_cur_buf_hbd(&x->e_mbd), x);

        const int count = av1_hash_table_count(ref_frame_hash, hash_value1);
        // for intra, at lest one matching can be found, itself.
        if (count <= (intra ? 1 : 0)) {
          break;
        }

        Iterator iterator =
            av1_hash_get_first_iterator(ref_frame_hash, hash_value1);
        for (int i = 0; i < count; i++, aom_iterator_increment(&iterator)) {
          block_hash ref_block_hash =
              *(block_hash *)(aom_iterator_get(&iterator));
          if (hash_value2 == ref_block_hash.hash_value2) {
            // For intra, make sure the prediction is from valid area.
            if (intra) {
              const int mi_col = x_pos / MI_SIZE;
              const int mi_row = y_pos / MI_SIZE;
              const MV dv = { 8 * (ref_block_hash.y - y_pos),
                              8 * (ref_block_hash.x - x_pos) };
              if (!av1_is_dv_valid(dv, &cpi->common, &x->e_mbd, mi_row, mi_col,
                                   bsize, cpi->common.seq_params.mib_size_log2))
                continue;
            }
            FULLPEL_MV hash_mv;
            hash_mv.col = ref_block_hash.x - x_pos;
            hash_mv.row = ref_block_hash.y - y_pos;
            if (!av1_is_fullmv_in_range(&x->mv_limits, hash_mv)) continue;
            const int refCost = av1_get_mvpred_var(x, &hash_mv, ref_mv, fn_ptr);
            if (refCost < best_hash_cost) {
              best_hash_cost = refCost;
              best_hash_mv = hash_mv;
            }
          }
        }
        if (best_hash_cost < var) {
          x->second_best_mv = x->best_mv;
          x->best_mv.as_fullmv = best_hash_mv;
          var = best_hash_cost;
        }
      }
    }
  } while (0);

  return var;
}

/* returns subpixel variance error function */
#define DIST(r, c)                                                             \
  vfp->osvf(pre(y, y_stride, r, c), y_stride, sp(c), sp(r), src_address, mask, \
            &sse)

/* checks if (r, c) has better score than previous best */
#define MVC(diff_mv)                                                          \
  (unsigned int)(mvcost                                                       \
                     ? (mv_cost((diff_mv), mvjcost, mvcost) * error_per_bit + \
                        4096) >>                                              \
                           13                                                 \
                     : 0)

#define CHECK_BETTER(v, r, c)                                  \
  {                                                            \
    const MV this_mv = { r, c };                               \
    if (av1_is_subpelmv_in_range(&mv_limits, this_mv)) {       \
      const MV diff_mv = { r - ref_mv->row, c - ref_mv->col }; \
      thismse = (DIST(r, c));                                  \
      if ((v = MVC(&diff_mv) + thismse) < besterr) {           \
        besterr = v;                                           \
        br = r;                                                \
        bc = c;                                                \
        *distortion = thismse;                                 \
        *sse1 = sse;                                           \
      }                                                        \
    } else {                                                   \
      v = INT_MAX;                                             \
    }                                                          \
  }

#undef CHECK_BETTER0
#define CHECK_BETTER0(v, r, c) CHECK_BETTER(v, r, c)

#undef CHECK_BETTER1
#define CHECK_BETTER1(v, r, c)                                              \
  {                                                                         \
    const MV this_mv = { r, c };                                            \
    if (av1_is_subpelmv_in_range(&mv_limits, this_mv)) {                    \
      thismse = upsampled_obmc_pref_error(                                  \
          xd, cm, &this_mv, mask, vfp, src_address, pre(y, y_stride, r, c), \
          y_stride, sp(c), sp(r), w, h, &sse, subpel_search_type);          \
      v = mv_err_cost(&this_mv, ref_mv, mvjcost, mvcost, error_per_bit,     \
                      mv_cost_type);                                        \
      if ((v + thismse) < besterr) {                                        \
        besterr = v + thismse;                                              \
        br = r;                                                             \
        bc = c;                                                             \
        *distortion = thismse;                                              \
        *sse1 = sse;                                                        \
      }                                                                     \
    } else {                                                                \
      v = INT_MAX;                                                          \
    }                                                                       \
  }

static unsigned int setup_obmc_center_error(
    const int32_t *mask, const MV *bestmv, const MV *ref_mv, int error_per_bit,
    const aom_variance_fn_ptr_t *vfp, const int32_t *const wsrc,
    const uint8_t *const y, int y_stride, const int *mvjcost,
    const int *const mvcost[2], unsigned int *sse1, int *distortion,
    MV_COST_TYPE mv_cost_type) {
  unsigned int besterr;
  besterr = vfp->ovf(y, y_stride, wsrc, mask, sse1);
  *distortion = besterr;
  besterr +=
      mv_err_cost(bestmv, ref_mv, mvjcost, mvcost, error_per_bit, mv_cost_type);
  return besterr;
}

static int upsampled_obmc_pref_error(
    MACROBLOCKD *xd, const AV1_COMMON *const cm, const MV *const mv,
    const int32_t *mask, const aom_variance_fn_ptr_t *vfp,
    const int32_t *const wsrc, const uint8_t *const y, int y_stride,
    int subpel_x_q3, int subpel_y_q3, int w, int h, unsigned int *sse,
    int subpel_search) {
  unsigned int besterr;

  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  DECLARE_ALIGNED(16, uint8_t, pred[2 * MAX_SB_SQUARE]);
#if CONFIG_AV1_HIGHBITDEPTH
  if (is_cur_buf_hbd(xd)) {
    uint8_t *pred8 = CONVERT_TO_BYTEPTR(pred);
    aom_highbd_upsampled_pred(xd, cm, mi_row, mi_col, mv, pred8, w, h,
                              subpel_x_q3, subpel_y_q3, y, y_stride, xd->bd,
                              subpel_search);
    besterr = vfp->ovf(pred8, w, wsrc, mask, sse);
  } else {
    aom_upsampled_pred(xd, cm, mi_row, mi_col, mv, pred, w, h, subpel_x_q3,
                       subpel_y_q3, y, y_stride, subpel_search);

    besterr = vfp->ovf(pred, w, wsrc, mask, sse);
  }
#else
  aom_upsampled_pred(xd, cm, mi_row, mi_col, mv, pred, w, h, subpel_x_q3,
                     subpel_y_q3, y, y_stride, subpel_search);

  besterr = vfp->ovf(pred, w, wsrc, mask, sse);
#endif
  return besterr;
}

static unsigned int upsampled_setup_obmc_center_error(
    MACROBLOCKD *xd, const AV1_COMMON *const cm, const int32_t *mask,
    const MV *bestmv, const MV *ref_mv, int error_per_bit,
    const aom_variance_fn_ptr_t *vfp, const int32_t *const wsrc,
    const uint8_t *const y, int y_stride, int w, int h, const int *mvjcost,
    const int *const mvcost[2], unsigned int *sse1, int *distortion,
    int subpel_search, MV_COST_TYPE mv_cost_type) {
  unsigned int besterr =
      upsampled_obmc_pref_error(xd, cm, bestmv, mask, vfp, wsrc, y, y_stride, 0,
                                0, w, h, sse1, subpel_search);
  *distortion = besterr;
  besterr +=
      mv_err_cost(bestmv, ref_mv, mvjcost, mvcost, error_per_bit, mv_cost_type);
  return besterr;
}

#define UNPACK_OBMC_MS_PARAMS                                               \
  const int allow_hp = ms_params->allow_hp;                                 \
  const int forced_stop = ms_params->forced_stop;                           \
  const int iters_per_step = ms_params->iters_per_step;                     \
  const MV *ref_mv = ms_params->mv_cost_params.ref_mv;                      \
  const int *mvjcost = ms_params->mv_cost_params.mvjcost;                   \
  const int *const *mvcost = ms_params->mv_cost_params.mvcost;              \
  const int error_per_bit = ms_params->mv_cost_params.error_per_bit;        \
  const MV_COST_TYPE mv_cost_type = ms_params->mv_cost_params.mv_cost_type; \
  const aom_variance_fn_ptr_t *vfp = ms_params->var_params.vfp;             \
  const SUBPEL_SEARCH_TYPE subpel_search_type =                             \
      ms_params->var_params.subpel_search_type;                             \
  const int w = ms_params->var_params.w;                                    \
  const int h = ms_params->var_params.h;

int av1_find_best_obmc_sub_pixel_tree_up(
    MACROBLOCK *x, const AV1_COMMON *const cm,
    const SUBPEL_MOTION_SEARCH_PARAMS *ms_params, int *distortion,
    unsigned int *sse1) {
  UNPACK_OBMC_MS_PARAMS;
  const int32_t *wsrc = x->wsrc_buf;
  const int32_t *mask = x->mask_buf;

  const int32_t *const src_address = wsrc;
  MACROBLOCKD *xd = &x->e_mbd;
  struct macroblockd_plane *const pd = &xd->plane[0];
  unsigned int besterr = INT_MAX;
  unsigned int sse;
  unsigned int thismse;
  const int y_stride = pd->pre[0].stride;
  const int offset = get_offset_from_mv(&x->best_mv.as_fullmv, y_stride);
  const uint8_t *y = pd->pre[0].buf;
  convert_fullmv_to_mv(&x->best_mv);
  MV *bestmv = &x->best_mv.as_mv;

  int br = bestmv->row;
  int bc = bestmv->col;
  int hstep = 4;
  int iter, round = FULL_PEL - forced_stop;
  int tr = br;
  int tc = bc;
  const MV *search_step = search_step_table;
  int idx, best_idx = -1;
  unsigned int cost_array[5];
  int kr, kc;

  SubpelMvLimits mv_limits;

  av1_set_subpel_mv_search_range(&mv_limits, &x->mv_limits, ref_mv);

  if (!allow_hp)
    if (round == 3) round = 2;

  if (subpel_search_type != USE_2_TAPS_ORIG)
    besterr = upsampled_setup_obmc_center_error(
        xd, cm, mask, bestmv, ref_mv, error_per_bit, vfp, src_address,
        y + offset, y_stride, w, h, mvjcost, mvcost, sse1, distortion,
        subpel_search_type, mv_cost_type);
  else
    besterr = setup_obmc_center_error(mask, bestmv, ref_mv, error_per_bit, vfp,
                                      src_address, y, y_stride, mvjcost, mvcost,
                                      sse1, distortion, mv_cost_type);

  for (iter = 0; iter < round; ++iter) {
    // Check vertical and horizontal sub-pixel positions.
    for (idx = 0; idx < 4; ++idx) {
      tr = br + search_step[idx].row;
      tc = bc + search_step[idx].col;
      MV this_mv = { tr, tc };
      if (av1_is_subpelmv_in_range(&mv_limits, this_mv)) {
        if (subpel_search_type != USE_2_TAPS_ORIG) {
          thismse = upsampled_obmc_pref_error(
              xd, cm, &this_mv, mask, vfp, src_address,
              pre(y, y_stride, tr, tc), y_stride, sp(tc), sp(tr), w, h, &sse,
              subpel_search_type);
        } else {
          thismse = vfp->osvf(pre(y, y_stride, tr, tc), y_stride, sp(tc),
                              sp(tr), src_address, mask, &sse);
        }

        cost_array[idx] =
            thismse + mv_err_cost(&this_mv, ref_mv, mvjcost, mvcost,
                                  error_per_bit, mv_cost_type);
        if (cost_array[idx] < besterr) {
          best_idx = idx;
          besterr = cost_array[idx];
          *distortion = thismse;
          *sse1 = sse;
        }
      } else {
        cost_array[idx] = INT_MAX;
      }
    }

    // Check diagonal sub-pixel position
    kc = (cost_array[0] <= cost_array[1] ? -hstep : hstep);
    kr = (cost_array[2] <= cost_array[3] ? -hstep : hstep);

    tc = bc + kc;
    tr = br + kr;
    {
      MV this_mv = { tr, tc };
      if (av1_is_subpelmv_in_range(&mv_limits, this_mv)) {
        if (subpel_search_type != USE_2_TAPS_ORIG) {
          thismse = upsampled_obmc_pref_error(
              xd, cm, &this_mv, mask, vfp, src_address,
              pre(y, y_stride, tr, tc), y_stride, sp(tc), sp(tr), w, h, &sse,
              subpel_search_type);
        } else {
          thismse = vfp->osvf(pre(y, y_stride, tr, tc), y_stride, sp(tc),
                              sp(tr), src_address, mask, &sse);
        }

        cost_array[4] = thismse + mv_err_cost(&this_mv, ref_mv, mvjcost, mvcost,
                                              error_per_bit, mv_cost_type);

        if (cost_array[4] < besterr) {
          best_idx = 4;
          besterr = cost_array[4];
          *distortion = thismse;
          *sse1 = sse;
        }
      } else {
        cost_array[idx] = INT_MAX;
      }
    }

    if (best_idx < 4 && best_idx >= 0) {
      br += search_step[best_idx].row;
      bc += search_step[best_idx].col;
    } else if (best_idx == 4) {
      br = tr;
      bc = tc;
    }

    if (iters_per_step > 1 && best_idx != -1) {
      if (subpel_search_type != USE_2_TAPS_ORIG) {
        SECOND_LEVEL_CHECKS_BEST(1);
      } else {
        SECOND_LEVEL_CHECKS_BEST(0);
      }
    }

    tr = br;
    tc = bc;

    search_step += 4;
    hstep >>= 1;
    best_idx = -1;
  }

  // These lines insure static analysis doesn't warn that
  // tr and tc aren't used after the above point.
  (void)tr;
  (void)tc;

  bestmv->row = br;
  bestmv->col = bc;

  return besterr;
}

#undef DIST
#undef MVC
#undef CHECK_BETTER

static int get_obmc_mvpred_var(const MACROBLOCK *x, const int32_t *wsrc,
                               const int32_t *mask, const FULLPEL_MV *best_mv,
                               const MV *ref_mv,
                               const aom_variance_fn_ptr_t *vfp) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const in_what = xd->plane[0].pre;
  const MV mv = get_mv_from_fullmv(best_mv);
  const MV_COST_TYPE mv_cost_type = x->mv_cost_type;
  unsigned int unused;

  return vfp->ovf(get_buf_from_mv(in_what, best_mv), in_what->stride, wsrc,
                  mask, &unused) +
         mv_err_cost(&mv, ref_mv, x->nmv_vec_cost,
                     CONVERT_TO_CONST_MVCOST(x->mv_cost_stack), x->errorperbit,
                     mv_cost_type);
}

static int obmc_refining_search_sad(const MACROBLOCK *x, const int32_t *wsrc,
                                    const int32_t *mask, FULLPEL_MV *start_mv,
                                    int error_per_bit, int search_range,
                                    const aom_variance_fn_ptr_t *fn_ptr,
                                    const MV *ref_mv) {
  const MV neighbors[4] = { { -1, 0 }, { 0, -1 }, { 0, 1 }, { 1, 0 } };
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const in_what = xd->plane[0].pre;
  const FULLPEL_MV full_ref_mv = get_fullmv_from_mv(ref_mv);
  unsigned int best_sad =
      fn_ptr->osdf(get_buf_from_mv(in_what, start_mv), in_what->stride, wsrc,
                   mask) +
      mvsad_err_cost(x, start_mv, &full_ref_mv, error_per_bit);
  int i, j;

  for (i = 0; i < search_range; i++) {
    int best_site = -1;

    for (j = 0; j < 4; j++) {
      const FULLPEL_MV mv = { start_mv->row + neighbors[j].row,
                              start_mv->col + neighbors[j].col };
      if (av1_is_fullmv_in_range(&x->mv_limits, mv)) {
        unsigned int sad = fn_ptr->osdf(get_buf_from_mv(in_what, &mv),
                                        in_what->stride, wsrc, mask);
        if (sad < best_sad) {
          sad += mvsad_err_cost(x, &mv, &full_ref_mv, error_per_bit);
          if (sad < best_sad) {
            best_sad = sad;
            best_site = j;
          }
        }
      }
    }

    if (best_site == -1) {
      break;
    } else {
      start_mv->row += neighbors[best_site].row;
      start_mv->col += neighbors[best_site].col;
    }
  }
  return best_sad;
}

static int obmc_diamond_search_sad(
    const MACROBLOCK *x, const search_site_config *cfg, const int32_t *wsrc,
    const int32_t *mask, FULLPEL_MV *start_mv, FULLPEL_MV *best_mv,
    int search_param, int sad_per_bit, int *num00,
    const aom_variance_fn_ptr_t *fn_ptr, const MV *ref_mv) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const in_what = xd->plane[0].pre;
  // search_param determines the length of the initial step and hence the number
  // of iterations
  // 0 = initial step (MAX_FIRST_STEP) pel : 1 = (MAX_FIRST_STEP/2) pel, 2 =
  // (MAX_FIRST_STEP/4) pel... etc.

  const int tot_steps = MAX_MVSEARCH_STEPS - 1 - search_param;
  const FULLPEL_MV full_ref_mv = get_fullmv_from_mv(ref_mv);
  const uint8_t *best_address, *in_what_ref;
  int best_sad = INT_MAX;
  int best_site = 0;
  int step;

  clamp_fullmv(start_mv, &x->mv_limits);
  in_what_ref = get_buf_from_mv(in_what, start_mv);
  best_address = in_what_ref;
  *num00 = 0;
  *best_mv = *start_mv;

  // Check the starting position
  best_sad = fn_ptr->osdf(best_address, in_what->stride, wsrc, mask) +
             mvsad_err_cost(x, best_mv, &full_ref_mv, sad_per_bit);

  for (step = tot_steps; step >= 0; --step) {
    const search_site *const ss = cfg->ss[step];
    best_site = 0;
    for (int idx = 1; idx <= cfg->searches_per_step[step]; ++idx) {
      const FULLPEL_MV mv = { best_mv->row + ss[idx].mv.row,
                              best_mv->col + ss[idx].mv.col };
      if (av1_is_fullmv_in_range(&x->mv_limits, mv)) {
        int sad = fn_ptr->osdf(best_address + ss[idx].offset, in_what->stride,
                               wsrc, mask);
        if (sad < best_sad) {
          sad += mvsad_err_cost(x, &mv, &full_ref_mv, sad_per_bit);
          if (sad < best_sad) {
            best_sad = sad;
            best_site = idx;
          }
        }
      }
    }

    if (best_site != 0) {
      best_mv->row += ss[best_site].mv.row;
      best_mv->col += ss[best_site].mv.col;
      best_address += ss[best_site].offset;
    } else if (best_address == in_what_ref) {
      (*num00)++;
    }
  }
  return best_sad;
}

static int obmc_full_pixel_diamond(const AV1_COMP *cpi, MACROBLOCK *x,
                                   FULLPEL_MV *start_mv, int step_param,
                                   int sadpb, int further_steps, int do_refine,
                                   const aom_variance_fn_ptr_t *fn_ptr,
                                   const MV *ref_mv, FULLPEL_MV *best_mv,
                                   const search_site_config *cfg) {
  (void)cpi;  // to silence compiler warning
  const int32_t *wsrc = x->wsrc_buf;
  const int32_t *mask = x->mask_buf;
  FULLPEL_MV tmp_mv;
  int thissme, n, num00 = 0;
  int bestsme = obmc_diamond_search_sad(x, cfg, wsrc, mask, start_mv, &tmp_mv,
                                        step_param, sadpb, &n, fn_ptr, ref_mv);
  if (bestsme < INT_MAX)
    bestsme = get_obmc_mvpred_var(x, wsrc, mask, &tmp_mv, ref_mv, fn_ptr);
  *best_mv = tmp_mv;

  // If there won't be more n-step search, check to see if refining search is
  // needed.
  if (n > further_steps) do_refine = 0;

  while (n < further_steps) {
    ++n;

    if (num00) {
      num00--;
    } else {
      thissme = obmc_diamond_search_sad(x, cfg, wsrc, mask, start_mv, &tmp_mv,
                                        step_param + n, sadpb, &num00, fn_ptr,
                                        ref_mv);
      if (thissme < INT_MAX)
        thissme = get_obmc_mvpred_var(x, wsrc, mask, &tmp_mv, ref_mv, fn_ptr);

      // check to see if refining search is needed.
      if (num00 > further_steps - n) do_refine = 0;

      if (thissme < bestsme) {
        bestsme = thissme;
        *best_mv = tmp_mv;
      }
    }
  }

  // final 1-away diamond refining search
  if (do_refine) {
    const int search_range = 8;
    tmp_mv = *best_mv;
    thissme = obmc_refining_search_sad(x, wsrc, mask, &tmp_mv, sadpb,
                                       search_range, fn_ptr, ref_mv);
    if (thissme < INT_MAX)
      thissme = get_obmc_mvpred_var(x, wsrc, mask, &tmp_mv, ref_mv, fn_ptr);
    if (thissme < bestsme) {
      bestsme = thissme;
      *best_mv = tmp_mv;
    }
  }
  return bestsme;
}

int av1_obmc_full_pixel_search(const AV1_COMP *cpi, MACROBLOCK *x,
                               FULLPEL_MV *start_mv, int step_param, int sadpb,
                               int further_steps, int do_refine,
                               const aom_variance_fn_ptr_t *fn_ptr,
                               const MV *ref_mv, FULLPEL_MV *best_mv,
                               const search_site_config *cfg) {
  if (cpi->sf.inter_sf.obmc_full_pixel_search_level == 0) {
    const int bestsme = obmc_full_pixel_diamond(cpi, x, start_mv, step_param,
                                                sadpb, further_steps, do_refine,
                                                fn_ptr, ref_mv, best_mv, cfg);
    return bestsme;
  } else {
    const int32_t *wsrc = x->wsrc_buf;
    const int32_t *mask = x->mask_buf;
    const int search_range = 8;
    *best_mv = *start_mv;
    clamp_fullmv(best_mv, &x->mv_limits);
    int thissme = obmc_refining_search_sad(x, wsrc, mask, best_mv, sadpb,
                                           search_range, fn_ptr, ref_mv);
    if (thissme < INT_MAX)
      thissme = get_obmc_mvpred_var(x, wsrc, mask, best_mv, ref_mv, fn_ptr);
    return thissme;
  }
}

// Note(yunqingwang): The following 2 functions are only used in the motion
// vector unit test, which return extreme motion vectors allowed by the MV
// limits.
// Return the maximum MV.
int av1_return_max_sub_pixel_mv(MACROBLOCK *x, const AV1_COMMON *const cm,
                                const SUBPEL_MOTION_SEARCH_PARAMS *ms_params,
                                int *distortion, unsigned int *sse1) {
  (void)cm;
  (void)sse1;
  (void)distortion;

  const int allow_hp = ms_params->allow_hp;
  const MV *ref_mv = ms_params->mv_cost_params.ref_mv;

  convert_fullmv_to_mv(&x->best_mv);
  MV *bestmv = &x->best_mv.as_mv;

  SubpelMvLimits mv_limits;
  av1_set_subpel_mv_search_range(&mv_limits, &x->mv_limits, ref_mv);

  bestmv->row = mv_limits.row_max;
  bestmv->col = mv_limits.col_max;

  unsigned int besterr = 0;

  // In the sub-pel motion search, if hp is not used, then the last bit of mv
  // has to be 0.
  lower_mv_precision(bestmv, allow_hp, 0);
  return besterr;
}
// Return the minimum MV.
int av1_return_min_sub_pixel_mv(MACROBLOCK *x, const AV1_COMMON *const cm,
                                const SUBPEL_MOTION_SEARCH_PARAMS *ms_params,
                                int *distortion, unsigned int *sse1) {
  (void)cm;
  (void)sse1;
  (void)distortion;

  const int allow_hp = ms_params->allow_hp;
  const MV *ref_mv = ms_params->mv_cost_params.ref_mv;

  convert_fullmv_to_mv(&x->best_mv);
  MV *bestmv = &x->best_mv.as_mv;

  SubpelMvLimits mv_limits;
  av1_set_subpel_mv_search_range(&mv_limits, &x->mv_limits, ref_mv);

  bestmv->row = mv_limits.row_min;
  bestmv->col = mv_limits.col_min;

  unsigned int besterr = 0;
  // In the sub-pel motion search, if hp is not used, then the last bit of mv
  // has to be 0.
  lower_mv_precision(bestmv, allow_hp, 0);
  return besterr;
}

void av1_simple_motion_search(AV1_COMP *const cpi, MACROBLOCK *x, int mi_row,
                              int mi_col, BLOCK_SIZE bsize, int ref,
                              FULLPEL_MV start_mv, int num_planes,
                              int use_subpixel) {
  assert(num_planes == 1 &&
         "Currently simple_motion_search only supports luma plane");
  assert(!frame_is_intra_only(&cpi->common) &&
         "Simple motion search only enabled for non-key frames");
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;

  set_offsets_for_motion_search(cpi, x, mi_row, mi_col, bsize);

  MB_MODE_INFO *mbmi = xd->mi[0];
  mbmi->sb_type = bsize;
  mbmi->ref_frame[0] = ref;
  mbmi->ref_frame[1] = NONE_FRAME;
  mbmi->motion_mode = SIMPLE_TRANSLATION;
  mbmi->interp_filters = av1_broadcast_interp_filter(EIGHTTAP_REGULAR);

  const YV12_BUFFER_CONFIG *yv12 = get_ref_frame_yv12_buf(cm, ref);
  const YV12_BUFFER_CONFIG *scaled_ref_frame =
      av1_get_scaled_ref_frame(cpi, ref);
  struct buf_2d backup_yv12;
  // ref_mv is used to calculate the cost of the motion vector
  const MV ref_mv = kZeroMv;
  const int step_param = cpi->mv_step_param;
  const FullMvLimits tmp_mv_limits = x->mv_limits;
  const SEARCH_METHODS search_methods = cpi->sf.mv_sf.search_method;
  const int do_mesh_search = 0;
  const int sadpb = x->sadperbit16;
  int cost_list[5];
  const int ref_idx = 0;
  int var;

  av1_setup_pre_planes(xd, ref_idx, yv12, mi_row, mi_col,
                       get_ref_scale_factors(cm, ref), num_planes);
  set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
  if (scaled_ref_frame) {
    backup_yv12 = xd->plane[AOM_PLANE_Y].pre[ref_idx];
    av1_setup_pre_planes(xd, ref_idx, scaled_ref_frame, mi_row, mi_col, NULL,
                         num_planes);
  }

  // This overwrites the mv_limits so we will need to restore it later.
  av1_set_mv_search_range(&x->mv_limits, &ref_mv);
  var = av1_full_pixel_search(
      cpi, x, bsize, &start_mv, step_param, search_methods, do_mesh_search,
      sadpb, cond_cost_list(cpi, cost_list), &ref_mv, INT_MAX, 1,
      mi_col * MI_SIZE, mi_row * MI_SIZE, 0, &cpi->ss_cfg[SS_CFG_SRC], 0);
  // Restore
  x->mv_limits = tmp_mv_limits;

  const int use_subpel_search =
      var < INT_MAX && !cpi->common.cur_frame_force_integer_mv && use_subpixel;
  if (scaled_ref_frame) {
    xd->plane[AOM_PLANE_Y].pre[ref_idx] = backup_yv12;
  }
  if (use_subpel_search) {
    int not_used = 0;

    const uint8_t *second_pred = NULL;
    const uint8_t *mask = NULL;
    const int mask_stride = 0;
    const int invert_mask = 0;
    const int reset_fractional_mv = 1;
    SUBPEL_MOTION_SEARCH_PARAMS ms_params;
    av1_make_default_subpel_ms_params(&ms_params, cpi, x, bsize, &ref_mv,
                                      cost_list, second_pred, mask, mask_stride,
                                      invert_mask, reset_fractional_mv);

    cpi->find_fractional_mv_step(x, cm, &ms_params, &not_used,
                                 &x->pred_sse[ref]);
  } else {
    // Manually convert from units of pixel to 1/8-pixels if we are not doing
    // subpel search
    x->best_mv.as_mv = get_mv_from_fullmv(&x->best_mv.as_fullmv);
  }

  mbmi->mv[0] = x->best_mv;

  // Get a copy of the prediction output
  av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, NULL, bsize,
                                AOM_PLANE_Y, AOM_PLANE_Y);

  aom_clear_system_state();

  if (scaled_ref_frame) {
    xd->plane[AOM_PLANE_Y].pre[ref_idx] = backup_yv12;
  }
}

void av1_simple_motion_sse_var(AV1_COMP *cpi, MACROBLOCK *x, int mi_row,
                               int mi_col, BLOCK_SIZE bsize,
                               const FULLPEL_MV start_mv, int use_subpixel,
                               unsigned int *sse, unsigned int *var) {
  MACROBLOCKD *xd = &x->e_mbd;
  const MV_REFERENCE_FRAME ref =
      cpi->rc.is_src_frame_alt_ref ? ALTREF_FRAME : LAST_FRAME;

  av1_simple_motion_search(cpi, x, mi_row, mi_col, bsize, ref, start_mv, 1,
                           use_subpixel);

  const uint8_t *src = x->plane[0].src.buf;
  const int src_stride = x->plane[0].src.stride;
  const uint8_t *dst = xd->plane[0].dst.buf;
  const int dst_stride = xd->plane[0].dst.stride;

  *var = cpi->fn_ptr[bsize].vf(src, src_stride, dst, dst_stride, sse);
}
