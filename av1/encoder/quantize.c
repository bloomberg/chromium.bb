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

#include <math.h>
#include "./aom_dsp_rtcd.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"

#include "av1/common/quant_common.h"
#include "av1/common/seg_common.h"

#include "av1/encoder/encoder.h"
#include "av1/encoder/quantize.h"
#include "av1/encoder/rd.h"

void av1_quantize_fp_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                        int skip_block, const int16_t *zbin_ptr,
                        const int16_t *round_ptr, const int16_t *quant_ptr,
                        const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
                        tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr,
                        uint16_t *eob_ptr, const int16_t *scan,
                        const int16_t *iscan
#if CONFIG_AOM_QM
                        ,
                        const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr
#endif
                        ) {
  int i, eob = -1;
  // TODO(jingning) Decide the need of these arguments after the
  // quantization process is completed.
  (void)zbin_ptr;
  (void)quant_shift_ptr;
  (void)iscan;

  memset(qcoeff_ptr, 0, n_coeffs * sizeof(*qcoeff_ptr));
  memset(dqcoeff_ptr, 0, n_coeffs * sizeof(*dqcoeff_ptr));

  if (!skip_block) {
    // Quantization pass: All coefficients with index >= zero_flag are
    // skippable. Note: zero_flag can be zero.
    for (i = 0; i < n_coeffs; i++) {
      const int rc = scan[i];
      const int coeff = coeff_ptr[rc];
#if CONFIG_AOM_QM
      const qm_val_t wt = qm_ptr[rc];
      const qm_val_t iwt = iqm_ptr[rc];
      const int dequant =
          (dequant_ptr[rc != 0] * iwt + (1 << (AOM_QM_BITS - 1))) >>
          AOM_QM_BITS;
#endif
      const int coeff_sign = (coeff >> 31);
      const int abs_coeff = (coeff ^ coeff_sign) - coeff_sign;

      int64_t tmp = clamp(abs_coeff + round_ptr[rc != 0], INT16_MIN, INT16_MAX);
      int tmp32;
#if CONFIG_AOM_QM
      tmp32 = (int)((tmp * wt * quant_ptr[rc != 0]) >> (16 + AOM_QM_BITS));
      qcoeff_ptr[rc] = (tmp32 ^ coeff_sign) - coeff_sign;
      dqcoeff_ptr[rc] = qcoeff_ptr[rc] * dequant;
#else
      tmp32 = (int)((tmp * quant_ptr[rc != 0]) >> 16);
      qcoeff_ptr[rc] = (tmp32 ^ coeff_sign) - coeff_sign;
      dqcoeff_ptr[rc] = qcoeff_ptr[rc] * dequant_ptr[rc != 0];
#endif

      if (tmp32) eob = i;
    }
  }
  *eob_ptr = eob + 1;
}

#if CONFIG_AOM_HIGHBITDEPTH
void av1_highbd_quantize_fp_c(const tran_low_t *coeff_ptr, intptr_t count,
                               int skip_block, const int16_t *zbin_ptr,
                               const int16_t *round_ptr,
                               const int16_t *quant_ptr,
                               const int16_t *quant_shift_ptr,
                               tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                               const int16_t *dequant_ptr, uint16_t *eob_ptr,
                               const int16_t *scan, const int16_t *iscan
#if CONFIG_AOM_QM
                               ,
                               const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr
#endif
                               ) {
  int i;
  int eob = -1;
  // TODO(jingning) Decide the need of these arguments after the
  // quantization process is completed.
  (void)zbin_ptr;
  (void)quant_shift_ptr;
  (void)iscan;

  memset(qcoeff_ptr, 0, count * sizeof(*qcoeff_ptr));
  memset(dqcoeff_ptr, 0, count * sizeof(*dqcoeff_ptr));

  if (!skip_block) {
    // Quantization pass: All coefficients with index >= zero_flag are
    // skippable. Note: zero_flag can be zero.
    for (i = 0; i < count; i++) {
      const int rc = scan[i];
      const int coeff = coeff_ptr[rc];
#if CONFIG_AOM_QM
      const qm_val_t wt = qm_ptr[rc];
      const qm_val_t iwt = iqm_ptr[rc];
      const int dequant =
          (dequant_ptr[rc != 0] * iwt + (1 << (AOM_QM_BITS - 1))) >>
          AOM_QM_BITS;
#endif
      const int coeff_sign = (coeff >> 31);
      const int abs_coeff = (coeff ^ coeff_sign) - coeff_sign;
      const int64_t tmp = abs_coeff + round_ptr[rc != 0];
#if CONFIG_AOM_QM
      const uint32_t abs_qcoeff =
          (uint32_t)((tmp * quant_ptr[rc != 0] * wt) >> (16 + AOM_QM_BITS));
      qcoeff_ptr[rc] = (tran_low_t)((abs_qcoeff ^ coeff_sign) - coeff_sign);
      dqcoeff_ptr[rc] = qcoeff_ptr[rc] * dequant;
#else
      const uint32_t abs_qcoeff = (uint32_t)((tmp * quant_ptr[rc != 0]) >> 16);
      qcoeff_ptr[rc] = (tran_low_t)((abs_qcoeff ^ coeff_sign) - coeff_sign);
      dqcoeff_ptr[rc] = qcoeff_ptr[rc] * dequant_ptr[rc != 0];
#endif
      if (abs_qcoeff) eob = i;
    }
  }
  *eob_ptr = eob + 1;
}
#endif

