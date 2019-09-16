/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdint.h>
#include <float.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_codec.h"
#include "aom_ports/system_state.h"

#include "av1/common/enums.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/reconintra.h"

#include "av1/encoder/encoder.h"
#include "av1/encoder/encode_strategy.h"
#include "av1/encoder/reconinter_enc.h"

#define MC_FLOW_BSIZE_1D 16
#define MC_FLOW_NUM_PELS (MC_FLOW_BSIZE_1D * MC_FLOW_BSIZE_1D)

static BLOCK_SIZE convert_length_to_bsize(int length) {
  switch (length) {
    case 64: return BLOCK_64X64;
    case 32: return BLOCK_32X32;
    case 16: return BLOCK_16X16;
    case 8: return BLOCK_8X8;
    case 4: return BLOCK_4X4;
    default:
      assert(0 && "Invalid block size for tpl model");
      return BLOCK_16X16;
  }
}

static AOM_INLINE void get_quantize_error(MACROBLOCK *x, int plane,
                                          tran_low_t *coeff, tran_low_t *qcoeff,
                                          tran_low_t *dqcoeff, TX_SIZE tx_size,
                                          int64_t *recon_error, int64_t *sse) {
  const struct macroblock_plane *const p = &x->plane[plane];
  const SCAN_ORDER *const scan_order = &av1_default_scan_orders[tx_size];
  uint16_t eob;
  int pix_num = 1 << num_pels_log2_lookup[txsize_to_bsize[tx_size]];
  const int shift = tx_size == TX_32X32 ? 0 : 2;

  av1_quantize_fp(coeff, pix_num, p->zbin_QTX, p->round_fp_QTX, p->quant_fp_QTX,
                  p->quant_shift_QTX, qcoeff, dqcoeff, p->dequant_QTX, &eob,
                  scan_order->scan, scan_order->iscan);

  *recon_error = av1_block_error(coeff, dqcoeff, pix_num, sse) >> shift;
  *recon_error = AOMMAX(*recon_error, 1);

  *sse = (*sse) >> shift;
  *sse = AOMMAX(*sse, 1);
}

static AOM_INLINE void wht_fwd_txfm(int16_t *src_diff, int bw,
                                    tran_low_t *coeff, TX_SIZE tx_size,
                                    int is_hbd) {
  if (is_hbd) {
    switch (tx_size) {
      case TX_8X8: aom_highbd_hadamard_8x8(src_diff, bw, coeff); break;
      case TX_16X16: aom_highbd_hadamard_16x16(src_diff, bw, coeff); break;
      case TX_32X32: aom_highbd_hadamard_32x32(src_diff, bw, coeff); break;
      default: assert(0);
    }
  } else {
    switch (tx_size) {
      case TX_8X8: aom_hadamard_8x8(src_diff, bw, coeff); break;
      case TX_16X16: aom_hadamard_16x16(src_diff, bw, coeff); break;
      case TX_32X32: aom_hadamard_32x32(src_diff, bw, coeff); break;
      default: assert(0);
    }
  }
}

static uint32_t motion_estimation(AV1_COMP *cpi, MACROBLOCK *x,
                                  uint8_t *cur_frame_buf,
                                  uint8_t *ref_frame_buf, int stride,
                                  int stride_ref, BLOCK_SIZE bsize, int mi_row,
                                  int mi_col) {
  AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MV_SPEED_FEATURES *const mv_sf = &cpi->sf.mv;
  const SEARCH_METHODS search_method = NSTEP;
  int step_param;
  int sadpb = x->sadperbit16;
  uint32_t bestsme = UINT_MAX;
  int distortion;
  uint32_t sse;
  int cost_list[5];
  const MvLimits tmp_mv_limits = x->mv_limits;
  search_site_config ss_cfg;

  MV best_ref_mv1 = { 0, 0 };
  MV best_ref_mv1_full; /* full-pixel value of best_ref_mv1 */

  best_ref_mv1_full.col = best_ref_mv1.col >> 3;
  best_ref_mv1_full.row = best_ref_mv1.row >> 3;

  // Setup frame pointers
  x->plane[0].src.buf = cur_frame_buf;
  x->plane[0].src.stride = stride;
  xd->plane[0].pre[0].buf = ref_frame_buf;
  xd->plane[0].pre[0].stride = stride_ref;

  step_param = mv_sf->reduce_first_step_size;
  step_param = AOMMIN(step_param, MAX_MVSEARCH_STEPS - 2);

  av1_set_mv_search_range(&x->mv_limits, &best_ref_mv1);

  av1_init3smotion_compensation(&ss_cfg, stride_ref);
  av1_full_pixel_search(cpi, x, bsize, &best_ref_mv1_full, step_param,
                        search_method, 0, sadpb, cond_cost_list(cpi, cost_list),
                        &best_ref_mv1, INT_MAX, 0, (MI_SIZE * mi_col),
                        (MI_SIZE * mi_row), 0, &ss_cfg);

  /* restore UMV window */
  x->mv_limits = tmp_mv_limits;

  const int pw = block_size_wide[bsize];
  const int ph = block_size_high[bsize];
  bestsme = cpi->find_fractional_mv_step(
      x, cm, mi_row, mi_col, &best_ref_mv1, cpi->common.allow_high_precision_mv,
      x->errorperbit, &cpi->fn_ptr[bsize], 0, mv_sf->subpel_iters_per_step,
      cond_cost_list(cpi, cost_list), NULL, NULL, &distortion, &sse, NULL, NULL,
      0, 0, pw, ph, 1, 1);

  return bestsme;
}

