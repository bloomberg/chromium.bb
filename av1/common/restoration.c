/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 *
 */

#include <math.h>

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "./aom_scale_rtcd.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/restoration.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"

static int domaintxfmrf_vtable[DOMAINTXFMRF_ITERS][DOMAINTXFMRF_PARAMS][256];

// Whether to filter only y or not
static const int override_y_only[RESTORE_TYPES] = { 1, 1, 1, 1, 1 };

static const int domaintxfmrf_params[DOMAINTXFMRF_PARAMS] = {
  48,  52,  56,  60,  64,  68,  72,  76,  80,  82,  84,  86,  88,
  90,  92,  94,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105,
  106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118,
  119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 130, 132, 134,
  136, 138, 140, 142, 146, 150, 154, 158, 162, 166, 170, 174
};

const sgr_params_type sgr_params[SGRPROJ_PARAMS] = {
  // r1, eps1, r2, eps2
  { 2, 27, 1, 11 }, { 2, 31, 1, 12 }, { 2, 37, 1, 12 }, { 2, 44, 1, 12 },
  { 2, 49, 1, 13 }, { 2, 54, 1, 14 }, { 2, 60, 1, 15 }, { 2, 68, 1, 15 },
};

typedef void (*restore_func_type)(uint8_t *data8, int width, int height,
                                  int stride, RestorationInternal *rst,
                                  uint8_t *tmpdata8, int tmpstride,
                                  uint8_t *dst8, int dst_stride);
#if CONFIG_AOM_HIGHBITDEPTH
typedef void (*restore_func_highbd_type)(uint8_t *data8, int width, int height,
                                         int stride, RestorationInternal *rst,
                                         uint8_t *tmpdata8, int tmpstride,
                                         int bit_depth, uint8_t *dst8,
                                         int dst_stride);
#endif  // CONFIG_AOM_HIGHBITDEPTH

static void GenDomainTxfmRFVtable() {
  int i, j;
  const double sigma_s = sqrt(2.0);
  for (i = 0; i < DOMAINTXFMRF_ITERS; ++i) {
    const int nm = (1 << (DOMAINTXFMRF_ITERS - i - 1));
    const double A = exp(-DOMAINTXFMRF_MULT / (sigma_s * nm));
    for (j = 0; j < DOMAINTXFMRF_PARAMS; ++j) {
      const double sigma_r =
          (double)domaintxfmrf_params[j] / DOMAINTXFMRF_SIGMA_SCALE;
      const double scale = sigma_s / sigma_r;
      int k;
      for (k = 0; k < 256; ++k) {
        domaintxfmrf_vtable[i][j][k] =
            RINT(DOMAINTXFMRF_VTABLE_PREC * pow(A, 1.0 + k * scale));
      }
    }
  }
}

void av1_loop_restoration_precal() { GenDomainTxfmRFVtable(); }

void av1_loop_restoration_init(RestorationInternal *rst, RestorationInfo *rsi,
                               int kf, int width, int height) {
  int i, tile_idx;
  rst->rsi = rsi;
  rst->keyframe = kf;
  rst->subsampling_x = 0;
  rst->subsampling_y = 0;
  rst->ntiles =
      av1_get_rest_ntiles(width, height, &rst->tile_width, &rst->tile_height,
                          &rst->nhtiles, &rst->nvtiles);
  if (rsi->frame_restoration_type == RESTORE_WIENER) {
    for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
      if (rsi->wiener_info[tile_idx].level) {
        rsi->wiener_info[tile_idx].vfilter[RESTORATION_HALFWIN] =
            rsi->wiener_info[tile_idx].hfilter[RESTORATION_HALFWIN] =
                RESTORATION_FILT_STEP;
        for (i = 0; i < RESTORATION_HALFWIN; ++i) {
          rsi->wiener_info[tile_idx].vfilter[RESTORATION_WIN - 1 - i] =
              rsi->wiener_info[tile_idx].vfilter[i];
          rsi->wiener_info[tile_idx].hfilter[RESTORATION_WIN - 1 - i] =
              rsi->wiener_info[tile_idx].hfilter[i];
          rsi->wiener_info[tile_idx].vfilter[RESTORATION_HALFWIN] -=
              2 * rsi->wiener_info[tile_idx].vfilter[i];
          rsi->wiener_info[tile_idx].hfilter[RESTORATION_HALFWIN] -=
              2 * rsi->wiener_info[tile_idx].hfilter[i];
        }
      }
    }
  } else if (rsi->frame_restoration_type == RESTORE_SWITCHABLE) {
    for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
      if (rsi->restoration_type[tile_idx] == RESTORE_WIENER) {
        rsi->wiener_info[tile_idx].vfilter[RESTORATION_HALFWIN] =
            rsi->wiener_info[tile_idx].hfilter[RESTORATION_HALFWIN] =
                RESTORATION_FILT_STEP;
        for (i = 0; i < RESTORATION_HALFWIN; ++i) {
          rsi->wiener_info[tile_idx].vfilter[RESTORATION_WIN - 1 - i] =
              rsi->wiener_info[tile_idx].vfilter[i];
          rsi->wiener_info[tile_idx].hfilter[RESTORATION_WIN - 1 - i] =
              rsi->wiener_info[tile_idx].hfilter[i];
          rsi->wiener_info[tile_idx].vfilter[RESTORATION_HALFWIN] -=
              2 * rsi->wiener_info[tile_idx].vfilter[i];
          rsi->wiener_info[tile_idx].hfilter[RESTORATION_HALFWIN] -=
              2 * rsi->wiener_info[tile_idx].hfilter[i];
        }
      }
    }
  }
}

// Some filters do not write the outermost RESTORATION_HALFWIN pixels,
// so copy them over explicitly.
static void copy_border(uint8_t *data, int width, int height, int stride,
                        uint8_t *dst, int dst_stride) {
  int i;
  for (i = RESTORATION_HALFWIN; i < height - RESTORATION_HALFWIN; ++i) {
    memcpy(dst + i * dst_stride, data + i * stride, RESTORATION_HALFWIN);
    memcpy(dst + i * dst_stride + (width - RESTORATION_HALFWIN),
           data + i * stride + (width - RESTORATION_HALFWIN),
           RESTORATION_HALFWIN);
  }
  for (i = 0; i < RESTORATION_HALFWIN; ++i) {
    memcpy(dst + i * dst_stride, data + i * stride, width);
  }
  for (i = height - RESTORATION_HALFWIN; i < height; ++i)
    memcpy(dst + i * dst_stride, data + i * stride, width);
}

static void loop_copy_tile(uint8_t *data, int tile_idx, int subtile_idx,
                           int subtile_bits, int width, int height, int stride,
                           RestorationInternal *rst, uint8_t *dst,
                           int dst_stride) {
  const int tile_width = rst->tile_width >> rst->subsampling_x;
  const int tile_height = rst->tile_height >> rst->subsampling_y;
  int i;
  int h_start, h_end, v_start, v_end;
  av1_get_rest_tile_limits(tile_idx, subtile_idx, subtile_bits, rst->nhtiles,
                           rst->nvtiles, tile_width, tile_height, width, height,
                           0, 0, &h_start, &h_end, &v_start, &v_end);
  for (i = v_start; i < v_end; ++i)
    memcpy(dst + i * dst_stride + h_start, data + i * stride + h_start,
           h_end - h_start);
}

