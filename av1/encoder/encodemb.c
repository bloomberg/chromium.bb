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

#include "./av1_rtcd.h"
#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"

#include "aom_dsp/bitwriter.h"
#include "aom_dsp/quantize.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"

#include "av1/common/idct.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/scan.h"

#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/encodemb.h"
#if CONFIG_LV_MAP
#include "av1/encoder/encodetxb.h"
#endif
#include "av1/encoder/hybrid_fwd_txfm.h"
#if CONFIG_DAALA_TX
#include "av1/common/daala_inv_txfm.h"
#endif
#include "av1/encoder/rd.h"
#include "av1/encoder/tokenize.h"

#if CONFIG_CFL
#include "av1/common/cfl.h"
#endif

// Check if one needs to use c version subtraction.
static int check_subtract_block_size(int w, int h) { return w < 4 || h < 4; }

static void subtract_block(const MACROBLOCKD *xd, int rows, int cols,
                           int16_t *diff, ptrdiff_t diff_stride,
                           const uint8_t *src8, ptrdiff_t src_stride,
                           const uint8_t *pred8, ptrdiff_t pred_stride) {
#if !CONFIG_HIGHBITDEPTH
  (void)xd;
#endif

  if (check_subtract_block_size(rows, cols)) {
#if CONFIG_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      aom_highbd_subtract_block_c(rows, cols, diff, diff_stride, src8,
                                  src_stride, pred8, pred_stride, xd->bd);
      return;
    }
#endif  // CONFIG_HIGHBITDEPTH
    aom_subtract_block_c(rows, cols, diff, diff_stride, src8, src_stride, pred8,
                         pred_stride);

    return;
  }

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    aom_highbd_subtract_block(rows, cols, diff, diff_stride, src8, src_stride,
                              pred8, pred_stride, xd->bd);
    return;
  }
#endif  // CONFIG_HIGHBITDEPTH
  aom_subtract_block(rows, cols, diff, diff_stride, src8, src_stride, pred8,
                     pred_stride);
}

void av1_subtract_txb(MACROBLOCK *x, int plane, BLOCK_SIZE plane_bsize,
                      int blk_col, int blk_row, TX_SIZE tx_size) {
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = &x->plane[plane];
  const struct macroblockd_plane *const pd = &x->e_mbd.plane[plane];
  const int diff_stride = block_size_wide[plane_bsize];
  const int src_stride = p->src.stride;
  const int dst_stride = pd->dst.stride;
  const int tx1d_width = tx_size_wide[tx_size];
  const int tx1d_height = tx_size_high[tx_size];
  uint8_t *dst =
      &pd->dst.buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];
  uint8_t *src =
      &p->src.buf[(blk_row * src_stride + blk_col) << tx_size_wide_log2[0]];
  int16_t *src_diff =
      &p->src_diff[(blk_row * diff_stride + blk_col) << tx_size_wide_log2[0]];
  subtract_block(xd, tx1d_height, tx1d_width, src_diff, diff_stride, src,
                 src_stride, dst, dst_stride);
}

void av1_subtract_plane(MACROBLOCK *x, BLOCK_SIZE bsize, int plane) {
  struct macroblock_plane *const p = &x->plane[plane];
  const struct macroblockd_plane *const pd = &x->e_mbd.plane[plane];
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
  const int bw = block_size_wide[plane_bsize];
  const int bh = block_size_high[plane_bsize];
  const MACROBLOCKD *xd = &x->e_mbd;

  subtract_block(xd, bh, bw, p->src_diff, bw, p->src.buf, p->src.stride,
                 pd->dst.buf, pd->dst.stride);
}

#if !CONFIG_LV_MAP
// Shifting negative values is undefined behaviour in C99,
// and could mislead the optimizer, who might assume the shifted is positive.
// This also avoids ubsan warnings.
// In practise, this gets inlined by the optimizer to a single instruction.
static INLINE int signed_shift_right(int x, int shift) {
  if (x >= 0)
    return x >> shift;
  else
    return -((-x) >> shift);
}

// These numbers are empirically obtained.
static const int plane_rd_mult[REF_TYPES][PLANE_TYPES] = {
  { 10, 7 }, { 8, 5 },
};