static AOM_INLINE void mode_estimation(
    AV1_COMP *cpi, MACROBLOCK *x, MACROBLOCKD *xd, struct scale_factors *sf,
    int frame_idx, int16_t *src_diff, tran_low_t *coeff, tran_low_t *qcoeff,
    tran_low_t *dqcoeff, int use_satd, int mi_row, int mi_col, BLOCK_SIZE bsize,
    TX_SIZE tx_size, const YV12_BUFFER_CONFIG *ref_frame[], uint8_t *predictor,
    int64_t *recon_error, int64_t *sse, TplDepStats *tpl_stats,
    int *ref_frame_index, int_mv *mv) {
  AV1_COMMON *cm = &cpi->common;
  const GF_GROUP *gf_group = &cpi->gf_group;

  const int bw = 4 << mi_size_wide_log2[bsize];
  const int bh = 4 << mi_size_high_log2[bsize];
  const int pix_num = bw * bh;
  const int_interpfilters kernel =
      av1_broadcast_interp_filter(EIGHTTAP_REGULAR);

  int64_t best_intra_cost = INT64_MAX;
  int64_t intra_cost;
  PREDICTION_MODE mode;
  int mb_y_offset = mi_row * MI_SIZE * xd->cur_buf->y_stride + mi_col * MI_SIZE;

  memset(tpl_stats, 0, sizeof(*tpl_stats));

  xd->above_mbmi = NULL;
  xd->left_mbmi = NULL;
  xd->mi[0]->sb_type = bsize;
  xd->mi[0]->motion_mode = SIMPLE_TRANSLATION;

  const int q_cur = gf_group->q_val[frame_idx];
  const int16_t qstep_cur =
      ROUND_POWER_OF_TWO(av1_ac_quant_QTX(q_cur, 0, xd->bd), xd->bd - 8);
  const int qstep_cur_noise =
      use_satd ? ((int)qstep_cur * pix_num + 16) / (4 * 8)
               : ((int)qstep_cur * (int)qstep_cur * pix_num + 384) / (12 * 64);

  // Intra prediction search
  xd->mi[0]->ref_frame[0] = INTRA_FRAME;
  for (mode = DC_PRED; mode <= PAETH_PRED; ++mode) {
    uint8_t *src, *dst;
    int src_stride, dst_stride;

    src = xd->cur_buf->y_buffer + mb_y_offset;
    src_stride = xd->cur_buf->y_stride;

    dst = predictor;
    dst_stride = bw;

    av1_predict_intra_block(
        cm, xd, block_size_wide[bsize], block_size_high[bsize], tx_size, mode,
        0, 0, FILTER_INTRA_MODES, src, src_stride, dst, dst_stride, 0, 0, 0);

    if (use_satd) {
#if CONFIG_AV1_HIGHBITDEPTH
      if (is_cur_buf_hbd(xd)) {
        aom_highbd_subtract_block(bh, bw, src_diff, bw, src, src_stride, dst,
                                  dst_stride, xd->bd);
      } else {
        aom_subtract_block(bh, bw, src_diff, bw, src, src_stride, dst,
                           dst_stride);
      }
#else
      aom_subtract_block(bh, bw, src_diff, bw, src, src_stride, dst,
                         dst_stride);
#endif
      wht_fwd_txfm(src_diff, bw, coeff, tx_size, is_cur_buf_hbd(xd));
      intra_cost = aom_satd(coeff, pix_num);
    } else {
      int64_t intra_sse;
#if CONFIG_AV1_HIGHBITDEPTH
      if (is_cur_buf_hbd(xd)) {
        intra_sse =
            aom_highbd_sse(xd->cur_buf->y_buffer + mb_y_offset,
                           xd->cur_buf->y_stride, predictor, bw, bw, bh);
      } else {
        intra_sse = aom_sse(xd->cur_buf->y_buffer + mb_y_offset,
                            xd->cur_buf->y_stride, predictor, bw, bw, bh);
      }
#else
      intra_sse = aom_sse(xd->cur_buf->y_buffer + mb_y_offset,
                          xd->cur_buf->y_stride, predictor, bw, bw, bh);
#endif
      intra_cost = ROUND_POWER_OF_TWO(intra_sse, (xd->bd - 8) * 2);
    }
    intra_cost += qstep_cur_noise;

    if (intra_cost < best_intra_cost) best_intra_cost = intra_cost;
  }

  // Motion compensated prediction
  xd->mi[0]->ref_frame[0] = GOLDEN_FRAME;

  int best_rf_idx = -1;
  int_mv best_mv;
  int64_t inter_cost;
  int64_t best_inter_cost;
  int64_t inter_cost_weighted;
  int64_t best_inter_cost_weighted = INT64_MAX;
  int rf_idx;

  best_mv.as_int = 0;

  for (rf_idx = 0; rf_idx < INTER_REFS_PER_FRAME; ++rf_idx) {
    if (ref_frame[rf_idx] == NULL) continue;

    int q_ref =
        cpi->tpl_frame[frame_idx].ref_map_index[rf_idx] > 0
            ? gf_group->q_val[cpi->tpl_frame[frame_idx].ref_map_index[rf_idx]]
            : 0;

    const int16_t qstep_ref =
        ROUND_POWER_OF_TWO(av1_ac_quant_QTX(q_ref, 0, xd->bd), xd->bd - 8);
    const int qstep_ref_noise =
        use_satd
            ? ((int)qstep_ref * pix_num + 16) / (4 * 8)
            : ((int)qstep_ref * (int)qstep_ref * pix_num + 384) / (12 * 64);
    int mb_y_offset_ref =
        mi_row * MI_SIZE * ref_frame[rf_idx]->y_stride + mi_col * MI_SIZE;

    motion_estimation(cpi, x, xd->cur_buf->y_buffer + mb_y_offset,
                      ref_frame[rf_idx]->y_buffer + mb_y_offset_ref,
                      xd->cur_buf->y_stride, ref_frame[rf_idx]->y_stride, bsize,
                      mi_row, mi_col);

    ConvolveParams conv_params = get_conv_params(0, 0, xd->bd);
    WarpTypesAllowed warp_types;
    memset(&warp_types, 0, sizeof(WarpTypesAllowed));

    av1_build_inter_predictor(ref_frame[rf_idx]->y_buffer + mb_y_offset_ref,
                              ref_frame[rf_idx]->y_stride, &predictor[0], bw,
                              &x->best_mv.as_mv, sf, bw, bh, &conv_params,
                              kernel, &warp_types, mi_col * MI_SIZE,
                              mi_row * MI_SIZE, 0, 0, MV_PRECISION_Q3,
                              mi_col * MI_SIZE, mi_row * MI_SIZE, xd, 0);
    if (use_satd) {
#if CONFIG_AV1_HIGHBITDEPTH
      if (is_cur_buf_hbd(xd)) {
        aom_highbd_subtract_block(bh, bw, src_diff, bw,
                                  xd->cur_buf->y_buffer + mb_y_offset,
                                  xd->cur_buf->y_stride, predictor, bw, xd->bd);
      } else {
        aom_subtract_block(bh, bw, src_diff, bw,
                           xd->cur_buf->y_buffer + mb_y_offset,
                           xd->cur_buf->y_stride, predictor, bw);
      }
#else
      aom_subtract_block(bh, bw, src_diff, bw,
                         xd->cur_buf->y_buffer + mb_y_offset,
                         xd->cur_buf->y_stride, predictor, bw);
#endif
      wht_fwd_txfm(src_diff, bw, coeff, tx_size, is_cur_buf_hbd(xd));

      inter_cost = aom_satd(coeff, pix_num);
    } else {
      int64_t inter_sse;
#if CONFIG_AV1_HIGHBITDEPTH
      if (is_cur_buf_hbd(xd)) {
        inter_sse =
            aom_highbd_sse(xd->cur_buf->y_buffer + mb_y_offset,
                           xd->cur_buf->y_stride, predictor, bw, bw, bh);
      } else {
        inter_sse = aom_sse(xd->cur_buf->y_buffer + mb_y_offset,
                            xd->cur_buf->y_stride, predictor, bw, bw, bh);
      }
#else
      inter_sse = aom_sse(xd->cur_buf->y_buffer + mb_y_offset,
                          xd->cur_buf->y_stride, predictor, bw, bw, bh);
#endif
      inter_cost = ROUND_POWER_OF_TWO(inter_sse, (xd->bd - 8) * 2);
    }
    inter_cost_weighted = inter_cost + qstep_ref_noise;

    if (inter_cost_weighted < best_inter_cost_weighted) {
      best_rf_idx = rf_idx;
      best_inter_cost_weighted = inter_cost_weighted;
      best_mv.as_int = x->best_mv.as_int;
      get_quantize_error(x, 0, coeff, qcoeff, dqcoeff, tx_size, recon_error,
                         sse);
    }
  }
  best_intra_cost = AOMMAX(best_intra_cost, 1);
  if (frame_idx == 0)
    best_inter_cost = 0;
  else
    best_inter_cost =
        AOMMIN(best_intra_cost, (int64_t)best_inter_cost_weighted);
  tpl_stats->inter_cost = best_inter_cost << TPL_DEP_COST_SCALE_LOG2;
  tpl_stats->intra_cost = best_intra_cost << TPL_DEP_COST_SCALE_LOG2;

  if (frame_idx && best_rf_idx != -1) {
    *ref_frame_index = cpi->tpl_frame[frame_idx].ref_map_index[best_rf_idx];
    mv->as_int = best_mv.as_int;
  }
}

