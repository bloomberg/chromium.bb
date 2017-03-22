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
#include <string.h>

#include "./aom_scale_rtcd.h"
#include "aom/aom_integer.h"
#include "av1/common/cdef.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/reconinter.h"
#include "av1/encoder/clpf_rdo.h"
#include "av1/encoder/encoder.h"

#define TOTAL_STRENGTHS (DERING_STRENGTHS * CLPF_STRENGTHS)

/* Search for the best strength to add as an option, knowing we
   already selected nb_strengths options. */
static uint64_t search_one(int *lev, int nb_strengths,
                           uint64_t mse[][TOTAL_STRENGTHS], int sb_count) {
  uint64_t tot_mse[TOTAL_STRENGTHS];
  int i, j;
  uint64_t best_tot_mse = (uint64_t)1 << 63;
  int best_id = 0;
  memset(tot_mse, 0, sizeof(tot_mse));
  for (i = 0; i < sb_count; i++) {
    int gi;
    uint64_t best_mse = (uint64_t)1 << 63;
    /* Find best mse among already selected options. */
    for (gi = 0; gi < nb_strengths; gi++) {
      if (mse[i][lev[gi]] < best_mse) {
        best_mse = mse[i][lev[gi]];
      }
    }
    /* Find best mse when adding each possible new option. */
    for (j = 0; j < TOTAL_STRENGTHS; j++) {
      uint64_t best = best_mse;
      if (mse[i][j] < best) best = mse[i][j];
      tot_mse[j] += best;
    }
  }
  for (j = 0; j < TOTAL_STRENGTHS; j++) {
    if (tot_mse[j] < best_tot_mse) {
      best_tot_mse = tot_mse[j];
      best_id = j;
    }
  }
  lev[nb_strengths] = best_id;
  return best_tot_mse;
}

/* Search for the set of strengths that minimizes mse. */
static uint64_t joint_strength_search(int *best_lev, int nb_strengths,
                                      uint64_t mse[][TOTAL_STRENGTHS],
                                      int sb_count) {
  uint64_t best_tot_mse;
  int i;
  best_tot_mse = (uint64_t)1 << 63;
  /* Greedy search: add one strength options at a time. */
  for (i = 0; i < nb_strengths; i++) {
    best_tot_mse = search_one(best_lev, i, mse, sb_count);
  }
  /* Trying to refine the greedy search by reconsidering each
     already-selected option. */
  for (i = 0; i < 4 * nb_strengths; i++) {
    int j;
    for (j = 0; j < nb_strengths - 1; j++) best_lev[j] = best_lev[j + 1];
    best_tot_mse = search_one(best_lev, nb_strengths - 1, mse, sb_count);
  }
  return best_tot_mse;
}

static double compute_dist(uint16_t *x, int xstride, uint16_t *y, int ystride,
                           int nhb, int nvb, int coeff_shift) {
  int i, j;
  double sum;
  sum = 0;
  for (i = 0; i < nvb << 3; i++) {
    for (j = 0; j < nhb << 3; j++) {
      double tmp;
      tmp = x[i * xstride + j] - y[i * ystride + j];
      sum += tmp * tmp;
    }
  }
  return sum / (double)(1 << 2 * coeff_shift);
}