// TODO(jingning) Refactor this file and combine functions with similar
// operations.
void av1_quantize_fp_32x32_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                              int skip_block, const int16_t *zbin_ptr,
                              const int16_t *round_ptr,
                              const int16_t *quant_ptr,
                              const int16_t *quant_shift_ptr,
                              tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                              const int16_t *dequant_ptr, uint16_t *eob_ptr,
                              const int16_t *scan, const int16_t *iscan
#if CONFIG_AOM_QM
                              ,
                              const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr
#endif
                              ) {
  int i, eob = -1;
  (void)zbin_ptr;
  (void)quant_shift_ptr;
  (void)iscan;

  memset(qcoeff_ptr, 0, n_coeffs * sizeof(*qcoeff_ptr));
  memset(dqcoeff_ptr, 0, n_coeffs * sizeof(*dqcoeff_ptr));

  if (!skip_block) {
    for (i = 0; i < n_coeffs; i++) {
      const int rc = scan[i];
      const int coeff = coeff_ptr[rc];
#if CONFIG_AOM_QM
      const qm_val_t wt = qm_ptr[rc];
      const qm_val_t iwt = iqm_ptr[rc];
      const int dequant =
          (dequant_ptr[rc != 0] * iwt + (1 << (AOM_QM_BITS - 1))) >>
          AOM_QM_BITS;
      int64_t tmp = 0;
#endif
      const int coeff_sign = (coeff >> 31);
      int tmp32 = 0;
      int abs_coeff = (coeff ^ coeff_sign) - coeff_sign;

#if CONFIG_AOM_QM
      if (abs_coeff * wt >= (dequant_ptr[rc != 0] << (AOM_QM_BITS - 2))) {
#else
      if (abs_coeff >= (dequant_ptr[rc != 0] >> 2)) {
#endif
        abs_coeff += ROUND_POWER_OF_TWO(round_ptr[rc != 0], 1);
        abs_coeff = clamp(abs_coeff, INT16_MIN, INT16_MAX);
#if CONFIG_AOM_QM
        tmp = abs_coeff * wt;
        tmp32 = (int)(tmp * quant_ptr[rc != 0]) >> (AOM_QM_BITS + 15);
        qcoeff_ptr[rc] = (tmp32 ^ coeff_sign) - coeff_sign;
        dqcoeff_ptr[rc] = (qcoeff_ptr[rc] * dequant) / 2;
#else
        tmp32 = (abs_coeff * quant_ptr[rc != 0]) >> 15;
        qcoeff_ptr[rc] = (tmp32 ^ coeff_sign) - coeff_sign;
        dqcoeff_ptr[rc] = (qcoeff_ptr[rc] * dequant_ptr[rc != 0]) / 2;
#endif
      }

      if (tmp32) eob = i;
    }
  }
  *eob_ptr = eob + 1;
}

#if CONFIG_AOM_HIGHBITDEPTH
void av1_highbd_quantize_fp_32x32_c(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block,
    const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan
#if CONFIG_AOM_QM
    ,
    const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr
#endif
    ) {
  int i, eob = -1;
  (void)zbin_ptr;
  (void)quant_shift_ptr;
  (void)iscan;

  memset(qcoeff_ptr, 0, n_coeffs * sizeof(*qcoeff_ptr));
  memset(dqcoeff_ptr, 0, n_coeffs * sizeof(*dqcoeff_ptr));

  if (!skip_block) {
    for (i = 0; i < n_coeffs; i++) {
      uint32_t abs_qcoeff = 0;
      const int rc = scan[i];
      const int coeff = coeff_ptr[rc];
#if CONFIG_AOM_QM
      const qm_val_t wt = qm_ptr[rc];
      const qm_val_t iwt = iqm_ptr[rc];
      const int dequant =
          (dequant_ptr[rc != 0] * iwt + (1 << (AOM_QM_BITS - 1))) >>
          AOM_QM_BITS;
#endif
      const int coeff_sign = (coeff >> 31);
      const int abs_coeff = (coeff ^ coeff_sign) - coeff_sign;
#if CONFIG_AOM_QM
      if (abs_coeff * wt >= (dequant_ptr[rc != 0] << (AOM_QM_BITS - 2))) {
#else
      if (abs_coeff >= (dequant_ptr[rc != 0] >> 2)) {
#endif
        const int64_t tmp =
            abs_coeff + ROUND_POWER_OF_TWO(round_ptr[rc != 0], 1);
#if CONFIG_AOM_QM
        abs_qcoeff =
            (uint32_t)((tmp * wt * quant_ptr[rc != 0]) >> (AOM_QM_BITS + 15));
        qcoeff_ptr[rc] = (tran_low_t)((abs_qcoeff ^ coeff_sign) - coeff_sign);
        dqcoeff_ptr[rc] = (qcoeff_ptr[rc] * dequant) / 2;
#else
        abs_qcoeff = (uint32_t)((tmp * quant_ptr[rc != 0]) >> 15);
        qcoeff_ptr[rc] = (tran_low_t)((abs_qcoeff ^ coeff_sign) - coeff_sign);
        dqcoeff_ptr[rc] = (qcoeff_ptr[rc] * dequant_ptr[rc != 0]) / 2;
#endif
      }

      if (abs_qcoeff) eob = i;
    }
  }
  *eob_ptr = eob + 1;
}
#endif

void av1_regular_quantize_b_4x4(MACROBLOCK *x, int plane, int block,
                                 const int16_t *scan, const int16_t *iscan) {
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
#if CONFIG_AOM_QM
  int seg_id = xd->mi[0]->mbmi.segment_id;
  int is_intra = is_inter_block(&xd->mi[0]->mbmi);
  const qm_val_t *qmatrix = pd->seg_qmatrix[seg_id][is_intra][0];
  const qm_val_t *iqmatrix = pd->seg_iqmatrix[seg_id][is_intra][0];
#endif

#if CONFIG_AOM_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    aom_highbd_quantize_b(BLOCK_OFFSET(p->coeff, block), 16, x->skip_block,
                          p->zbin, p->round, p->quant, p->quant_shift,
                          BLOCK_OFFSET(p->qcoeff, block),
                          BLOCK_OFFSET(pd->dqcoeff, block), pd->dequant,
#if !CONFIG_AOM_QM
                          &p->eobs[block], scan, iscan);
#else
                          &p->eobs[block], scan, iscan,
                          qmatrix, iqmatrix);
#endif
    return;
  }