static int round_floor(int ref_pos, int bsize_pix) {
  int round;
  if (ref_pos < 0)
    round = -(1 + (-ref_pos - 1) / bsize_pix);
  else
    round = ref_pos / bsize_pix;

  return round;
}

static int get_overlap_area(int grid_pos_row, int grid_pos_col, int ref_pos_row,
                            int ref_pos_col, int block, BLOCK_SIZE bsize) {
  int width = 0, height = 0;
  int bw = 4 << mi_size_wide_log2[bsize];
  int bh = 4 << mi_size_high_log2[bsize];

  switch (block) {
    case 0:
      width = grid_pos_col + bw - ref_pos_col;
      height = grid_pos_row + bh - ref_pos_row;
      break;
    case 1:
      width = ref_pos_col + bw - grid_pos_col;
      height = grid_pos_row + bh - ref_pos_row;
      break;
    case 2:
      width = grid_pos_col + bw - ref_pos_col;
      height = ref_pos_row + bh - grid_pos_row;
      break;
    case 3:
      width = ref_pos_col + bw - grid_pos_col;
      height = ref_pos_row + bh - grid_pos_row;
      break;
    default: assert(0);
  }

  return width * height;
}

static double iiratio_nonlinear(double iiratio) {
  double z = 8 * (iiratio - 0.5);
  double sigmoid = 1.0 / (1.0 + exp(-z));
  return sigmoid;
  return iiratio * iiratio;
}

int av1_tpl_ptr_pos(AV1_COMP *cpi, int mi_row, int mi_col, int stride) {
  const int right_shift = cpi->tpl_stats_block_mis_log2;

  return (mi_row >> right_shift) * stride + (mi_col >> right_shift);
}

static AOM_INLINE void tpl_model_update_b(AV1_COMP *cpi, TplDepFrame *tpl_frame,
                                          TplDepStats *tpl_stats_ptr,
                                          int mi_row, int mi_col,
                                          double quant_ratio,
                                          const BLOCK_SIZE bsize,
                                          int ref_frame_index, int_mv mv) {
  TplDepFrame *ref_tpl_frame = &tpl_frame[ref_frame_index];
  TplDepStats *ref_stats_ptr = ref_tpl_frame->tpl_stats_ptr;

  const int ref_pos_row = mi_row * MI_SIZE + (mv.as_mv.row >> 3);
  const int ref_pos_col = mi_col * MI_SIZE + (mv.as_mv.col >> 3);

  const int bw = 4 << mi_size_wide_log2[bsize];
  const int bh = 4 << mi_size_high_log2[bsize];
  const int mi_height = mi_size_high[bsize];
  const int mi_width = mi_size_wide[bsize];
  const int pix_num = bw * bh;

  // top-left on grid block location in pixel
  int grid_pos_row_base = round_floor(ref_pos_row, bh) * bh;
  int grid_pos_col_base = round_floor(ref_pos_col, bw) * bw;
  int block;

  for (block = 0; block < 4; ++block) {
    int grid_pos_row = grid_pos_row_base + bh * (block >> 1);
    int grid_pos_col = grid_pos_col_base + bw * (block & 0x01);

    if (grid_pos_row >= 0 && grid_pos_row < ref_tpl_frame->mi_rows * MI_SIZE &&
        grid_pos_col >= 0 && grid_pos_col < ref_tpl_frame->mi_cols * MI_SIZE) {
      int overlap_area = get_overlap_area(
          grid_pos_row, grid_pos_col, ref_pos_row, ref_pos_col, block, bsize);
      int ref_mi_row = round_floor(grid_pos_row, bh) * mi_height;
      int ref_mi_col = round_floor(grid_pos_col, bw) * mi_width;

      const double iiratio_nl = iiratio_nonlinear(
          (double)tpl_stats_ptr->inter_cost / tpl_stats_ptr->intra_cost);
      int64_t mc_flow = (int64_t)(quant_ratio * tpl_stats_ptr->mc_dep_cost *
                                  (1.0 - iiratio_nl));
#if !USE_TPL_CLASSIC_MODEL
      int64_t mc_saved = tpl_stats_ptr->intra_cost - tpl_stats_ptr->inter_cost;
#endif  // #if !USE_TPL_CLASSIC_MODEL
      const int step = 1 << cpi->tpl_stats_block_mis_log2;
      for (int idy = 0; idy < mi_height; idy += step) {
        for (int idx = 0; idx < mi_width; idx += step) {
          TplDepStats *des_stats = &ref_stats_ptr[av1_tpl_ptr_pos(
              cpi, ref_mi_row + idy, ref_mi_col + idx, ref_tpl_frame->stride)];
          des_stats->mc_flow += (mc_flow * overlap_area) / pix_num;
#if !USE_TPL_CLASSIC_MODEL
          des_stats->mc_count += overlap_area << TPL_DEP_COST_SCALE_LOG2;
          des_stats->mc_saved += (mc_saved * overlap_area) / pix_num;
#endif  // !USE_TPL_CLASSIC_MODEL
          assert(overlap_area >= 0);
        }
      }
    }
  }
}

static AOM_INLINE void tpl_model_update(AV1_COMP *cpi, TplDepFrame *tpl_frame,
                                        TplDepStats *tpl_stats_ptr, int mi_row,
                                        int mi_col, double quant_ratio,
                                        const BLOCK_SIZE bsize,
                                        int ref_frame_index, int_mv mv) {
  const int mi_height = mi_size_high[bsize];
  const int mi_width = mi_size_wide[bsize];
  const int step = 1 << cpi->tpl_stats_block_mis_log2;
  const BLOCK_SIZE tpl_block_size =
      convert_length_to_bsize(MI_SIZE << cpi->tpl_stats_block_mis_log2);

  for (int idy = 0; idy < mi_height; idy += step) {
    for (int idx = 0; idx < mi_width; idx += step) {
      TplDepStats *tpl_ptr = &tpl_stats_ptr[av1_tpl_ptr_pos(
          cpi, mi_row + idy, mi_col + idx, tpl_frame->stride)];
      tpl_model_update_b(cpi, tpl_frame, tpl_ptr, mi_row + idy, mi_col + idx,
                         quant_ratio, tpl_block_size, ref_frame_index, mv);
    }
  }
}