void av1_cdef_search(YV12_BUFFER_CONFIG *frame, const YV12_BUFFER_CONFIG *ref,
                     AV1_COMMON *cm, MACROBLOCKD *xd) {
  int r, c;
  int sbr, sbc;
  uint16_t *src;
  uint16_t *ref_coeff;
  dering_list dlist[MAX_MIB_SIZE * MAX_MIB_SIZE];
  int dir[OD_DERING_NBLOCKS][OD_DERING_NBLOCKS] = { { 0 } };
  int stride;
  int bsize[3];
  int dec[3];
  int pli;
  int level;
  int dering_count;
  int coeff_shift = AOMMAX(cm->bit_depth - 8, 0);
  uint64_t best_tot_mse = (uint64_t)1 << 63;
  uint64_t tot_mse;
  int sb_count;
  int nvsb = (cm->mi_rows + MAX_MIB_SIZE - 1) / MAX_MIB_SIZE;
  int nhsb = (cm->mi_cols + MAX_MIB_SIZE - 1) / MAX_MIB_SIZE;
  int *sb_index = aom_malloc(nvsb * nhsb * sizeof(*sb_index));
  uint64_t(*mse)[DERING_STRENGTHS * CLPF_STRENGTHS] =
      aom_malloc(sizeof(*mse) * nvsb * nhsb);
  int clpf_damping = 3 + (cm->base_qindex >> 6);
  int i;
  int best_lev[CDEF_MAX_STRENGTHS];
  int nb_strengths;
  int nb_strength_bits;
  int quantizer;
  double lambda;
  quantizer =
      av1_ac_quant(cm->base_qindex, 0, cm->bit_depth) >> (cm->bit_depth - 8);
  lambda = .12 * quantizer * quantizer / 256.;

  src = aom_memalign(32, sizeof(*src) * cm->mi_rows * cm->mi_cols * 64);
  ref_coeff =
      aom_memalign(32, sizeof(*ref_coeff) * cm->mi_rows * cm->mi_cols * 64);
  av1_setup_dst_planes(xd->plane, frame, 0, 0);
  for (pli = 0; pli < 3; pli++) {
    dec[pli] = xd->plane[pli].subsampling_x;
    bsize[pli] = OD_DERING_SIZE_LOG2 - dec[pli];
  }
  stride = cm->mi_cols << bsize[0];
  for (r = 0; r < cm->mi_rows << bsize[0]; ++r) {
    for (c = 0; c < cm->mi_cols << bsize[0]; ++c) {
#if CONFIG_AOM_HIGHBITDEPTH
      if (cm->use_highbitdepth) {
        src[r * stride + c] = CONVERT_TO_SHORTPTR(
            xd->plane[0].dst.buf)[r * xd->plane[0].dst.stride + c];
        ref_coeff[r * stride + c] =
            CONVERT_TO_SHORTPTR(ref->y_buffer)[r * ref->y_stride + c];
      } else {
#endif
        src[r * stride + c] =
            xd->plane[0].dst.buf[r * xd->plane[0].dst.stride + c];
        ref_coeff[r * stride + c] = ref->y_buffer[r * ref->y_stride + c];
#if CONFIG_AOM_HIGHBITDEPTH
      }
#endif
    }
  }
  sb_count = 0;
  for (sbr = 0; sbr < nvsb; sbr++) {
    for (sbc = 0; sbc < nhsb; sbc++) {
      int nvb, nhb;
      int gi;
      DECLARE_ALIGNED(32, uint16_t, dst[MAX_MIB_SIZE * MAX_MIB_SIZE * 8 * 8]);
      DECLARE_ALIGNED(32, uint16_t,
                      tmp_dst[MAX_MIB_SIZE * MAX_MIB_SIZE * 8 * 8]);
      nhb = AOMMIN(MAX_MIB_SIZE, cm->mi_cols - MAX_MIB_SIZE * sbc);
      nvb = AOMMIN(MAX_MIB_SIZE, cm->mi_rows - MAX_MIB_SIZE * sbr);
      dering_count = sb_compute_dering_list(cm, sbr * MAX_MIB_SIZE,
                                            sbc * MAX_MIB_SIZE, dlist);
      if (dering_count == 0) continue;
      for (gi = 0; gi < TOTAL_STRENGTHS; gi++) {
        int threshold;
        int clpf_strength;
        DECLARE_ALIGNED(32, uint16_t, inbuf[OD_DERING_INBUF_SIZE]);
        uint16_t *in;
        int j;
        level = dering_level_table[gi / CLPF_STRENGTHS];
        threshold = level << coeff_shift;
        for (r = 0; r < nvb << bsize[0]; r++) {
          for (c = 0; c < nhb << bsize[0]; c++) {
            dst[(r * MAX_MIB_SIZE << bsize[0]) + c] =
                src[((sbr * MAX_MIB_SIZE << bsize[0]) + r) * stride +
                    (sbc * MAX_MIB_SIZE << bsize[0]) + c];
          }
        }
        in = inbuf + OD_FILT_VBORDER * OD_FILT_BSTRIDE + OD_FILT_HBORDER;
        /* We avoid filtering the pixels for which some of the pixels to average
           are outside the frame. We could change the filter instead, but it
           would
           add special cases for any future vectorization. */
        for (i = 0; i < OD_DERING_INBUF_SIZE; i++)
          inbuf[i] = OD_DERING_VERY_LARGE;
        for (i = -OD_FILT_VBORDER * (sbr != 0);
             i < (nvb << bsize[0]) + OD_FILT_VBORDER * (sbr != nvsb - 1); i++) {
          for (j = -OD_FILT_HBORDER * (sbc != 0);
               j < (nhb << bsize[0]) + OD_FILT_HBORDER * (sbc != nhsb - 1);
               j++) {
            uint16_t *x;
            x = &src[(sbr * stride * MAX_MIB_SIZE << bsize[0]) +
                     (sbc * MAX_MIB_SIZE << bsize[0])];
            in[i * OD_FILT_BSTRIDE + j] = x[i * stride + j];
          }
        }
        clpf_strength = gi % CLPF_STRENGTHS;
        od_dering(tmp_dst, in, 0, dir, 0, dlist, dering_count, threshold,
                  clpf_strength + (clpf_strength == 3), clpf_damping,
                  coeff_shift);
        copy_dering_16bit_to_16bit(dst, MAX_MIB_SIZE << bsize[0], tmp_dst,
                                   dlist, dering_count, bsize[0]);
        mse[sb_count][gi] = (int)compute_dist(
            dst, MAX_MIB_SIZE << bsize[0],
            &ref_coeff[(sbr * stride * MAX_MIB_SIZE << bsize[0]) +
                       (sbc * MAX_MIB_SIZE << bsize[0])],
            stride, nhb, nvb, coeff_shift);
        sb_index[sb_count] =
            MAX_MIB_SIZE * sbr * cm->mi_stride + MAX_MIB_SIZE * sbc;
      }
      sb_count++;
    }
  }

  nb_strength_bits = 0;
  /* Search for different number of signalling bits. */
  for (i = 0; i <= 3; i++) {
    nb_strengths = 1 << i;
    tot_mse = joint_strength_search(best_lev, nb_strengths, mse, sb_count);
    /* Count superblock signalling cost. */
    tot_mse += (uint64_t)(sb_count * lambda * i);
    /* Count header signalling cost. */
    tot_mse += (uint64_t)(nb_strengths * lambda * CDEF_STRENGTH_BITS);
    if (tot_mse < best_tot_mse) {
      best_tot_mse = tot_mse;
      nb_strength_bits = i;
    }
  }
  nb_strengths = 1 << nb_strength_bits;

  cm->cdef_bits = nb_strength_bits;
  cm->nb_cdef_strengths = nb_strengths;
  for (i = 0; i < nb_strengths; i++) cm->cdef_strengths[i] = best_lev[i];
  for (i = 0; i < sb_count; i++) {
    int gi;
    int best_gi;
    uint64_t best_mse = (uint64_t)1 << 63;
    best_gi = 0;
    for (gi = 0; gi < cm->nb_cdef_strengths; gi++) {
      if (mse[i][best_lev[gi]] < best_mse) {
        best_gi = gi;
        best_mse = mse[i][best_lev[gi]];
      }
    }
    cm->mi_grid_visible[sb_index[i]]->mbmi.cdef_strength = best_gi;
  }

  aom_free(src);
  aom_free(ref_coeff);
  aom_free(mse);
  aom_free(sb_index);

  av1_clpf_test_plane(cm->frame_to_show, ref, cm, &cm->clpf_strength_u,
                      AOM_PLANE_U);
  av1_clpf_test_plane(cm->frame_to_show, ref, cm, &cm->clpf_strength_v,
                      AOM_PLANE_V);
}