#endif
  aom_quantize_b(BLOCK_OFFSET(p->coeff, block), 16, x->skip_block, p->zbin,
                 p->round, p->quant, p->quant_shift,
                 BLOCK_OFFSET(p->qcoeff, block),
                 BLOCK_OFFSET(pd->dqcoeff, block), pd->dequant, &p->eobs[block],
#if !CONFIG_AOM_QM
                          scan, iscan);
#else
                          scan, iscan,
                          qmatrix, iqmatrix);
#endif
}

static void invert_quant(int16_t *quant, int16_t *shift, int d) {
  unsigned t;
  int l;
  t = d;
  for (l = 0; t > 1; l++) t >>= 1;
  t = 1 + (1 << (16 + l)) / d;
  *quant = (int16_t)(t - (1 << 16));
  *shift = 1 << (16 - l);
}

static int get_qzbin_factor(int q, aom_bit_depth_t bit_depth) {
  const int quant = av1_dc_quant(q, 0, bit_depth);
#if CONFIG_AOM_HIGHBITDEPTH
  switch (bit_depth) {
    case AOM_BITS_8:
      return q == 0 ? 64 : (quant < 148 ? 84 : 80);
    case AOM_BITS_10:
      return q == 0 ? 64 : (quant < 592 ? 84 : 80);
    case AOM_BITS_12:
      return q == 0 ? 64 : (quant < 2368 ? 84 : 80);
    default:
      assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
      return -1;
  }
#else
  (void)bit_depth;
  return q == 0 ? 64 : (quant < 148 ? 84 : 80);
#endif
}