uint8_t hor_sym_filter(uint8_t *d, int *hfilter) {
  int32_t s =
      (1 << (RESTORATION_FILT_BITS - 1)) + d[0] * hfilter[RESTORATION_HALFWIN];
  int i;
  for (i = 1; i <= RESTORATION_HALFWIN; ++i)
    s += (d[i] + d[-i]) * hfilter[RESTORATION_HALFWIN + i];
  return clip_pixel(s >> RESTORATION_FILT_BITS);
}

uint8_t ver_sym_filter(uint8_t *d, int stride, int *vfilter) {
  int32_t s =
      (1 << (RESTORATION_FILT_BITS - 1)) + d[0] * vfilter[RESTORATION_HALFWIN];
  int i;
  for (i = 1; i <= RESTORATION_HALFWIN; ++i)
    s += (d[i * stride] + d[-i * stride]) * vfilter[RESTORATION_HALFWIN + i];
  return clip_pixel(s >> RESTORATION_FILT_BITS);
}

static void loop_wiener_filter_tile(uint8_t *data, int tile_idx, int width,
                                    int height, int stride,
                                    RestorationInternal *rst, uint8_t *tmpdata,
                                    int tmpstride, uint8_t *dst,
                                    int dst_stride) {
  const int tile_width = rst->tile_width >> rst->subsampling_x;
  const int tile_height = rst->tile_height >> rst->subsampling_y;
  int i, j;
  int h_start, h_end, v_start, v_end;
  uint8_t *data_p, *tmpdata_p, *dst_p;

  if (rst->rsi->wiener_info[tile_idx].level == 0) {
    loop_copy_tile(data, tile_idx, 0, 0, width, height, stride, rst, dst,
                   dst_stride);
    return;
  }
  // Filter row-wise
  av1_get_rest_tile_limits(tile_idx, 0, 0, rst->nhtiles, rst->nvtiles,
                           tile_width, tile_height, width, height, 1, 0,
                           &h_start, &h_end, &v_start, &v_end);
  data_p = data + h_start + v_start * stride;
  tmpdata_p = tmpdata + h_start + v_start * tmpstride;
  for (i = 0; i < (v_end - v_start); ++i) {
    for (j = 0; j < (h_end - h_start); ++j) {
      *tmpdata_p++ =
          hor_sym_filter(data_p++, rst->rsi->wiener_info[tile_idx].hfilter);
    }
    data_p += stride - (h_end - h_start);
    tmpdata_p += tmpstride - (h_end - h_start);
  }
  // Filter col-wise
  av1_get_rest_tile_limits(tile_idx, 0, 0, rst->nhtiles, rst->nvtiles,
                           tile_width, tile_height, width, height, 0, 1,
                           &h_start, &h_end, &v_start, &v_end);
  dst_p = dst + h_start + v_start * dst_stride;
  tmpdata_p = tmpdata + h_start + v_start * tmpstride;
  for (i = 0; i < (v_end - v_start); ++i) {
    for (j = 0; j < (h_end - h_start); ++j) {
      *dst_p++ = ver_sym_filter(tmpdata_p++, tmpstride,
                                rst->rsi->wiener_info[tile_idx].vfilter);
    }
    dst_p += dst_stride - (h_end - h_start);
    tmpdata_p += tmpstride - (h_end - h_start);
  }
}

static void loop_wiener_filter(uint8_t *data, int width, int height, int stride,
                               RestorationInternal *rst, uint8_t *tmpdata,
                               int tmpstride, uint8_t *dst, int dst_stride) {
  int tile_idx, i;
  // Initialize tmp buffer
  uint8_t *data_p = data;
  uint8_t *tmpdata_p = tmpdata;
  for (i = 0; i < height; ++i) {
    memcpy(tmpdata_p, data_p, sizeof(*data_p) * width);
    data_p += stride;
    tmpdata_p += tmpstride;
  }
  copy_border(data, width, height, stride, dst, dst_stride);
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    loop_wiener_filter_tile(data, tile_idx, width, height, stride, rst, tmpdata,
                            tmpstride, dst, dst_stride);
  }
}

static void boxsum(int64_t *src, int width, int height, int src_stride, int r,
                   int sqr, int64_t *dst, int dst_stride, int64_t *tmp,
                   int tmp_stride) {
  int i, j;

  if (sqr) {
    for (j = 0; j < width; ++j) tmp[j] = src[j] * src[j];
    for (j = 0; j < width; ++j)
      for (i = 1; i < height; ++i)
        tmp[i * tmp_stride + j] =
            tmp[(i - 1) * tmp_stride + j] +
            src[i * src_stride + j] * src[i * src_stride + j];
  } else {
    memcpy(tmp, src, sizeof(*tmp) * width);
    for (j = 0; j < width; ++j)
      for (i = 1; i < height; ++i)
        tmp[i * tmp_stride + j] =
            tmp[(i - 1) * tmp_stride + j] + src[i * src_stride + j];
  }
  for (i = 0; i <= r; ++i)
    memcpy(&dst[i * dst_stride], &tmp[(i + r) * tmp_stride],
           sizeof(*tmp) * width);
  for (i = r + 1; i < height - r; ++i)
    for (j = 0; j < width; ++j)
      dst[i * dst_stride + j] =
          tmp[(i + r) * tmp_stride + j] - tmp[(i - r - 1) * tmp_stride + j];
  for (i = height - r; i < height; ++i)
    for (j = 0; j < width; ++j)
      dst[i * dst_stride + j] = tmp[(height - 1) * tmp_stride + j] -
                                tmp[(i - r - 1) * tmp_stride + j];

  for (i = 0; i < height; ++i) tmp[i * tmp_stride] = dst[i * dst_stride];
  for (i = 0; i < height; ++i)
    for (j = 1; j < width; ++j)
      tmp[i * tmp_stride + j] =
          tmp[i * tmp_stride + j - 1] + dst[i * src_stride + j];

  for (j = 0; j <= r; ++j)
    for (i = 0; i < height; ++i)
      dst[i * dst_stride + j] = tmp[i * tmp_stride + j + r];
  for (j = r + 1; j < width - r; ++j)
    for (i = 0; i < height; ++i)
      dst[i * dst_stride + j] =
          tmp[i * tmp_stride + j + r] - tmp[i * tmp_stride + j - r - 1];
  for (j = width - r; j < width; ++j)
    for (i = 0; i < height; ++i)
      dst[i * dst_stride + j] =
          tmp[i * tmp_stride + width - 1] - tmp[i * tmp_stride + j - r - 1];
}