static AOM_INLINE void tpl_model_store(AV1_COMP *cpi,
                                       TplDepStats *tpl_stats_ptr, int mi_row,
                                       int mi_col, BLOCK_SIZE bsize, int stride,
                                       const TplDepStats *src_stats) {
  const int mi_height = mi_size_high[bsize];
  const int mi_width = mi_size_wide[bsize];
  const int step = 1 << cpi->tpl_stats_block_mis_log2;

  int64_t intra_cost = src_stats->intra_cost / (mi_height * mi_width);
  int64_t inter_cost = src_stats->inter_cost / (mi_height * mi_width);

  TplDepStats *tpl_ptr;

  intra_cost = AOMMAX(1, intra_cost);
  inter_cost = AOMMAX(1, inter_cost);

  for (int idy = 0; idy < mi_height; idy += step) {
    tpl_ptr =
        &tpl_stats_ptr[av1_tpl_ptr_pos(cpi, mi_row + idy, mi_col, stride)];
    for (int idx = 0; idx < mi_width; idx += step) {
      tpl_ptr->intra_cost = intra_cost;
      tpl_ptr->inter_cost = inter_cost;
      tpl_ptr->mc_dep_cost = tpl_ptr->intra_cost + tpl_ptr->mc_flow;
      ++tpl_ptr;
    }
  }
}

static YV12_BUFFER_CONFIG *get_framebuf(
    AV1_COMP *cpi, const EncodeFrameInput *const frame_input, int frame_idx) {
  if (frame_idx == 0) {
    RefCntBuffer *ref_buf = get_ref_frame_buf(&cpi->common, GOLDEN_FRAME);
    return &ref_buf->buf;
  } else if (frame_idx == 1) {
    return frame_input ? frame_input->source : NULL;
  } else {
    const GF_GROUP *gf_group = &cpi->gf_group;
    const int frame_disp_idx = gf_group->frame_disp_idx[frame_idx];
    struct lookahead_entry *buf = av1_lookahead_peek(
        cpi->lookahead, frame_disp_idx - cpi->num_gf_group_show_frames);
    return &buf->img;
  }
}

static AOM_INLINE void mc_flow_dispenser(AV1_COMP *cpi, int frame_idx) {
  const GF_GROUP *gf_group = &cpi->gf_group;
  if (frame_idx == gf_group->size) return;
  TplDepFrame *tpl_frame = &cpi->tpl_frame[frame_idx];
  const YV12_BUFFER_CONFIG *this_frame = tpl_frame->gf_picture;
  const YV12_BUFFER_CONFIG *ref_frame[7] = { NULL, NULL, NULL, NULL,
                                             NULL, NULL, NULL };

  AV1_COMMON *cm = &cpi->common;
  struct scale_factors sf;
  int rdmult;
  ThreadData *td = &cpi->td;
  MACROBLOCK *x = &td->mb;
  MACROBLOCKD *xd = &x->e_mbd;
  int mi_row, mi_col;
  const BLOCK_SIZE bsize = convert_length_to_bsize(MC_FLOW_BSIZE_1D);
  av1_tile_init(&xd->tile, cm, 0, 0);

  DECLARE_ALIGNED(32, uint8_t, predictor8[MC_FLOW_NUM_PELS * 2]);
  DECLARE_ALIGNED(32, int16_t, src_diff[MC_FLOW_NUM_PELS]);
  DECLARE_ALIGNED(32, tran_low_t, coeff[MC_FLOW_NUM_PELS]);
  DECLARE_ALIGNED(32, tran_low_t, qcoeff[MC_FLOW_NUM_PELS]);
  DECLARE_ALIGNED(32, tran_low_t, dqcoeff[MC_FLOW_NUM_PELS]);

  const TX_SIZE tx_size = max_txsize_lookup[bsize];
  const int mi_height = mi_size_high[bsize];
  const int mi_width = mi_size_wide[bsize];

  int64_t recon_error = 1, sse = 1;

  // Setup scaling factor
  av1_setup_scale_factors_for_frame(
      &sf, this_frame->y_crop_width, this_frame->y_crop_height,
      this_frame->y_crop_width, this_frame->y_crop_height);

  xd->cur_buf = this_frame;

  uint8_t *predictor =
      is_cur_buf_hbd(xd) ? CONVERT_TO_BYTEPTR(predictor8) : predictor8;

  // Remove duplicate frames
  for (int idx1 = 0; idx1 < INTER_REFS_PER_FRAME; ++idx1) {
    const int rf_idx1 = tpl_frame->ref_map_index[idx1];
    int duplicate = 0;
    for (int idx2 = 0; idx2 < idx1; ++idx2) {
      const int rf_idx2 = tpl_frame->ref_map_index[idx2];
      if (cpi->tpl_frame[rf_idx1].gf_picture ==
          cpi->tpl_frame[rf_idx2].gf_picture) {
        duplicate = 1;
        break;
      }
    }
    if (!duplicate) ref_frame[idx1] = cpi->tpl_frame[rf_idx1].gf_picture;
  }

  // Make a temporary mbmi for tpl model
  MB_MODE_INFO **backup_mi_grid = xd->mi;
  MB_MODE_INFO mbmi;
  memset(&mbmi, 0, sizeof(mbmi));
  MB_MODE_INFO *mbmi_ptr = &mbmi;
  xd->mi = &mbmi_ptr;

  xd->block_ref_scale_factors[0] = &sf;

  const int base_qindex = gf_group->q_val[frame_idx];
  // Get rd multiplier set up.
  rdmult = (int)av1_compute_rd_mult(cpi, base_qindex);
  if (rdmult < 1) rdmult = 1;
  set_error_per_bit(x, rdmult);
  av1_initialize_me_consts(cpi, x, base_qindex);

  tpl_frame->is_valid = 1;

  cm->base_qindex = base_qindex;
  av1_frame_init_quantizer(cpi);

  for (mi_row = 0; mi_row < cm->mi_rows; mi_row += mi_height) {
    // Motion estimation row boundary
    x->mv_limits.row_min = -((mi_row * MI_SIZE) + (17 - 2 * AOM_INTERP_EXTEND));
    x->mv_limits.row_max = (cm->mi_rows - mi_height - mi_row) * MI_SIZE +
                           (17 - 2 * AOM_INTERP_EXTEND);
    xd->mb_to_top_edge = -((mi_row * MI_SIZE) * 8);
    xd->mb_to_bottom_edge = ((cm->mi_rows - mi_height - mi_row) * MI_SIZE) * 8;
    for (mi_col = 0; mi_col < cm->mi_cols; mi_col += mi_width) {
      TplDepStats tpl_stats;
      int ref_frame_index = -1;
      int_mv mv;

      mv.as_int = 0;

      // Motion estimation column boundary
      x->mv_limits.col_min =
          -((mi_col * MI_SIZE) + (17 - 2 * AOM_INTERP_EXTEND));
      x->mv_limits.col_max = ((cm->mi_cols - mi_width - mi_col) * MI_SIZE) +
                             (17 - 2 * AOM_INTERP_EXTEND);
      xd->mb_to_left_edge = -((mi_col * MI_SIZE) * 8);
      xd->mb_to_right_edge = ((cm->mi_cols - mi_width - mi_col) * MI_SIZE) * 8;
      mode_estimation(cpi, x, xd, &sf, frame_idx, src_diff, coeff, qcoeff,
                      dqcoeff, 1, mi_row, mi_col, bsize, tx_size, ref_frame,
                      predictor, &recon_error, &sse, &tpl_stats,
                      &ref_frame_index, &mv);

      // Motion flow dependency dispenser.
      tpl_model_store(cpi, tpl_frame->tpl_stats_ptr, mi_row, mi_col, bsize,
                      tpl_frame->stride, &tpl_stats);
      double quant_ratio = (double)recon_error / sse;
      if (frame_idx) {
        tpl_model_update(cpi, cpi->tpl_frame, tpl_frame->tpl_stats_ptr, mi_row,
                         mi_col, quant_ratio, bsize, ref_frame_index, mv);
      }
    }
  }

  // Restore the mi_grid
  xd->mi = backup_mi_grid;
}