static int optimize_b_greedy(const AV1_COMMON *cm, MACROBLOCK *mb, int plane,
                             int blk_row, int blk_col, int block,
                             TX_SIZE tx_size, int ctx, int fast_mode) {
  MACROBLOCKD *const xd = &mb->e_mbd;
  struct macroblock_plane *const p = &mb->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const PLANE_TYPE plane_type = pd->plane_type;
  const int eob = p->eobs[block];
  assert(mb->qindex > 0);
  assert((!plane_type && !plane) || (plane_type && plane));
  assert(eob <= av1_get_max_eob(tx_size));
  const int ref = is_inter_block(&xd->mi[0]->mbmi);
  const tran_low_t *const coeff = BLOCK_OFFSET(p->coeff, block);
  tran_low_t *const qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  tran_low_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  const int16_t *const dequant_ptr = p->dequant_QTX;
  const uint8_t *const band_translate = get_band_translate(tx_size);
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  const SCAN_ORDER *const scan_order =
      get_scan(cm, tx_size, tx_type, &xd->mi[0]->mbmi);
  const int16_t *const scan = scan_order->scan;
  const int16_t *const nb = scan_order->neighbors;
#if CONFIG_DAALA_TX
  // This is one of the few places where RDO is done on coeffs; it
  // expects the coeffs to be in Q3/D11, so we need to scale them.
  int depth_shift = (TX_COEFF_DEPTH - 11) * 2;
  int depth_round = depth_shift > 1 ? (1 << depth_shift >> 1) : 0;
#else
  const int shift = av1_get_tx_scale(tx_size);
#endif
#if CONFIG_AOM_QM
  int seg_id = xd->mi[0]->mbmi.segment_id;
  // Use a flat matrix (i.e. no weighting) for 1D and Identity transforms
  const qm_val_t *iqmatrix = IS_2D_TRANSFORM(tx_type)
                                 ? pd->seg_iqmatrix[seg_id][tx_size]
                                 : cm->giqmatrix[NUM_QM_LEVELS - 1][0][tx_size];
#endif
#if CONFIG_NEW_QUANT
  int dq = get_dq_profile_from_ctx(mb->qindex, ctx, ref, plane_type);
  const dequant_val_type_nuq *dequant_val = p->dequant_val_nuq_QTX[dq];
#endif  // CONFIG_NEW_QUANT
  int64_t rd_cost0, rd_cost1;
  int16_t t0, t1;
  int i, final_eob = 0;
  const int cat6_bits = av1_get_cat6_extrabits_size(tx_size, xd->bd);
  int(*head_token_costs)[COEFF_CONTEXTS][TAIL_TOKENS] =
      mb->token_head_costs[txsize_sqr_map[tx_size]][plane_type][ref];
  int(*tail_token_costs)[COEFF_CONTEXTS][TAIL_TOKENS] =
      mb->token_tail_costs[txsize_sqr_map[tx_size]][plane_type][ref];

  const int64_t rdmult = (mb->rdmult * plane_rd_mult[ref][plane_type]) >> 1;
  int64_t rate0, rate1;
  int64_t eob_cost0, eob_cost1;
  tran_low_t before_best_eob_qc = 0;
  tran_low_t before_best_eob_dqc = 0;

  uint8_t token_cache[MAX_TX_SQUARE];

  // TODO(debargha): Implement a fast mode. For now just skip.
  if (fast_mode) return eob;

  for (i = 0; i < eob; i++) {
    const int rc = scan[i];
    token_cache[rc] = av1_pt_energy_class[av1_get_token(qcoeff[rc])];
  }

  /* Record the r-d cost */
  int64_t accu_rate = 0;
  // Initialized to the worst possible error for the largest transform size.
  // This ensures that it never goes negative.
  int64_t accu_error = ((int64_t)1) << 50;
  rate0 = head_token_costs[0][ctx][0];
  int64_t best_block_rd_cost = RDCOST(rdmult, rate0, accu_error);

  // int64_t best_block_rd_cost_all0 = best_block_rd_cost;
  const int seg_eob =
      av1_get_tx_eob(&cm->seg, xd->mi[0]->mbmi.segment_id, tx_size);
  for (i = 0; i < eob; i++) {
    const int rc = scan[i];
    const int x = qcoeff[rc];
    const int sz = -(x < 0);
    const int band_cur = band_translate[i];
    const int ctx_cur = (i == 0) ? ctx : get_coef_context(nb, token_cache, i);
    const int8_t eob_val =
        (i + 1 == eob) ? (i + 1 == seg_eob ? LAST_EOB : EARLY_EOB) : NO_EOB;
    const int is_first = (i == 0);

    if (x == 0) {
      // no need to search when x == 0
      accu_rate += av1_get_coeff_token_cost(
          ZERO_TOKEN, eob_val, is_first, head_token_costs[band_cur][ctx_cur],
          tail_token_costs[band_cur][ctx_cur]);
      // accu_error does not change when x==0
    } else {
/*  Computing distortion
 */
// compute the distortion for the first candidate
// and the distortion for quantizing to 0.
#if CONFIG_DAALA_TX
      int dx0 = coeff[rc];
      const int64_t d0 = ((int64_t)dx0 * dx0 + depth_round) >> depth_shift;
#else
      int dx0 = abs(coeff[rc]) * (1 << shift);
      dx0 >>= xd->bd - 8;

      const int64_t d0 = (int64_t)dx0 * dx0;
#endif
      const int x_a = x - 2 * sz - 1;
      int dqv;
#if CONFIG_AOM_QM
      int iwt;
      dqv = dequant_ptr[rc != 0];
      if (iqmatrix != NULL) {
        iwt = iqmatrix[rc];
        dqv = ((iwt * (int)dqv) + (1 << (AOM_QM_BITS - 1))) >> AOM_QM_BITS;
      }
#else
      dqv = dequant_ptr[rc != 0];
#endif

#if CONFIG_DAALA_TX
      int dx = dqcoeff[rc] - coeff[rc];
      const int64_t d2 = ((int64_t)dx * dx + depth_round) >> depth_shift;
#else
      int dx = (dqcoeff[rc] - coeff[rc]) * (1 << shift);
      dx = signed_shift_right(dx, xd->bd - 8);
      const int64_t d2 = (int64_t)dx * dx;
#endif

      /* compute the distortion for the second candidate
       * x_a = x - 2 * sz + 1;
       */
      int64_t d2_a;
      if (x_a != 0) {
#if CONFIG_DAALA_TX
#if CONFIG_NEW_QUANT
        dx = av1_dequant_coeff_nuq(x, dqv, dequant_val[band_translate[i]]) -
             coeff[rc];
#else   // CONFIG_NEW_QUANT
        dx -= (dqv + sz) ^ sz;
#endif  // CONFIG_NEW_QUANT
        d2_a = ((int64_t)dx * dx + depth_round) >> depth_shift;
#else  // CONFIG_DAALA_TX
#if CONFIG_NEW_QUANT
        dx = av1_dequant_coeff_nuq(x_a, dqv, dequant_val[band_translate[i]]) -
             (coeff[rc] * (1 << shift));
        dx >>= xd->bd - 8;
#else   // CONFIG_NEW_QUANT
        dx -= ((dqv >> (xd->bd - 8)) + sz) ^ sz;
#endif  // CONFIG_NEW_QUANT
        d2_a = (int64_t)dx * dx;
#endif  // CONFIG_DAALA_TX
      } else {
        d2_a = d0;
      }

      // Computing RD cost
      int64_t base_bits;
      // rate cost of x
      base_bits = av1_get_token_cost(x, &t0, cat6_bits);
      rate0 = base_bits +
              av1_get_coeff_token_cost(t0, eob_val, is_first,
                                       head_token_costs[band_cur][ctx_cur],
                                       tail_token_costs[band_cur][ctx_cur]);
      // rate cost of x_a
      base_bits = av1_get_token_cost(x_a, &t1, cat6_bits);
      if (t1 == ZERO_TOKEN && eob_val) {
        rate1 = base_bits;
      } else {
        rate1 = base_bits +
                av1_get_coeff_token_cost(t1, eob_val, is_first,
                                         head_token_costs[band_cur][ctx_cur],
                                         tail_token_costs[band_cur][ctx_cur]);
      }

      int64_t next_bits0 = 0, next_bits1 = 0;
      if (i < eob - 1) {
        int ctx_next;
        const int band_next = band_translate[i + 1];
        const int token_next = av1_get_token(qcoeff[scan[i + 1]]);
        const int8_t eob_val_next =
            (i + 2 == eob) ? (i + 2 == seg_eob ? LAST_EOB : EARLY_EOB) : NO_EOB;

        token_cache[rc] = av1_pt_energy_class[t0];
        ctx_next = get_coef_context(nb, token_cache, i + 1);
        next_bits0 = av1_get_coeff_token_cost(
            token_next, eob_val_next, 0, head_token_costs[band_next][ctx_next],
            tail_token_costs[band_next][ctx_next]);

        token_cache[rc] = av1_pt_energy_class[t1];
        ctx_next = get_coef_context(nb, token_cache, i + 1);
        next_bits1 = av1_get_coeff_token_cost(
            token_next, eob_val_next, 0, head_token_costs[band_next][ctx_next],
            tail_token_costs[band_next][ctx_next]);
      }

      rd_cost0 = RDCOST(rdmult, (rate0 + next_bits0), d2);
      rd_cost1 = RDCOST(rdmult, (rate1 + next_bits1), d2_a);
      const int best_x = (rd_cost1 < rd_cost0);

      const int eob_v = (i + 1 == seg_eob) ? LAST_EOB : EARLY_EOB;
      int64_t next_eob_bits0, next_eob_bits1;
      int best_eob_x;
      next_eob_bits0 = av1_get_coeff_token_cost(
          t0, eob_v, is_first, head_token_costs[band_cur][ctx_cur],
          tail_token_costs[band_cur][ctx_cur]);
      eob_cost0 =
          RDCOST(rdmult, (accu_rate + next_eob_bits0), (accu_error + d2 - d0));
      eob_cost1 = eob_cost0;
      if (x_a != 0) {
        next_eob_bits1 = av1_get_coeff_token_cost(
            t1, eob_v, is_first, head_token_costs[band_cur][ctx_cur],
            tail_token_costs[band_cur][ctx_cur]);
        eob_cost1 = RDCOST(rdmult, (accu_rate + next_eob_bits1),
                           (accu_error + d2_a - d0));
        best_eob_x = (eob_cost1 < eob_cost0);
      } else {
        best_eob_x = 0;
      }

      const int dqc = dqcoeff[rc];
      int dqc_a = 0;
      if (best_x || best_eob_x) {
        if (x_a != 0) {
#if CONFIG_DAALA_TX
#if CONFIG_NEW_QUANT
          dqc_a = av1_dequant_abscoeff_nuq(abs(x_a), dqv,
                                           dequant_val[band_translate[i]]);
          if (sz) dqc_a = -dqc_a;
#else
          dqc_a = x_a * dqv;
#endif  // CONFIG_NEW_QUANT
#else   // CONFIG_DAALA_TX
#if CONFIG_NEW_QUANT
          dqc_a = av1_dequant_abscoeff_nuq(abs(x_a), dqv,
                                           dequant_val[band_translate[i]]);
          dqc_a = shift ? ROUND_POWER_OF_TWO(dqc_a, shift) : dqc_a;
          if (sz) dqc_a = -dqc_a;
#else
          if (x_a < 0)
            dqc_a = -((-x_a * dqv) >> shift);
          else
            dqc_a = (x_a * dqv) >> shift;
#endif  // CONFIG_NEW_QUANT
#endif  // CONFIG_DAALA_TX
        } else {
          dqc_a = 0;
        }
      }

      // record the better quantized value
      if (best_x) {
        assert(d2_a <= d0);
        qcoeff[rc] = x_a;
        dqcoeff[rc] = dqc_a;
        accu_rate += rate1;
        accu_error += d2_a - d0;
        token_cache[rc] = av1_pt_energy_class[t1];
      } else {
        assert(d2 <= d0);
        accu_rate += rate0;
        accu_error += d2 - d0;
        token_cache[rc] = av1_pt_energy_class[t0];
      }
      assert(accu_error >= 0);

      // determine whether to move the eob position to i+1
      const int use_a = (x_a != 0) && (best_eob_x);
      const int64_t best_eob_cost_i = use_a ? eob_cost1 : eob_cost0;
      if (best_eob_cost_i < best_block_rd_cost) {
        best_block_rd_cost = best_eob_cost_i;
        final_eob = i + 1;
        if (use_a) {
          before_best_eob_qc = x_a;
          before_best_eob_dqc = dqc_a;
        } else {
          before_best_eob_qc = x;
          before_best_eob_dqc = dqc;
        }
      }
    }  // if (x==0)
  }    // for (i)

  assert(final_eob <= eob);
  if (final_eob > 0) {
    assert(before_best_eob_qc != 0);
    i = final_eob - 1;
    int rc = scan[i];
    qcoeff[rc] = before_best_eob_qc;
    dqcoeff[rc] = before_best_eob_dqc;
  }

  for (i = final_eob; i < eob; i++) {
    int rc = scan[i];
    qcoeff[rc] = 0;
    dqcoeff[rc] = 0;
  }

  p->eobs[block] = final_eob;
  return final_eob;
}
#endif  // !CONFIG_LV_MAP

