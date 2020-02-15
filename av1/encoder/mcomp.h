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

#ifndef AOM_AV1_ENCODER_MCOMP_H_
#define AOM_AV1_ENCODER_MCOMP_H_

#include "av1/common/mv.h"
#include "av1/encoder/block.h"

#include "aom_dsp/variance.h"

#ifdef __cplusplus
extern "C" {
#endif

// In this file, the following variables always have the same meaning:
// start_mv: the motion vector where we start the motion search
// ref_mv: the motion vector with respect to which we calculate the mv_cost
// best_mv: when it is not const, it is the destination where to store the
//   best motion vector
// full_*: a prefix of full indicates that the mv is a FULLPEL_MV
//
// When a mv needs to both act as a fullpel_mv and subpel_mv, it is stored as an
// int_mv, which is a union of int, FULLPEL_MV, and MV

// The maximum number of steps in a step search given the largest
// allowed initial step
#define MAX_MVSEARCH_STEPS 11
// Max full pel mv specified in the unit of full pixel
// Enable the use of motion vector in range [-1023, 1023].
#define MAX_FULL_PEL_VAL ((1 << (MAX_MVSEARCH_STEPS - 1)) - 1)
// Maximum size of the first step in full pel units
#define MAX_FIRST_STEP (1 << (MAX_MVSEARCH_STEPS - 1))
// Allowed motion vector pixel distance outside image border
// for Block_16x16
#define BORDER_MV_PIXELS_B16 (16 + AOM_INTERP_EXTEND)

#define SEARCH_RANGE_8P 3
#define SEARCH_GRID_STRIDE_8P (2 * SEARCH_RANGE_8P + 1)
#define SEARCH_GRID_CENTER_8P \
  (SEARCH_RANGE_8P * SEARCH_GRID_STRIDE_8P + SEARCH_RANGE_8P)

// motion search site
typedef struct search_site {
  FULLPEL_MV mv;
  int offset;
} search_site;

typedef struct search_site_config {
  search_site ss[MAX_MVSEARCH_STEPS * 2][16 + 1];
  int ss_count;
  int searches_per_step[MAX_MVSEARCH_STEPS * 2];
  int radius[MAX_MVSEARCH_STEPS * 2];
  int stride;
} search_site_config;

typedef struct {
  MV coord;
  int coord_offset;
} search_neighbors;

void av1_init_dsmotion_compensation(search_site_config *cfg, int stride);
void av1_init_motion_fpf(search_site_config *cfg, int stride);
void av1_init3smotion_compensation(search_site_config *cfg, int stride);

void av1_set_mv_search_range(FullMvLimits *mv_limits, const MV *mv);

int av1_mv_bit_cost(const MV *mv, const MV *ref_mv, const int *mvjcost,
                    int *mvcost[2], int weight);

// Utility to compute variance + MV rate cost for a given MV
int av1_get_mvpred_sse(const MACROBLOCK *x, const FULLPEL_MV *best_mv,
                       const MV *ref_mv, const aom_variance_fn_ptr_t *vfp);
int av1_get_mvpred_var(const MACROBLOCK *x, const FULLPEL_MV *best_mv,
                       const MV *ref_mv, const aom_variance_fn_ptr_t *vfp);
int av1_get_mvpred_av_var(const MACROBLOCK *x, const FULLPEL_MV *best_mv,
                          const MV *ref_mv, const uint8_t *second_pred,
                          const aom_variance_fn_ptr_t *vfp,
                          const struct buf_2d *src, const struct buf_2d *pre);
int av1_get_mvpred_mask_var(const MACROBLOCK *x, const FULLPEL_MV *best_mv,
                            const MV *ref_mv, const uint8_t *second_pred,
                            const uint8_t *mask, int mask_stride,
                            int invert_mask, const aom_variance_fn_ptr_t *vfp,
                            const struct buf_2d *src, const struct buf_2d *pre);

struct AV1_COMP;
struct SPEED_FEATURES;

int av1_init_search_range(int size);

unsigned int av1_int_pro_motion_estimation(const struct AV1_COMP *cpi,
                                           MACROBLOCK *x, BLOCK_SIZE bsize,
                                           int mi_row, int mi_col,
                                           const MV *ref_mv);

// Runs sequence of diamond searches in smaller steps for RD.
int av1_hex_search(MACROBLOCK *x, FULLPEL_MV *start_mv, int search_param,
                   int sad_per_bit, int do_init_search, int *cost_list,
                   const aom_variance_fn_ptr_t *vfp, const MV *ref_mv);

enum {
  EIGHTH_PEL,
  QUARTER_PEL,
  HALF_PEL,
  FULL_PEL
} UENUM1BYTE(SUBPEL_FORCE_STOP);

typedef struct {
  const MV *ref_mv;
  const int *mvjcost;
  const int *mvcost[2];
  int error_per_bit;
  MV_COST_TYPE mv_cost_type;
} MV_COST_PARAMS;

typedef struct {
  const aom_variance_fn_ptr_t *vfp;
  SUBPEL_SEARCH_TYPE subpel_search_type;
  const uint8_t *second_pred;
  const uint8_t *mask;
  int mask_stride;
  int invert_mask;
  int w, h;
} SUBPEL_SEARCH_VAR_PARAMS;

// This struct holds subpixel motion search parameters that should be constant
// during the search
typedef struct {
  // High level motion search settings
  int allow_hp;
  const int *cost_list;
  SUBPEL_FORCE_STOP forced_stop;
  int iters_per_step;
  int do_reset_fractional_mv;

  // For calculating mv cost
  MV_COST_PARAMS mv_cost_params;

  // Distortion calculation params
  SUBPEL_SEARCH_VAR_PARAMS var_params;
} SUBPEL_MOTION_SEARCH_PARAMS;

void av1_make_default_subpel_ms_params(
    SUBPEL_MOTION_SEARCH_PARAMS *ms_params, const struct AV1_COMP *cpi,
    const MACROBLOCK *x, BLOCK_SIZE bsize, const MV *ref_mv,
    const int *cost_list, const uint8_t *second_pred, const uint8_t *mask,
    int mask_stride, int invert_mask, int do_reset_fractional_mv);

typedef int(fractional_mv_step_fp)(MACROBLOCK *x, const AV1_COMMON *const cm,
                                   const SUBPEL_MOTION_SEARCH_PARAMS *ms_params,
                                   int *distortion, unsigned int *sse1);

extern fractional_mv_step_fp av1_find_best_sub_pixel_tree;
extern fractional_mv_step_fp av1_find_best_sub_pixel_tree_pruned;
extern fractional_mv_step_fp av1_find_best_sub_pixel_tree_pruned_more;
extern fractional_mv_step_fp av1_find_best_sub_pixel_tree_pruned_evenmore;
extern fractional_mv_step_fp av1_return_max_sub_pixel_mv;
extern fractional_mv_step_fp av1_return_min_sub_pixel_mv;

int av1_refining_search_8p_c(MACROBLOCK *x, int error_per_bit, int search_range,
                             const aom_variance_fn_ptr_t *fn_ptr,
                             const uint8_t *mask, int mask_stride,
                             int invert_mask, const MV *ref_mv,
                             const uint8_t *second_pred,
                             const struct buf_2d *src,
                             const struct buf_2d *pre);

int av1_diamond_search_sad_c(MACROBLOCK *x, const search_site_config *cfg,
                             FULLPEL_MV *start_mv, FULLPEL_MV *best_mv,
                             int search_param, int sad_per_bit, int *num00,
                             const aom_variance_fn_ptr_t *fn_ptr,
                             const MV *ref_mv, uint8_t *second_pred,
                             uint8_t *mask, int mask_stride, int inv_mask);

int av1_full_pixel_search(const struct AV1_COMP *cpi, MACROBLOCK *x,
                          BLOCK_SIZE bsize, FULLPEL_MV *start_mv,
                          int step_param, int method, int run_mesh_search,
                          int error_per_bit, int *cost_list, const MV *ref_mv,
                          int var_max, int rd, int x_pos, int y_pos, int intra,
                          const search_site_config *cfg,
                          int use_intrabc_mesh_pattern);

int av1_obmc_full_pixel_search(const struct AV1_COMP *cpi, MACROBLOCK *x,
                               FULLPEL_MV *start_mv, int step_param, int sadpb,
                               int further_steps, int do_refine,
                               const aom_variance_fn_ptr_t *fn_ptr,
                               const MV *ref_mv, FULLPEL_MV *dst_mv,
                               const search_site_config *cfg);

extern fractional_mv_step_fp av1_find_best_obmc_sub_pixel_tree_up;

unsigned int av1_compute_motion_cost(const struct AV1_COMP *cpi,
                                     MACROBLOCK *const x, BLOCK_SIZE bsize,
                                     const MV *this_mv);
unsigned int av1_refine_warped_mv(const struct AV1_COMP *cpi,
                                  MACROBLOCK *const x, BLOCK_SIZE bsize,
                                  int *pts0, int *pts_inref0,
                                  int total_samples);

// Performs a motion search in SIMPLE_TRANSLATION mode using reference frame
// ref. Note that this sets the offset of mbmi, so we will need to reset it
// after calling this function.
void av1_simple_motion_search(struct AV1_COMP *const cpi, MACROBLOCK *x,
                              int mi_row, int mi_col, BLOCK_SIZE bsize, int ref,
                              FULLPEL_MV start_mv, int num_planes,
                              int use_subpixel);

// Performs a simple motion search to calculate the sse and var of the residue
void av1_simple_motion_sse_var(struct AV1_COMP *cpi, MACROBLOCK *x, int mi_row,
                               int mi_col, BLOCK_SIZE bsize,
                               const FULLPEL_MV start_mv, int use_subpixel,
                               unsigned int *sse, unsigned int *var);

static INLINE void av1_set_fractional_mv(int_mv *fractional_best_mv) {
  for (int z = 0; z < 3; z++) {
    fractional_best_mv[z].as_int = INVALID_MV;
  }
}

static INLINE void av1_set_subpel_mv_search_range(SubpelMvLimits *subpel_limits,
                                                  const FullMvLimits *mv_limits,
                                                  const MV *ref_mv) {
  const int max_mv = GET_MV_SUBPEL(MAX_FULL_PEL_VAL);
  const int minc =
      AOMMAX(GET_MV_SUBPEL(mv_limits->col_min), ref_mv->col - max_mv);
  const int maxc =
      AOMMIN(GET_MV_SUBPEL(mv_limits->col_max), ref_mv->col + max_mv);
  const int minr =
      AOMMAX(GET_MV_SUBPEL(mv_limits->row_min), ref_mv->row - max_mv);
  const int maxr =
      AOMMIN(GET_MV_SUBPEL(mv_limits->row_max), ref_mv->row + max_mv);

  subpel_limits->col_min = AOMMAX(MV_LOW + 1, minc);
  subpel_limits->col_max = AOMMIN(MV_UPP - 1, maxc);
  subpel_limits->row_min = AOMMAX(MV_LOW + 1, minr);
  subpel_limits->row_max = AOMMIN(MV_UPP - 1, maxr);
}

static INLINE int av1_is_fullmv_in_range(const FullMvLimits *mv_limits,
                                         FULLPEL_MV mv) {
  return (mv.col >= mv_limits->col_min) && (mv.col <= mv_limits->col_max) &&
         (mv.row >= mv_limits->row_min) && (mv.row <= mv_limits->row_max);
}

static INLINE int av1_is_subpelmv_in_range(const SubpelMvLimits *mv_limits,
                                           MV mv) {
  return (mv.col >= mv_limits->col_min) && (mv.col <= mv_limits->col_max) &&
         (mv.row >= mv_limits->row_min) && (mv.row <= mv_limits->row_max);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_MCOMP_H_