static void boxnum(int width, int height, int r, int8_t *num, int num_stride) {
  int i, j;
  for (i = 0; i <= AOMMIN(r, height - 1); ++i) {
    for (j = 0; j <= AOMMIN(r, width - 1); ++j) {
      num[i * num_stride + j] =
          AOMMIN(r + 1 + i, height) * AOMMIN(r + 1 + j, width);
      num[i * num_stride + (width - 1 - j)] = num[i * num_stride + j];
      num[(height - 1 - i) * num_stride + j] = num[i * num_stride + j];
      num[(height - 1 - i) * num_stride + (width - 1 - j)] =
          num[i * num_stride + j];
    }
  }
  for (j = 0; j <= AOMMIN(r, width - 1); ++j) {
    const int val = AOMMIN(2 * r + 1, height) * AOMMIN(r + 1 + j, width);
    for (i = r + 1; i < height - r; ++i) {
      num[i * num_stride + j] = val;
      num[i * num_stride + (width - 1 - j)] = val;
    }
  }
  for (i = 0; i <= AOMMIN(r, height - 1); ++i) {
    const int val = AOMMIN(2 * r + 1, width) * AOMMIN(r + 1 + i, height);
    for (j = r + 1; j < width - r; ++j) {
      num[i * num_stride + j] = val;
      num[(height - 1 - i) * num_stride + j] = val;
    }
  }
  for (i = r + 1; i < height - r; ++i) {
    for (j = r + 1; j < width - r; ++j) {
      num[i * num_stride + j] =
          AOMMIN(2 * r + 1, height) * AOMMIN(2 * r + 1, width);
    }
  }
}

void decode_xq(int *xqd, int *xq) {
  xq[0] = -xqd[0];
  xq[1] = (1 << SGRPROJ_PRJ_BITS) - xq[0] - xqd[1];
}

#define APPROXIMATE_SGR 1
void av1_selfguided_restoration(int64_t *dgd, int width, int height, int stride,
                                int bit_depth, int r, int eps, void *tmpbuf) {
  int64_t *A = (int64_t *)tmpbuf;
  int64_t *B = A + RESTORATION_TILEPELS_MAX;
  int64_t *T = B + RESTORATION_TILEPELS_MAX;
  int8_t num[RESTORATION_TILEPELS_MAX];
  int i, j;
  eps <<= 2 * (bit_depth - 8);

  boxsum(dgd, width, height, stride, r, 0, B, width, T, width);
  boxsum(dgd, width, height, stride, r, 1, A, width, T, width);
  boxnum(width, height, r, num, width);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int k = i * width + j;
      const int n = num[k];
      int64_t den;
      A[k] = A[k] * n - B[k] * B[k];
      den = A[k] + n * n * eps;
      A[k] = ((A[k] << SGRPROJ_SGR_BITS) + (den >> 1)) / den;
      B[k] = ((SGRPROJ_SGR - A[k]) * B[k] + (n >> 1)) / n;
    }
  }
