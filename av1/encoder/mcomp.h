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

// The maximum number of steps in a step search given the largest
// allowed initial step
#define MAX_MVSEARCH_STEPS 11
// Max full pel mv specified in the unit of full pixel
// Enable the use of motion vector in range [-1023, 1023].
#define MAX_FULL_PEL_VAL ((1 << (MAX_MVSEARCH_STEPS - 1)) - 1)
// Maximum size of the first step in full pel units
#define MAX_FIRST_STEP (1 << (MAX_MVSEARCH_STEPS - 1))

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
  FULLPEL_MV coord;
  int coord_offset;
} search_neighbors;

struct AV1_COMP;
struct SPEED_FEATURES;

// =============================================================================
//  Cost functions
// =============================================================================
typedef struct {
  const MV *ref_mv;
  const int *mvjcost;
  const int *mvcost[2];
  int error_per_bit;
  MV_COST_TYPE mv_cost_type;
} MV_COST_PARAMS;

int av1_mv_bit_cost(const MV *mv, const MV *ref_mv, const int *mvjcost,
                    int *mvcost[2], int weight);

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

unsigned int av1_compute_motion_cost(const struct AV1_COMP *cpi,
                                     MACROBLOCK *const x, BLOCK_SIZE bsize,
                                     const MV *this_mv);

// =============================================================================
//  Fullpixel Motion Search
// =============================================================================
// Sets up configs for fullpixel diamond search
void av1_init_dsmotion_compensation(search_site_config *cfg, int stride);
// Sets up configs for firstpass motion search
void av1_init_motion_fpf(search_site_config *cfg, int stride);
// Sets up configs for all other types of motion search
void av1_init3smotion_compensation(search_site_config *cfg, int stride);

// Set up limit values for MV components.
// Mv beyond the range do not produce new/different prediction block.
static INLINE void av1_set_mv_row_limits(const AV1_COMMON *const cm,
                                         FullMvLimits *mv_limits, int mi_row,
                                         int mi_height, int border) {
  const int min1 = -(mi_row * MI_SIZE + border - 2 * AOM_INTERP_EXTEND);
  const int min2 = -(((mi_row + mi_height) * MI_SIZE) + 2 * AOM_INTERP_EXTEND);
  mv_limits->row_min = AOMMAX(min1, min2);
  const int max1 = (cm->mi_rows - mi_row - mi_height) * MI_SIZE + border -
                   2 * AOM_INTERP_EXTEND;
  const int max2 = (cm->mi_rows - mi_row) * MI_SIZE + 2 * AOM_INTERP_EXTEND;
  mv_limits->row_max = AOMMIN(max1, max2);
}

static INLINE void av1_set_mv_col_limits(const AV1_COMMON *const cm,
                                         FullMvLimits *mv_limits, int mi_col,
                                         int mi_width, int border) {
  const int min1 = -(mi_col * MI_SIZE + border - 2 * AOM_INTERP_EXTEND);
  const int min2 = -(((mi_col + mi_width) * MI_SIZE) + 2 * AOM_INTERP_EXTEND);
  mv_limits->col_min = AOMMAX(min1, min2);
  const int max1 = (cm->mi_cols - mi_col - mi_width) * MI_SIZE + border -
                   2 * AOM_INTERP_EXTEND;
  const int max2 = (cm->mi_cols - mi_col) * MI_SIZE + 2 * AOM_INTERP_EXTEND;
  mv_limits->col_max = AOMMIN(max1, max2);
}

static INLINE void av1_set_mv_limits(const AV1_COMMON *const cm,
                                     FullMvLimits *mv_limits, int mi_row,
                                     int mi_col, int mi_height, int mi_width,
                                     int border) {
  av1_set_mv_row_limits(cm, mv_limits, mi_row, mi_height, border);
  av1_set_mv_col_limits(cm, mv_limits, mi_col, mi_width, border);
}

void av1_set_mv_search_range(FullMvLimits *mv_limits, const MV *mv);

int av1_init_search_range(int size);

unsigned int av1_int_pro_motion_estimation(const struct AV1_COMP *cpi,
                                           MACROBLOCK *x, BLOCK_SIZE bsize,
                                           int mi_row, int mi_col,
                                           const MV *ref_mv);

int av1_refining_search_8p_c(MACROBLOCK *x, int error_per_bit, int search_range,
                             const aom_variance_fn_ptr_t *fn_ptr,
                             const uint8_t *mask, int mask_stride,
                             int invert_mask, const MV *ref_mv,
                             const uint8_t *second_pred,
                             const struct buf_2d *src,
                             const struct buf_2d *pre);

int av1_full_pixel_search(const struct AV1_COMP *cpi, const MACROBLOCK *x,
                          BLOCK_SIZE bsize, const FULLPEL_MV start_mv,
                          int step_param, int method, int run_mesh_search,
                          int error_per_bit, int *cost_list, const MV *ref_mv,
                          int is_intra_mode, const search_site_config *cfg,
                          FULLPEL_MV *best_mv, FULLPEL_MV *second_best_mv);

void av1_intrabc_hash_search(const struct AV1_COMP *cpi, MACROBLOCK *x,
                             BLOCK_SIZE bsize, const MV *ref_mv, int *bestsme,
                             FULLPEL_MV *best_mv);

int av1_obmc_full_pixel_search(const struct AV1_COMP *cpi, const MACROBLOCK *x,
                               const FULLPEL_MV start_mv, int step_param,
                               int sadpb, const aom_variance_fn_ptr_t *fn_ptr,
                               const MV *ref_mv, FULLPEL_MV *best_mv,
                               const search_site_config *cfg);

unsigned int av1_refine_warped_mv(const struct AV1_COMP *cpi,
                                  MACROBLOCK *const x, BLOCK_SIZE bsize,
                                  int *pts0, int *pts_inref0,
                                  int total_samples);

static INLINE int av1_is_fullmv_in_range(const FullMvLimits *mv_limits,
                                         FULLPEL_MV mv) {
  return (mv.col >= mv_limits->col_min) && (mv.col <= mv_limits->col_max) &&
         (mv.row >= mv_limits->row_min) && (mv.row <= mv_limits->row_max);
}
// =============================================================================
//  Subpixel Motion Search
// =============================================================================
enum {
  EIGHTH_PEL,
  QUARTER_PEL,
  HALF_PEL,
  FULL_PEL
} UENUM1BYTE(SUBPEL_FORCE_STOP);

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
extern fractional_mv_step_fp av1_find_best_obmc_sub_pixel_tree_up;

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

static INLINE int av1_is_subpelmv_in_range(const SubpelMvLimits *mv_limits,
                                           MV mv) {
  return (mv.col >= mv_limits->col_min) && (mv.col <= mv_limits->col_max) &&
         (mv.row >= mv_limits->row_min) && (mv.row <= mv_limits->row_max);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_MCOMP_H_