int av1_optimize_b(const AV1_COMMON *cm, MACROBLOCK *mb, int plane, int blk_row,
                   int blk_col, int block, BLOCK_SIZE plane_bsize,
                   TX_SIZE tx_size, const ENTROPY_CONTEXT *a,
                   const ENTROPY_CONTEXT *l, int fast_mode) {
  MACROBLOCKD *const xd = &mb->e_mbd;
  struct macroblock_plane *const p = &mb->plane[plane];
  const int eob = p->eobs[block];
  assert((mb->qindex == 0) ^ (xd->lossless[xd->mi[0]->mbmi.segment_id] == 0));
  if (eob == 0 || !mb->optimize || xd->lossless[xd->mi[0]->mbmi.segment_id])
    return eob;

#if !CONFIG_LV_MAP
  (void)plane_bsize;
  (void)blk_row;
  (void)blk_col;
  int ctx = get_entropy_context(tx_size, a, l);
  return optimize_b_greedy(cm, mb, plane, blk_row, blk_col, block, tx_size, ctx,
                           fast_mode);
#else   // !CONFIG_LV_MAP
  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, plane, a, l, &txb_ctx);
  return av1_optimize_txb(cm, mb, plane, blk_row, blk_col, block, tx_size,
                          &txb_ctx, fast_mode);
