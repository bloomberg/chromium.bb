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

#include "./aom_dsp_rtcd.h"
#include "aom_dsp/inv_txfm.h"

void aom_iwht4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int stride) {
  /* 4-point reversible, orthonormal inverse Walsh-Hadamard in 3.5 adds,
     0.5 shifts per pixel. */
  int i;
  tran_low_t output[16];
  tran_high_t a1, b1, c1, d1, e1;
  const tran_low_t *ip = input;
  tran_low_t *op = output;

  for (i = 0; i < 4; i++) {
    a1 = ip[0] >> UNIT_QUANT_SHIFT;
    c1 = ip[1] >> UNIT_QUANT_SHIFT;
    d1 = ip[2] >> UNIT_QUANT_SHIFT;
    b1 = ip[3] >> UNIT_QUANT_SHIFT;
    a1 += c1;
    d1 -= b1;
    e1 = (a1 - d1) >> 1;
    b1 = e1 - b1;
    c1 = e1 - c1;
    a1 -= b1;
    d1 += c1;
    op[0] = WRAPLOW(a1);
    op[1] = WRAPLOW(b1);
    op[2] = WRAPLOW(c1);
    op[3] = WRAPLOW(d1);
    ip += 4;
    op += 4;
  }

  ip = output;
  for (i = 0; i < 4; i++) {
    a1 = ip[4 * 0];
    c1 = ip[4 * 1];
    d1 = ip[4 * 2];
    b1 = ip[4 * 3];
    a1 += c1;
    d1 -= b1;
    e1 = (a1 - d1) >> 1;
    b1 = e1 - b1;
    c1 = e1 - c1;
    a1 -= b1;
    d1 += c1;
    dest[stride * 0] = clip_pixel_add(dest[stride * 0], WRAPLOW(a1));
    dest[stride * 1] = clip_pixel_add(dest[stride * 1], WRAPLOW(b1));
    dest[stride * 2] = clip_pixel_add(dest[stride * 2], WRAPLOW(c1));
    dest[stride * 3] = clip_pixel_add(dest[stride * 3], WRAPLOW(d1));

    ip++;
    dest++;
  }
}

void aom_iwht4x4_1_add_c(const tran_low_t *in, uint8_t *dest, int dest_stride) {
  int i;
  tran_high_t a1, e1;
  tran_low_t tmp[4];
  const tran_low_t *ip = in;
  tran_low_t *op = tmp;

  a1 = ip[0] >> UNIT_QUANT_SHIFT;
  e1 = a1 >> 1;
  a1 -= e1;
  op[0] = WRAPLOW(a1);
  op[1] = op[2] = op[3] = WRAPLOW(e1);

  ip = tmp;
  for (i = 0; i < 4; i++) {
    e1 = ip[0] >> 1;
    a1 = ip[0] - e1;
    dest[dest_stride * 0] = clip_pixel_add(dest[dest_stride * 0], a1);
    dest[dest_stride * 1] = clip_pixel_add(dest[dest_stride * 1], e1);
    dest[dest_stride * 2] = clip_pixel_add(dest[dest_stride * 2], e1);
    dest[dest_stride * 3] = clip_pixel_add(dest[dest_stride * 3], e1);
    ip++;
    dest++;
  }
}

void aom_highbd_iwht4x4_16_add_c(const tran_low_t *input, uint8_t *dest8,
                                 int stride, int bd) {
  /* 4-point reversible, orthonormal inverse Walsh-Hadamard in 3.5 adds,
     0.5 shifts per pixel. */
  int i;
  tran_low_t output[16];
  tran_high_t a1, b1, c1, d1, e1;
  const tran_low_t *ip = input;
  tran_low_t *op = output;
  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);

  for (i = 0; i < 4; i++) {
    a1 = ip[0] >> UNIT_QUANT_SHIFT;
    c1 = ip[1] >> UNIT_QUANT_SHIFT;
    d1 = ip[2] >> UNIT_QUANT_SHIFT;
    b1 = ip[3] >> UNIT_QUANT_SHIFT;
    a1 += c1;
    d1 -= b1;
    e1 = (a1 - d1) >> 1;
    b1 = e1 - b1;
    c1 = e1 - c1;
    a1 -= b1;
    d1 += c1;
    op[0] = HIGHBD_WRAPLOW(a1, bd);
    op[1] = HIGHBD_WRAPLOW(b1, bd);
    op[2] = HIGHBD_WRAPLOW(c1, bd);
    op[3] = HIGHBD_WRAPLOW(d1, bd);
    ip += 4;
    op += 4;
  }

  ip = output;
  for (i = 0; i < 4; i++) {
    a1 = ip[4 * 0];
    c1 = ip[4 * 1];
    d1 = ip[4 * 2];
    b1 = ip[4 * 3];
    a1 += c1;
    d1 -= b1;
    e1 = (a1 - d1) >> 1;
    b1 = e1 - b1;
    c1 = e1 - c1;
    a1 -= b1;
    d1 += c1;
    dest[stride * 0] =
        highbd_clip_pixel_add(dest[stride * 0], HIGHBD_WRAPLOW(a1, bd), bd);
    dest[stride * 1] =
        highbd_clip_pixel_add(dest[stride * 1], HIGHBD_WRAPLOW(b1, bd), bd);
    dest[stride * 2] =
        highbd_clip_pixel_add(dest[stride * 2], HIGHBD_WRAPLOW(c1, bd), bd);
    dest[stride * 3] =
        highbd_clip_pixel_add(dest[stride * 3], HIGHBD_WRAPLOW(d1, bd), bd);

    ip++;
    dest++;
  }
}

void aom_highbd_iwht4x4_1_add_c(const tran_low_t *in, uint8_t *dest8,
                                int dest_stride, int bd) {
  int i;
  tran_high_t a1, e1;
  tran_low_t tmp[4];
  const tran_low_t *ip = in;
  tran_low_t *op = tmp;
  uint16_t *dest = CONVERT_TO_SHORTPTR(dest8);
  (void)bd;

  a1 = ip[0] >> UNIT_QUANT_SHIFT;
  e1 = a1 >> 1;
  a1 -= e1;
  op[0] = HIGHBD_WRAPLOW(a1, bd);
  op[1] = op[2] = op[3] = HIGHBD_WRAPLOW(e1, bd);

  ip = tmp;
  for (i = 0; i < 4; i++) {
    e1 = ip[0] >> 1;
    a1 = ip[0] - e1;
    dest[dest_stride * 0] =
        highbd_clip_pixel_add(dest[dest_stride * 0], a1, bd);
    dest[dest_stride * 1] =
        highbd_clip_pixel_add(dest[dest_stride * 1], e1, bd);
    dest[dest_stride * 2] =
        highbd_clip_pixel_add(dest[dest_stride * 2], e1, bd);
    dest[dest_stride * 3] =
        highbd_clip_pixel_add(dest[dest_stride * 3], e1, bd);
    ip++;
    dest++;
  }
}
