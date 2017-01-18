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

static const int domaintxfmrf_params[DOMAINTXFMRF_PARAMS] = {
  32,  40,  48,  56,  64,  68,  72,  76,  80,  82,  84,  86,  88,
  90,  92,  94,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105,
  106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118,
  119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 130, 132, 134,
  136, 138, 140, 142, 146, 150, 154, 158, 162, 166, 170, 174
};

const sgr_params_type sgr_params[SGRPROJ_PARAMS] = {
  // r1, eps1, r2, eps2
  { 2, 25, 1, 11 }, { 2, 35, 1, 12 }, { 2, 45, 1, 13 }, { 2, 55, 1, 14 },
  { 2, 65, 1, 15 }, { 3, 50, 2, 25 }, { 3, 60, 2, 35 }, { 3, 70, 2, 45 },
};

typedef void (*restore_func_type)(uint8_t *data8, int width, int height,
                                  int stride, RestorationInternal *rst,
                                  uint8_t *dst8, int dst_stride);
#if CONFIG_AOM_HIGHBITDEPTH
typedef void (*restore_func_highbd_type)(uint8_t *data8, int width, int height,
                                         int stride, RestorationInternal *rst,
                                         int bit_depth, uint8_t *dst8,
                                         int dst_stride);
#endif  // CONFIG_AOM_HIGHBITDEPTH

int av1_alloc_restoration_struct(RestorationInfo *rst_info, int width,
                                 int height) {
  const int ntiles = av1_get_rest_ntiles(width, height, NULL, NULL, NULL, NULL);
  rst_info->restoration_type = (RestorationType *)aom_realloc(
      rst_info->restoration_type, sizeof(*rst_info->restoration_type) * ntiles);
  rst_info->wiener_info = (WienerInfo *)aom_realloc(
      rst_info->wiener_info, sizeof(*rst_info->wiener_info) * ntiles);
  assert(rst_info->wiener_info != NULL);
  rst_info->sgrproj_info = (SgrprojInfo *)aom_realloc(
      rst_info->sgrproj_info, sizeof(*rst_info->sgrproj_info) * ntiles);
  assert(rst_info->sgrproj_info != NULL);
  rst_info->domaintxfmrf_info = (DomaintxfmrfInfo *)aom_realloc(
      rst_info->domaintxfmrf_info,
      sizeof(*rst_info->domaintxfmrf_info) * ntiles);
  assert(rst_info->domaintxfmrf_info != NULL);
  return ntiles;
}

void av1_free_restoration_struct(RestorationInfo *rst_info) {
  aom_free(rst_info->restoration_type);
  rst_info->restoration_type = NULL;
  aom_free(rst_info->wiener_info);
  rst_info->wiener_info = NULL;
  aom_free(rst_info->sgrproj_info);
  rst_info->sgrproj_info = NULL;
  aom_free(rst_info->domaintxfmrf_info);
  rst_info->domaintxfmrf_info = NULL;
}

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

static void loop_restoration_init(RestorationInternal *rst, int kf) {
  rst->keyframe = kf;
}

void extend_frame(uint8_t *data, int width, int height, int stride) {
  uint8_t *data_p;
  int i;
  for (i = 0; i < height; ++i) {
    data_p = data + i * stride;
    memset(data_p - WIENER_HALFWIN, data_p[0], WIENER_HALFWIN);
    memset(data_p + width, data_p[width - 1], WIENER_HALFWIN);
  }
  data_p = data - WIENER_HALFWIN;
  for (i = -WIENER_HALFWIN; i < 0; ++i) {
    memcpy(data_p + i * stride, data_p, width + 2 * WIENER_HALFWIN);
  }
  for (i = height; i < height + WIENER_HALFWIN; ++i) {
    memcpy(data_p + i * stride, data_p + (height - 1) * stride,
           width + 2 * WIENER_HALFWIN);
  }
}

static void loop_copy_tile(uint8_t *data, int tile_idx, int subtile_idx,
                           int subtile_bits, int width, int height, int stride,
                           RestorationInternal *rst, uint8_t *dst,
                           int dst_stride) {
  const int tile_width = rst->tile_width;
  const int tile_height = rst->tile_height;
  int i;
  int h_start, h_end, v_start, v_end;
  av1_get_rest_tile_limits(tile_idx, subtile_idx, subtile_bits, rst->nhtiles,
                           rst->nvtiles, tile_width, tile_height, width, height,
                           0, 0, &h_start, &h_end, &v_start, &v_end);
  for (i = v_start; i < v_end; ++i)
    memcpy(dst + i * dst_stride + h_start, data + i * stride + h_start,
           h_end - h_start);
}

static void loop_wiener_filter_tile(uint8_t *data, int tile_idx, int width,
                                    int height, int stride,
                                    RestorationInternal *rst, uint8_t *dst,
                                    int dst_stride) {
  const int tile_width = rst->tile_width;
  const int tile_height = rst->tile_height;
  int i, j;
  int h_start, h_end, v_start, v_end;
  DECLARE_ALIGNED(16, InterpKernel, hkernel);
  DECLARE_ALIGNED(16, InterpKernel, vkernel);

  if (rst->rsi->restoration_type[tile_idx] == RESTORE_NONE) {
    loop_copy_tile(data, tile_idx, 0, 0, width, height, stride, rst, dst,
                   dst_stride);
    return;
  }
  // TODO(david.barker): Store hfilter/vfilter as an InterpKernel
  // instead of the current format. Then this can be removed.
  assert(WIENER_WIN == SUBPEL_TAPS - 1);
  for (i = 0; i < WIENER_WIN; ++i) {
    hkernel[i] = rst->rsi->wiener_info[tile_idx].hfilter[i];
    vkernel[i] = rst->rsi->wiener_info[tile_idx].vfilter[i];
  }
  hkernel[WIENER_WIN] = 0;
  vkernel[WIENER_WIN] = 0;
  hkernel[3] -= 128;
  vkernel[3] -= 128;
  av1_get_rest_tile_limits(tile_idx, 0, 0, rst->nhtiles, rst->nvtiles,
                           tile_width, tile_height, width, height, 0, 0,
                           &h_start, &h_end, &v_start, &v_end);
  // Convolve the whole tile (done in blocks here to match the requirements
  // of the vectorized convolve functions, but the result is equivalent)
  for (i = v_start; i < v_end; i += MAX_SB_SIZE)
    for (j = h_start; j < h_end; j += MAX_SB_SIZE) {
      int w = AOMMIN(MAX_SB_SIZE, (h_end - j + 15) & ~15);
      int h = AOMMIN(MAX_SB_SIZE, (v_end - i + 15) & ~15);
      const uint8_t *data_p = data + i * stride + j;
      uint8_t *dst_p = dst + i * dst_stride + j;
      aom_convolve8_add_src(data_p, stride, dst_p, dst_stride, hkernel, 16,
                            vkernel, 16, w, h);
    }
}

static void loop_wiener_filter(uint8_t *data, int width, int height, int stride,
                               RestorationInternal *rst, uint8_t *dst,
                               int dst_stride) {
  int tile_idx;
  extend_frame(data, width, height, stride);
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    loop_wiener_filter_tile(data, tile_idx, width, height, stride, rst, dst,
                            dst_stride);
  }
}