#endif  // !CONFIG_LV_MAP
}

typedef enum QUANT_FUNC {
  QUANT_FUNC_LOWBD = 0,
  QUANT_FUNC_HIGHBD = 1,
  QUANT_FUNC_TYPES = 2
} QUANT_FUNC;

static AV1_QUANT_FACADE
    quant_func_list[AV1_XFORM_QUANT_TYPES][QUANT_FUNC_TYPES] = {
#if !CONFIG_NEW_QUANT
      { av1_quantize_fp_facade, av1_highbd_quantize_fp_facade },
      { av1_quantize_b_facade, av1_highbd_quantize_b_facade },
      { av1_quantize_dc_facade, av1_highbd_quantize_dc_facade },
#else   // !CONFIG_NEW_QUANT
      { av1_quantize_fp_nuq_facade, av1_highbd_quantize_fp_nuq_facade },
      { av1_quantize_b_nuq_facade, av1_highbd_quantize_b_nuq_facade },
      { av1_quantize_dc_nuq_facade, av1_highbd_quantize_dc_nuq_facade },
#endif  // !CONFIG_NEW_QUANT
      { NULL, NULL }
    };

#if !CONFIG_TXMG
typedef void (*fwdTxfmFunc)(const int16_t *diff, tran_low_t *coeff, int stride,
                            TxfmParam *txfm_param);