#if APPROXIMATE_SGR
  i = 0;
  j = 0;
  {
    const int k = i * width + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int64_t a =
        3 * A[k] + 2 * A[k + 1] + 2 * A[k + width] + A[k + width + 1];
    const int64_t b =
        3 * B[k] + 2 * B[k + 1] + 2 * B[k + width] + B[k + width + 1];
    const int64_t v =
        (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
  }
  i = 0;
  j = width - 1;
  {
    const int k = i * width + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int64_t a =
        3 * A[k] + 2 * A[k - 1] + 2 * A[k + width] + A[k + width - 1];
    const int64_t b =
        3 * B[k] + 2 * B[k - 1] + 2 * B[k + width] + B[k + width - 1];
    const int64_t v =
        (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
  }
  i = height - 1;
  j = 0;
  {
    const int k = i * width + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int64_t a =
        3 * A[k] + 2 * A[k + 1] + 2 * A[k - width] + A[k - width + 1];
    const int64_t b =
        3 * B[k] + 2 * B[k + 1] + 2 * B[k - width] + B[k - width + 1];
    const int64_t v =
        (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
  }
  i = height - 1;
  j = width - 1;
  {
    const int k = i * width + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int64_t a =
        3 * A[k] + 2 * A[k - 1] + 2 * A[k - width] + A[k - width - 1];
    const int64_t b =
        3 * B[k] + 2 * B[k - 1] + 2 * B[k - width] + B[k - width - 1];
    const int64_t v =
        (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
  }
  i = 0;
  for (j = 1; j < width - 1; ++j) {
    const int k = i * width + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int64_t a = A[k] + 2 * (A[k - 1] + A[k + 1]) + A[k + width] +
                      A[k + width - 1] + A[k + width + 1];
    const int64_t b = B[k] + 2 * (B[k - 1] + B[k + 1]) + B[k + width] +
                      B[k + width - 1] + B[k + width + 1];
    const int64_t v =
        (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
  }
  i = height - 1;
  for (j = 1; j < width - 1; ++j) {
    const int k = i * width + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int64_t a = A[k] + 2 * (A[k - 1] + A[k + 1]) + A[k - width] +
                      A[k - width - 1] + A[k - width + 1];
    const int64_t b = B[k] + 2 * (B[k - 1] + B[k + 1]) + B[k - width] +
                      B[k - width - 1] + B[k - width + 1];
    const int64_t v =
        (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
  }
  j = 0;
  for (i = 1; i < height - 1; ++i) {
    const int k = i * width + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int64_t a = A[k] + 2 * (A[k - width] + A[k + width]) + A[k + 1] +
                      A[k - width + 1] + A[k + width + 1];
    const int64_t b = B[k] + 2 * (B[k - width] + B[k + width]) + B[k + 1] +
                      B[k - width + 1] + B[k + width + 1];
    const int64_t v =
        (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
  }
  j = width - 1;
  for (i = 1; i < height - 1; ++i) {
    const int k = i * width + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int64_t a = A[k] + 2 * (A[k - width] + A[k + width]) + A[k - 1] +
                      A[k - width - 1] + A[k + width - 1];
    const int64_t b = B[k] + 2 * (B[k - width] + B[k + width]) + B[k - 1] +
                      B[k - width - 1] + B[k + width - 1];
    const int64_t v =
        (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
  }
  for (i = 1; i < height - 1; ++i) {
    for (j = 1; j < width - 1; ++j) {
      const int k = i * width + j;
      const int l = i * stride + j;
      const int nb = 5;
      const int64_t a =
          (A[k] + A[k - 1] + A[k + 1] + A[k - width] + A[k + width]) * 4 +
          (A[k - 1 - width] + A[k - 1 + width] + A[k + 1 - width] +
           A[k + 1 + width]) *
              3;
      const int64_t b =
          (B[k] + B[k - 1] + B[k + 1] + B[k - width] + B[k + width]) * 4 +
          (B[k - 1 - width] + B[k - 1 + width] + B[k + 1 - width] +
           B[k + 1 + width]) *
              3;
      const int64_t v =
          (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
      dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
    }
  }
#else
  if (r > 1) boxnum(width, height, r = 1, num, width);
  boxsum(A, width, height, width, r, 0, A, width, T, width);
  boxsum(B, width, height, width, r, 0, B, width, T, width);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int k = i * width + j;
      const int l = i * stride + j;
      const int n = num[k];
      const int64_t v =
          (((A[k] * dgd[l] + B[k]) << SGRPROJ_RST_BITS) + (n >> 1)) / n;
      dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
    }
  }
#endif  // APPROXIMATE_SGR
}

static void apply_selfguided_restoration(int64_t *dat, int width, int height,
                                         int stride, int bit_depth, int eps,
                                         int *xqd, void *tmpbuf) {
  int xq[2];
  int64_t *flt1 = (int64_t *)tmpbuf;
  int64_t *flt2 = flt1 + RESTORATION_TILEPELS_MAX;
  uint8_t *tmpbuf2 = (uint8_t *)(flt2 + RESTORATION_TILEPELS_MAX);
  int i, j;
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      assert(i * width + j < RESTORATION_TILEPELS_MAX);
      flt1[i * width + j] = dat[i * stride + j];
      flt2[i * width + j] = dat[i * stride + j];
    }
  }
  av1_selfguided_restoration(flt1, width, height, width, bit_depth,
                             sgr_params[eps].r1, sgr_params[eps].e1, tmpbuf2);
  av1_selfguided_restoration(flt2, width, height, width, bit_depth,
                             sgr_params[eps].r2, sgr_params[eps].e2, tmpbuf2);
  decode_xq(xqd, xq);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int k = i * width + j;
      const int l = i * stride + j;
      const int64_t u = ((int64_t)dat[l] << SGRPROJ_RST_BITS);
      const int64_t f1 = (int64_t)flt1[k] - u;
      const int64_t f2 = (int64_t)flt2[k] - u;
      const int64_t v = xq[0] * f1 + xq[1] * f2 + (u << SGRPROJ_PRJ_BITS);
      const int16_t w =
          (int16_t)ROUND_POWER_OF_TWO(v, SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);
      dat[l] = w;
    }
  }
}

static void loop_sgrproj_filter_tile(uint8_t *data, int tile_idx, int width,
                                     int height, int stride,
                                     RestorationInternal *rst, void *tmpbuf,
                                     uint8_t *dst, int dst_stride) {
  const int tile_width = rst->tile_width >> rst->subsampling_x;
  const int tile_height = rst->tile_height >> rst->subsampling_y;
  int i, j;
  int h_start, h_end, v_start, v_end;
  uint8_t *data_p, *dst_p;
  int64_t *dat = (int64_t *)tmpbuf;
  tmpbuf = (uint8_t *)tmpbuf + RESTORATION_TILEPELS_MAX * sizeof(*dat);

  if (rst->rsi->sgrproj_info[tile_idx].level == 0) {
    loop_copy_tile(data, tile_idx, 0, 0, width, height, stride, rst, dst,
                   dst_stride);
    return;
  }
  av1_get_rest_tile_limits(tile_idx, 0, 0, rst->nhtiles, rst->nvtiles,
                           tile_width, tile_height, width, height, 0, 0,
                           &h_start, &h_end, &v_start, &v_end);
  data_p = data + h_start + v_start * stride;
  for (i = 0; i < (v_end - v_start); ++i) {
    for (j = 0; j < (h_end - h_start); ++j) {
      dat[i * (h_end - h_start) + j] = data_p[i * stride + j];
    }
  }
  apply_selfguided_restoration(dat, h_end - h_start, v_end - v_start,
                               h_end - h_start, 8,
                               rst->rsi->sgrproj_info[tile_idx].ep,
                               rst->rsi->sgrproj_info[tile_idx].xqd, tmpbuf);
  dst_p = dst + h_start + v_start * dst_stride;
  for (i = 0; i < (v_end - v_start); ++i) {
    for (j = 0; j < (h_end - h_start); ++j) {
      dst_p[i * dst_stride + j] =
          clip_pixel((int)dat[i * (h_end - h_start) + j]);
    }
  }
}

static void loop_sgrproj_filter(uint8_t *data, int width, int height,
                                int stride, RestorationInternal *rst,
                                uint8_t *tmpdata, int tmpstride, uint8_t *dst,
                                int dst_stride) {
  int tile_idx;
  uint8_t *tmpbuf = aom_malloc(SGRPROJ_TMPBUF_SIZE);
  (void)tmpdata;
  (void)tmpstride;
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    loop_sgrproj_filter_tile(data, tile_idx, width, height, stride, rst, tmpbuf,
                             dst, dst_stride);
  }
  aom_free(tmpbuf);
}

static void apply_domaintxfmrf_hor(int iter, int param, uint8_t *img, int width,
                                   int height, int img_stride, int32_t *dat,
                                   int dat_stride) {
  int i, j;
  for (i = 0; i < height; ++i) {
    uint8_t *ip = &img[i * img_stride];
    int32_t *dp = &dat[i * dat_stride];
    *dp *= DOMAINTXFMRF_VTABLE_PREC;
    dp++;
    ip++;
    // left to right
    for (j = 1; j < width; ++j, dp++, ip++) {
      const int v = domaintxfmrf_vtable[iter][param][abs(ip[0] - ip[-1])];
      dp[0] = dp[0] * (DOMAINTXFMRF_VTABLE_PREC - v) +
              ((v * dp[-1] + DOMAINTXFMRF_VTABLE_PREC / 2) >>
               DOMAINTXFMRF_VTABLE_PRECBITS);
    }
    // right to left
    dp -= 2;
    ip -= 2;
    for (j = width - 2; j >= 0; --j, dp--, ip--) {
      const int v = domaintxfmrf_vtable[iter][param][abs(ip[1] - ip[0])];
      dp[0] = (dp[0] * (DOMAINTXFMRF_VTABLE_PREC - v) + v * dp[1] +
               DOMAINTXFMRF_VTABLE_PREC / 2) >>
              DOMAINTXFMRF_VTABLE_PRECBITS;
    }
  }
}

static void apply_domaintxfmrf_ver(int iter, int param, uint8_t *img, int width,
                                   int height, int img_stride, int32_t *dat,
                                   int dat_stride) {
  int i, j;
  for (j = 0; j < width; ++j) {
    uint8_t *ip = &img[j];
    int32_t *dp = &dat[j];
    dp += dat_stride;
    ip += img_stride;
    // top to bottom
    for (i = 1; i < height; ++i, dp += dat_stride, ip += img_stride) {
      const int v =
          domaintxfmrf_vtable[iter][param][abs(ip[0] - ip[-img_stride])];
      dp[0] = (dp[0] * (DOMAINTXFMRF_VTABLE_PREC - v) +
               (dp[-dat_stride] * v + DOMAINTXFMRF_VTABLE_PREC / 2)) >>
              DOMAINTXFMRF_VTABLE_PRECBITS;
    }
    // bottom to top
    dp -= 2 * dat_stride;
    ip -= 2 * img_stride;
    for (i = height - 2; i >= 0; --i, dp -= dat_stride, ip -= img_stride) {
      const int v =
          domaintxfmrf_vtable[iter][param][abs(ip[img_stride] - ip[0])];
      dp[0] = (dp[0] * (DOMAINTXFMRF_VTABLE_PREC - v) + dp[dat_stride] * v +
               DOMAINTXFMRF_VTABLE_PREC / 2) >>
              DOMAINTXFMRF_VTABLE_PRECBITS;
    }
  }
}

static void apply_domaintxfmrf_reduce_prec(int32_t *dat, int width, int height,
                                           int dat_stride) {
  int i, j;
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      dat[i * dat_stride + j] = ROUND_POWER_OF_TWO_SIGNED(
          dat[i * dat_stride + j], DOMAINTXFMRF_VTABLE_PRECBITS);
    }
  }
}

