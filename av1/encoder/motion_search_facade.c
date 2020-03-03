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

#include "aom_ports/system_state.h"

#include "av1/common/reconinter.h"

#include "av1/encoder/encodemv.h"
#include "av1/encoder/motion_search_facade.h"
#include "av1/encoder/partition_strategy.h"
#include "av1/encoder/reconinter_enc.h"

void av1_single_motion_search(const AV1_COMP *const cpi, MACROBLOCK *x,
                              BLOCK_SIZE bsize, int ref_idx, int *rate_mv,
                              int search_range, inter_mode_info *mode_info) {
  MACROBLOCKD *xd = &x->e_mbd;
  const AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MB_MODE_INFO *mbmi = xd->mi[0];
  struct buf_2d backup_yv12[MAX_MB_PLANE] = { { 0, 0, 0, 0, 0 } };
  int bestsme = INT_MAX;
  const int ref = mbmi->ref_frame[ref_idx];
  FullMvLimits tmp_mv_limits = x->mv_limits;
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
  int_mv second_best_mv;
  x->best_mv.as_int = second_best_mv.as_int = INVALID_MV;
  switch (mbmi->motion_mode) {
    case SIMPLE_TRANSLATION:
      bestsme = av1_full_pixel_search(
          cpi, x, bsize, start_mv, step_param, cpi->sf.mv_sf.search_method, 0,
          sadpb, cond_cost_list(cpi, cost_list), &ref_mv, 0,
          &cpi->ss_cfg[SS_CFG_SRC], &x->best_mv.as_fullmv,
          &second_best_mv.as_fullmv);
      break;
    case OBMC_CAUSAL:
      bestsme = av1_obmc_full_pixel_search(
          cpi, x, start_mv, step_param, sadpb, &cpi->fn_ptr[bsize], &ref_mv,
          &(x->best_mv.as_fullmv), &cpi->ss_cfg[SS_CFG_SRC]);
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

  // Terminate search with the current ref_idx if we have already encountered
  // another ref_mv in the drl such that:
  //  1. The other drl has the same fullpel_mv during the SIMPLE_TRANSLATION
  //     search process as the current fullpel_mv.
  //  2. The rate needed to encode the current fullpel_mv is larger than that
  //     for the other ref_mv.
  if (cpi->sf.inter_sf.skip_repeated_full_newmv &&
      mbmi->motion_mode == SIMPLE_TRANSLATION &&
      x->best_mv.as_int != INVALID_MV) {
    int_mv this_mv;
    this_mv.as_mv = get_mv_from_fullmv(&x->best_mv.as_fullmv);
    const int ref_mv_idx = mbmi->ref_mv_idx;
    const int this_mv_rate =
        av1_mv_bit_cost(&this_mv.as_mv, &ref_mv, x->nmv_vec_cost,
                        x->mv_cost_stack, MV_COST_WEIGHT);
    mode_info[ref_mv_idx].full_search_mv.as_int = this_mv.as_int;
    mode_info[ref_mv_idx].full_mv_rate = this_mv_rate;

    for (int prev_ref_idx = 0; prev_ref_idx < ref_mv_idx; ++prev_ref_idx) {
      // Check if the motion search result same as previous results
      if (this_mv.as_int == mode_info[prev_ref_idx].full_search_mv.as_int) {
        // Compare the rate cost
        const int prev_rate_cost = mode_info[prev_ref_idx].full_mv_rate +
                                   mode_info[prev_ref_idx].drl_cost;
        const int this_rate_cost =
            this_mv_rate + mode_info[ref_mv_idx].drl_cost;

        if (prev_rate_cost <= this_rate_cost) {
          // If the current rate_cost is worse than the previous rate_cost, then
          // we terminate the search. Since av1_single_motion_search is only
          // called by handle_new_mv in SIMPLE_TRANSLATION mode, we set the
          // best_mv to INVALID mv to signal that we wish to terminate search
          // for the current mode.
          x->best_mv.as_int = INVALID_MV;
          return;
        }
      }
    }
  }

  if (cpi->common.cur_frame_force_integer_mv) {
    convert_fullmv_to_mv(&x->best_mv);
  }

  const int use_fractional_mv =
      bestsme < INT_MAX && cpi->common.cur_frame_force_integer_mv == 0;
  if (use_fractional_mv) {
    const uint8_t *second_pred = NULL;
    const uint8_t *mask = NULL;
    const int mask_stride = 0;
    const int invert_mask = 0;
    const int reset_fractional_mv = 1;
    int dis; /* TODO: use dis in distortion calculation later. */

    SUBPEL_MOTION_SEARCH_PARAMS ms_params;
    av1_make_default_subpel_ms_params(&ms_params, cpi, x, bsize, &ref_mv,
                                      cost_list, second_pred, mask, mask_stride,
                                      invert_mask, reset_fractional_mv);
    switch (mbmi->motion_mode) {
      case SIMPLE_TRANSLATION:
        if (cpi->sf.mv_sf.use_accurate_subpel_search) {
          const int try_second = second_best_mv.as_int != INVALID_MV &&
                                 second_best_mv.as_int != x->best_mv.as_int;
          const int best_mv_var = cpi->find_fractional_mv_step(
              x, cm, &ms_params, &dis, &x->pred_sse[ref]);

          if (try_second) {
            SubpelMvLimits subpel_limits;
            av1_set_subpel_mv_search_range(&subpel_limits, &x->mv_limits,
                                           &ref_mv);
            MV best_mv = x->best_mv.as_mv;

            x->best_mv = second_best_mv;
            if (av1_is_subpelmv_in_range(
                    &subpel_limits,
                    get_mv_from_fullmv(&x->best_mv.as_fullmv))) {
              ms_params.do_reset_fractional_mv = !reset_fractional_mv;

              const int this_var = cpi->find_fractional_mv_step(
                  x, cm, &ms_params, &dis, &x->pred_sse[ref]);
              if (this_var < best_mv_var) best_mv = x->best_mv.as_mv;
            }
            x->best_mv.as_mv = best_mv;
          }
        } else {
          cpi->find_fractional_mv_step(x, cm, &ms_params, &dis,
                                       &x->pred_sse[ref]);
        }
        break;
      case OBMC_CAUSAL:
        av1_find_best_obmc_sub_pixel_tree_up(x, cm, &ms_params, &dis,
                                             &x->pred_sse[ref]);
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
    FullMvLimits tmp_mv_limits = x->mv_limits;
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
                                          &x->plane[0].src, &ref_yv12[id]);
      else
        bestsme = av1_get_mvpred_av_var(
            x, &best_int_mv->as_fullmv, &ref_mv[id].as_mv, second_pred,
            &cpi->fn_ptr[bsize], &x->plane[0].src, &ref_yv12[id]);
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
      convert_fullmv_to_mv(best_int_mv);
    }
    if (bestsme < INT_MAX && cpi->common.cur_frame_force_integer_mv == 0) {
      int dis; /* TODO: use dis in distortion calculation later. */
      unsigned int sse;
      SUBPEL_MOTION_SEARCH_PARAMS ms_params;
      av1_make_default_subpel_ms_params(&ms_params, cpi, x, bsize,
                                        &ref_mv[id].as_mv, NULL, second_pred,
                                        mask, mask_stride, id, 1);
      ms_params.forced_stop = EIGHTH_PEL;
      bestsme = cpi->find_fractional_mv_step(x, cm, &ms_params, &dis, &sse);
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

  FullMvLimits tmp_mv_limits = x->mv_limits;

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
          &ref_yv12);
    else
      bestsme = av1_get_mvpred_av_var(x, &best_int_mv->as_fullmv, &ref_mv.as_mv,
                                      second_pred, &cpi->fn_ptr[bsize],
                                      &x->plane[0].src, &ref_yv12);
  }

  x->mv_limits = tmp_mv_limits;

  if (scaled_ref_frame) {
    // Swap back the original buffers for subpel motion search.
    for (int i = 0; i < num_planes; i++) {
      xd->plane[i].pre[ref_idx] = backup_yv12[i];
    }
  }

  if (cpi->common.cur_frame_force_integer_mv) {
    convert_fullmv_to_mv(best_int_mv);
  }
  const int use_fractional_mv =
      bestsme < INT_MAX && cpi->common.cur_frame_force_integer_mv == 0;
  if (use_fractional_mv) {
    int dis; /* TODO: use dis in distortion calculation later. */
    unsigned int sse;
    SUBPEL_MOTION_SEARCH_PARAMS ms_params;
    av1_make_default_subpel_ms_params(&ms_params, cpi, x, bsize, &ref_mv.as_mv,
                                      NULL, second_pred, mask, mask_stride,
                                      ref_idx, 1);
    ms_params.forced_stop = EIGHTH_PEL;
    bestsme = cpi->find_fractional_mv_step(x, cm, &ms_params, &dis, &sse);
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
      cpi, x, bsize, start_mv, step_param, search_methods, do_mesh_search,
      sadpb, cond_cost_list(cpi, cost_list), &ref_mv, 0,
      &cpi->ss_cfg[SS_CFG_SRC], &x->best_mv.as_fullmv, NULL);
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
