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
  uint64_t best_tot_mse = 0;
  int sb_count;
  int nvsb = (cm->mi_rows + MAX_MIB_SIZE - 1) / MAX_MIB_SIZE;
  int nhsb = (cm->mi_cols + MAX_MIB_SIZE - 1) / MAX_MIB_SIZE;
  int *sb_index = aom_malloc(nvsb * nhsb * sizeof(*sb_index));
  uint64_t(*mse)[DERING_STRENGTHS][CLPF_STRENGTHS] =
      aom_malloc(sizeof(*mse) * nvsb * nhsb);
  int clpf_damping = 3 + (cm->base_qindex >> 6);
  int i;
  int lev[DERING_REFINEMENT_LEVELS];
  int best_lev[DERING_REFINEMENT_LEVELS];
  int str[CLPF_REFINEMENT_LEVELS];
  int best_str[CLPF_REFINEMENT_LEVELS];
  double lambda = exp(cm->base_qindex / 36.0);
  static int log2[] = { 0, 1, 2, 2 };

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
      for (gi = 0; gi < DERING_STRENGTHS; gi++) {
        int threshold;
        DECLARE_ALIGNED(32, uint16_t, inbuf[OD_DERING_INBUF_SIZE]);
        uint16_t *in;
        int j;
        level = dering_level_table[gi];
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
        for (i = 0; i < CLPF_STRENGTHS; i++) {
          od_dering(tmp_dst, in, 0, dir, 0, dlist, dering_count, threshold,
                    i + (i == 3), clpf_damping, coeff_shift);
          copy_dering_16bit_to_16bit(dst, MAX_MIB_SIZE << bsize[0], tmp_dst,
                                     dlist, dering_count, bsize[0]);
          mse[sb_count][gi][i] = (int)compute_dist(
              dst, MAX_MIB_SIZE << bsize[0],
              &ref_coeff[(sbr * stride * MAX_MIB_SIZE << bsize[0]) +
                         (sbc * MAX_MIB_SIZE << bsize[0])],
              stride, nhb, nvb, coeff_shift);
        }
        sb_index[sb_count] =
            MAX_MIB_SIZE * sbr * cm->mi_stride + MAX_MIB_SIZE * sbc;
      }
      sb_count++;
    }
  }
  best_tot_mse = (uint64_t)1 << 63;

  int l0;
  for (l0 = 0; l0 < DERING_STRENGTHS; l0++) {
    int l1;
    lev[0] = l0;
    for (l1 = l0; l1 < DERING_STRENGTHS; l1++) {
      int l2;
      lev[1] = l1;
      for (l2 = l1; l2 < DERING_STRENGTHS; l2++) {
        int l3;
        lev[2] = l2;
        for (l3 = l2; l3 < DERING_STRENGTHS; l3++) {
          int cs0;
          lev[3] = l3;
          for (cs0 = 0; cs0 < CLPF_STRENGTHS; cs0++) {
            int cs1;
            str[0] = cs0;
            for (cs1 = cs0; cs1 < CLPF_STRENGTHS; cs1++) {
              uint64_t tot_mse = 0;
              str[1] = cs1;
              for (i = 0; i < sb_count; i++) {
                int gi;
                int cs;
                uint64_t best_mse = (uint64_t)1 << 63;
                for (gi = 0; gi < DERING_REFINEMENT_LEVELS; gi++) {
                  for (cs = 0; cs < CLPF_REFINEMENT_LEVELS; cs++) {
                    if (mse[i][lev[gi]][str[cs]] < best_mse) {
                      best_mse = mse[i][lev[gi]][str[cs]];
                    }
                  }
                }
                tot_mse += best_mse;
              }

              // Add the bit cost
              int dering_diffs = 0, clpf_diffs = 0;
              for (i = 1; i < DERING_REFINEMENT_LEVELS; i++)
                dering_diffs += lev[i] != lev[i - 1];
              for (i = 1; i < CLPF_REFINEMENT_LEVELS; i++)
                clpf_diffs += str[i] != str[i - 1];
              tot_mse += (uint64_t)(sb_count * lambda *
                                    (log2[dering_diffs] + log2[clpf_diffs]));

              if (tot_mse < best_tot_mse) {
                for (i = 0; i < DERING_REFINEMENT_LEVELS; i++)
                  best_lev[i] = lev[i];
                for (i = 0; i < CLPF_REFINEMENT_LEVELS; i++)
                  best_str[i] = str[i];
                best_tot_mse = tot_mse;
              }
            }
          }
        }
      }
    }
  }
  for (i = 0; i < DERING_REFINEMENT_LEVELS; i++) lev[i] = best_lev[i];
  for (i = 0; i < CLPF_REFINEMENT_LEVELS; i++) str[i] = best_str[i];

  id_to_levels(lev, str, levels_to_id(lev, str));  // Pack tables
  cdef_get_bits(lev, str, &cm->dering_bits, &cm->clpf_bits);

  for (i = 0; i < sb_count; i++) {
    int gi, cs;
    int best_gi, best_clpf;
    uint64_t best_mse = (uint64_t)1 << 63;
    best_gi = best_clpf = 0;
    for (gi = 0; gi < (1 << cm->dering_bits); gi++) {
      for (cs = 0; cs < (1 << cm->clpf_bits); cs++) {
        if (mse[i][lev[gi]][str[cs]] < best_mse) {
          best_gi = gi;
          best_clpf = cs;
          best_mse = mse[i][lev[gi]][str[cs]];
        }
      }
    }
    cm->mi_grid_visible[sb_index[i]]->mbmi.dering_gain = best_gi;
    cm->mi_grid_visible[sb_index[i]]->mbmi.clpf_strength = best_clpf;
  }

  aom_free(src);
  aom_free(ref_coeff);
  aom_free(mse);
  aom_free(sb_index);

  av1_clpf_test_plane(cm->frame_to_show, ref, cm, &cm->clpf_strength_u,
                      AOM_PLANE_U);
  av1_clpf_test_plane(cm->frame_to_show, ref, cm, &cm->clpf_strength_v,
                      AOM_PLANE_V);
  cm->dering_level = levels_to_id(best_lev, best_str);
}