void av1_domaintxfmrf_restoration(uint8_t *dgd, int width, int height,
                                  int stride, int param, uint8_t *dst,
                                  int dst_stride) {
  int32_t dat[RESTORATION_TILEPELS_MAX];
  int i, j, t;
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      dat[i * width + j] = dgd[i * stride + j];
    }
  }
  for (t = 0; t < DOMAINTXFMRF_ITERS; ++t) {
    apply_domaintxfmrf_hor(t, param, dgd, width, height, stride, dat, width);
    apply_domaintxfmrf_ver(t, param, dgd, width, height, stride, dat, width);
    apply_domaintxfmrf_reduce_prec(dat, width, height, width);
  }
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      dst[i * dst_stride + j] = clip_pixel(dat[i * width + j]);
    }
  }
}

static void loop_domaintxfmrf_filter_tile(uint8_t *data, int tile_idx,
                                          int width, int height, int stride,
                                          RestorationInternal *rst,
                                          uint8_t *dst, int dst_stride) {
  const int tile_width = rst->tile_width >> rst->subsampling_x;
  const int tile_height = rst->tile_height >> rst->subsampling_y;
  int h_start, h_end, v_start, v_end;

  if (rst->rsi->domaintxfmrf_info[tile_idx].level == 0) {
    loop_copy_tile(data, tile_idx, 0, 0, width, height, stride, rst, dst,
                   dst_stride);
    return;
  }
  av1_get_rest_tile_limits(tile_idx, 0, 0, rst->nhtiles, rst->nvtiles,
                           tile_width, tile_height, width, height, 0, 0,
                           &h_start, &h_end, &v_start, &v_end);
  av1_domaintxfmrf_restoration(
      data + h_start + v_start * stride, h_end - h_start, v_end - v_start,
      stride, rst->rsi->domaintxfmrf_info[tile_idx].sigma_r,
      dst + h_start + v_start * dst_stride, dst_stride);
}

static void loop_domaintxfmrf_filter(uint8_t *data, int width, int height,
                                     int stride, RestorationInternal *rst,
                                     uint8_t *tmpdata, int tmpstride,
                                     uint8_t *dst, int dst_stride) {
  int tile_idx;
  (void)tmpdata;
  (void)tmpstride;
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    loop_domaintxfmrf_filter_tile(data, tile_idx, width, height, stride, rst,
                                  dst, dst_stride);
  }
}

static void loop_switchable_filter(uint8_t *data, int width, int height,
                                   int stride, RestorationInternal *rst,
                                   uint8_t *tmpdata, int tmpstride,
                                   uint8_t *dst, int dst_stride) {
  int i, tile_idx;
  uint8_t *tmpbuf = aom_malloc(SGRPROJ_TMPBUF_SIZE);
  // Initialize tmp buffer
  uint8_t *data_p = data;
  uint8_t *tmpdata_p = tmpdata;
  for (i = 0; i < height; ++i) {
    memcpy(tmpdata_p, data_p, sizeof(*data_p) * width);
    data_p += stride;
    tmpdata_p += tmpstride;
  }
  copy_border(data, width, height, stride, dst, dst_stride);
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    if (rst->rsi->restoration_type[tile_idx] == RESTORE_NONE) {
      loop_copy_tile(data, tile_idx, 0, 0, width, height, stride, rst, dst,
                     dst_stride);
    } else if (rst->rsi->restoration_type[tile_idx] == RESTORE_WIENER) {
      loop_wiener_filter_tile(data, tile_idx, width, height, stride, rst,
                              tmpdata, tmpstride, dst, dst_stride);
    } else if (rst->rsi->restoration_type[tile_idx] == RESTORE_SGRPROJ) {
      loop_sgrproj_filter_tile(data, tile_idx, width, height, stride, rst,
                               tmpbuf, dst, dst_stride);
    } else if (rst->rsi->restoration_type[tile_idx] == RESTORE_DOMAINTXFMRF) {
      loop_domaintxfmrf_filter_tile(data, tile_idx, width, height, stride, rst,
                                    dst, dst_stride);
    }
  }
  aom_free(tmpbuf);
}

#if CONFIG_AOM_HIGHBITDEPTH
static void copy_border_highbd(uint16_t *data, int width, int height,
                               int stride, uint16_t *dst, int dst_stride) {
  int i;
  for (i = RESTORATION_HALFWIN; i < height - RESTORATION_HALFWIN; ++i) {
    memcpy(dst + i * dst_stride, data + i * stride,
           RESTORATION_HALFWIN * sizeof(*dst));
    memcpy(dst + i * dst_stride + (width - RESTORATION_HALFWIN),
           data + i * stride + (width - RESTORATION_HALFWIN),
           RESTORATION_HALFWIN * sizeof(*dst));
  }
  for (i = 0; i < RESTORATION_HALFWIN; ++i) {
    memcpy(dst + i * dst_stride, data + i * stride, width * sizeof(*dst));
  }
  for (i = height - RESTORATION_HALFWIN; i < height; ++i)
    memcpy(dst + i * dst_stride, data + i * stride, width * sizeof(*dst));
}

static void loop_copy_tile_highbd(uint16_t *data, int tile_idx, int subtile_idx,
                                  int subtile_bits, int width, int height,
                                  int stride, RestorationInternal *rst,
                                  uint16_t *dst, int dst_stride) {
  const int tile_width = rst->tile_width >> rst->subsampling_x;
  const int tile_height = rst->tile_height >> rst->subsampling_y;
  int i;
  int h_start, h_end, v_start, v_end;
  av1_get_rest_tile_limits(tile_idx, subtile_idx, subtile_bits, rst->nhtiles,
                           rst->nvtiles, tile_width, tile_height, width, height,
                           0, 0, &h_start, &h_end, &v_start, &v_end);
  for (i = v_start; i < v_end; ++i)
    memcpy(dst + i * dst_stride + h_start, data + i * stride + h_start,
           (h_end - h_start) * sizeof(*dst));
}

uint16_t hor_sym_filter_highbd(uint16_t *d, int *hfilter, int bd) {
  int32_t s =
      (1 << (RESTORATION_FILT_BITS - 1)) + d[0] * hfilter[RESTORATION_HALFWIN];
  int i;
  for (i = 1; i <= RESTORATION_HALFWIN; ++i)
    s += (d[i] + d[-i]) * hfilter[RESTORATION_HALFWIN + i];
  return clip_pixel_highbd(s >> RESTORATION_FILT_BITS, bd);
}

uint16_t ver_sym_filter_highbd(uint16_t *d, int stride, int *vfilter, int bd) {
  int32_t s =
      (1 << (RESTORATION_FILT_BITS - 1)) + d[0] * vfilter[RESTORATION_HALFWIN];
  int i;
  for (i = 1; i <= RESTORATION_HALFWIN; ++i)
    s += (d[i * stride] + d[-i * stride]) * vfilter[RESTORATION_HALFWIN + i];
  return clip_pixel_highbd(s >> RESTORATION_FILT_BITS, bd);
}