static AOM_INLINE void init_gop_frames_for_tpl(
    AV1_COMP *cpi, const EncodeFrameParams *const init_frame_params,
    GF_GROUP *gf_group, int *tpl_group_frames,
    const EncodeFrameInput *const frame_input) {
  AV1_COMMON *cm = &cpi->common;
  const SequenceHeader *const seq_params = &cm->seq_params;
  int frame_idx = 0;
  RefCntBuffer *frame_bufs = cm->buffer_pool->frame_bufs;
  int pframe_qindex = 0;
  int cur_frame_idx = gf_group->index;

  RefBufferStack ref_buffer_stack = cpi->ref_buffer_stack;
  EncodeFrameParams frame_params = *init_frame_params;

  int ref_picture_map[REF_FRAMES];
  for (int i = 0; i < FRAME_BUFFERS && frame_idx < INTER_REFS_PER_FRAME + 1;
       ++i) {
    if (frame_bufs[i].ref_count == 0) {
      alloc_frame_mvs(cm, &frame_bufs[i]);
      if (aom_realloc_frame_buffer(
              &frame_bufs[i].buf, cm->width, cm->height,
              seq_params->subsampling_x, seq_params->subsampling_y,
              seq_params->use_highbitdepth, cpi->oxcf.border_in_pixels,
              cm->byte_alignment, NULL, NULL, NULL))
        aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                           "Failed to allocate frame buffer");
      ++frame_idx;
    }
  }

  for (int i = 0; i < REF_FRAMES; ++i) {
    if (frame_params.frame_type == KEY_FRAME)
      cpi->tpl_frame[-i - 1].gf_picture = NULL;
    else
      cpi->tpl_frame[-i - 1].gf_picture = &cm->ref_frame_map[i]->buf;

    ref_picture_map[i] = -i - 1;
  }

  *tpl_group_frames = 0;

  int gf_index;
  int use_arf = gf_group->update_type[1] == ARF_UPDATE;
  const int gop_length =
      AOMMIN(gf_group->size - 1 + use_arf, MAX_LENGTH_TPL_FRAME_STATS - 1);
  for (gf_index = cur_frame_idx; gf_index <= gop_length; ++gf_index) {
    TplDepFrame *tpl_frame = &cpi->tpl_frame[gf_index];
    FRAME_UPDATE_TYPE frame_update_type = gf_group->update_type[gf_index];

    frame_params.show_frame = frame_update_type != ARF_UPDATE &&
                              frame_update_type != INTNL_ARF_UPDATE;
    frame_params.show_existing_frame =
        frame_update_type == INTNL_OVERLAY_UPDATE ||
        frame_update_type == OVERLAY_UPDATE;
    frame_params.frame_type =
        frame_update_type == KF_UPDATE ? KEY_FRAME : INTER_FRAME;

    if (frame_update_type == LF_UPDATE)
      pframe_qindex = gf_group->q_val[gf_index];

    if (gf_index == cur_frame_idx) {
      tpl_frame->gf_picture = frame_input->source;
    } else {
      int frame_display_index = gf_index == gf_group->size
                                    ? cpi->rc.baseline_gf_interval
                                    : gf_group->frame_disp_idx[gf_index];
      struct lookahead_entry *buf =
          av1_lookahead_peek(cpi->lookahead, frame_display_index - 1);
      if (buf == NULL) break;
      tpl_frame->gf_picture = &buf->img;
    }

    av1_get_ref_frames(cpi, &ref_buffer_stack);
    int refresh_mask = av1_get_refresh_frame_flags(
        cpi, &frame_params, frame_update_type, &ref_buffer_stack);
    int refresh_frame_map_index = av1_get_refresh_ref_frame_map(refresh_mask);
    av1_update_ref_frame_map(cpi, frame_update_type, refresh_frame_map_index,
                             &ref_buffer_stack);

    for (int i = LAST_FRAME; i <= ALTREF_FRAME; ++i)
      tpl_frame->ref_map_index[i - LAST_FRAME] =
          ref_picture_map[cm->remapped_ref_idx[i - LAST_FRAME]];

    if (refresh_mask) ref_picture_map[refresh_frame_map_index] = gf_index;

    ++*tpl_group_frames;
  }

  if (cur_frame_idx == 0) return;

  int extend_frame_count = 0;
  int frame_display_index = cpi->rc.baseline_gf_interval + 1;

  for (; gf_index < MAX_LENGTH_TPL_FRAME_STATS && extend_frame_count < 2;
       ++gf_index) {
    TplDepFrame *tpl_frame = &cpi->tpl_frame[gf_index];
    FRAME_UPDATE_TYPE frame_update_type = LF_UPDATE;
    frame_params.show_frame = frame_update_type != ARF_UPDATE &&
                              frame_update_type != INTNL_ARF_UPDATE;
    frame_params.show_existing_frame =
        frame_update_type == INTNL_OVERLAY_UPDATE;
    frame_params.frame_type = INTER_FRAME;

    struct lookahead_entry *buf =
        av1_lookahead_peek(cpi->lookahead, frame_display_index - 1);

    if (buf == NULL) break;

    tpl_frame->gf_picture = &buf->img;

    gf_group->update_type[gf_index] = LF_UPDATE;
    gf_group->q_val[gf_index] = pframe_qindex;

    av1_get_ref_frames(cpi, &ref_buffer_stack);
    int refresh_mask = av1_get_refresh_frame_flags(
        cpi, &frame_params, frame_update_type, &ref_buffer_stack);
    int refresh_frame_map_index = av1_get_refresh_ref_frame_map(refresh_mask);
    av1_update_ref_frame_map(cpi, frame_update_type, refresh_frame_map_index,
                             &ref_buffer_stack);

    for (int i = LAST_FRAME; i <= ALTREF_FRAME; ++i)
      tpl_frame->ref_map_index[i - LAST_FRAME] =
          ref_picture_map[cm->remapped_ref_idx[i - LAST_FRAME]];

    if (refresh_mask) ref_picture_map[refresh_frame_map_index] = gf_index;

    ++*tpl_group_frames;
    ++extend_frame_count;
    ++frame_display_index;
  }

  av1_get_ref_frames(cpi, &cpi->ref_buffer_stack);
}