void av1_init_quantizer(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  QUANTS *const quants = &cpi->quants;
  int i, q, quant;

  for (q = 0; q < QINDEX_RANGE; q++) {
    const int qzbin_factor = get_qzbin_factor(q, cm->bit_depth);
    const int qrounding_factor = q == 0 ? 64 : 48;

    for (i = 0; i < 2; ++i) {
      int qrounding_factor_fp = i == 0 ? 48 : 42;
      if (q == 0) qrounding_factor_fp = 64;

      // y
      quant = i == 0 ? av1_dc_quant(q, cm->y_dc_delta_q, cm->bit_depth)
                     : av1_ac_quant(q, 0, cm->bit_depth);
      invert_quant(&quants->y_quant[q][i], &quants->y_quant_shift[q][i], quant);
      quants->y_quant_fp[q][i] = (1 << 16) / quant;
      quants->y_round_fp[q][i] = (qrounding_factor_fp * quant) >> 7;
      quants->y_zbin[q][i] = ROUND_POWER_OF_TWO(qzbin_factor * quant, 7);
      quants->y_round[q][i] = (qrounding_factor * quant) >> 7;
      cpi->y_dequant[q][i] = quant;

      // uv
      quant = i == 0 ? av1_dc_quant(q, cm->uv_dc_delta_q, cm->bit_depth)
                     : av1_ac_quant(q, cm->uv_ac_delta_q, cm->bit_depth);
      invert_quant(&quants->uv_quant[q][i], &quants->uv_quant_shift[q][i],
                   quant);
      quants->uv_quant_fp[q][i] = (1 << 16) / quant;
      quants->uv_round_fp[q][i] = (qrounding_factor_fp * quant) >> 7;
      quants->uv_zbin[q][i] = ROUND_POWER_OF_TWO(qzbin_factor * quant, 7);
      quants->uv_round[q][i] = (qrounding_factor * quant) >> 7;
      cpi->uv_dequant[q][i] = quant;
    }

    for (i = 2; i < 8; i++) {
      quants->y_quant[q][i] = quants->y_quant[q][1];
      quants->y_quant_fp[q][i] = quants->y_quant_fp[q][1];
      quants->y_round_fp[q][i] = quants->y_round_fp[q][1];
      quants->y_quant_shift[q][i] = quants->y_quant_shift[q][1];
      quants->y_zbin[q][i] = quants->y_zbin[q][1];
      quants->y_round[q][i] = quants->y_round[q][1];
      cpi->y_dequant[q][i] = cpi->y_dequant[q][1];

      quants->uv_quant[q][i] = quants->uv_quant[q][1];
      quants->uv_quant_fp[q][i] = quants->uv_quant_fp[q][1];
      quants->uv_round_fp[q][i] = quants->uv_round_fp[q][1];
      quants->uv_quant_shift[q][i] = quants->uv_quant_shift[q][1];
      quants->uv_zbin[q][i] = quants->uv_zbin[q][1];
      quants->uv_round[q][i] = quants->uv_round[q][1];
      cpi->uv_dequant[q][i] = cpi->uv_dequant[q][1];
    }
  }
}

void av1_init_plane_quantizers(AV1_COMP *cpi, MACROBLOCK *x) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  QUANTS *const quants = &cpi->quants;
  const int segment_id = xd->mi[0]->mbmi.segment_id;
  const int qindex = av1_get_qindex(&cm->seg, segment_id, cm->base_qindex);
  const int rdmult = av1_compute_rd_mult(cpi, qindex + cm->y_dc_delta_q);
  int i;