/* Calculate windowed sums (if sqr=0) or sums of squares (if sqr=1)
   over the input. The window is of size (2r + 1)x(2r + 1), and we
   specialize to r = 1, 2, 3. A default function is used for r > 3.

   Each loop follows the same format: We keep a window's worth of input
   in individual variables and select data out of that as appropriate.
*/
static void boxsum1(int32_t *src, int width, int height, int src_stride,
                    int sqr, int32_t *dst, int dst_stride) {
  int i, j, a, b, c;

  // Vertical sum over 3-pixel regions, from src into dst.
  if (!sqr) {
    for (j = 0; j < width; ++j) {
      a = src[j];
      b = src[src_stride + j];
      c = src[2 * src_stride + j];

      dst[j] = a + b;
      for (i = 1; i < height - 2; ++i) {
        // Loop invariant: At the start of each iteration,
        // a = src[(i - 1) * src_stride + j]
        // b = src[(i    ) * src_stride + j]
        // c = src[(i + 1) * src_stride + j]
        dst[i * dst_stride + j] = a + b + c;
        a = b;
        b = c;
        c = src[(i + 2) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c;
      dst[(i + 1) * dst_stride + j] = b + c;
    }
  } else {
    for (j = 0; j < width; ++j) {
      a = src[j] * src[j];
      b = src[src_stride + j] * src[src_stride + j];
      c = src[2 * src_stride + j] * src[2 * src_stride + j];

      dst[j] = a + b;
      for (i = 1; i < height - 2; ++i) {
        dst[i * dst_stride + j] = a + b + c;
        a = b;
        b = c;
        c = src[(i + 2) * src_stride + j] * src[(i + 2) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c;
      dst[(i + 1) * dst_stride + j] = b + c;
    }
  }

  // Horizontal sum over 3-pixel regions of dst
  for (i = 0; i < height; ++i) {
    a = dst[i * dst_stride];
    b = dst[i * dst_stride + 1];
    c = dst[i * dst_stride + 2];

    dst[i * dst_stride] = a + b;
    for (j = 1; j < width - 2; ++j) {
      // Loop invariant: At the start of each iteration,
      // a = src[i * src_stride + (j - 1)]
      // b = src[i * src_stride + (j    )]
      // c = src[i * src_stride + (j + 1)]
      dst[i * dst_stride + j] = a + b + c;
      a = b;
      b = c;
      c = dst[i * dst_stride + (j + 2)];
    }
    dst[i * dst_stride + j] = a + b + c;
    dst[i * dst_stride + (j + 1)] = b + c;
  }
}

static void boxsum2(int32_t *src, int width, int height, int src_stride,
                    int sqr, int32_t *dst, int dst_stride) {
  int i, j, a, b, c, d, e;

  // Vertical sum over 5-pixel regions, from src into dst.
  if (!sqr) {
    for (j = 0; j < width; ++j) {
      a = src[j];
      b = src[src_stride + j];
      c = src[2 * src_stride + j];
      d = src[3 * src_stride + j];
      e = src[4 * src_stride + j];

      dst[j] = a + b + c;
      dst[dst_stride + j] = a + b + c + d;
      for (i = 2; i < height - 3; ++i) {
        // Loop invariant: At the start of each iteration,
        // a = src[(i - 2) * src_stride + j]
        // b = src[(i - 1) * src_stride + j]
        // c = src[(i    ) * src_stride + j]
        // d = src[(i + 1) * src_stride + j]
        // e = src[(i + 2) * src_stride + j]
        dst[i * dst_stride + j] = a + b + c + d + e;
        a = b;
        b = c;
        c = d;
        d = e;
        e = src[(i + 3) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e;
      dst[(i + 1) * dst_stride + j] = b + c + d + e;
      dst[(i + 2) * dst_stride + j] = c + d + e;
    }
  } else {
    for (j = 0; j < width; ++j) {
      a = src[j] * src[j];
      b = src[src_stride + j] * src[src_stride + j];
      c = src[2 * src_stride + j] * src[2 * src_stride + j];
      d = src[3 * src_stride + j] * src[3 * src_stride + j];
      e = src[4 * src_stride + j] * src[4 * src_stride + j];

      dst[j] = a + b + c;
      dst[dst_stride + j] = a + b + c + d;
      for (i = 2; i < height - 3; ++i) {
        dst[i * dst_stride + j] = a + b + c + d + e;
        a = b;
        b = c;
        c = d;
        d = e;
        e = src[(i + 3) * src_stride + j] * src[(i + 3) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e;
      dst[(i + 1) * dst_stride + j] = b + c + d + e;
      dst[(i + 2) * dst_stride + j] = c + d + e;
    }
  }

  // Horizontal sum over 5-pixel regions of dst
  for (i = 0; i < height; ++i) {
    a = dst[i * dst_stride];
    b = dst[i * dst_stride + 1];
    c = dst[i * dst_stride + 2];
    d = dst[i * dst_stride + 3];
    e = dst[i * dst_stride + 4];

    dst[i * dst_stride] = a + b + c;
    dst[i * dst_stride + 1] = a + b + c + d;
    for (j = 2; j < width - 3; ++j) {
      // Loop invariant: At the start of each iteration,
      // a = src[i * src_stride + (j - 2)]
      // b = src[i * src_stride + (j - 1)]
      // c = src[i * src_stride + (j    )]
      // d = src[i * src_stride + (j + 1)]
      // e = src[i * src_stride + (j + 2)]
      dst[i * dst_stride + j] = a + b + c + d + e;
      a = b;
      b = c;
      c = d;
      d = e;
      e = dst[i * dst_stride + (j + 3)];
    }
    dst[i * dst_stride + j] = a + b + c + d + e;
    dst[i * dst_stride + (j + 1)] = b + c + d + e;
    dst[i * dst_stride + (j + 2)] = c + d + e;
  }
}

static void boxsum3(int32_t *src, int width, int height, int src_stride,
                    int sqr, int32_t *dst, int dst_stride) {
  int i, j, a, b, c, d, e, f, g;

  // Vertical sum over 7-pixel regions, from src into dst.
  if (!sqr) {
    for (j = 0; j < width; ++j) {
      a = src[j];
      b = src[1 * src_stride + j];
      c = src[2 * src_stride + j];
      d = src[3 * src_stride + j];
      e = src[4 * src_stride + j];
      f = src[5 * src_stride + j];
      g = src[6 * src_stride + j];

      dst[j] = a + b + c + d;
      dst[dst_stride + j] = a + b + c + d + e;
      dst[2 * dst_stride + j] = a + b + c + d + e + f;
      for (i = 3; i < height - 4; ++i) {
        dst[i * dst_stride + j] = a + b + c + d + e + f + g;
        a = b;
        b = c;
        c = d;
        d = e;
        e = f;
        f = g;
        g = src[(i + 4) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e + f + g;
      dst[(i + 1) * dst_stride + j] = b + c + d + e + f + g;
      dst[(i + 2) * dst_stride + j] = c + d + e + f + g;
      dst[(i + 3) * dst_stride + j] = d + e + f + g;
    }
  } else {
    for (j = 0; j < width; ++j) {
      a = src[j] * src[j];
      b = src[1 * src_stride + j] * src[1 * src_stride + j];
      c = src[2 * src_stride + j] * src[2 * src_stride + j];
      d = src[3 * src_stride + j] * src[3 * src_stride + j];
      e = src[4 * src_stride + j] * src[4 * src_stride + j];
      f = src[5 * src_stride + j] * src[5 * src_stride + j];
      g = src[6 * src_stride + j] * src[6 * src_stride + j];

      dst[j] = a + b + c + d;
      dst[dst_stride + j] = a + b + c + d + e;
      dst[2 * dst_stride + j] = a + b + c + d + e + f;
      for (i = 3; i < height - 4; ++i) {
        dst[i * dst_stride + j] = a + b + c + d + e + f + g;
        a = b;
        b = c;
        c = d;
        d = e;
        e = f;
        f = g;
        g = src[(i + 4) * src_stride + j] * src[(i + 4) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e + f + g;
      dst[(i + 1) * dst_stride + j] = b + c + d + e + f + g;
      dst[(i + 2) * dst_stride + j] = c + d + e + f + g;
      dst[(i + 3) * dst_stride + j] = d + e + f + g;
    }
  }

  // Horizontal sum over 7-pixel regions of dst
  for (i = 0; i < height; ++i) {
    a = dst[i * dst_stride];
    b = dst[i * dst_stride + 1];
    c = dst[i * dst_stride + 2];
    d = dst[i * dst_stride + 3];
    e = dst[i * dst_stride + 4];
    f = dst[i * dst_stride + 5];
    g = dst[i * dst_stride + 6];

    dst[i * dst_stride] = a + b + c + d;
    dst[i * dst_stride + 1] = a + b + c + d + e;
    dst[i * dst_stride + 2] = a + b + c + d + e + f;
    for (j = 3; j < width - 4; ++j) {
      dst[i * dst_stride + j] = a + b + c + d + e + f + g;
      a = b;
      b = c;
      c = d;
      d = e;
      e = f;
      f = g;
      g = dst[i * dst_stride + (j + 4)];
    }
    dst[i * dst_stride + j] = a + b + c + d + e + f + g;
    dst[i * dst_stride + (j + 1)] = b + c + d + e + f + g;
    dst[i * dst_stride + (j + 2)] = c + d + e + f + g;
    dst[i * dst_stride + (j + 3)] = d + e + f + g;
  }
}

// Generic version for any r. To be removed after experiments are done.
static void boxsumr(int32_t *src, int width, int height, int src_stride, int r,
                    int sqr, int32_t *dst, int dst_stride) {
  int32_t *tmp = aom_malloc(width * height * sizeof(*tmp));
  int tmp_stride = width;
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
  aom_free(tmp);
}

static void boxsum(int32_t *src, int width, int height, int src_stride, int r,
                   int sqr, int32_t *dst, int dst_stride) {
  if (r == 1)
    boxsum1(src, width, height, src_stride, sqr, dst, dst_stride);
  else if (r == 2)
    boxsum2(src, width, height, src_stride, sqr, dst, dst_stride);
  else if (r == 3)
    boxsum3(src, width, height, src_stride, sqr, dst, dst_stride);
  else
    boxsumr(src, width, height, src_stride, r, sqr, dst, dst_stride);
}

static void boxnum(int width, int height, int r, int8_t *num, int num_stride) {
  int i, j;
  for (i = 0; i <= r; ++i) {
    for (j = 0; j <= r; ++j) {
      num[i * num_stride + j] = (r + 1 + i) * (r + 1 + j);
      num[i * num_stride + (width - 1 - j)] = num[i * num_stride + j];
      num[(height - 1 - i) * num_stride + j] = num[i * num_stride + j];
      num[(height - 1 - i) * num_stride + (width - 1 - j)] =
          num[i * num_stride + j];
    }
  }
  for (j = 0; j <= r; ++j) {
    const int val = (2 * r + 1) * (r + 1 + j);
    for (i = r + 1; i < height - r; ++i) {
      num[i * num_stride + j] = val;
      num[i * num_stride + (width - 1 - j)] = val;
    }
  }
  for (i = 0; i <= r; ++i) {
    const int val = (2 * r + 1) * (r + 1 + i);
    for (j = r + 1; j < width - r; ++j) {
      num[i * num_stride + j] = val;
      num[(height - 1 - i) * num_stride + j] = val;
    }
  }
  for (i = r + 1; i < height - r; ++i) {
    for (j = r + 1; j < width - r; ++j) {
      num[i * num_stride + j] = (2 * r + 1) * (2 * r + 1);
    }
  }
}

void decode_xq(int *xqd, int *xq) {
  xq[0] = -xqd[0];
  xq[1] = (1 << SGRPROJ_PRJ_BITS) - xq[0] - xqd[1];
}

#define APPROXIMATE_SGR 1
void av1_selfguided_restoration(int32_t *dgd, int width, int height, int stride,
                                int bit_depth, int r, int eps,
                                int32_t *tmpbuf) {
  int32_t *A = tmpbuf;
  int32_t *B = A + RESTORATION_TILEPELS_MAX;
  int8_t num[RESTORATION_TILEPELS_MAX];
  int i, j;
  eps <<= 2 * (bit_depth - 8);

  // Don't filter tiles with dimensions < 5 on any axis
  if ((width < 5) || (height < 5)) return;

  boxsum(dgd, width, height, stride, r, 0, B, width);
  boxsum(dgd, width, height, stride, r, 1, A, width);
  boxnum(width, height, r, num, width);
  // The following loop is optimized assuming r <= 2. If we allow
  // r > 2, then the loop will need modifying.
  assert(r <= 3);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int k = i * width + j;
      const int n = num[k];
      // Assuming that we only allow up to 12-bit depth and r <= 2,
      // we calculate p = n^2 * Var(n-pixel block of original image)
      // (where n = 2 * r + 1 <= 5).
      //
      // There is an inequality which gives a bound on the variance:
      // https://en.wikipedia.org/wiki/Popoviciu's_inequality_on_variances
      // In this case, since each pixel is in the range [0, 2^12),
      // the variance is at most 1/4 * (2^12)^2 = 2^22.
      // Then p <= 25^2 * 2^22 < 2^32, and also q <= p + 25^2 * 68 < 2^32.
      //
      // The point of all this is to guarantee that q < 2^32, so that
      // platforms with a 64-bit by 32-bit divide unit (eg, x86)
      // can do the division by q more efficiently.
      const uint32_t p = (uint32_t)((uint64_t)A[k] * n - (uint64_t)B[k] * B[k]);
      const uint32_t q = (uint32_t)(p + n * n * eps);
      assert((uint64_t)A[k] * n - (uint64_t)B[k] * B[k] < (25 * 25U << 22));
      A[k] = (int32_t)(((uint64_t)p << SGRPROJ_SGR_BITS) + (q >> 1)) / q;
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
    const int32_t a =
        3 * A[k] + 2 * A[k + 1] + 2 * A[k + width] + A[k + width + 1];
    const int32_t b =
        3 * B[k] + 2 * B[k + 1] + 2 * B[k + width] + B[k + width + 1];
    const int32_t v =
        (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
  }
  i = 0;
  j = width - 1;
  {
    const int k = i * width + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int32_t a =
        3 * A[k] + 2 * A[k - 1] + 2 * A[k + width] + A[k + width - 1];
    const int32_t b =
        3 * B[k] + 2 * B[k - 1] + 2 * B[k + width] + B[k + width - 1];
    const int32_t v =
        (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
  }
  i = height - 1;
  j = 0;
  {
    const int k = i * width + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int32_t a =
        3 * A[k] + 2 * A[k + 1] + 2 * A[k - width] + A[k - width + 1];
    const int32_t b =
        3 * B[k] + 2 * B[k + 1] + 2 * B[k - width] + B[k - width + 1];
    const int32_t v =
        (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
  }
  i = height - 1;
  j = width - 1;
  {
    const int k = i * width + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int32_t a =
        3 * A[k] + 2 * A[k - 1] + 2 * A[k - width] + A[k - width - 1];
    const int32_t b =
        3 * B[k] + 2 * B[k - 1] + 2 * B[k - width] + B[k - width - 1];
    const int32_t v =
        (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
  }
  i = 0;
  for (j = 1; j < width - 1; ++j) {
    const int k = i * width + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int32_t a = A[k] + 2 * (A[k - 1] + A[k + 1]) + A[k + width] +
                      A[k + width - 1] + A[k + width + 1];
    const int32_t b = B[k] + 2 * (B[k - 1] + B[k + 1]) + B[k + width] +
                      B[k + width - 1] + B[k + width + 1];
    const int32_t v =
        (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
  }
  i = height - 1;
  for (j = 1; j < width - 1; ++j) {
    const int k = i * width + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int32_t a = A[k] + 2 * (A[k - 1] + A[k + 1]) + A[k - width] +
                      A[k - width - 1] + A[k - width + 1];
    const int32_t b = B[k] + 2 * (B[k - 1] + B[k + 1]) + B[k - width] +
                      B[k - width - 1] + B[k - width + 1];
    const int32_t v =
        (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
  }
  j = 0;
  for (i = 1; i < height - 1; ++i) {
    const int k = i * width + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int32_t a = A[k] + 2 * (A[k - width] + A[k + width]) + A[k + 1] +
                      A[k - width + 1] + A[k + width + 1];
    const int32_t b = B[k] + 2 * (B[k - width] + B[k + width]) + B[k + 1] +
                      B[k - width + 1] + B[k + width + 1];
    const int32_t v =
        (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
  }
  j = width - 1;
  for (i = 1; i < height - 1; ++i) {
    const int k = i * width + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int32_t a = A[k] + 2 * (A[k - width] + A[k + width]) + A[k - 1] +
                      A[k - width - 1] + A[k + width - 1];
    const int32_t b = B[k] + 2 * (B[k - width] + B[k + width]) + B[k - 1] +
                      B[k - width - 1] + B[k + width - 1];
    const int32_t v =
        (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
  }
  for (i = 1; i < height - 1; ++i) {
    for (j = 1; j < width - 1; ++j) {
      const int k = i * width + j;
      const int l = i * stride + j;
      const int nb = 5;
      const int32_t a =
          (A[k] + A[k - 1] + A[k + 1] + A[k - width] + A[k + width]) * 4 +
          (A[k - 1 - width] + A[k - 1 + width] + A[k + 1 - width] +
           A[k + 1 + width]) *
              3;
      const int32_t b =
          (B[k] + B[k - 1] + B[k + 1] + B[k - width] + B[k + width]) * 4 +
          (B[k - 1 - width] + B[k - 1 + width] + B[k + 1 - width] +
           B[k + 1 + width]) *
              3;
      const int32_t v =
          (((a * dgd[l] + b) << SGRPROJ_RST_BITS) + (1 << nb) / 2) >> nb;
      dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
    }
  }
#else
  if (r > 1) boxnum(width, height, r = 1, num, width);
  boxsum(A, width, height, width, r, 0, A, width);
  boxsum(B, width, height, width, r, 0, B, width);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int k = i * width + j;
      const int l = i * stride + j;
      const int n = num[k];
      const int32_t v =
          (((A[k] * dgd[l] + B[k]) << SGRPROJ_RST_BITS) + (n >> 1)) / n;
      dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS);
    }
  }
#endif  // APPROXIMATE_SGR
}

static void apply_selfguided_restoration(uint8_t *dat, int width, int height,
                                         int stride, int bit_depth, int eps,
                                         int *xqd, uint8_t *dst, int dst_stride,
                                         int32_t *tmpbuf) {
  int xq[2];
  int32_t *flt1 = tmpbuf;
  int32_t *flt2 = flt1 + RESTORATION_TILEPELS_MAX;
  int32_t *tmpbuf2 = flt2 + RESTORATION_TILEPELS_MAX;
  int i, j;
  assert(width * height <= RESTORATION_TILEPELS_MAX);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
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
      const int m = i * dst_stride + j;
      const int32_t u = ((int32_t)dat[l] << SGRPROJ_RST_BITS);
      const int32_t f1 = (int32_t)flt1[k] - u;
      const int32_t f2 = (int32_t)flt2[k] - u;
      const int64_t v = xq[0] * f1 + xq[1] * f2 + (u << SGRPROJ_PRJ_BITS);
      const int16_t w =
          (int16_t)ROUND_POWER_OF_TWO(v, SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);
      dst[m] = clip_pixel(w);
    }
  }
}

static void loop_sgrproj_filter_tile(uint8_t *data, int tile_idx, int width,
                                     int height, int stride,
                                     RestorationInternal *rst, uint8_t *dst,
                                     int dst_stride) {
  const int tile_width = rst->tile_width;
  const int tile_height = rst->tile_height;
  int h_start, h_end, v_start, v_end;
  uint8_t *data_p, *dst_p;

  if (rst->rsi->restoration_type[tile_idx] == RESTORE_NONE) {
    loop_copy_tile(data, tile_idx, 0, 0, width, height, stride, rst, dst,
                   dst_stride);
    return;
  }
  av1_get_rest_tile_limits(tile_idx, 0, 0, rst->nhtiles, rst->nvtiles,
                           tile_width, tile_height, width, height, 0, 0,
                           &h_start, &h_end, &v_start, &v_end);
  data_p = data + h_start + v_start * stride;
  dst_p = dst + h_start + v_start * dst_stride;
  apply_selfguided_restoration(data_p, h_end - h_start, v_end - v_start, stride,
                               8, rst->rsi->sgrproj_info[tile_idx].ep,
                               rst->rsi->sgrproj_info[tile_idx].xqd, dst_p,
                               dst_stride, rst->tmpbuf);
}

static void loop_sgrproj_filter(uint8_t *data, int width, int height,
                                int stride, RestorationInternal *rst,
                                uint8_t *dst, int dst_stride) {
  int tile_idx;
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    loop_sgrproj_filter_tile(data, tile_idx, width, height, stride, rst, dst,
                             dst_stride);
  }
}

static void apply_domaintxfmrf(int iter, int param, uint8_t *diff_right,
                               uint8_t *diff_down, int width, int height,
                               int32_t *dat, int dat_stride) {
  int i, j, acc;
  // Do first row separately, to initialize the top to bottom filter
  i = 0;
  {
    // left to right
    acc = dat[i * dat_stride] * DOMAINTXFMRF_VTABLE_PREC;
    dat[i * dat_stride] = acc;
    for (j = 1; j < width; ++j) {
      const int in = dat[i * dat_stride + j];
      const int diff =
          diff_right[i * width + j - 1];  // Left absolute difference
      const int v = domaintxfmrf_vtable[iter][param][diff];
      acc = in * (DOMAINTXFMRF_VTABLE_PREC - v) +
            ROUND_POWER_OF_TWO(v * acc, DOMAINTXFMRF_VTABLE_PRECBITS);
      dat[i * dat_stride + j] = acc;
    }
    // right to left
    for (j = width - 2; j >= 0; --j) {
      const int in = dat[i * dat_stride + j];
      const int diff = diff_right[i * width + j];  // Right absolute difference
      const int v = domaintxfmrf_vtable[iter][param][diff];
      acc = ROUND_POWER_OF_TWO(in * (DOMAINTXFMRF_VTABLE_PREC - v) + acc * v,
                               DOMAINTXFMRF_VTABLE_PRECBITS);
      dat[i * dat_stride + j] = acc;
    }
  }

  for (i = 1; i < height; ++i) {
    // left to right
    acc = dat[i * dat_stride] * DOMAINTXFMRF_VTABLE_PREC;
    dat[i * dat_stride] = acc;
    for (j = 1; j < width; ++j) {
      const int in = dat[i * dat_stride + j];
      const int diff =
          diff_right[i * width + j - 1];  // Left absolute difference
      const int v = domaintxfmrf_vtable[iter][param][diff];
      acc = in * (DOMAINTXFMRF_VTABLE_PREC - v) +
            ROUND_POWER_OF_TWO(v * acc, DOMAINTXFMRF_VTABLE_PRECBITS);
      dat[i * dat_stride + j] = acc;
    }
    // right to left
    for (j = width - 2; j >= 0; --j) {
      const int in = dat[i * dat_stride + j];
      const int diff = diff_right[i * width + j];  // Right absolute difference
      const int v = domaintxfmrf_vtable[iter][param][diff];
      acc = ROUND_POWER_OF_TWO(in * (DOMAINTXFMRF_VTABLE_PREC - v) + acc * v,
                               DOMAINTXFMRF_VTABLE_PRECBITS);
      dat[i * dat_stride + j] = acc;
    }
    // top to bottom
    for (j = 0; j < width; ++j) {
      const int in = dat[i * dat_stride + j];
      const int in_above = dat[(i - 1) * dat_stride + j];
      const int diff =
          diff_down[(i - 1) * width + j];  // Upward absolute difference
      const int v = domaintxfmrf_vtable[iter][param][diff];
      acc =
          ROUND_POWER_OF_TWO(in * (DOMAINTXFMRF_VTABLE_PREC - v) + in_above * v,
                             DOMAINTXFMRF_VTABLE_PRECBITS);
      dat[i * dat_stride + j] = acc;
    }
  }
  for (j = 0; j < width; ++j) {
    // bottom to top + output rounding
    acc = dat[(height - 1) * dat_stride + j];
    dat[(height - 1) * dat_stride + j] =
        ROUND_POWER_OF_TWO(acc, DOMAINTXFMRF_VTABLE_PRECBITS);
    for (i = height - 2; i >= 0; --i) {
      const int in = dat[i * dat_stride + j];
      const int diff =
          diff_down[i * width + j];  // Downward absolute difference
      const int v = domaintxfmrf_vtable[iter][param][diff];
      acc = ROUND_POWER_OF_TWO(in * (DOMAINTXFMRF_VTABLE_PREC - v) + acc * v,
                               DOMAINTXFMRF_VTABLE_PRECBITS);
      dat[i * dat_stride + j] =
          ROUND_POWER_OF_TWO(acc, DOMAINTXFMRF_VTABLE_PRECBITS);
    }
  }
}

void av1_domaintxfmrf_restoration(uint8_t *dgd, int width, int height,
                                  int stride, int param, uint8_t *dst,
                                  int dst_stride, int32_t *tmpbuf) {
  int32_t *dat = tmpbuf;
  uint8_t *diff_right = (uint8_t *)(tmpbuf + RESTORATION_TILEPELS_MAX);
  uint8_t *diff_down = diff_right + RESTORATION_TILEPELS_MAX;
  int i, j, t;

  for (i = 0; i < height; ++i) {
    int cur_px = dgd[i * stride];
    for (j = 0; j < width - 1; ++j) {
      const int next_px = dgd[i * stride + j + 1];
      diff_right[i * width + j] = abs(cur_px - next_px);
      cur_px = next_px;
    }
  }
  for (j = 0; j < width; ++j) {
    int cur_px = dgd[j];
    for (i = 0; i < height - 1; ++i) {
      const int next_px = dgd[(i + 1) * stride + j];
      diff_down[i * width + j] = abs(cur_px - next_px);
      cur_px = next_px;
    }
  }
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      dat[i * width + j] = dgd[i * stride + j];
    }
  }

  for (t = 0; t < DOMAINTXFMRF_ITERS; ++t) {
    apply_domaintxfmrf(t, param, diff_right, diff_down, width, height, dat,
                       width);
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
  const int tile_width = rst->tile_width;
  const int tile_height = rst->tile_height;
  int h_start, h_end, v_start, v_end;
  int32_t *tmpbuf = (int32_t *)rst->tmpbuf;

  if (rst->rsi->restoration_type[tile_idx] == RESTORE_NONE) {
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
      dst + h_start + v_start * dst_stride, dst_stride, tmpbuf);
}

static void loop_domaintxfmrf_filter(uint8_t *data, int width, int height,
                                     int stride, RestorationInternal *rst,
                                     uint8_t *dst, int dst_stride) {
  int tile_idx;
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    loop_domaintxfmrf_filter_tile(data, tile_idx, width, height, stride, rst,
                                  dst, dst_stride);
  }
}

static void loop_switchable_filter(uint8_t *data, int width, int height,
                                   int stride, RestorationInternal *rst,
                                   uint8_t *dst, int dst_stride) {
  int tile_idx;
  extend_frame(data, width, height, stride);
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    if (rst->rsi->restoration_type[tile_idx] == RESTORE_NONE) {
      loop_copy_tile(data, tile_idx, 0, 0, width, height, stride, rst, dst,
                     dst_stride);
    } else if (rst->rsi->restoration_type[tile_idx] == RESTORE_WIENER) {
      loop_wiener_filter_tile(data, tile_idx, width, height, stride, rst, dst,
                              dst_stride);
    } else if (rst->rsi->restoration_type[tile_idx] == RESTORE_SGRPROJ) {
      loop_sgrproj_filter_tile(data, tile_idx, width, height, stride, rst, dst,
                               dst_stride);
    } else if (rst->rsi->restoration_type[tile_idx] == RESTORE_DOMAINTXFMRF) {
      loop_domaintxfmrf_filter_tile(data, tile_idx, width, height, stride, rst,
                                    dst, dst_stride);
    }
  }
}

#if CONFIG_AOM_HIGHBITDEPTH
void extend_frame_highbd(uint16_t *data, int width, int height, int stride) {
  uint16_t *data_p;
  int i, j;
  for (i = 0; i < height; ++i) {
    data_p = data + i * stride;
    for (j = -WIENER_HALFWIN; j < 0; ++j) data_p[j] = data_p[0];
    for (j = width; j < width + WIENER_HALFWIN; ++j)
      data_p[j] = data_p[width - 1];
  }
  data_p = data - WIENER_HALFWIN;
  for (i = -WIENER_HALFWIN; i < 0; ++i) {
    memcpy(data_p + i * stride, data_p,
           (width + 2 * WIENER_HALFWIN) * sizeof(uint16_t));
  }
  for (i = height; i < height + WIENER_HALFWIN; ++i) {
    memcpy(data_p + i * stride, data_p + (height - 1) * stride,
           (width + 2 * WIENER_HALFWIN) * sizeof(uint16_t));
  }
}

static void loop_copy_tile_highbd(uint16_t *data, int tile_idx, int subtile_idx,
                                  int subtile_bits, int width, int height,
                                  int stride, RestorationInternal *rst,
                                  uint16_t *dst, int dst_stride) {
  const int tile_width = rst->tile_width;
  const int tile_height = rst->tile_height;
  int i;
  int h_start, h_end, v_start, v_end;
  av1_get_rest_tile_limits(tile_idx, subtile_idx, subtile_bits, rst->nhtiles,
                           rst->nvtiles, tile_width, tile_height, width, height,
                           0, 0, &h_start, &h_end, &v_start, &v_end);
  for (i = v_start; i < v_end; ++i)
    memcpy(dst + i * dst_stride + h_start, data + i * stride + h_start,
           (h_end - h_start) * sizeof(*dst));
}

static void loop_wiener_filter_tile_highbd(uint16_t *data, int tile_idx,
                                           int width, int height, int stride,
                                           RestorationInternal *rst,
                                           int bit_depth, uint16_t *dst,
                                           int dst_stride) {
  const int tile_width = rst->tile_width;
  const int tile_height = rst->tile_height;
  int h_start, h_end, v_start, v_end;
  int i, j;
  DECLARE_ALIGNED(16, InterpKernel, hkernel);
  DECLARE_ALIGNED(16, InterpKernel, vkernel);

  if (rst->rsi->restoration_type[tile_idx] == RESTORE_NONE) {
    loop_copy_tile_highbd(data, tile_idx, 0, 0, width, height, stride, rst, dst,
                          dst_stride);
    return;
  }
  // TODO(david.barker): Store hfilter/vfilter as an InterpKernel
  // instead of the current format. Then this can be removed.
  assert(WIENER_WIN == SUBPEL_TAPS - 1);
  for (i = 0; i < WIENER_WIN; ++i) {
    hkernel[i] = rst->rsi->wiener_info[tile_idx].hfilter[i];
    vkernel[i] = rst->rsi->wiener_info[tile_idx].vfilter[i];
  }
  hkernel[WIENER_WIN] = 0;
  vkernel[WIENER_WIN] = 0;
  hkernel[3] -= 128;
  vkernel[3] -= 128;
  av1_get_rest_tile_limits(tile_idx, 0, 0, rst->nhtiles, rst->nvtiles,
                           tile_width, tile_height, width, height, 0, 0,
                           &h_start, &h_end, &v_start, &v_end);
  // Convolve the whole tile (done in blocks here to match the requirements
  // of the vectorized convolve functions, but the result is equivalent)
  for (i = v_start; i < v_end; i += MAX_SB_SIZE)
    for (j = h_start; j < h_end; j += MAX_SB_SIZE) {
      int w = AOMMIN(MAX_SB_SIZE, (h_end - j + 15) & ~15);
      int h = AOMMIN(MAX_SB_SIZE, (v_end - i + 15) & ~15);
      const uint16_t *data_p = data + i * stride + j;
      uint16_t *dst_p = dst + i * dst_stride + j;
      aom_highbd_convolve8_add_src(CONVERT_TO_BYTEPTR(data_p), stride,
                                   CONVERT_TO_BYTEPTR(dst_p), dst_stride,
                                   hkernel, 16, vkernel, 16, w, h, bit_depth);
    }
}

static void loop_wiener_filter_highbd(uint8_t *data8, int width, int height,
                                      int stride, RestorationInternal *rst,
                                      int bit_depth, uint8_t *dst8,
                                      int dst_stride) {
  uint16_t *data = CONVERT_TO_SHORTPTR(data8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  int tile_idx;
  extend_frame_highbd(data, width, height, stride);
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    loop_wiener_filter_tile_highbd(data, tile_idx, width, height, stride, rst,
                                   bit_depth, dst, dst_stride);
  }
}

static void apply_selfguided_restoration_highbd(
    uint16_t *dat, int width, int height, int stride, int bit_depth, int eps,
    int *xqd, uint16_t *dst, int dst_stride, int32_t *tmpbuf) {
  int xq[2];
  int32_t *flt1 = tmpbuf;
  int32_t *flt2 = flt1 + RESTORATION_TILEPELS_MAX;
  int32_t *tmpbuf2 = flt2 + RESTORATION_TILEPELS_MAX;
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
      const int m = i * dst_stride + j;
      const int32_t u = ((int32_t)dat[l] << SGRPROJ_RST_BITS);
      const int32_t f1 = (int32_t)flt1[k] - u;
      const int32_t f2 = (int32_t)flt2[k] - u;
      const int64_t v = xq[0] * f1 + xq[1] * f2 + (u << SGRPROJ_PRJ_BITS);
      const int16_t w =
          (int16_t)ROUND_POWER_OF_TWO(v, SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);
      dst[m] = (uint16_t)clip_pixel_highbd(w, bit_depth);
    }
  }
}

static void loop_sgrproj_filter_tile_highbd(uint16_t *data, int tile_idx,
                                            int width, int height, int stride,
                                            RestorationInternal *rst,
                                            int bit_depth, uint16_t *dst,
                                            int dst_stride) {
  const int tile_width = rst->tile_width;
  const int tile_height = rst->tile_height;
  int h_start, h_end, v_start, v_end;
  uint16_t *data_p, *dst_p;

  if (rst->rsi->restoration_type[tile_idx] == RESTORE_NONE) {
    loop_copy_tile_highbd(data, tile_idx, 0, 0, width, height, stride, rst, dst,
                          dst_stride);
    return;
  }
  av1_get_rest_tile_limits(tile_idx, 0, 0, rst->nhtiles, rst->nvtiles,
                           tile_width, tile_height, width, height, 0, 0,
                           &h_start, &h_end, &v_start, &v_end);
  data_p = data + h_start + v_start * stride;
  dst_p = dst + h_start + v_start * dst_stride;
  apply_selfguided_restoration_highbd(
      data_p, h_end - h_start, v_end - v_start, stride, bit_depth,
      rst->rsi->sgrproj_info[tile_idx].ep, rst->rsi->sgrproj_info[tile_idx].xqd,
      dst_p, dst_stride, rst->tmpbuf);
}

static void loop_sgrproj_filter_highbd(uint8_t *data8, int width, int height,
                                       int stride, RestorationInternal *rst,
                                       int bit_depth, uint8_t *dst8,
                                       int dst_stride) {
  int tile_idx;
  uint16_t *data = CONVERT_TO_SHORTPTR(data8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    loop_sgrproj_filter_tile_highbd(data, tile_idx, width, height, stride, rst,
                                    bit_depth, dst, dst_stride);
  }
}

static void apply_domaintxfmrf_hor_highbd(int iter, int param, uint16_t *img,
                                          int width, int height, int img_stride,
                                          int32_t *dat, int dat_stride,
                                          int bd) {
  const int shift = (bd - 8);
  int i, j, acc, old_px;
  for (i = 0; i < height; ++i) {
    // left to right
    acc = dat[i * dat_stride] * DOMAINTXFMRF_VTABLE_PREC;
    dat[i * dat_stride] = acc;
    old_px = img[i * img_stride] >> shift;
    for (j = 1; j < width; ++j) {
      const int cur_px = img[i * img_stride + j] >> shift;
      const int v = domaintxfmrf_vtable[iter][param][abs(cur_px - old_px)];
      acc = dat[i * dat_stride + j] * (DOMAINTXFMRF_VTABLE_PREC - v) +
            ((v * acc + DOMAINTXFMRF_VTABLE_PREC / 2) >>
             DOMAINTXFMRF_VTABLE_PRECBITS);
      dat[i * dat_stride + j] = acc;
      old_px = cur_px;
    }
    // right to left
    for (j = width - 2; j >= 0; --j) {
      const int cur_px = img[i * img_stride + j] >> shift;
      const int v = domaintxfmrf_vtable[iter][param][abs(cur_px - old_px)];
      acc = (dat[i * dat_stride + j] * (DOMAINTXFMRF_VTABLE_PREC - v) +
             v * acc + DOMAINTXFMRF_VTABLE_PREC / 2) >>
            DOMAINTXFMRF_VTABLE_PRECBITS;
      dat[i * dat_stride + j] = acc;
      old_px = cur_px;
    }
  }
}

static void apply_domaintxfmrf_ver_highbd(int iter, int param, uint16_t *img,
                                          int width, int height, int img_stride,
                                          int32_t *dat, int dat_stride,
                                          int bd) {
  const int shift = (bd - 8);
  int i, j, acc, old_px;
  for (j = 0; j < width; ++j) {
    // top to bottom
    acc = dat[j];
    old_px = img[j] >> shift;
    for (i = 1; i < height; ++i) {
      const int cur_px = img[i * img_stride + j] >> shift;
      const int v = domaintxfmrf_vtable[iter][param][abs(cur_px - old_px)];
      acc = (dat[i * dat_stride + j] * (DOMAINTXFMRF_VTABLE_PREC - v) +
             (acc * v + DOMAINTXFMRF_VTABLE_PREC / 2)) >>
            DOMAINTXFMRF_VTABLE_PRECBITS;
      dat[i * dat_stride + j] = acc;
      old_px = cur_px;
    }
    // bottom to top
    dat[(height - 1) * dat_stride + j] =
        ROUND_POWER_OF_TWO(acc, DOMAINTXFMRF_VTABLE_PRECBITS);
    for (i = height - 2; i >= 0; --i) {
      const int cur_px = img[i * img_stride + j] >> shift;
      const int v = domaintxfmrf_vtable[iter][param][abs(old_px - cur_px)];
      acc = (dat[i * dat_stride + j] * (DOMAINTXFMRF_VTABLE_PREC - v) +
             acc * v + DOMAINTXFMRF_VTABLE_PREC / 2) >>
            DOMAINTXFMRF_VTABLE_PRECBITS;
      dat[i * dat_stride + j] =
          ROUND_POWER_OF_TWO(acc, DOMAINTXFMRF_VTABLE_PRECBITS);
      old_px = cur_px;
    }
  }
}

void av1_domaintxfmrf_restoration_highbd(uint16_t *dgd, int width, int height,
                                         int stride, int param, int bit_depth,
                                         uint16_t *dst, int dst_stride,
                                         int32_t *tmpbuf) {
  int32_t *dat = tmpbuf;
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
  const int tile_width = rst->tile_width;
  const int tile_height = rst->tile_height;
  int h_start, h_end, v_start, v_end;
  int32_t *tmpbuf = (int32_t *)rst->tmpbuf;

  if (rst->rsi->restoration_type[tile_idx] == RESTORE_NONE) {
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
      dst + h_start + v_start * dst_stride, dst_stride, tmpbuf);
}

static void loop_domaintxfmrf_filter_highbd(uint8_t *data8, int width,
                                            int height, int stride,
                                            RestorationInternal *rst,
                                            int bit_depth, uint8_t *dst8,
                                            int dst_stride) {
  int tile_idx;
  uint16_t *data = CONVERT_TO_SHORTPTR(data8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    loop_domaintxfmrf_filter_tile_highbd(data, tile_idx, width, height, stride,
                                         rst, bit_depth, dst, dst_stride);
  }
}

static void loop_switchable_filter_highbd(uint8_t *data8, int width, int height,
                                          int stride, RestorationInternal *rst,
                                          int bit_depth, uint8_t *dst8,
                                          int dst_stride) {
  uint16_t *data = CONVERT_TO_SHORTPTR(data8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  int tile_idx;
  extend_frame_highbd(data, width, height, stride);
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    if (rst->rsi->restoration_type[tile_idx] == RESTORE_NONE) {
      loop_copy_tile_highbd(data, tile_idx, 0, 0, width, height, stride, rst,
                            dst, dst_stride);
    } else if (rst->rsi->restoration_type[tile_idx] == RESTORE_WIENER) {
      loop_wiener_filter_tile_highbd(data, tile_idx, width, height, stride, rst,
                                     bit_depth, dst, dst_stride);
    } else if (rst->rsi->restoration_type[tile_idx] == RESTORE_SGRPROJ) {
      loop_sgrproj_filter_tile_highbd(data, tile_idx, width, height, stride,
                                      rst, bit_depth, dst, dst_stride);
    } else if (rst->rsi->restoration_type[tile_idx] == RESTORE_DOMAINTXFMRF) {
      loop_domaintxfmrf_filter_tile_highbd(data, tile_idx, width, height,
                                           stride, rst, bit_depth, dst,
                                           dst_stride);
    }
  }
}
#endif  // CONFIG_AOM_HIGHBITDEPTH

static void loop_restoration_rows(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                                  int start_mi_row, int end_mi_row,
                                  int components_pattern, RestorationInfo *rsi,
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
  restore_func_type restore_func;
#if CONFIG_AOM_HIGHBITDEPTH
  restore_func_highbd_type restore_func_highbd;
#endif  // CONFIG_AOM_HIGHBITDEPTH
  YV12_BUFFER_CONFIG dst_;

  yend = AOMMIN(yend, cm->height);
  uvend = AOMMIN(uvend, cm->subsampling_y ? (cm->height + 1) >> 1 : cm->height);

  if (components_pattern == (1 << AOM_PLANE_Y)) {
    // Only y
    if (rsi[0].frame_restoration_type == RESTORE_NONE) {
      if (dst) aom_yv12_copy_y(frame, dst);
      return;
    }
  } else if (components_pattern == (1 << AOM_PLANE_U)) {
    // Only U
    if (rsi[1].frame_restoration_type == RESTORE_NONE) {
      if (dst) aom_yv12_copy_u(frame, dst);
      return;
    }
  } else if (components_pattern == (1 << AOM_PLANE_V)) {
    // Only V
    if (rsi[2].frame_restoration_type == RESTORE_NONE) {
      if (dst) aom_yv12_copy_v(frame, dst);
      return;
    }
  } else if (components_pattern ==
             ((1 << AOM_PLANE_Y) | (1 << AOM_PLANE_U) | (1 << AOM_PLANE_V))) {
    // All components
    if (rsi[0].frame_restoration_type == RESTORE_NONE &&
        rsi[1].frame_restoration_type == RESTORE_NONE &&
        rsi[2].frame_restoration_type == RESTORE_NONE) {
      if (dst) aom_yv12_copy_frame(frame, dst);
      return;
    }
  }

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

  if ((components_pattern >> AOM_PLANE_Y) & 1) {
    if (rsi[0].frame_restoration_type != RESTORE_NONE) {
      cm->rst_internal.ntiles = av1_get_rest_ntiles(
          cm->width, cm->height, &cm->rst_internal.tile_width,
          &cm->rst_internal.tile_height, &cm->rst_internal.nhtiles,
          &cm->rst_internal.nvtiles);
      cm->rst_internal.rsi = &rsi[0];
      restore_func =
          restore_funcs[cm->rst_internal.rsi->frame_restoration_type];
#if CONFIG_AOM_HIGHBITDEPTH
      restore_func_highbd =
          restore_funcs_highbd[cm->rst_internal.rsi->frame_restoration_type];
      if (cm->use_highbitdepth)
        restore_func_highbd(
            frame->y_buffer + ystart * ystride, ywidth, yend - ystart, ystride,
            &cm->rst_internal, cm->bit_depth,
            dst->y_buffer + ystart * dst->y_stride, dst->y_stride);
      else
#endif  // CONFIG_AOM_HIGHBITDEPTH
        restore_func(frame->y_buffer + ystart * ystride, ywidth, yend - ystart,
                     ystride, &cm->rst_internal,
                     dst->y_buffer + ystart * dst->y_stride, dst->y_stride);
    } else {
      aom_yv12_copy_y(frame, dst);
    }
  }

  if ((components_pattern >> AOM_PLANE_U) & 1) {
    if (rsi[AOM_PLANE_U].frame_restoration_type != RESTORE_NONE) {
      cm->rst_internal.ntiles = av1_get_rest_ntiles(
          cm->width >> cm->subsampling_x, cm->height >> cm->subsampling_y,
          &cm->rst_internal.tile_width, &cm->rst_internal.tile_height,
          &cm->rst_internal.nhtiles, &cm->rst_internal.nvtiles);
      cm->rst_internal.rsi = &rsi[AOM_PLANE_U];
      restore_func =
          restore_funcs[cm->rst_internal.rsi->frame_restoration_type];
#if CONFIG_AOM_HIGHBITDEPTH
      restore_func_highbd =
          restore_funcs_highbd[cm->rst_internal.rsi->frame_restoration_type];
      if (cm->use_highbitdepth)
        restore_func_highbd(
            frame->u_buffer + uvstart * uvstride, uvwidth, uvend - uvstart,
            uvstride, &cm->rst_internal, cm->bit_depth,
            dst->u_buffer + uvstart * dst->uv_stride, dst->uv_stride);
      else
#endif  // CONFIG_AOM_HIGHBITDEPTH
        restore_func(frame->u_buffer + uvstart * uvstride, uvwidth,
                     uvend - uvstart, uvstride, &cm->rst_internal,
                     dst->u_buffer + uvstart * dst->uv_stride, dst->uv_stride);
    } else {
      aom_yv12_copy_u(frame, dst);
    }
  }

  if ((components_pattern >> AOM_PLANE_V) & 1) {
    if (rsi[AOM_PLANE_V].frame_restoration_type != RESTORE_NONE) {
      cm->rst_internal.ntiles = av1_get_rest_ntiles(
          cm->width >> cm->subsampling_x, cm->height >> cm->subsampling_y,
          &cm->rst_internal.tile_width, &cm->rst_internal.tile_height,
          &cm->rst_internal.nhtiles, &cm->rst_internal.nvtiles);
      cm->rst_internal.rsi = &rsi[AOM_PLANE_V];
      restore_func =
          restore_funcs[cm->rst_internal.rsi->frame_restoration_type];
#if CONFIG_AOM_HIGHBITDEPTH
      restore_func_highbd =
          restore_funcs_highbd[cm->rst_internal.rsi->frame_restoration_type];
      if (cm->use_highbitdepth)
        restore_func_highbd(
            frame->v_buffer + uvstart * uvstride, uvwidth, uvend - uvstart,
            uvstride, &cm->rst_internal, cm->bit_depth,
            dst->v_buffer + uvstart * dst->uv_stride, dst->uv_stride);
      else
#endif  // CONFIG_AOM_HIGHBITDEPTH
        restore_func(frame->v_buffer + uvstart * uvstride, uvwidth,
                     uvend - uvstart, uvstride, &cm->rst_internal,
                     dst->v_buffer + uvstart * dst->uv_stride, dst->uv_stride);
    } else {
      aom_yv12_copy_v(frame, dst);
    }
  }

  if (dst == &dst_) {
    if ((components_pattern >> AOM_PLANE_Y) & 1) aom_yv12_copy_y(dst, frame);
    if ((components_pattern >> AOM_PLANE_U) & 1) aom_yv12_copy_u(dst, frame);
    if ((components_pattern >> AOM_PLANE_V) & 1) aom_yv12_copy_v(dst, frame);
    aom_free_frame_buffer(dst);
  }
}

void av1_loop_restoration_frame(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                                RestorationInfo *rsi, int components_pattern,
                                int partial_frame, YV12_BUFFER_CONFIG *dst) {
  int start_mi_row, end_mi_row, mi_rows_to_filter;
  start_mi_row = 0;
  mi_rows_to_filter = cm->mi_rows;
  if (partial_frame && cm->mi_rows > 8) {
    start_mi_row = cm->mi_rows >> 1;
    start_mi_row &= 0xfffffff8;
    mi_rows_to_filter = AOMMAX(cm->mi_rows / 8, 8);
  }
  end_mi_row = start_mi_row + mi_rows_to_filter;
  loop_restoration_init(&cm->rst_internal, cm->frame_type == KEY_FRAME);
  loop_restoration_rows(frame, cm, start_mi_row, end_mi_row, components_pattern,
                        rsi, dst);
}