static AOM_INLINE void init_tpl_stats(AV1_COMP *cpi) {
  for (int frame_idx = 0; frame_idx < MAX_LENGTH_TPL_FRAME_STATS; ++frame_idx) {
    TplDepFrame *tpl_frame = &cpi->tpl_stats_buffer[frame_idx];
    memset(tpl_frame->tpl_stats_ptr, 0,
           tpl_frame->height * tpl_frame->width *
               sizeof(*tpl_frame->tpl_stats_ptr));
    tpl_frame->is_valid = 0;
  }
}

void av1_tpl_setup_stats(AV1_COMP *cpi,
                         const EncodeFrameParams *const frame_params,
                         const EncodeFrameInput *const frame_input) {
  AV1_COMMON *cm = &cpi->common;
  GF_GROUP *gf_group = &cpi->gf_group;
  int bottom_index, top_index;
  EncodeFrameParams this_frame_params = *frame_params;

  for (int gf_index = gf_group->index; gf_index < gf_group->size; ++gf_index) {
    av1_configure_buffer_updates(cpi, &this_frame_params,
                                 gf_group->update_type[gf_index], 0);

    cpi->refresh_last_frame = this_frame_params.refresh_last_frame;
    cpi->refresh_golden_frame = this_frame_params.refresh_golden_frame;
    cpi->refresh_bwd_ref_frame = this_frame_params.refresh_bwd_ref_frame;
    cpi->refresh_alt_ref_frame = this_frame_params.refresh_alt_ref_frame;

    gf_group->q_val[gf_index] = av1_rc_pick_q_and_bounds(
        cpi, cm->width, cm->height, gf_index, &bottom_index, &top_index);

    cm->current_frame.frame_type = INTER_FRAME;
  }

  init_gop_frames_for_tpl(cpi, frame_params, gf_group,
                          &cpi->tpl_gf_group_frames, frame_input);

  init_tpl_stats(cpi);

  if (cpi->oxcf.enable_tpl_model == 1) {
    // Backward propagation from tpl_group_frames to 1.
    for (int frame_idx = cpi->tpl_gf_group_frames - 1;
         frame_idx >= gf_group->index; --frame_idx) {
      if (gf_group->update_type[frame_idx] == INTNL_OVERLAY_UPDATE ||
          gf_group->update_type[frame_idx] == OVERLAY_UPDATE)
        continue;

      mc_flow_dispenser(cpi, frame_idx);
    }
  }

  av1_configure_buffer_updates(cpi, &this_frame_params,
                               gf_group->update_type[gf_group->index], 0);
  cm->current_frame.frame_type = frame_params->frame_type;
}