static void loop_wiener_filter_tile_highbd(uint16_t *data, int tile_idx,
                                           int width, int height, int stride,
                                           RestorationInternal *rst,
                                           uint16_t *tmpdata, int tmpstride,
                                           int bit_depth, uint16_t *dst,
                                           int dst_stride) {
  const int tile_width = rst->tile_width >> rst->subsampling_x;
  const int tile_height = rst->tile_height >> rst->subsampling_y;
  int h_start, h_end, v_start, v_end;
  int i, j;
  uint16_t *data_p, *tmpdata_p, *dst_p;

  if (rst->rsi->wiener_info[tile_idx].level == 0) {
    loop_copy_tile_highbd(data, tile_idx, 0, 0, width, height, stride, rst, dst,
                          dst_stride);
    return;
  }
  // Filter row-wise
  av1_get_rest_tile_limits(tile_idx, 0, 0, rst->nhtiles, rst->nvtiles,
                           tile_width, tile_height, width, height, 1, 0,
                           &h_start, &h_end, &v_start, &v_end);
  data_p = data + h_start + v_start * stride;
  tmpdata_p = tmpdata + h_start + v_start * tmpstride;
  for (i = 0; i < (v_end - v_start); ++i) {
    for (j = 0; j < (h_end - h_start); ++j) {
      *tmpdata_p++ = hor_sym_filter_highbd(
          data_p++, rst->rsi->wiener_info[tile_idx].hfilter, bit_depth);
    }
    data_p += stride - (h_end - h_start);
    tmpdata_p += tmpstride - (h_end - h_start);
  }
  // Filter col-wise
  av1_get_rest_tile_limits(tile_idx, 0, 0, rst->nhtiles, rst->nvtiles,
                           tile_width, tile_height, width, height, 0, 1,
                           &h_start, &h_end, &v_start, &v_end);
  dst_p = dst + h_start + v_start * dst_stride;
  tmpdata_p = tmpdata + h_start + v_start * tmpstride;
  for (i = 0; i < (v_end - v_start); ++i) {
    for (j = 0; j < (h_end - h_start); ++j) {
      *dst_p++ = ver_sym_filter_highbd(tmpdata_p++, tmpstride,
                                       rst->rsi->wiener_info[tile_idx].vfilter,
                                       bit_depth);
    }
    dst_p += dst_stride - (h_end - h_start);
    tmpdata_p += tmpstride - (h_end - h_start);
  }
}

static void loop_wiener_filter_highbd(uint8_t *data8, int width, int height,
                                      int stride, RestorationInternal *rst,
                                      uint8_t *tmpdata8, int tmpstride,
                                      int bit_depth, uint8_t *dst8,
                                      int dst_stride) {
  uint16_t *data = CONVERT_TO_SHORTPTR(data8);
  uint16_t *tmpdata = CONVERT_TO_SHORTPTR(tmpdata8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  int tile_idx, i;

  // Initialize tmp buffer
  uint16_t *data_p = data;
  uint16_t *tmpdata_p = tmpdata;
  for (i = 0; i < height; ++i) {
    memcpy(tmpdata_p, data_p, sizeof(*data_p) * width);
    data_p += stride;
    tmpdata_p += tmpstride;
  }
  copy_border_highbd(data, width, height, stride, dst, dst_stride);
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    loop_wiener_filter_tile_highbd(data, tile_idx, width, height, stride, rst,
                                   tmpdata, tmpstride, bit_depth, dst,
                                   dst_stride);
  }
}

static void loop_sgrproj_filter_tile_highbd(uint16_t *data, int tile_idx,
                                            int width, int height, int stride,
                                            RestorationInternal *rst,
                                            int bit_depth, void *tmpbuf,
                                            uint16_t *dst, int dst_stride) {
  const int tile_width = rst->tile_width >> rst->subsampling_x;
  const int tile_height = rst->tile_height >> rst->subsampling_y;
  int i, j;
  int h_start, h_end, v_start, v_end;
  uint16_t *data_p, *dst_p;
  int64_t *dat = (int64_t *)tmpbuf;
  tmpbuf = (uint8_t *)tmpbuf + RESTORATION_TILEPELS_MAX * sizeof(*dat);

  if (rst->rsi->sgrproj_info[tile_idx].level == 0) {
    loop_copy_tile_highbd(data, tile_idx, 0, 0, width, height, stride, rst, dst,
                          dst_stride);
    return;
  }
  av1_get_rest_tile_limits(tile_idx, 0, 0, rst->nhtiles, rst->nvtiles,
                           tile_width, tile_height, width, height, 0, 0,
                           &h_start, &h_end, &v_start, &v_end);
  data_p = data + h_start + v_start * stride;
  for (i = 0; i < (v_end - v_start); ++i) {
    for (j = 0; j < (h_end - h_start); ++j) {
      dat[i * (h_end - h_start) + j] = data_p[i * stride + j];
    }
  }
  apply_selfguided_restoration(dat, h_end - h_start, v_end - v_start,
                               h_end - h_start, bit_depth,
                               rst->rsi->sgrproj_info[tile_idx].ep,
                               rst->rsi->sgrproj_info[tile_idx].xqd, tmpbuf);
  dst_p = dst + h_start + v_start * dst_stride;
  for (i = 0; i < (v_end - v_start); ++i) {
    for (j = 0; j < (h_end - h_start); ++j) {
      dst_p[i * dst_stride + j] =
          clip_pixel_highbd((int)dat[i * (h_end - h_start) + j], bit_depth);
    }
  }
}

static void loop_sgrproj_filter_highbd(uint8_t *data8, int width, int height,
                                       int stride, RestorationInternal *rst,
                                       uint8_t *tmpdata8, int tmpstride,
                                       int bit_depth, uint8_t *dst8,
                                       int dst_stride) {
  int tile_idx;
  uint16_t *data = CONVERT_TO_SHORTPTR(data8);
  uint8_t *tmpbuf = aom_malloc(SGRPROJ_TMPBUF_SIZE);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  (void)tmpdata8;
  (void)tmpstride;
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    loop_sgrproj_filter_tile_highbd(data, tile_idx, width, height, stride, rst,
                                    bit_depth, tmpbuf, dst, dst_stride);
  }
  aom_free(tmpbuf);
}

static void apply_domaintxfmrf_hor_highbd(int iter, int param, uint16_t *img,
                                          int width, int height, int img_stride,
                                          int32_t *dat, int dat_stride,
                                          int bd) {
  const int shift = (bd - 8);
  int i, j;
  for (i = 0; i < height; ++i) {
    uint16_t *ip = &img[i * img_stride];
    int32_t *dp = &dat[i * dat_stride];
    *dp *= DOMAINTXFMRF_VTABLE_PREC;
    dp++;
    ip++;
    // left to right
    for (j = 1; j < width; ++j, dp++, ip++) {
      const int v =
          domaintxfmrf_vtable[iter][param]
                             [abs((ip[0] >> shift) - (ip[-1] >> shift))];
      dp[0] = dp[0] * (DOMAINTXFMRF_VTABLE_PREC - v) +
              ((v * dp[-1] + DOMAINTXFMRF_VTABLE_PREC / 2) >>
               DOMAINTXFMRF_VTABLE_PRECBITS);
    }
    // right to left
    dp -= 2;
    ip -= 2;
    for (j = width - 2; j >= 0; --j, dp--, ip--) {
      const int v =
          domaintxfmrf_vtable[iter][param]
                             [abs((ip[1] >> shift) - (ip[0] >> shift))];
      dp[0] = (dp[0] * (DOMAINTXFMRF_VTABLE_PREC - v) + v * dp[1] +
               DOMAINTXFMRF_VTABLE_PREC / 2) >>
              DOMAINTXFMRF_VTABLE_PRECBITS;
    }
  }
}

