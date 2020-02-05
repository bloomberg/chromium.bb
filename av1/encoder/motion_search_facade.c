/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/common/reconinter.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/motion_search_facade.h"
#include "av1/encoder/reconinter_enc.h"

void av1_single_motion_search(const AV1_COMP *const cpi, MACROBLOCK *x,
                              BLOCK_SIZE bsize, int ref_idx, int *rate_mv,
                              int search_range) {
  MACROBLOCKD *xd = &x->e_mbd;
  const AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MB_MODE_INFO *mbmi = xd->mi[0];
  struct buf_2d backup_yv12[MAX_MB_PLANE] = { { 0, 0, 0, 0, 0 } };
  int bestsme = INT_MAX;
  const int ref = mbmi->ref_frame[ref_idx];
  MvLimits tmp_mv_limits = x->mv_limits;
  const YV12_BUFFER_CONFIG *scaled_ref_frame =
      av1_get_scaled_ref_frame(cpi, ref);
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;

  if (scaled_ref_frame) {
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // full-pixel motion search code to be used without additional
    // modifications.
    for (int i = 0; i < num_planes; i++) {
      backup_yv12[i] = xd->plane[i].pre[ref_idx];
    }
    av1_setup_pre_planes(xd, ref_idx, scaled_ref_frame, mi_row, mi_col, NULL,
                         num_planes);
  }

  // Work out the size of the first step in the mv step search.
  // 0 here is maximum length first step. 1 is AOMMAX >> 1 etc.
  int step_param;
  if (cpi->sf.mv_sf.auto_mv_step_size && cm->show_frame) {
    // Take the weighted average of the step_params based on the last frame's
    // max mv magnitude and that based on the best ref mvs of the current
    // block for the given reference.
    step_param =
        (av1_init_search_range(x->max_mv_context[ref]) + cpi->mv_step_param) /
        2;
  } else {
    step_param = cpi->mv_step_param;
  }

  if (cpi->sf.mv_sf.adaptive_motion_search && bsize < cm->seq_params.sb_size) {
    int boffset =
        2 * (mi_size_wide_log2[cm->seq_params.sb_size] -
             AOMMIN(mi_size_high_log2[bsize], mi_size_wide_log2[bsize]));
    step_param = AOMMAX(step_param, boffset);
  }

  if (cpi->sf.mv_sf.adaptive_motion_search) {
    int bwl = mi_size_wide_log2[bsize];
    int bhl = mi_size_high_log2[bsize];
    int tlevel = x->pred_mv_sad[ref] >> (bwl + bhl + 4);

    if (tlevel < 5) {
      step_param += 2;
      step_param = AOMMIN(step_param, MAX_MVSEARCH_STEPS - 1);
    }

    // prev_mv_sad is not setup for dynamically scaled frames.
    if (cpi->oxcf.resize_mode != RESIZE_RANDOM) {
      int i;
      for (i = LAST_FRAME; i <= ALTREF_FRAME && cm->show_frame; ++i) {
        if ((x->pred_mv_sad[ref] >> 3) > x->pred_mv_sad[i]) {
          x->pred_mv[ref].row = 0;
          x->pred_mv[ref].col = 0;
          x->best_mv.as_int = INVALID_MV;

          if (scaled_ref_frame) {
            // Swap back the original buffers before returning.
            for (int j = 0; j < num_planes; ++j)
              xd->plane[j].pre[ref_idx] = backup_yv12[j];
          }
          return;
        }
      }
    }
  }

  const MV ref_mv = av1_get_ref_mv(x, ref_idx).as_mv;

  // Further reduce the search range.
  if (search_range < INT_MAX) {
    const search_site_config *ss_cfg = &cpi->ss_cfg[SS_CFG_SRC];
    // MAx step_param is ss_cfg->ss_count.
    if (search_range < 1) {
      step_param = ss_cfg->ss_count;
    } else {
      while (ss_cfg->radius[ss_cfg->ss_count - step_param - 1] >
                 (search_range << 1) &&
             ss_cfg->ss_count - step_param - 1 > 0)
        step_param++;
    }
  }

  // Note: MV limits are modified here. Always restore the original values
  // after full-pixel motion search.
  av1_set_mv_search_range(&x->mv_limits, &ref_mv);

  FULLPEL_MV start_mv;
  if (mbmi->motion_mode != SIMPLE_TRANSLATION)
    start_mv = get_fullmv_from_mv(&mbmi->mv[0].as_mv);
  else
    start_mv = get_fullmv_from_mv(&ref_mv);

  const int sadpb = x->sadperbit16;
  int cost_list[5];
  x->best_mv.as_int = x->second_best_mv.as_int = INVALID_MV;
  switch (mbmi->motion_mode) {
    case SIMPLE_TRANSLATION:
      bestsme = av1_full_pixel_search(
          cpi, x, bsize, &start_mv, step_param, 1, cpi->sf.mv_sf.search_method,
          0, sadpb, cond_cost_list(cpi, cost_list), &ref_mv, INT_MAX, 1,
          (MI_SIZE * mi_col), (MI_SIZE * mi_row), 0, &cpi->ss_cfg[SS_CFG_SRC],
          0);
      break;
    case OBMC_CAUSAL:
      bestsme = av1_obmc_full_pixel_search(
          cpi, x, &start_mv, step_param, sadpb,
          MAX_MVSEARCH_STEPS - 1 - step_param, 1, &cpi->fn_ptr[bsize], &ref_mv,
          &(x->best_mv.as_fullmv), 0, &cpi->ss_cfg[SS_CFG_SRC]);
      break;
    default: assert(0 && "Invalid motion mode!\n");
  }

  if (scaled_ref_frame) {
    // Swap back the original buffers for subpel motion search.
    for (int i = 0; i < num_planes; i++) {
      xd->plane[i].pre[ref_idx] = backup_yv12[i];
    }
  }

  x->mv_limits = tmp_mv_limits;

  if (cpi->common.cur_frame_force_integer_mv) {
    x->best_mv.as_mv.row *= 8;
    x->best_mv.as_mv.col *= 8;
  }
  const int use_fractional_mv =
      bestsme < INT_MAX && cpi->common.cur_frame_force_integer_mv == 0;
  if (use_fractional_mv) {
    int dis; /* TODO: use dis in distortion calculation later. */
    switch (mbmi->motion_mode) {
      case SIMPLE_TRANSLATION:
        if (cpi->sf.mv_sf.use_accurate_subpel_search) {
          const int try_second = x->second_best_mv.as_int != INVALID_MV &&
                                 x->second_best_mv.as_int != x->best_mv.as_int;
          const int pw = block_size_wide[bsize];
          const int ph = block_size_high[bsize];
          const int best_mv_var = cpi->find_fractional_mv_step(
              x, cm, mi_row, mi_col, &ref_mv, cm->allow_high_precision_mv,
              x->errorperbit, &cpi->fn_ptr[bsize],
              cpi->sf.mv_sf.subpel_force_stop,
              cpi->sf.mv_sf.subpel_iters_per_step,
              cond_cost_list(cpi, cost_list), x->nmv_vec_cost, x->mv_cost_stack,
              &dis, &x->pred_sse[ref], NULL, NULL, 0, 0, pw, ph,
              cpi->sf.mv_sf.use_accurate_subpel_search, 1);

          if (try_second) {
            const int minc =
                AOMMAX(x->mv_limits.col_min * 8, ref_mv.col - MV_MAX);
            const int maxc =
                AOMMIN(x->mv_limits.col_max * 8, ref_mv.col + MV_MAX);
            const int minr =
                AOMMAX(x->mv_limits.row_min * 8, ref_mv.row - MV_MAX);
            const int maxr =
                AOMMIN(x->mv_limits.row_max * 8, ref_mv.row + MV_MAX);
            MV best_mv = x->best_mv.as_mv;

            x->best_mv = x->second_best_mv;
            if (x->best_mv.as_mv.row * 8 <= maxr &&
                x->best_mv.as_mv.row * 8 >= minr &&
                x->best_mv.as_mv.col * 8 <= maxc &&
                x->best_mv.as_mv.col * 8 >= minc) {
              const int this_var = cpi->find_fractional_mv_step(
                  x, cm, mi_row, mi_col, &ref_mv, cm->allow_high_precision_mv,
                  x->errorperbit, &cpi->fn_ptr[bsize],
                  cpi->sf.mv_sf.subpel_force_stop,
                  cpi->sf.mv_sf.subpel_iters_per_step,
                  cond_cost_list(cpi, cost_list), x->nmv_vec_cost,
                  x->mv_cost_stack, &dis, &x->pred_sse[ref], NULL, NULL, 0, 0,
                  pw, ph, cpi->sf.mv_sf.use_accurate_subpel_search, 0);
              if (this_var < best_mv_var) best_mv = x->best_mv.as_mv;
            }
            x->best_mv.as_mv = best_mv;
          }
        } else {
          cpi->find_fractional_mv_step(
              x, cm, mi_row, mi_col, &ref_mv, cm->allow_high_precision_mv,
              x->errorperbit, &cpi->fn_ptr[bsize],
              cpi->sf.mv_sf.subpel_force_stop,
              cpi->sf.mv_sf.subpel_iters_per_step,
              cond_cost_list(cpi, cost_list), x->nmv_vec_cost, x->mv_cost_stack,
              &dis, &x->pred_sse[ref], NULL, NULL, 0, 0, 0, 0, 0, 1);
        }
        break;
      case OBMC_CAUSAL:
        av1_find_best_obmc_sub_pixel_tree_up(
            x, cm, mi_row, mi_col, &x->best_mv.as_mv, &ref_mv,
            cm->allow_high_precision_mv, x->errorperbit, &cpi->fn_ptr[bsize],
            cpi->sf.mv_sf.subpel_force_stop,
            cpi->sf.mv_sf.subpel_iters_per_step, x->nmv_vec_cost,
            x->mv_cost_stack, &dis, &x->pred_sse[ref], 0,
            cpi->sf.mv_sf.use_accurate_subpel_search);
        break;
      default: assert(0 && "Invalid motion mode!\n");
    }
  }
  *rate_mv = av1_mv_bit_cost(&x->best_mv.as_mv, &ref_mv, x->nmv_vec_cost,
                             x->mv_cost_stack, MV_COST_WEIGHT);

  if (cpi->sf.mv_sf.adaptive_motion_search &&
      mbmi->motion_mode == SIMPLE_TRANSLATION)
    x->pred_mv[ref] = x->best_mv.as_mv;
}