static const fwdTxfmFunc fwd_txfm_func[2] = { av1_fwd_txfm,
                                              av1_highbd_fwd_txfm };
#endif

void av1_xform_quant(const AV1_COMMON *cm, MACROBLOCK *x, int plane, int block,
                     int blk_row, int blk_col, BLOCK_SIZE plane_bsize,
                     TX_SIZE tx_size, int ctx,
                     AV1_XFORM_QUANT xform_quant_idx) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
#if !CONFIG_DIST_8X8
  const struct macroblock_plane *const p = &x->plane[plane];
  const struct macroblockd_plane *const pd = &xd->plane[plane];
#else
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
#endif
  PLANE_TYPE plane_type = get_plane_type(plane);
  TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);

#if CONFIG_NEW_QUANT
  const int is_inter = is_inter_block(mbmi);
#endif
  const SCAN_ORDER *const scan_order = get_scan(cm, tx_size, tx_type, mbmi);
  tran_low_t *const coeff = BLOCK_OFFSET(p->coeff, block);
  tran_low_t *const qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  tran_low_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  uint16_t *const eob = &p->eobs[block];
  const int diff_stride = block_size_wide[plane_bsize];
#if CONFIG_AOM_QM
  int seg_id = mbmi->segment_id;
  // Use a flat matrix (i.e. no weighting) for 1D and Identity transforms
  const TX_SIZE qm_tx_size =
#if CONFIG_TX64X64
      tx_size == TX_64X64 || tx_size == TX_64X32 || tx_size == TX_32X64
          ? TX_32X32
          :
#endif  // CONFIG_TX64X64
          tx_size;
  const qm_val_t *qmatrix =
      IS_2D_TRANSFORM(tx_type) ? pd->seg_qmatrix[seg_id][qm_tx_size]
                               : cm->gqmatrix[NUM_QM_LEVELS - 1][0][qm_tx_size];
  const qm_val_t *iqmatrix =
      IS_2D_TRANSFORM(tx_type)
          ? pd->seg_iqmatrix[seg_id][qm_tx_size]
          : cm->giqmatrix[NUM_QM_LEVELS - 1][0][qm_tx_size];
#endif

  TxfmParam txfm_param;

#if CONFIG_DIST_8X8 || CONFIG_MRC_TX
  uint8_t *dst;
  const int dst_stride = pd->dst.stride;
#if CONFIG_DIST_8X8
  int16_t *pred;
  const int txw = tx_size_wide[tx_size];
  const int txh = tx_size_high[tx_size];
  int i, j;
#endif
#endif

  QUANT_PARAM qparam;
  const int16_t *src_diff;

  src_diff =
      &p->src_diff[(blk_row * diff_stride + blk_col) << tx_size_wide_log2[0]];
#if CONFIG_DAALA_TX
  qparam.log_scale = 0;
#else
  qparam.log_scale = av1_get_tx_scale(tx_size);
#endif
#if CONFIG_NEW_QUANT
  qparam.tx_size = tx_size;
  qparam.dq = get_dq_profile_from_ctx(x->qindex, ctx, is_inter, plane_type);
#endif  // CONFIG_NEW_QUANT
#if CONFIG_AOM_QM
  qparam.qmatrix = qmatrix;
  qparam.iqmatrix = iqmatrix;
#endif  // CONFIG_AOM_QM

#if CONFIG_DIST_8X8 || CONFIG_MRC_TX
  dst = &pd->dst.buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];
#endif  // CONFIG_DIST_8X8 ||
        // CONFIG_MRC_TX

#if CONFIG_DIST_8X8
  if (x->using_dist_8x8) {
    pred = &pd->pred[(blk_row * diff_stride + blk_col) << tx_size_wide_log2[0]];

// copy uint8 orig and predicted block to int16 buffer
// in order to use existing VP10 transform functions
#if CONFIG_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      for (j = 0; j < txh; j++)
        for (i = 0; i < txw; i++)
          pred[diff_stride * j + i] =
              CONVERT_TO_SHORTPTR(dst)[dst_stride * j + i];
    } else {
#endif  // CONFIG_HIGHBITDEPTH
      for (j = 0; j < txh; j++)
        for (i = 0; i < txw; i++)
          pred[diff_stride * j + i] = dst[dst_stride * j + i];
#if CONFIG_HIGHBITDEPTH
    }
#endif  // CONFIG_HIGHBITDEPTH
  }
#endif  // CONFIG_DIST_8X8

  (void)ctx;

  txfm_param.tx_type = tx_type;
  txfm_param.tx_size = tx_size;
  txfm_param.lossless = xd->lossless[mbmi->segment_id];
  txfm_param.tx_set_type =
      get_ext_tx_set_type(txfm_param.tx_size, plane_bsize, is_inter_block(mbmi),
                          cm->reduced_tx_set_used);