static void apply_domaintxfmrf_ver_highbd(int iter, int param, uint16_t *img,
                                          int width, int height, int img_stride,
                                          int32_t *dat, int dat_stride,
                                          int bd) {
  int i, j;
  const int shift = (bd - 8);
  for (j = 0; j < width; ++j) {
    uint16_t *ip = &img[j];
    int32_t *dp = &dat[j];
    dp += dat_stride;
    ip += img_stride;
    // top to bottom
    for (i = 1; i < height; ++i, dp += dat_stride, ip += img_stride) {
      const int v = domaintxfmrf_vtable[iter][param][abs(
          (ip[0] >> shift) - (ip[-img_stride] >> shift))];
      dp[0] = (dp[0] * (DOMAINTXFMRF_VTABLE_PREC - v) +
               (dp[-dat_stride] * v + DOMAINTXFMRF_VTABLE_PREC / 2)) >>
              DOMAINTXFMRF_VTABLE_PRECBITS;
    }
    // bottom to top
    dp -= 2 * dat_stride;
    ip -= 2 * img_stride;
    for (i = height - 2; i >= 0; --i, dp -= dat_stride, ip -= img_stride) {
      const int v = domaintxfmrf_vtable[iter][param][abs(
          (ip[img_stride] >> shift) - (ip[0] >> shift))];
      dp[0] = (dp[0] * (DOMAINTXFMRF_VTABLE_PREC - v) + dp[dat_stride] * v +
               DOMAINTXFMRF_VTABLE_PREC / 2) >>
              DOMAINTXFMRF_VTABLE_PRECBITS;
    }
  }
}

void av1_domaintxfmrf_restoration_highbd(uint16_t *dgd, int width, int height,
                                         int stride, int param, int bit_depth,
                                         uint16_t *dst, int dst_stride) {
  int32_t dat[RESTORATION_TILEPELS_MAX];
  int i, j, t;
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      dat[i * width + j] = dgd[i * stride + j];
    }
  }
  for (t = 0; t < DOMAINTXFMRF_ITERS; ++t) {
    apply_domaintxfmrf_hor_highbd(t, param, dgd, width, height, stride, dat,
                                  width, bit_depth);
    apply_domaintxfmrf_ver_highbd(t, param, dgd, width, height, stride, dat,
                                  width, bit_depth);
    apply_domaintxfmrf_reduce_prec(dat, width, height, width);
  }
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      dst[i * dst_stride + j] =
          clip_pixel_highbd(dat[i * width + j], bit_depth);
    }
  }
}

static void loop_domaintxfmrf_filter_tile_highbd(
    uint16_t *data, int tile_idx, int width, int height, int stride,
    RestorationInternal *rst, int bit_depth, uint16_t *dst, int dst_stride) {
  const int tile_width = rst->tile_width >> rst->subsampling_x;
  const int tile_height = rst->tile_height >> rst->subsampling_y;
  int h_start, h_end, v_start, v_end;

  if (rst->rsi->domaintxfmrf_info[tile_idx].level == 0) {
    loop_copy_tile_highbd(data, tile_idx, 0, 0, width, height, stride, rst, dst,
                          dst_stride);
    return;
  }
  av1_get_rest_tile_limits(tile_idx, 0, 0, rst->nhtiles, rst->nvtiles,
                           tile_width, tile_height, width, height, 0, 0,
                           &h_start, &h_end, &v_start, &v_end);
  av1_domaintxfmrf_restoration_highbd(
      data + h_start + v_start * stride, h_end - h_start, v_end - v_start,
      stride, rst->rsi->domaintxfmrf_info[tile_idx].sigma_r, bit_depth,
      dst + h_start + v_start * dst_stride, dst_stride);
}

static void loop_domaintxfmrf_filter_highbd(uint8_t *data8, int width,
                                            int height, int stride,
                                            RestorationInternal *rst,
                                            uint8_t *tmpdata, int tmpstride,
                                            int bit_depth, uint8_t *dst8,
                                            int dst_stride) {
  int tile_idx;
  uint16_t *data = CONVERT_TO_SHORTPTR(data8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  (void)tmpdata;
  (void)tmpstride;
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    loop_domaintxfmrf_filter_tile_highbd(data, tile_idx, width, height, stride,
                                         rst, bit_depth, dst, dst_stride);
  }
}

static void loop_switchable_filter_highbd(uint8_t *data8, int width, int height,
                                          int stride, RestorationInternal *rst,
                                          uint8_t *tmpdata8, int tmpstride,
                                          int bit_depth, uint8_t *dst8,
                                          int dst_stride) {
  uint16_t *data = CONVERT_TO_SHORTPTR(data8);
  uint16_t *tmpdata = CONVERT_TO_SHORTPTR(tmpdata8);
  uint8_t *tmpbuf = aom_malloc(SGRPROJ_TMPBUF_SIZE);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  int i, tile_idx;

  // Initialize tmp buffer
  uint16_t *data_p = data;
  uint16_t *tmpdata_p = tmpdata;
  for (i = 0; i < height; ++i) {
    memcpy(tmpdata_p, data_p, sizeof(*data_p) * width);
    data_p += stride;
    tmpdata_p += tmpstride;
  }
  copy_border_highbd(data, width, height, stride, dst, dst_stride);
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    if (rst->rsi->restoration_type[tile_idx] == RESTORE_NONE) {
      loop_copy_tile_highbd(data, tile_idx, 0, 0, width, height, stride, rst,
                            dst, dst_stride);
    } else if (rst->rsi->restoration_type[tile_idx] == RESTORE_WIENER) {
      loop_wiener_filter_tile_highbd(data, tile_idx, width, height, stride, rst,
                                     tmpdata, tmpstride, bit_depth, dst,
                                     dst_stride);
    } else if (rst->rsi->restoration_type[tile_idx] == RESTORE_SGRPROJ) {
      loop_sgrproj_filter_tile_highbd(data, tile_idx, width, height, stride,
                                      rst, bit_depth, tmpbuf, dst, dst_stride);
    } else if (rst->rsi->restoration_type[tile_idx] == RESTORE_DOMAINTXFMRF) {
      loop_domaintxfmrf_filter_tile_highbd(data, tile_idx, width, height,
                                           stride, rst, bit_depth, dst,
                                           dst_stride);
    }
  }
  aom_free(tmpbuf);
}
#endif  // CONFIG_AOM_HIGHBITDEPTH