void av1_joint_motion_search(const AV1_COMP *cpi, MACROBLOCK *x,
                             BLOCK_SIZE bsize, int_mv *cur_mv,
                             const uint8_t *mask, int mask_stride,
                             int *rate_mv) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const int pw = block_size_wide[bsize];
  const int ph = block_size_high[bsize];
  const int plane = 0;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  // This function should only ever be called for compound modes
  assert(has_second_ref(mbmi));
  const int_mv init_mv[2] = { cur_mv[0], cur_mv[1] };
  const int refs[2] = { mbmi->ref_frame[0], mbmi->ref_frame[1] };
  int_mv ref_mv[2];
  int ite, ref;

  // Get the prediction block from the 'other' reference frame.
  const int_interpfilters interp_filters =
      av1_broadcast_interp_filter(EIGHTTAP_REGULAR);

  InterPredParams inter_pred_params;
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;

  // Do joint motion search in compound mode to get more accurate mv.
  struct buf_2d backup_yv12[2][MAX_MB_PLANE];
  int last_besterr[2] = { INT_MAX, INT_MAX };
  const YV12_BUFFER_CONFIG *const scaled_ref_frame[2] = {
    av1_get_scaled_ref_frame(cpi, refs[0]),
    av1_get_scaled_ref_frame(cpi, refs[1])
  };

  // Prediction buffer from second frame.
  DECLARE_ALIGNED(16, uint8_t, second_pred16[MAX_SB_SQUARE * sizeof(uint16_t)]);
  uint8_t *second_pred = get_buf_by_bd(xd, second_pred16);
  int_mv *best_int_mv = &x->best_mv;

  const int search_range = SEARCH_RANGE_8P;
  const int sadpb = x->sadperbit16;
  // Allow joint search multiple times iteratively for each reference frame
  // and break out of the search loop if it couldn't find a better mv.
  for (ite = 0; ite < 4; ite++) {
    struct buf_2d ref_yv12[2];
    int bestsme = INT_MAX;
    MvLimits tmp_mv_limits = x->mv_limits;
    int id = ite % 2;  // Even iterations search in the first reference frame,
                       // odd iterations search in the second. The predictor
                       // found for the 'other' reference frame is factored in.
    if (ite >= 2 && cur_mv[!id].as_int == init_mv[!id].as_int) {
      if (cur_mv[id].as_int == init_mv[id].as_int) {
        break;
      } else {
        int_mv cur_int_mv, init_int_mv;
        cur_int_mv.as_mv.col = cur_mv[id].as_mv.col >> 3;
        cur_int_mv.as_mv.row = cur_mv[id].as_mv.row >> 3;
        init_int_mv.as_mv.row = init_mv[id].as_mv.row >> 3;
        init_int_mv.as_mv.col = init_mv[id].as_mv.col >> 3;
        if (cur_int_mv.as_int == init_int_mv.as_int) {
          break;
        }
      }
    }
    for (ref = 0; ref < 2; ++ref) {
      ref_mv[ref] = av1_get_ref_mv(x, ref);
      // Swap out the reference frame for a version that's been scaled to
      // match the resolution of the current frame, allowing the existing
      // motion search code to be used without additional modifications.
      if (scaled_ref_frame[ref]) {
        int i;
        for (i = 0; i < num_planes; i++)
          backup_yv12[ref][i] = xd->plane[i].pre[ref];
        av1_setup_pre_planes(xd, ref, scaled_ref_frame[ref], mi_row, mi_col,
                             NULL, num_planes);
      }
    }

    assert(IMPLIES(scaled_ref_frame[0] != NULL,
                   cm->width == scaled_ref_frame[0]->y_crop_width &&
                       cm->height == scaled_ref_frame[0]->y_crop_height));
    assert(IMPLIES(scaled_ref_frame[1] != NULL,
                   cm->width == scaled_ref_frame[1]->y_crop_width &&
                       cm->height == scaled_ref_frame[1]->y_crop_height));

    // Initialize based on (possibly scaled) prediction buffers.
    ref_yv12[0] = xd->plane[plane].pre[0];
    ref_yv12[1] = xd->plane[plane].pre[1];

    av1_init_inter_params(&inter_pred_params, pw, ph, mi_row * MI_SIZE,
                          mi_col * MI_SIZE, 0, 0, xd->bd, is_cur_buf_hbd(xd), 0,
                          &cm->sf_identity, &ref_yv12[!id], interp_filters);
    inter_pred_params.conv_params = get_conv_params(0, 0, xd->bd);

    // Since we have scaled the reference frames to match the size of the
    // current frame we must use a unit scaling factor during mode selection.
    av1_build_inter_predictor(second_pred, pw, &cur_mv[!id].as_mv,
                              &inter_pred_params);

    const int order_idx = id != 0;
    av1_dist_wtd_comp_weight_assign(
        cm, mbmi, order_idx, &xd->jcp_param.fwd_offset,
        &xd->jcp_param.bck_offset, &xd->jcp_param.use_dist_wtd_comp_avg, 1);

    // Do full-pixel compound motion search on the current reference frame.
    if (id) xd->plane[plane].pre[0] = ref_yv12[id];
    av1_set_mv_search_range(&x->mv_limits, &ref_mv[id].as_mv);

    // Use the mv result from the single mode as mv predictor.
    best_int_mv->as_fullmv = get_fullmv_from_mv(&cur_mv[id].as_mv);

    // Small-range full-pixel motion search.
    bestsme = av1_refining_search_8p_c(
        x, sadpb, search_range, &cpi->fn_ptr[bsize], mask, mask_stride, id,
        &ref_mv[id].as_mv, second_pred, &x->plane[0].src, &ref_yv12[id]);
    if (bestsme < INT_MAX) {
      if (mask)
        bestsme = av1_get_mvpred_mask_var(x, &best_int_mv->as_fullmv,
                                          &ref_mv[id].as_mv, second_pred, mask,
                                          mask_stride, id, &cpi->fn_ptr[bsize],
                                          &x->plane[0].src, &ref_yv12[id], 1);
      else
        bestsme = av1_get_mvpred_av_var(
            x, &best_int_mv->as_fullmv, &ref_mv[id].as_mv, second_pred,
            &cpi->fn_ptr[bsize], &x->plane[0].src, &ref_yv12[id], 1);
    }

    x->mv_limits = tmp_mv_limits;

    // Restore the pointer to the first (possibly scaled) prediction buffer.
    if (id) xd->plane[plane].pre[0] = ref_yv12[0];

    for (ref = 0; ref < 2; ++ref) {
      if (scaled_ref_frame[ref]) {
        // Swap back the original buffers for subpel motion search.
        for (int i = 0; i < num_planes; i++) {
          xd->plane[i].pre[ref] = backup_yv12[ref][i];
        }
        // Re-initialize based on unscaled prediction buffers.
        ref_yv12[ref] = xd->plane[plane].pre[ref];
      }
    }

    // Do sub-pixel compound motion search on the current reference frame.
    if (id) xd->plane[plane].pre[0] = ref_yv12[id];

    if (cpi->common.cur_frame_force_integer_mv) {
      best_int_mv->as_mv = get_mv_from_fullmv(&best_int_mv->as_fullmv);
    }
    if (bestsme < INT_MAX && cpi->common.cur_frame_force_integer_mv == 0) {
      int dis; /* TODO: use dis in distortion calculation later. */
      unsigned int sse;
      bestsme = cpi->find_fractional_mv_step(
          x, cm, mi_row, mi_col, &ref_mv[id].as_mv,
          cpi->common.allow_high_precision_mv, x->errorperbit,
          &cpi->fn_ptr[bsize], 0, cpi->sf.mv_sf.subpel_iters_per_step, NULL,
          x->nmv_vec_cost, x->mv_cost_stack, &dis, &sse, second_pred, mask,
          mask_stride, id, pw, ph, cpi->sf.mv_sf.use_accurate_subpel_search, 1);
    }

    // Restore the pointer to the first prediction buffer.
    if (id) xd->plane[plane].pre[0] = ref_yv12[0];
    if (bestsme < last_besterr[id]) {
      cur_mv[id] = *best_int_mv;
      last_besterr[id] = bestsme;
    } else {
      break;
    }
  }

  *rate_mv = 0;

  for (ref = 0; ref < 2; ++ref) {
    const int_mv curr_ref_mv = av1_get_ref_mv(x, ref);
    *rate_mv +=
        av1_mv_bit_cost(&cur_mv[ref].as_mv, &curr_ref_mv.as_mv, x->nmv_vec_cost,
                        x->mv_cost_stack, MV_COST_WEIGHT);
  }
}