#if CONFIG_AOM_QM
  int minqm = cm->min_qmlevel;
  int maxqm = cm->max_qmlevel;
  // Quant matrix only depends on the base QP so there is only one set per frame
  int qmlevel = (xd->lossless[segment_id] || cm->using_qmatrix == 0)
                    ? NUM_QM_LEVELS - 1
                    : aom_get_qmlevel(cm->base_qindex, minqm, maxqm);
#endif

  // Y
  x->plane[0].quant = quants->y_quant[qindex];
  x->plane[0].quant_fp = quants->y_quant_fp[qindex];
  x->plane[0].round_fp = quants->y_round_fp[qindex];
  x->plane[0].quant_shift = quants->y_quant_shift[qindex];
  x->plane[0].zbin = quants->y_zbin[qindex];
  x->plane[0].round = quants->y_round[qindex];
#if CONFIG_AOM_QM
  memcpy(&xd->plane[0].seg_qmatrix[segment_id], cm->gqmatrix[qmlevel][0],
         sizeof(cm->gqmatrix[qmlevel][0]));
  memcpy(&xd->plane[0].seg_iqmatrix[segment_id], cm->giqmatrix[qmlevel][0],
         sizeof(cm->giqmatrix[qmlevel][0]));
#endif
  xd->plane[0].dequant = cpi->y_dequant[qindex];

  x->plane[0].quant_thred[0] = x->plane[0].zbin[0] * x->plane[0].zbin[0];
  x->plane[0].quant_thred[1] = x->plane[0].zbin[1] * x->plane[0].zbin[1];

  // UV
  for (i = 1; i < 3; i++) {
    x->plane[i].quant = quants->uv_quant[qindex];
    x->plane[i].quant_fp = quants->uv_quant_fp[qindex];
    x->plane[i].round_fp = quants->uv_round_fp[qindex];
    x->plane[i].quant_shift = quants->uv_quant_shift[qindex];
    x->plane[i].zbin = quants->uv_zbin[qindex];
    x->plane[i].round = quants->uv_round[qindex];
#if CONFIG_AOM_QM
    memcpy(&xd->plane[i].seg_qmatrix[segment_id], cm->gqmatrix[qmlevel][1],
           sizeof(cm->gqmatrix[qmlevel][1]));
    memcpy(&xd->plane[i].seg_iqmatrix[segment_id], cm->giqmatrix[qmlevel][1],
           sizeof(cm->giqmatrix[qmlevel][1]));
#endif
    xd->plane[i].dequant = cpi->uv_dequant[qindex];

    x->plane[i].quant_thred[0] = x->plane[i].zbin[0] * x->plane[i].zbin[0];
    x->plane[i].quant_thred[1] = x->plane[i].zbin[1] * x->plane[i].zbin[1];
  }

  x->skip_block = segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP);
  x->q_index = qindex;

  set_error_per_bit(x, rdmult);

  av1_initialize_me_consts(cpi, x, x->q_index);
}

void av1_frame_init_quantizer(AV1_COMP *cpi) {
  av1_init_plane_quantizers(cpi, &cpi->td.mb);
}

void av1_set_quantizer(AV1_COMMON *cm, int q) {
  // quantizer has to be reinitialized with av1_init_quantizer() if any
  // delta_q changes.
  cm->base_qindex = q;
  cm->y_dc_delta_q = 0;
  cm->uv_dc_delta_q = 0;
  cm->uv_ac_delta_q = 0;
}

// Table that converts 0-63 Q-range values passed in outside to the Qindex
// range used internally.
static const int quantizer_to_qindex[] = {
  0,   4,   8,   12,  16,  20,  24,  28,  32,  36,  40,  44,  48,
  52,  56,  60,  64,  68,  72,  76,  80,  84,  88,  92,  96,  100,
  104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 148, 152,
  156, 160, 164, 168, 172, 176, 180, 184, 188, 192, 196, 200, 204,
  208, 212, 216, 220, 224, 228, 232, 236, 240, 244, 249, 255,
};

int av1_quantizer_to_qindex(int quantizer) {
  return quantizer_to_qindex[quantizer];
}

int av1_qindex_to_quantizer(int qindex) {
  int quantizer;

  for (quantizer = 0; quantizer < 64; ++quantizer)
    if (quantizer_to_qindex[quantizer] >= qindex) return quantizer;

  return 63;
}