void av1_loop_restoration_rows(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                               int start_mi_row, int end_mi_row, int y_only,
                               YV12_BUFFER_CONFIG *dst) {
  const int ywidth = frame->y_crop_width;
  const int ystride = frame->y_stride;
  const int uvwidth = frame->uv_crop_width;
  const int uvstride = frame->uv_stride;
  const int ystart = start_mi_row << MI_SIZE_LOG2;
  const int uvstart = ystart >> cm->subsampling_y;
  int yend = end_mi_row << MI_SIZE_LOG2;
  int uvend = yend >> cm->subsampling_y;
  restore_func_type restore_funcs[RESTORE_TYPES] = { NULL, loop_wiener_filter,
                                                     loop_sgrproj_filter,
                                                     loop_domaintxfmrf_filter,
                                                     loop_switchable_filter };
#if CONFIG_AOM_HIGHBITDEPTH
  restore_func_highbd_type restore_funcs_highbd[RESTORE_TYPES] = {
    NULL, loop_wiener_filter_highbd, loop_sgrproj_filter_highbd,
    loop_domaintxfmrf_filter_highbd, loop_switchable_filter_highbd
  };
#endif  // CONFIG_AOM_HIGHBITDEPTH
  restore_func_type restore_func =
      restore_funcs[cm->rst_internal.rsi->frame_restoration_type];
#if CONFIG_AOM_HIGHBITDEPTH
  restore_func_highbd_type restore_func_highbd =
      restore_funcs_highbd[cm->rst_internal.rsi->frame_restoration_type];
#endif  // CONFIG_AOM_HIGHBITDEPTH
  YV12_BUFFER_CONFIG tmp_buf;
  YV12_BUFFER_CONFIG dst_;

  yend = AOMMIN(yend, cm->height);
  uvend = AOMMIN(uvend, cm->subsampling_y ? (cm->height + 1) >> 1 : cm->height);

  if (cm->rst_internal.rsi->frame_restoration_type == RESTORE_NONE) {
    if (dst) {
      if (y_only)
        aom_yv12_copy_y(frame, dst);
      else
        aom_yv12_copy_frame(frame, dst);
    }
    return;
  }

  memset(&tmp_buf, 0, sizeof(YV12_BUFFER_CONFIG));

  if (aom_realloc_frame_buffer(
          &tmp_buf, cm->width, cm->height, cm->subsampling_x, cm->subsampling_y,
#if CONFIG_AOM_HIGHBITDEPTH
          cm->use_highbitdepth,
#endif
          AOM_BORDER_IN_PIXELS, cm->byte_alignment, NULL, NULL, NULL) < 0)
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate tmp restoration buffer");

  if (y_only == 0)
    y_only = override_y_only[cm->rst_internal.rsi->frame_restoration_type];
  if (!dst) {
    dst = &dst_;
    memset(dst, 0, sizeof(YV12_BUFFER_CONFIG));
    if (aom_realloc_frame_buffer(
            dst, cm->width, cm->height, cm->subsampling_x, cm->subsampling_y,
#if CONFIG_AOM_HIGHBITDEPTH
            cm->use_highbitdepth,
#endif
            AOM_BORDER_IN_PIXELS, cm->byte_alignment, NULL, NULL, NULL) < 0)
      aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                         "Failed to allocate restoration dst buffer");
  }

#if CONFIG_AOM_HIGHBITDEPTH
  if (cm->use_highbitdepth)
    restore_func_highbd(frame->y_buffer + ystart * ystride, ywidth,
                        yend - ystart, ystride, &cm->rst_internal,
                        tmp_buf.y_buffer + ystart * tmp_buf.y_stride,
                        tmp_buf.y_stride, cm->bit_depth,
                        dst->y_buffer + ystart * dst->y_stride, dst->y_stride);
  else
#endif  // CONFIG_AOM_HIGHBITDEPTH
    restore_func(frame->y_buffer + ystart * ystride, ywidth, yend - ystart,
                 ystride, &cm->rst_internal,
                 tmp_buf.y_buffer + ystart * tmp_buf.y_stride, tmp_buf.y_stride,
                 dst->y_buffer + ystart * dst->y_stride, dst->y_stride);
  if (!y_only) {
    cm->rst_internal.subsampling_x = cm->subsampling_x;
    cm->rst_internal.subsampling_y = cm->subsampling_y;
#if CONFIG_AOM_HIGHBITDEPTH
    if (cm->use_highbitdepth) {
      restore_func_highbd(frame->u_buffer + uvstart * uvstride, uvwidth,
                          uvend - uvstart, uvstride, &cm->rst_internal,
                          tmp_buf.u_buffer + uvstart * tmp_buf.uv_stride,
                          tmp_buf.uv_stride, cm->bit_depth,
                          dst->u_buffer + uvstart * dst->uv_stride,
                          dst->uv_stride);
      restore_func_highbd(frame->v_buffer + uvstart * uvstride, uvwidth,
                          uvend - uvstart, uvstride, &cm->rst_internal,
                          tmp_buf.v_buffer + uvstart * tmp_buf.uv_stride,
                          tmp_buf.uv_stride, cm->bit_depth,
                          dst->v_buffer + uvstart * dst->uv_stride,
                          dst->uv_stride);
    } else {
#endif  // CONFIG_AOM_HIGHBITDEPTH
      restore_func(frame->u_buffer + uvstart * uvstride, uvwidth,
                   uvend - uvstart, uvstride, &cm->rst_internal,
                   tmp_buf.u_buffer + uvstart * tmp_buf.uv_stride,
                   tmp_buf.uv_stride, dst->u_buffer + uvstart * dst->uv_stride,
                   dst->uv_stride);
      restore_func(frame->v_buffer + uvstart * uvstride, uvwidth,
                   uvend - uvstart, uvstride, &cm->rst_internal,
                   tmp_buf.v_buffer + uvstart * tmp_buf.uv_stride,
                   tmp_buf.uv_stride, dst->v_buffer + uvstart * dst->uv_stride,
                   dst->uv_stride);
#if CONFIG_AOM_HIGHBITDEPTH
    }
#endif  // CONFIG_AOM_HIGHBITDEPTH
  }

  aom_free_frame_buffer(&tmp_buf);
  if (dst == &dst_) {
    if (y_only)
      aom_yv12_copy_y(dst, frame);
    else
      aom_yv12_copy_frame(dst, frame);
    aom_free_frame_buffer(dst);
  }
}

void av1_loop_restoration_frame(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                                RestorationInfo *rsi, int y_only,
                                int partial_frame, YV12_BUFFER_CONFIG *dst) {
  int start_mi_row, end_mi_row, mi_rows_to_filter;
  if (rsi->frame_restoration_type != RESTORE_NONE) {
    start_mi_row = 0;
    mi_rows_to_filter = cm->mi_rows;
    if (partial_frame && cm->mi_rows > 8) {
      start_mi_row = cm->mi_rows >> 1;
      start_mi_row &= 0xfffffff8;
      mi_rows_to_filter = AOMMAX(cm->mi_rows / 8, 8);
    }
    end_mi_row = start_mi_row + mi_rows_to_filter;
    av1_loop_restoration_init(&cm->rst_internal, rsi,
                              cm->frame_type == KEY_FRAME, cm->width,
                              cm->height);
    av1_loop_restoration_rows(frame, cm, start_mi_row, end_mi_row, y_only, dst);
  }
}