// Search for the best mv for one component of a compound,
// given that the other component is fixed.
void av1_compound_single_motion_search(const AV1_COMP *cpi, MACROBLOCK *x,
                                       BLOCK_SIZE bsize, MV *this_mv,
                                       const uint8_t *second_pred,
                                       const uint8_t *mask, int mask_stride,
                                       int *rate_mv, int ref_idx) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const int pw = block_size_wide[bsize];
  const int ph = block_size_high[bsize];
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const int ref = mbmi->ref_frame[ref_idx];
  const int_mv ref_mv = av1_get_ref_mv(x, ref_idx);
  struct macroblockd_plane *const pd = &xd->plane[0];

  struct buf_2d backup_yv12[MAX_MB_PLANE];
  const YV12_BUFFER_CONFIG *const scaled_ref_frame =
      av1_get_scaled_ref_frame(cpi, ref);

  // Check that this is either an interinter or an interintra block
  assert(has_second_ref(mbmi) || (ref_idx == 0 && is_interintra_mode(mbmi)));

  // Store the first prediction buffer.
  struct buf_2d orig_yv12;
  struct buf_2d ref_yv12 = pd->pre[ref_idx];
  if (ref_idx) {
    orig_yv12 = pd->pre[0];
    pd->pre[0] = pd->pre[ref_idx];
  }

  if (scaled_ref_frame) {
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // full-pixel motion search code to be used without additional
    // modifications.
    for (int i = 0; i < num_planes; i++) {
      backup_yv12[i] = xd->plane[i].pre[ref_idx];
    }
    const int mi_row = xd->mi_row;
    const int mi_col = xd->mi_col;
    av1_setup_pre_planes(xd, ref_idx, scaled_ref_frame, mi_row, mi_col, NULL,
                         num_planes);
  }

  int bestsme = INT_MAX;
  int sadpb = x->sadperbit16;
  int_mv *best_int_mv = &x->best_mv;
  int search_range = SEARCH_RANGE_8P;

  MvLimits tmp_mv_limits = x->mv_limits;

  // Do compound motion search on the current reference frame.
  av1_set_mv_search_range(&x->mv_limits, &ref_mv.as_mv);

  // Use the mv result from the single mode as mv predictor.
  best_int_mv->as_fullmv = get_fullmv_from_mv(this_mv);

  // Small-range full-pixel motion search.
  bestsme = av1_refining_search_8p_c(
      x, sadpb, search_range, &cpi->fn_ptr[bsize], mask, mask_stride, ref_idx,
      &ref_mv.as_mv, second_pred, &x->plane[0].src, &ref_yv12);
  if (bestsme < INT_MAX) {
    if (mask)
      bestsme = av1_get_mvpred_mask_var(
          x, &best_int_mv->as_fullmv, &ref_mv.as_mv, second_pred, mask,
          mask_stride, ref_idx, &cpi->fn_ptr[bsize], &x->plane[0].src,
          &ref_yv12, 1);
    else
      bestsme = av1_get_mvpred_av_var(x, &best_int_mv->as_fullmv, &ref_mv.as_mv,
                                      second_pred, &cpi->fn_ptr[bsize],
                                      &x->plane[0].src, &ref_yv12, 1);
  }

  x->mv_limits = tmp_mv_limits;

  if (scaled_ref_frame) {
    // Swap back the original buffers for subpel motion search.
    for (int i = 0; i < num_planes; i++) {
      xd->plane[i].pre[ref_idx] = backup_yv12[i];
    }
  }

  if (cpi->common.cur_frame_force_integer_mv) {
    best_int_mv->as_mv = get_mv_from_fullmv(&best_int_mv->as_fullmv);
  }
  const int use_fractional_mv =
      bestsme < INT_MAX && cpi->common.cur_frame_force_integer_mv == 0;
  if (use_fractional_mv) {
    int dis; /* TODO: use dis in distortion calculation later. */
    unsigned int sse;
    const int mi_row = xd->mi_row;
    const int mi_col = xd->mi_col;
    bestsme = cpi->find_fractional_mv_step(
        x, cm, mi_row, mi_col, &ref_mv.as_mv,
        cpi->common.allow_high_precision_mv, x->errorperbit,
        &cpi->fn_ptr[bsize], 0, cpi->sf.mv_sf.subpel_iters_per_step, NULL,
        x->nmv_vec_cost, x->mv_cost_stack, &dis, &sse, second_pred, mask,
        mask_stride, ref_idx, pw, ph, cpi->sf.mv_sf.use_accurate_subpel_search,
        1);
  }

  // Restore the pointer to the first unscaled prediction buffer.
  if (ref_idx) pd->pre[0] = orig_yv12;

  if (bestsme < INT_MAX) *this_mv = best_int_mv->as_mv;

  *rate_mv = 0;

  *rate_mv += av1_mv_bit_cost(this_mv, &ref_mv.as_mv, x->nmv_vec_cost,
                              x->mv_cost_stack, MV_COST_WEIGHT);
}

