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

#include "av1/common/clpf.h"
#include "./aom_dsp_rtcd.h"
#include "aom/aom_image.h"
#include "aom/aom_integer.h"
#include "av1/common/quant_common.h"

// Calculate the error of a filtered and unfiltered block
void aom_clpf_detect_c(const uint8_t *rec, const uint8_t *org, int rstride,
                       int ostride, int x0, int y0, int width, int height,
                       int *sum0, int *sum1, unsigned int strength, int size,
                       unsigned int dmp) {
  int x, y;
  for (y = y0; y < y0 + size; y++) {
    for (x = x0; x < x0 + size; x++) {
      const int O = org[y * ostride + x];
      const int X = rec[y * rstride + x];
      const int A = rec[AOMMAX(0, y - 2) * rstride + x];
      const int B = rec[AOMMAX(0, y - 1) * rstride + x];
      const int C = rec[y * rstride + AOMMAX(0, x - 2)];
      const int D = rec[y * rstride + AOMMAX(0, x - 1)];
      const int E = rec[y * rstride + AOMMIN(width - 1, x + 1)];
      const int F = rec[y * rstride + AOMMIN(width - 1, x + 2)];
      const int G = rec[AOMMIN(height - 1, y + 1) * rstride + x];
      const int H = rec[AOMMIN(height - 1, y + 2) * rstride + x];
      const int delta =
          av1_clpf_sample(X, A, B, C, D, E, F, G, H, strength, dmp);
      const int Y = X + delta;
      *sum0 += (O - X) * (O - X);
      *sum1 += (O - Y) * (O - Y);
    }
  }
}

void aom_clpf_detect_multi_c(const uint8_t *rec, const uint8_t *org,
                             int rstride, int ostride, int x0, int y0,
                             int width, int height, int *sum, int size,
                             unsigned int dmp) {
  int x, y;

  for (y = y0; y < y0 + size; y++) {
    for (x = x0; x < x0 + size; x++) {
      const int O = org[y * ostride + x];
      const int X = rec[y * rstride + x];
      const int A = rec[AOMMAX(0, y - 2) * rstride + x];
      const int B = rec[AOMMAX(0, y - 1) * rstride + x];
      const int C = rec[y * rstride + AOMMAX(0, x - 2)];
      const int D = rec[y * rstride + AOMMAX(0, x - 1)];
      const int E = rec[y * rstride + AOMMIN(width - 1, x + 1)];
      const int F = rec[y * rstride + AOMMIN(width - 1, x + 2)];
      const int G = rec[AOMMIN(height - 1, y + 1) * rstride + x];
      const int H = rec[AOMMIN(height - 1, y + 2) * rstride + x];
      const int delta1 = av1_clpf_sample(X, A, B, C, D, E, F, G, H, 1, dmp);
      const int delta2 = av1_clpf_sample(X, A, B, C, D, E, F, G, H, 2, dmp);
      const int delta3 = av1_clpf_sample(X, A, B, C, D, E, F, G, H, 4, dmp);
      const int F1 = X + delta1;
      const int F2 = X + delta2;
      const int F3 = X + delta3;
      sum[0] += (O - X) * (O - X);
      sum[1] += (O - F1) * (O - F1);
      sum[2] += (O - F2) * (O - F2);
      sum[3] += (O - F3) * (O - F3);
    }
  }
}

#if CONFIG_AOM_HIGHBITDEPTH
// Identical to aom_clpf_detect_c() apart from "rec" and "org".
void aom_clpf_detect_hbd_c(const uint16_t *rec, const uint16_t *org,
                           int rstride, int ostride, int x0, int y0, int width,
                           int height, int *sum0, int *sum1,
                           unsigned int strength, int size, unsigned int bd,
                           unsigned int dmp) {
  const int shift = bd - 8;
  int x, y;
  for (y = y0; y < y0 + size; y++) {
    for (x = x0; x < x0 + size; x++) {
      const int O = org[y * ostride + x] >> shift;
      const int X = rec[y * rstride + x] >> shift;
      const int A = rec[AOMMAX(0, y - 2) * rstride + x] >> shift;
      const int B = rec[AOMMAX(0, y - 1) * rstride + x] >> shift;
      const int C = rec[y * rstride + AOMMAX(0, x - 2)] >> shift;
      const int D = rec[y * rstride + AOMMAX(0, x - 1)] >> shift;
      const int E = rec[y * rstride + AOMMIN(width - 1, x + 1)] >> shift;
      const int F = rec[y * rstride + AOMMIN(width - 1, x + 2)] >> shift;
      const int G = rec[AOMMIN(height - 1, y + 1) * rstride + x] >> shift;
      const int H = rec[AOMMIN(height - 1, y + 2) * rstride + x] >> shift;
      const int delta = av1_clpf_sample(X, A, B, C, D, E, F, G, H,
                                        strength >> shift, dmp - shift);
      const int Y = X + delta;
      *sum0 += (O - X) * (O - X);
      *sum1 += (O - Y) * (O - Y);
    }
  }
}