#if CONFIG_MRC_TX
  txfm_param.is_inter = is_inter_block(mbmi);
#endif
#if CONFIG_MRC_TX
  txfm_param.dst = dst;
  txfm_param.stride = dst_stride;
#if CONFIG_MRC_TX
  txfm_param.valid_mask = &mbmi->valid_mrc_mask;
#if SIGNAL_ANY_MRC_MASK
  txfm_param.mask = BLOCK_OFFSET(xd->mrc_mask, block);
#endif  // SIGNAL_ANY_MRC_MASK
#endif  // CONFIG_MRC_TX
#endif  // CONFIG_MRC_TX

  txfm_param.bd = xd->bd;
  txfm_param.is_hbd = get_bitdepth_data_path_index(xd);

#if CONFIG_TXMG
  av1_highbd_fwd_txfm(src_diff, coeff, diff_stride, &txfm_param);
#else   // CONFIG_TXMG
  fwd_txfm_func[txfm_param.is_hbd](src_diff, coeff, diff_stride, &txfm_param);
#endif  // CONFIG_TXMG

  if (xform_quant_idx != AV1_XFORM_QUANT_SKIP_QUANT) {
    const int n_coeffs = av1_get_max_eob(tx_size);
    if (LIKELY(!x->skip_block)) {
#if CONFIG_DAALA_TX
      quant_func_list[xform_quant_idx][1](coeff, n_coeffs, p, qcoeff, dqcoeff,
                                          eob, scan_order, &qparam);
#else
      quant_func_list[xform_quant_idx][txfm_param.is_hbd](
          coeff, n_coeffs, p, qcoeff, dqcoeff, eob, scan_order, &qparam);
#endif
    } else {
      av1_quantize_skip(n_coeffs, qcoeff, dqcoeff, eob);
    }
  }
#if CONFIG_LV_MAP
  p->txb_entropy_ctx[block] =
      (uint8_t)av1_get_txb_entropy_context(qcoeff, scan_order, *eob);
#endif  // CONFIG_LV_MAP
  return;
}

static void encode_block(int plane, int block, int blk_row, int blk_col,
                         BLOCK_SIZE plane_bsize, TX_SIZE tx_size, void *arg) {
  struct encode_b_args *const args = arg;
  AV1_COMMON *cm = args->cm;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  int ctx;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  tran_low_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
  uint8_t *mrc_mask = BLOCK_OFFSET(xd->mrc_mask, block);
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
  uint8_t *dst;
  ENTROPY_CONTEXT *a, *l;

  int bw = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
  dst = &pd->dst
             .buf[(blk_row * pd->dst.stride + blk_col) << tx_size_wide_log2[0]];

  a = &args->ta[blk_col];
  l = &args->tl[blk_row];
  ctx = get_entropy_context(tx_size, a, l);

  // Assert not magic number (uninitialized).
  assert(x->blk_skip[plane][blk_row * bw + blk_col] != 234);

  if (x->blk_skip[plane][blk_row * bw + blk_col] == 0) {
    av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                    ctx, AV1_XFORM_QUANT_FP);
  } else {
    p->eobs[block] = 0;
  }

  av1_optimize_b(cm, x, plane, blk_row, blk_col, block, plane_bsize, tx_size, a,
                 l, CONFIG_LV_MAP);

  av1_set_txb_context(x, plane, block, tx_size, a, l);

  if (p->eobs[block]) *(args->skip) = 0;

  if (p->eobs[block] != 0)

  {
    TX_TYPE tx_type =
        av1_get_tx_type(pd->plane_type, xd, blk_row, blk_col, block, tx_size);
    av1_inverse_transform_block(xd, dqcoeff,
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                                mrc_mask,
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                                plane, tx_type, tx_size, dst, pd->dst.stride,
                                p->eobs[block], cm->reduced_tx_set_used);
  }
}

static void encode_block_inter(int plane, int block, int blk_row, int blk_col,
                               BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                               void *arg) {
  struct encode_b_args *const args = arg;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const BLOCK_SIZE bsize = txsize_to_bsize[tx_size];
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const int tx_row = blk_row >> (1 - pd->subsampling_y);
  const int tx_col = blk_col >> (1 - pd->subsampling_x);
  TX_SIZE plane_tx_size;
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  plane_tx_size =
      plane ? uv_txsize_lookup[bsize][mbmi->inter_tx_size[tx_row][tx_col]][0][0]
            : mbmi->inter_tx_size[tx_row][tx_col];

  if (tx_size == plane_tx_size) {
    encode_block(plane, block, blk_row, blk_col, plane_bsize, tx_size, arg);
  } else {
    assert(tx_size < TX_SIZES_ALL);
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    assert(IMPLIES(tx_size <= TX_4X4, sub_txs == tx_size));
    assert(IMPLIES(tx_size > TX_4X4, sub_txs < tx_size));
    // This is the square transform block partition entry point.
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];
    const int step = bsh * bsw;
    assert(bsw > 0 && bsh > 0);

    for (int row = 0; row < tx_size_high_unit[tx_size]; row += bsh) {
      for (int col = 0; col < tx_size_wide_unit[tx_size]; col += bsw) {
        const int offsetr = blk_row + row;
        const int offsetc = blk_col + col;

        if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;

        encode_block_inter(plane, block, offsetr, offsetc, plane_bsize, sub_txs,
                           arg);
        block += step;
      }
    }
  }
}