static AOM_INLINE void build_second_inter_pred(const AV1_COMP *cpi,
                                               MACROBLOCK *x, BLOCK_SIZE bsize,
                                               const MV *other_mv, int ref_idx,
                                               uint8_t *second_pred) {
  const AV1_COMMON *const cm = &cpi->common;
  const int pw = block_size_wide[bsize];
  const int ph = block_size_high[bsize];
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  struct macroblockd_plane *const pd = &xd->plane[0];
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  const int p_col = ((mi_col * MI_SIZE) >> pd->subsampling_x);
  const int p_row = ((mi_row * MI_SIZE) >> pd->subsampling_y);

  // This function should only ever be called for compound modes
  assert(has_second_ref(mbmi));

  const int plane = 0;
  struct buf_2d ref_yv12 = xd->plane[plane].pre[!ref_idx];

  struct scale_factors sf;
  av1_setup_scale_factors_for_frame(&sf, ref_yv12.width, ref_yv12.height,
                                    cm->width, cm->height);

  InterPredParams inter_pred_params;

  av1_init_inter_params(&inter_pred_params, pw, ph, p_row, p_col,
                        pd->subsampling_x, pd->subsampling_y, xd->bd,
                        is_cur_buf_hbd(xd), 0, &sf, &ref_yv12,
                        mbmi->interp_filters);
  inter_pred_params.conv_params = get_conv_params(0, plane, xd->bd);

  // Get the prediction block from the 'other' reference frame.
  av1_build_inter_predictor(second_pred, pw, other_mv, &inter_pred_params);

  av1_dist_wtd_comp_weight_assign(cm, mbmi, 0, &xd->jcp_param.fwd_offset,
                                  &xd->jcp_param.bck_offset,
                                  &xd->jcp_param.use_dist_wtd_comp_avg, 1);
}

