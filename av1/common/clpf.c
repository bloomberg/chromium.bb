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
#include "aom_dsp/aom_dsp_common.h"

static int sign(int i) { return i < 0 ? -1 : 1; }

static int constrain(int x, int s, unsigned int damping) {
  return sign(x) *
         AOMMAX(0, abs(x) - AOMMAX(0, abs(x) - s +
                                          (abs(x) >> (damping - get_msb(s)))));
}

int av1_clpf_sample(int X, int A, int B, int C, int D, int E, int F, int G,
                    int H, int s, unsigned int dmp) {
  int delta = 1 * constrain(A - X, s, dmp) + 3 * constrain(B - X, s, dmp) +
              1 * constrain(C - X, s, dmp) + 3 * constrain(D - X, s, dmp) +
              3 * constrain(E - X, s, dmp) + 1 * constrain(F - X, s, dmp) +
              3 * constrain(G - X, s, dmp) + 1 * constrain(H - X, s, dmp);
  return (8 + delta - (delta < 0)) >> 4;
}

void aom_clpf_block_c(const uint8_t *src, uint8_t *dst, int sstride,
                      int dstride, int x0, int y0, int sizex, int sizey,
                      unsigned int strength, unsigned int damping) {
  int x, y;

  for (y = y0; y < y0 + sizey; y++) {
    for (x = x0; x < x0 + sizex; x++) {
      const int X = src[y * sstride + x];
      const int A = src[(y - 2) * sstride + x];
      const int B = src[(y - 1) * sstride + x];
      const int C = src[y * sstride + x - 2];
      const int D = src[y * sstride + x - 1];
      const int E = src[y * sstride + x + 1];
      const int F = src[y * sstride + x + 2];
      const int G = src[(y + 1) * sstride + x];
      const int H = src[(y + 2) * sstride + x];
      const int delta =
          av1_clpf_sample(X, A, B, C, D, E, F, G, H, strength, damping);
      dst[y * dstride + x] = X + delta;
    }
  }
}

// Identical to aom_clpf_block_c() apart from "src" and "dst".
// TODO(stemidts): Put under CONFIG_AOM_HIGHBITDEPTH if CDEF do 8 bit internally
void aom_clpf_block_hbd_c(const uint16_t *src, uint16_t *dst, int sstride,
                          int dstride, int x0, int y0, int sizex, int sizey,
                          unsigned int strength, unsigned int damping) {
  int x, y;

  for (y = y0; y < y0 + sizey; y++) {
    for (x = x0; x < x0 + sizex; x++) {
      const int X = src[y * sstride + x];
      const int A = src[(y - 2) * sstride + x];
      const int B = src[(y - 1) * sstride + x];
      const int C = src[y * sstride + x - 2];
      const int D = src[y * sstride + x - 1];
      const int E = src[y * sstride + x + 1];
      const int F = src[y * sstride + x + 2];
      const int G = src[(y + 1) * sstride + x];
      const int H = src[(y + 2) * sstride + x];
      const int delta =
          av1_clpf_sample(X, A, B, C, D, E, F, G, H, strength, damping);
      dst[y * dstride + x] = X + delta;
    }
  }
}