static AOM_INLINE void get_tpl_forward_stats(AV1_COMP *cpi, MACROBLOCK *x,
                                             MACROBLOCKD *xd, BLOCK_SIZE bsize,
                                             int use_satd,
                                             YV12_BUFFER_CONFIG *ref,
                                             YV12_BUFFER_CONFIG *src,
                                             TplDepFrame *ref_tpl_frame) {
// TODO(yuec) Consider deleting forward tpl model completely
#if !USE_TPL_CLASSIC_MODEL
  AV1_COMMON *cm = &cpi->common;
  const int bw = 4 << mi_size_wide_log2[bsize];
  const int bh = 4 << mi_size_high_log2[bsize];
  const int mi_height = mi_size_high[bsize];
  const int mi_width = mi_size_wide[bsize];
  const int pix_num = bw * bh;
  const TX_SIZE tx_size = max_txsize_lookup[bsize];

  DECLARE_ALIGNED(32, uint8_t, predictor8[MC_FLOW_NUM_PELS * 2]);
  DECLARE_ALIGNED(32, int16_t, src_diff[MC_FLOW_NUM_PELS]);
  DECLARE_ALIGNED(32, tran_low_t, coeff[MC_FLOW_NUM_PELS]);
  uint8_t *predictor =
      is_cur_buf_hbd(xd) ? CONVERT_TO_BYTEPTR(predictor8) : predictor8;

  // Initialize advanced prediction parameters as default values
  struct scale_factors sf;
  av1_setup_scale_factors_for_frame(&sf, ref->y_crop_width, ref->y_crop_height,
                                    src->y_crop_width, src->y_crop_height);
  ConvolveParams conv_params = get_conv_params(0, 0, xd->bd);
  WarpTypesAllowed warp_types;
  memset(&warp_types, 0, sizeof(WarpTypesAllowed));
  const int_interpfilters kernel =
      av1_broadcast_interp_filter(EIGHTTAP_REGULAR);
  xd->above_mbmi = NULL;
  xd->left_mbmi = NULL;
  xd->mi[0]->sb_type = bsize;
  xd->mi[0]->motion_mode = SIMPLE_TRANSLATION;
  xd->block_ref_scale_factors[0] = &sf;

  for (int mi_row = 0; mi_row < cm->mi_rows; mi_row += mi_height) {
    // Motion estimation row boundary
    x->mv_limits.row_min = -((mi_row * MI_SIZE) + (17 - 2 * AOM_INTERP_EXTEND));
    x->mv_limits.row_max = (cm->mi_rows - mi_height - mi_row) * MI_SIZE +
                           (17 - 2 * AOM_INTERP_EXTEND);
    xd->mb_to_top_edge = -((mi_row * MI_SIZE) * 8);
    xd->mb_to_bottom_edge = ((cm->mi_rows - mi_height - mi_row) * MI_SIZE) * 8;

    for (int mi_col = 0; mi_col < cm->mi_cols; mi_col += mi_width) {
      int64_t inter_cost, intra_cost;
      x->mv_limits.col_min =
          -((mi_col * MI_SIZE) + (17 - 2 * AOM_INTERP_EXTEND));
      x->mv_limits.col_max = ((cm->mi_cols - mi_width - mi_col) * MI_SIZE) +
                             (17 - 2 * AOM_INTERP_EXTEND);
      xd->mb_to_left_edge = -((mi_col * MI_SIZE) * 8);
      xd->mb_to_right_edge = ((cm->mi_cols - mi_width - mi_col) * MI_SIZE) * 8;

      // Intra mode
      xd->mi[0]->ref_frame[0] = INTRA_FRAME;
      int64_t best_intra_cost = INT64_MAX;
      for (PREDICTION_MODE mode = DC_PRED; mode <= PAETH_PRED; ++mode) {
        uint8_t *src_buf =
            src->y_buffer + mi_row * MI_SIZE * src->y_stride + mi_col * MI_SIZE;
        const int src_stride = src->y_stride;

        uint8_t *dst_buf = predictor;
        const int dst_stride = bw;

        av1_predict_intra_block(cm, xd, bw, bh, tx_size, mode, 0, 0,
                                FILTER_INTRA_MODES, src_buf, src_stride,
                                dst_buf, dst_stride, 0, 0, 0);

        if (use_satd) {
#if CONFIG_AV1_HIGHBITDEPTH
          if (is_cur_buf_hbd(xd)) {
            aom_highbd_subtract_block(bh, bw, src_diff, bw, src_buf, src_stride,
                                      dst_buf, dst_stride, xd->bd);
          } else {
            aom_subtract_block(bh, bw, src_diff, bw, src_buf, src_stride,
                               dst_buf, dst_stride);
          }
#else
          aom_subtract_block(bh, bw, src_diff, bw, src_buf, src_stride, dst_buf,
                             dst_stride);
#endif
          wht_fwd_txfm(src_diff, bw, coeff, tx_size, is_cur_buf_hbd(xd));

          intra_cost = aom_satd(coeff, pix_num);
        } else {
          int64_t sse;
#if CONFIG_AV1_HIGHBITDEPTH
          if (is_cur_buf_hbd(xd)) {
            sse = aom_highbd_sse(src_buf, src_stride, dst_buf, dst_stride, bw,
                                 bh);
          } else {
            sse = aom_sse(src_buf, src_stride, dst_buf, dst_stride, bw, bh);
          }
#else
          sse = aom_sse(src_buf, src_stride, dst_buf, dst_stride, bw, bh);
#endif
          intra_cost = ROUND_POWER_OF_TWO(sse, (xd->bd - 8) * 2);
        }
        if (intra_cost < best_intra_cost) best_intra_cost = intra_cost;
      }

      // Inter mode
      // Motion estimation column boundary
      xd->mi[0]->ref_frame[0] = GOLDEN_FRAME;

      const int mb_y_offset =
          mi_row * MI_SIZE * src->y_stride + mi_col * MI_SIZE;
      const int mb_y_offset_ref =
          mi_row * MI_SIZE * ref->y_stride + mi_col * MI_SIZE;
      motion_estimation(cpi, x, src->y_buffer + mb_y_offset,
                        ref->y_buffer + mb_y_offset_ref, src->y_stride,
                        ref->y_stride, bsize, mi_row, mi_col);

      av1_build_inter_predictor(
          ref->y_buffer + mb_y_offset_ref, ref->y_stride, predictor, bw,
          &x->best_mv.as_mv, &sf, bw, bh, &conv_params, kernel, &warp_types,
          mi_col * MI_SIZE, mi_row * MI_SIZE, 0, 0, MV_PRECISION_Q3,
          mi_col * MI_SIZE, mi_row * MI_SIZE, xd, 0);
      if (use_satd) {
#if CONFIG_AV1_HIGHBITDEPTH
        if (is_cur_buf_hbd(xd)) {
          aom_highbd_subtract_block(bh, bw, src_diff, bw,
                                    src->y_buffer + mb_y_offset, src->y_stride,
                                    predictor, bw, xd->bd);
        } else {
          aom_subtract_block(bh, bw, src_diff, bw, src->y_buffer + mb_y_offset,
                             src->y_stride, predictor, bw);
        }
#else
        aom_subtract_block(bh, bw, src_diff, bw, src->y_buffer + mb_y_offset,
                           src->y_stride, predictor, bw);
#endif
        wht_fwd_txfm(src_diff, bw, coeff, tx_size, is_cur_buf_hbd(xd));
        inter_cost = aom_satd(coeff, pix_num);
      } else {
        int64_t sse;
#if CONFIG_AV1_HIGHBITDEPTH
        if (is_cur_buf_hbd(xd)) {
          sse = aom_highbd_sse(src->y_buffer + mb_y_offset, src->y_stride,
                               predictor, bw, bw, bh);
        } else {
          sse = aom_sse(src->y_buffer + mb_y_offset, src->y_stride, predictor,
                        bw, bw, bh);
        }
#else
        sse = aom_sse(src->y_buffer + mb_y_offset, src->y_stride, predictor, bw,
                      bw, bh);
#endif
        inter_cost = ROUND_POWER_OF_TWO(sse, (xd->bd - 8) * 2);
      }

      // Finalize stats
      best_intra_cost = AOMMAX(best_intra_cost, 1);
      inter_cost = AOMMIN(best_intra_cost, inter_cost);

      // Project stats to reference block
      TplDepStats *ref_stats_ptr = ref_tpl_frame->tpl_stats_ptr;
      const MV mv = x->best_mv.as_mv;
      const int mv_row = mv.row >> 3;
      const int mv_col = mv.col >> 3;
      const int ref_pos_row = mi_row * MI_SIZE + mv_row;
      const int ref_pos_col = mi_col * MI_SIZE + mv_col;
      const int grid_pos_row_base = round_floor(ref_pos_row, bh) * bh;
      const int grid_pos_col_base = round_floor(ref_pos_col, bw) * bw;

      for (int block = 0; block < 4; ++block) {
        const int grid_pos_row = grid_pos_row_base + bh * (block >> 1);
        const int grid_pos_col = grid_pos_col_base + bw * (block & 0x01);

        if (grid_pos_row >= 0 &&
            grid_pos_row < ref_tpl_frame->mi_rows * MI_SIZE &&
            grid_pos_col >= 0 &&
            grid_pos_col < ref_tpl_frame->mi_cols * MI_SIZE) {
          const int overlap_area =
              get_overlap_area(grid_pos_row, grid_pos_col, ref_pos_row,
                               ref_pos_col, block, bsize);
          const int ref_mi_row = round_floor(grid_pos_row, bh) * mi_height;
          const int ref_mi_col = round_floor(grid_pos_col, bw) * mi_width;

          const int64_t mc_saved = (best_intra_cost - inter_cost)
                                   << TPL_DEP_COST_SCALE_LOG2;
          for (int idy = 0; idy < mi_height; ++idy) {
            for (int idx = 0; idx < mi_width; ++idx) {
              TplDepStats *des_stats =
                  &ref_stats_ptr[(ref_mi_row + idy) * ref_tpl_frame->stride +
                                 (ref_mi_col + idx)];
              des_stats->mc_count += overlap_area << TPL_DEP_COST_SCALE_LOG2;
              des_stats->mc_saved += (mc_saved * overlap_area) / pix_num;

              assert(overlap_area >= 0);
            }
          }
        }
      }
    }
  }
#else
  (void)cpi;
  (void)x;
  (void)xd;
  (void)bsize;
  (void)use_satd;
  (void)ref;
  (void)src;
  (void)ref_tpl_frame;
#endif  // !USE_TPL_CLASSIC_MODEL
}