typedef struct encode_block_pass1_args {
  AV1_COMMON *cm;
  MACROBLOCK *x;
} encode_block_pass1_args;

static void encode_block_pass1(int plane, int block, int blk_row, int blk_col,
                               BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                               void *arg) {
  encode_block_pass1_args *args = (encode_block_pass1_args *)arg;
  AV1_COMMON *cm = args->cm;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  tran_low_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  TxfmParam txfm_param;
  uint8_t *dst;
  int ctx = 0;
  dst = &pd->dst
             .buf[(blk_row * pd->dst.stride + blk_col) << tx_size_wide_log2[0]];

  av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                  ctx, AV1_XFORM_QUANT_B);

  if (p->eobs[block] > 0) {
    txfm_param.bd = xd->bd;
    txfm_param.is_hbd = get_bitdepth_data_path_index(xd);
    txfm_param.tx_type = DCT_DCT;
    txfm_param.tx_size = tx_size;
    txfm_param.eob = p->eobs[block];
    txfm_param.lossless = xd->lossless[xd->mi[0]->mbmi.segment_id];
    txfm_param.tx_set_type = get_ext_tx_set_type(
        txfm_param.tx_size, plane_bsize, is_inter_block(&xd->mi[0]->mbmi),
        cm->reduced_tx_set_used);
#if CONFIG_DAALA_TX
    daala_inv_txfm_add(dqcoeff, dst, pd->dst.stride, &txfm_param);
#else
#if CONFIG_HIGHBITDEPTH
    if (txfm_param.is_hbd) {
      av1_highbd_inv_txfm_add_4x4(dqcoeff, dst, pd->dst.stride, &txfm_param);
      return;
    }
#endif  //  CONFIG_HIGHBITDEPTH
    if (xd->lossless[xd->mi[0]->mbmi.segment_id]) {
      av1_iwht4x4_add(dqcoeff, dst, pd->dst.stride, &txfm_param);
    } else {
      av1_idct4x4_add(dqcoeff, dst, pd->dst.stride, &txfm_param);
    }
#endif
  }
}

void av1_encode_sby_pass1(AV1_COMMON *cm, MACROBLOCK *x, BLOCK_SIZE bsize) {
  encode_block_pass1_args args = { cm, x };
  av1_subtract_plane(x, bsize, 0);
  av1_foreach_transformed_block_in_plane(&x->e_mbd, bsize, 0,
                                         encode_block_pass1, &args);
}

void av1_encode_sb(AV1_COMMON *cm, MACROBLOCK *x, BLOCK_SIZE bsize, int mi_row,
                   int mi_col) {
  MACROBLOCKD *const xd = &x->e_mbd;
  struct optimize_ctx ctx;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  struct encode_b_args arg = { cm, x, &ctx, &mbmi->skip, NULL, NULL, 1 };
  int plane;

  mbmi->skip = 1;

  if (x->skip) return;

  for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
    const int subsampling_x = xd->plane[plane].subsampling_x;
    const int subsampling_y = xd->plane[plane].subsampling_y;

    if (!is_chroma_reference(mi_row, mi_col, bsize, subsampling_x,
                             subsampling_y))
      continue;

    bsize = scale_chroma_bsize(bsize, subsampling_x, subsampling_y);

    // TODO(jingning): Clean this up.
    const struct macroblockd_plane *const pd = &xd->plane[plane];
    const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
    const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
    const int mi_height = block_size_high[plane_bsize] >> tx_size_wide_log2[0];
    const TX_SIZE max_tx_size = get_vartx_max_txsize(
        mbmi, plane_bsize, pd->subsampling_x || pd->subsampling_y);
    const BLOCK_SIZE txb_size = txsize_to_bsize[max_tx_size];
    const int bw = block_size_wide[txb_size] >> tx_size_wide_log2[0];
    const int bh = block_size_high[txb_size] >> tx_size_wide_log2[0];
    int idx, idy;
    int block = 0;
    int step = tx_size_wide_unit[max_tx_size] * tx_size_high_unit[max_tx_size];
    av1_get_entropy_contexts(bsize, 0, pd, ctx.ta[plane], ctx.tl[plane]);

    av1_subtract_plane(x, bsize, plane);

    arg.ta = ctx.ta[plane];
    arg.tl = ctx.tl[plane];

    const BLOCK_SIZE max_unit_bsize = get_plane_block_size(BLOCK_64X64, pd);
    int mu_blocks_wide =
        block_size_wide[max_unit_bsize] >> tx_size_wide_log2[0];
    int mu_blocks_high =
        block_size_high[max_unit_bsize] >> tx_size_high_log2[0];

    mu_blocks_wide = AOMMIN(mi_width, mu_blocks_wide);
    mu_blocks_high = AOMMIN(mi_height, mu_blocks_high);

    for (idy = 0; idy < mi_height; idy += mu_blocks_high) {
      for (idx = 0; idx < mi_width; idx += mu_blocks_wide) {
        int blk_row, blk_col;
        const int unit_height = AOMMIN(mu_blocks_high + idy, mi_height);
        const int unit_width = AOMMIN(mu_blocks_wide + idx, mi_width);
        for (blk_row = idy; blk_row < unit_height; blk_row += bh) {
          for (blk_col = idx; blk_col < unit_width; blk_col += bw) {
            encode_block_inter(plane, block, blk_row, blk_col, plane_bsize,
                               max_tx_size, &arg);
            block += step;
          }
        }
      }
    }
  }
}