// aom_clpf_detect_multi_c() apart from "rec" and "org".
void aom_clpf_detect_multi_hbd_c(const uint16_t *rec, const uint16_t *org,
                                 int rstride, int ostride, int x0, int y0,
                                 int width, int height, int *sum, int size,
                                 unsigned int bd, unsigned int dmp) {
  const int shift = bd - 8;
  int x, y;

  for (y = y0; y < y0 + size; y++) {
    for (x = x0; x < x0 + size; x++) {
      int O = org[y * ostride + x] >> shift;
      int X = rec[y * rstride + x] >> shift;
      const int A = rec[AOMMAX(0, y - 2) * rstride + x] >> shift;
      const int B = rec[AOMMAX(0, y - 1) * rstride + x] >> shift;
      const int C = rec[y * rstride + AOMMAX(0, x - 2)] >> shift;
      const int D = rec[y * rstride + AOMMAX(0, x - 1)] >> shift;
      const int E = rec[y * rstride + AOMMIN(width - 1, x + 1)] >> shift;
      const int F = rec[y * rstride + AOMMIN(width - 1, x + 2)] >> shift;
      const int G = rec[AOMMIN(height - 1, y + 1) * rstride + x] >> shift;
      const int H = rec[AOMMIN(height - 1, y + 2) * rstride + x] >> shift;
      const int delta1 =
          av1_clpf_sample(X, A, B, C, D, E, F, G, H, 1, dmp - shift);
      const int delta2 =
          av1_clpf_sample(X, A, B, C, D, E, F, G, H, 2, dmp - shift);
      const int delta3 =
          av1_clpf_sample(X, A, B, C, D, E, F, G, H, 4, dmp - shift);
      const int F1 = X + delta1;
      const int F2 = X + delta2;
      const int F3 = X + delta3;
      sum[0] += (O - X) * (O - X);
      sum[1] += (O - F1) * (O - F1);
      sum[2] += (O - F2) * (O - F2);
      sum[3] += (O - F3) * (O - F3);
    }
  }
}
#endif

// Calculate the square error of all filter settings.  Result:
// res[0][0]   : unfiltered
// res[0][1-3] : strength=1,2,4, no signals
static void clpf_rdo(const YV12_BUFFER_CONFIG *rec,
                     const YV12_BUFFER_CONFIG *org, const AV1_COMMON *cm,
                     unsigned int block_size, int w, int h, uint64_t res[4],
                     int plane) {
  int m, n;
  int sum[4];
  const int subx = plane != AOM_PLANE_Y && rec->subsampling_x;
  const int suby = plane != AOM_PLANE_Y && rec->subsampling_y;
  uint8_t *rec_buffer =
      plane != AOM_PLANE_Y
          ? (plane == AOM_PLANE_U ? rec->u_buffer : rec->v_buffer)
          : rec->y_buffer;
  uint8_t *org_buffer =
      plane != AOM_PLANE_Y
          ? (plane == AOM_PLANE_U ? org->u_buffer : org->v_buffer)
          : org->y_buffer;
  int rec_width = plane != AOM_PLANE_Y ? rec->uv_crop_width : rec->y_crop_width;
  int rec_height =
      plane != AOM_PLANE_Y ? rec->uv_crop_height : rec->y_crop_height;
  int rec_stride = plane != AOM_PLANE_Y ? rec->uv_stride : rec->y_stride;
  int org_stride = plane != AOM_PLANE_Y ? org->uv_stride : org->y_stride;
  int damping =
      cm->bit_depth - 5 - (plane != AOM_PLANE_Y) + (cm->base_qindex >> 6);

  sum[0] = sum[1] = sum[2] = sum[3] = 0;

  for (m = 0; m < h; m++) {
    for (n = 0; n < w; n++) {
      int xpos = n * block_size;
      int ypos = m * block_size;
      if (!cm->mi_grid_visible[(ypos << suby) / MI_SIZE * cm->mi_stride +
                               (xpos << subx) / MI_SIZE]
               ->mbmi.skip) {
#if CONFIG_AOM_HIGHBITDEPTH
        if (cm->use_highbitdepth) {
          aom_clpf_detect_multi_hbd(
              CONVERT_TO_SHORTPTR(rec_buffer), CONVERT_TO_SHORTPTR(org_buffer),
              rec_stride, org_stride, xpos, ypos, rec_width, rec_height, sum,
              block_size, cm->bit_depth, damping);
        } else {
          aom_clpf_detect_multi(rec_buffer, org_buffer, rec_stride, org_stride,
                                xpos, ypos, rec_width, rec_height, sum,
                                block_size, damping);
        }
#else
        aom_clpf_detect_multi(rec_buffer, org_buffer, rec_stride, org_stride,
                              xpos, ypos, rec_width, rec_height, sum,
                              block_size, damping);
#endif
      }
    }
  }

  res[0] += sum[0];
  res[1] += sum[1];
  res[2] += sum[2];
  res[3] += sum[3];
}

void av1_clpf_test_plane(const YV12_BUFFER_CONFIG *rec,
                         const YV12_BUFFER_CONFIG *org, const AV1_COMMON *cm,
                         int *best_strength, int plane) {
  int i;
  uint64_t best, sums[4];
  int width = plane != AOM_PLANE_Y ? rec->uv_crop_width : rec->y_crop_width;
  int height = plane != AOM_PLANE_Y ? rec->uv_crop_height : rec->y_crop_height;
  const int bs = MI_SIZE;
  const int bslog = get_msb(bs);

  memset(sums, 0, sizeof(sums));

  clpf_rdo(rec, org, cm, bs, width >> bslog, height >> bslog, sums, plane);

  // Add a favourable bias for conservative strengths
  for (i = 0; i < 4; i++) sums[i] -= sums[i] >> (7 + i);

  // Tag the strength to the error
  for (i = 0; i < 4; i++) sums[i] = (sums[i] << 2) + i;

  // Identify the strength with the smallest error
  best = (uint64_t)1 << 63;
  for (i = 0; i < 4; i++)
    if (sums[i] < best) best = sums[i];
  *best_strength = best & 3 ? 1 << ((best - 1) & 3) : 0;
}