// Wrapper for av1_compound_single_motion_search, for the common case
// where the second prediction is also an inter mode.
void av1_compound_single_motion_search_interinter(
    const AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bsize, int_mv *cur_mv,
    const uint8_t *mask, int mask_stride, int *rate_mv, int ref_idx) {
  MACROBLOCKD *xd = &x->e_mbd;
  // This function should only ever be called for compound modes
  assert(has_second_ref(xd->mi[0]));

  // Prediction buffer from second frame.
  DECLARE_ALIGNED(16, uint16_t, second_pred_alloc_16[MAX_SB_SQUARE]);
  uint8_t *second_pred;
  if (is_cur_buf_hbd(xd))
    second_pred = CONVERT_TO_BYTEPTR(second_pred_alloc_16);
  else
    second_pred = (uint8_t *)second_pred_alloc_16;

  MV *this_mv = &cur_mv[ref_idx].as_mv;
  const MV *other_mv = &cur_mv[!ref_idx].as_mv;
  build_second_inter_pred(cpi, x, bsize, other_mv, ref_idx, second_pred);
  av1_compound_single_motion_search(cpi, x, bsize, this_mv, second_pred, mask,
                                    mask_stride, rate_mv, ref_idx);
}

static AOM_INLINE void do_masked_motion_search_indexed(
    const AV1_COMP *const cpi, MACROBLOCK *x, const int_mv *const cur_mv,
    const INTERINTER_COMPOUND_DATA *const comp_data, BLOCK_SIZE bsize,
    int_mv *tmp_mv, int *rate_mv, int which) {
  // NOTE: which values: 0 - 0 only, 1 - 1 only, 2 - both
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  BLOCK_SIZE sb_type = mbmi->sb_type;
  const uint8_t *mask;
  const int mask_stride = block_size_wide[bsize];

  mask = av1_get_compound_type_mask(comp_data, sb_type);

  tmp_mv[0].as_int = cur_mv[0].as_int;
  tmp_mv[1].as_int = cur_mv[1].as_int;
  if (which == 0 || which == 1) {
    av1_compound_single_motion_search_interinter(cpi, x, bsize, tmp_mv, mask,
                                                 mask_stride, rate_mv, which);
  } else if (which == 2) {
    av1_joint_motion_search(cpi, x, bsize, tmp_mv, mask, mask_stride, rate_mv);
  }
}