void av1_tpl_setup_forward_stats(AV1_COMP *cpi) {
  ThreadData *td = &cpi->td;
  MACROBLOCK *x = &td->mb;
  MACROBLOCKD *xd = &x->e_mbd;
  const BLOCK_SIZE bsize = convert_length_to_bsize(MC_FLOW_BSIZE_1D);

  const GF_GROUP *gf_group = &cpi->gf_group;
  assert(IMPLIES(gf_group->size > 0, gf_group->index < gf_group->size));
  const int tpl_cur_idx = gf_group->frame_disp_idx[gf_group->index];
  TplDepFrame *tpl_frame = &cpi->tpl_frame[tpl_cur_idx];
  memset(
      tpl_frame->tpl_stats_ptr, 0,
      tpl_frame->height * tpl_frame->width * sizeof(*tpl_frame->tpl_stats_ptr));
  tpl_frame->is_valid = 0;
  int tpl_used_mask[MAX_LENGTH_TPL_FRAME_STATS] = { 0 };
  for (int idx = gf_group->index + 1; idx < cpi->tpl_gf_group_frames; ++idx) {
    const int tpl_future_idx = gf_group->frame_disp_idx[idx];

    if (gf_group->update_type[idx] == OVERLAY_UPDATE ||
        gf_group->update_type[idx] == INTNL_OVERLAY_UPDATE)
      continue;
    if (tpl_future_idx == tpl_cur_idx) continue;
    if (tpl_used_mask[tpl_future_idx]) continue;

    for (int ridx = 0; ridx < INTER_REFS_PER_FRAME; ++ridx) {
      const int ref_idx = gf_group->ref_frame_gop_idx[idx][ridx];
      const int tpl_ref_idx = gf_group->frame_disp_idx[ref_idx];
      if (tpl_ref_idx == tpl_cur_idx) {
        // Do tpl stats computation between current buffer and the one at
        // gf_group index given by idx (and with disp index given by
        // tpl_future_idx).
        assert(idx >= 2);
        YV12_BUFFER_CONFIG *cur_buf = &cpi->common.cur_frame->buf;
        YV12_BUFFER_CONFIG *future_buf = get_framebuf(cpi, NULL, idx);
        get_tpl_forward_stats(cpi, x, xd, bsize, 0, cur_buf, future_buf,
                              tpl_frame);
        tpl_frame->is_valid = 1;
        tpl_used_mask[tpl_future_idx] = 1;
      }
    }
  }
}

void av1_tpl_rdmult_setup(AV1_COMP *cpi) {
  const AV1_COMMON *const cm = &cpi->common;
  const GF_GROUP *const gf_group = &cpi->gf_group;
  const int tpl_idx = gf_group->index;

  assert(IMPLIES(gf_group->size > 0, tpl_idx < gf_group->size));

  const TplDepFrame *const tpl_frame = &cpi->tpl_frame[tpl_idx];

  if (!tpl_frame->is_valid) return;
  if (cpi->oxcf.superres_mode != SUPERRES_NONE) return;

  const TplDepStats *const tpl_stats = tpl_frame->tpl_stats_ptr;
  const int tpl_stride = tpl_frame->stride;
  const int mi_cols_sr = av1_pixels_to_mi(cm->superres_upscaled_width);

  const int block_size = BLOCK_16X16;
  const int num_mi_w = mi_size_wide[block_size];
  const int num_mi_h = mi_size_high[block_size];
  const int num_cols = (mi_cols_sr + num_mi_w - 1) / num_mi_w;
  const int num_rows = (cm->mi_rows + num_mi_h - 1) / num_mi_h;
  const double c = 1.2;
  const int step = 1 << cpi->tpl_stats_block_mis_log2;

  aom_clear_system_state();

  // Loop through each 'block_size' X 'block_size' block.
  for (int row = 0; row < num_rows; row++) {
    for (int col = 0; col < num_cols; col++) {
      double intra_cost = 0.0, mc_dep_cost = 0.0;
      // Loop through each mi block.
      for (int mi_row = row * num_mi_h; mi_row < (row + 1) * num_mi_h;
           mi_row += step) {
        for (int mi_col = col * num_mi_w; mi_col < (col + 1) * num_mi_w;
             mi_col += step) {
          if (mi_row >= cm->mi_rows || mi_col >= mi_cols_sr) continue;
          const TplDepStats *this_stats =
              &tpl_stats[av1_tpl_ptr_pos(cpi, mi_row, mi_col, tpl_stride)];
          intra_cost += (double)this_stats->intra_cost;
          mc_dep_cost += (double)this_stats->intra_cost + this_stats->mc_flow;
        }
      }
      const double rk = intra_cost / mc_dep_cost;
      const int index = row * num_cols + col;
      cpi->tpl_rdmult_scaling_factors[index] = rk / cpi->rd.r0 + c;
    }
  }
  aom_clear_system_state();
}

void av1_tpl_rdmult_setup_sb(AV1_COMP *cpi, MACROBLOCK *const x,
                             BLOCK_SIZE sb_size, int mi_row, int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  assert(IMPLIES(cpi->gf_group.size > 0,
                 cpi->gf_group.index < cpi->gf_group.size));
  const int tpl_idx = cpi->gf_group.index;
  TplDepFrame *tpl_frame = &cpi->tpl_frame[tpl_idx];

  if (cpi->tpl_model_pass == 1) {
    assert(cpi->oxcf.enable_tpl_model == 2);
    return;
  }
  if (tpl_frame->is_valid == 0) return;
  if (!is_frame_tpl_eligible(cpi)) return;
  if (tpl_idx >= MAX_LAG_BUFFERS) return;
  if (cpi->oxcf.superres_mode != SUPERRES_NONE) return;
  if (cpi->oxcf.aq_mode != NO_AQ) return;

  const int bsize_base = BLOCK_16X16;
  const int num_mi_w = mi_size_wide[bsize_base];
  const int num_mi_h = mi_size_high[bsize_base];
  const int num_cols = (cm->mi_cols + num_mi_w - 1) / num_mi_w;
  const int num_rows = (cm->mi_rows + num_mi_h - 1) / num_mi_h;
  const int num_bcols = (mi_size_wide[sb_size] + num_mi_w - 1) / num_mi_w;
  const int num_brows = (mi_size_high[sb_size] + num_mi_h - 1) / num_mi_h;
  int row, col;

  double base_block_count = 0.0;
  double log_sum = 0.0;

  aom_clear_system_state();
  for (row = mi_row / num_mi_w;
       row < num_rows && row < mi_row / num_mi_w + num_brows; ++row) {
    for (col = mi_col / num_mi_h;
         col < num_cols && col < mi_col / num_mi_h + num_bcols; ++col) {
      const int index = row * num_cols + col;
      log_sum += log(cpi->tpl_rdmult_scaling_factors[index]);
      base_block_count += 1.0;
    }
  }

  MACROBLOCKD *const xd = &x->e_mbd;
  const int orig_rdmult =
      av1_compute_rd_mult(cpi, cm->base_qindex + cm->y_dc_delta_q);
  const int new_rdmult = av1_compute_rd_mult(
      cpi, cm->base_qindex + xd->delta_qindex + cm->y_dc_delta_q);
  const double scaling_factor = (double)new_rdmult / (double)orig_rdmult;

  double scale_adj = log(scaling_factor) - log_sum / base_block_count;
  scale_adj = exp(scale_adj);

  for (row = mi_row / num_mi_w;
       row < num_rows && row < mi_row / num_mi_w + num_brows; ++row) {
    for (col = mi_col / num_mi_h;
         col < num_cols && col < mi_col / num_mi_h + num_bcols; ++col) {
      const int index = row * num_cols + col;
      cpi->tpl_sb_rdmult_scaling_factors[index] =
          scale_adj * cpi->tpl_rdmult_scaling_factors[index];
    }
  }
  aom_clear_system_state();
}