void av1_set_txb_context(MACROBLOCK *x, int plane, int block, TX_SIZE tx_size,
                         ENTROPY_CONTEXT *a, ENTROPY_CONTEXT *l) {
  (void)tx_size;
  struct macroblock_plane *p = &x->plane[plane];

#if !CONFIG_LV_MAP
  *a = *l = p->eobs[block] > 0;
#else   // !CONFIG_LV_MAP
  *a = *l = p->txb_entropy_ctx[block];
#endif  // !CONFIG_LV_MAP

  int i;
  for (i = 0; i < tx_size_wide_unit[tx_size]; ++i) a[i] = a[0];

  for (i = 0; i < tx_size_high_unit[tx_size]; ++i) l[i] = l[0];
}

static void encode_block_intra_and_set_context(int plane, int block,
                                               int blk_row, int blk_col,
                                               BLOCK_SIZE plane_bsize,
                                               TX_SIZE tx_size, void *arg) {
  av1_encode_block_intra(plane, block, blk_row, blk_col, plane_bsize, tx_size,
                         arg);

  struct encode_b_args *const args = arg;
  MACROBLOCK *x = args->x;
  ENTROPY_CONTEXT *a = &args->ta[blk_col];
  ENTROPY_CONTEXT *l = &args->tl[blk_row];
  av1_set_txb_context(x, plane, block, tx_size, a, l);
}

void av1_encode_block_intra(int plane, int block, int blk_row, int blk_col,
                            BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                            void *arg) {
  struct encode_b_args *const args = arg;
  AV1_COMMON *cm = args->cm;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  tran_low_t *dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
  uint8_t *mrc_mask = BLOCK_OFFSET(xd->mrc_mask, block);
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
  PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  uint16_t *eob = &p->eobs[block];
  const int dst_stride = pd->dst.stride;
  uint8_t *dst =
      &pd->dst.buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];

  av1_predict_intra_block_facade(cm, xd, plane, block, blk_col, blk_row,
                                 tx_size);

  av1_subtract_txb(x, plane, plane_bsize, blk_col, blk_row, tx_size);

  const ENTROPY_CONTEXT *a = &args->ta[blk_col];
  const ENTROPY_CONTEXT *l = &args->tl[blk_row];
  int ctx = combine_entropy_contexts(*a, *l);
  if (args->enable_optimize_b) {
    av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                    ctx, AV1_XFORM_QUANT_FP);
    av1_optimize_b(cm, x, plane, blk_row, blk_col, block, plane_bsize, tx_size,
                   a, l, CONFIG_LV_MAP);
  } else {
    av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                    ctx, AV1_XFORM_QUANT_B);
  }

  av1_inverse_transform_block(xd, dqcoeff,
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                              mrc_mask,
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                              plane, tx_type, tx_size, dst, dst_stride, *eob,
                              cm->reduced_tx_set_used);

  if (*eob) *(args->skip) = 0;

#if CONFIG_CFL
  if (plane == AOM_PLANE_Y && xd->cfl->store_y) {
    cfl_store_tx(xd, blk_row, blk_col, tx_size, plane_bsize);
  }
#endif  // CONFIG_CFL
}

void av1_encode_intra_block_plane(AV1_COMMON *cm, MACROBLOCK *x,
                                  BLOCK_SIZE bsize, int plane,
                                  int enable_optimize_b, int mi_row,
                                  int mi_col) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  ENTROPY_CONTEXT ta[2 * MAX_MIB_SIZE] = { 0 };
  ENTROPY_CONTEXT tl[2 * MAX_MIB_SIZE] = { 0 };

  struct encode_b_args arg = {
    cm, x, NULL, &xd->mi[0]->mbmi.skip, ta, tl, enable_optimize_b
  };

  if (!is_chroma_reference(mi_row, mi_col, bsize,
                           xd->plane[plane].subsampling_x,
                           xd->plane[plane].subsampling_y))
    return;

  if (enable_optimize_b) {
    const struct macroblockd_plane *const pd = &xd->plane[plane];
    const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
    av1_get_entropy_contexts(bsize, tx_size, pd, ta, tl);
  }
  av1_foreach_transformed_block_in_plane(
      xd, bsize, plane, encode_block_intra_and_set_context, &arg);
}