int av1_interinter_compound_motion_search(const AV1_COMP *const cpi,
                                          MACROBLOCK *x,
                                          const int_mv *const cur_mv,
                                          const BLOCK_SIZE bsize,
                                          const PREDICTION_MODE this_mode) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  int_mv tmp_mv[2];
  int tmp_rate_mv = 0;
  mbmi->interinter_comp.seg_mask = xd->seg_mask;
  const INTERINTER_COMPOUND_DATA *compound_data = &mbmi->interinter_comp;

  if (this_mode == NEW_NEWMV) {
    do_masked_motion_search_indexed(cpi, x, cur_mv, compound_data, bsize,
                                    tmp_mv, &tmp_rate_mv, 2);
    mbmi->mv[0].as_int = tmp_mv[0].as_int;
    mbmi->mv[1].as_int = tmp_mv[1].as_int;
  } else if (this_mode >= NEAREST_NEWMV && this_mode <= NEW_NEARMV) {
    // which = 1 if this_mode == NEAREST_NEWMV || this_mode == NEAR_NEWMV
    // which = 0 if this_mode == NEW_NEARESTMV || this_mode == NEW_NEARMV
    int which = (NEWMV == compound_ref1_mode(this_mode));
    do_masked_motion_search_indexed(cpi, x, cur_mv, compound_data, bsize,
                                    tmp_mv, &tmp_rate_mv, which);
    mbmi->mv[which].as_int = tmp_mv[which].as_int;
  }
  return tmp_rate_mv;
}
