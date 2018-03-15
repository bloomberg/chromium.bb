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

void aom_idct4_c(const tran_low_t *input, tran_low_t *output) {
  tran_low_t step[4];
  tran_high_t temp1, temp2;
  // stage 1
  temp1 = (input[0] + input[2]) * cospi_16_64;
  temp2 = (input[0] - input[2]) * cospi_16_64;
  step[0] = WRAPLOW(dct_const_round_shift(temp1));
  step[1] = WRAPLOW(dct_const_round_shift(temp2));
  temp1 = input[1] * cospi_24_64 - input[3] * cospi_8_64;
  temp2 = input[1] * cospi_8_64 + input[3] * cospi_24_64;
  step[2] = WRAPLOW(dct_const_round_shift(temp1));
  step[3] = WRAPLOW(dct_const_round_shift(temp2));

  // stage 2
  output[0] = WRAPLOW(step[0] + step[3]);
  output[1] = WRAPLOW(step[1] + step[2]);
  output[2] = WRAPLOW(step[1] - step[2]);
  output[3] = WRAPLOW(step[0] - step[3]);
}

void aom_idct4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int stride) {
  tran_low_t out[4 * 4];
  tran_low_t *outptr = out;
  int i, j;
  tran_low_t temp_in[4], temp_out[4];

  // Rows
  for (i = 0; i < 4; ++i) {
    aom_idct4_c(input, outptr);
    input += 4;
    outptr += 4;
  }

  // Columns
  for (i = 0; i < 4; ++i) {
    for (j = 0; j < 4; ++j) temp_in[j] = out[j * 4 + i];
    aom_idct4_c(temp_in, temp_out);
    for (j = 0; j < 4; ++j) {
      dest[j * stride + i] = clip_pixel_add(dest[j * stride + i],
                                            ROUND_POWER_OF_TWO(temp_out[j], 4));
    }
  }
}

void aom_idct4x4_1_add_c(const tran_low_t *input, uint8_t *dest,
                         int dest_stride) {
  int i;
  tran_high_t a1;
  tran_low_t out = WRAPLOW(dct_const_round_shift(input[0] * cospi_16_64));
  out = WRAPLOW(dct_const_round_shift(out * cospi_16_64));
  a1 = ROUND_POWER_OF_TWO(out, 4);

  if (a1 == 0) return;

  for (i = 0; i < 4; i++) {
    dest[0] = clip_pixel_add(dest[0], a1);
    dest[1] = clip_pixel_add(dest[1], a1);
    dest[2] = clip_pixel_add(dest[2], a1);
    dest[3] = clip_pixel_add(dest[3], a1);
    dest += dest_stride;
  }
}

void aom_iadst4_c(const tran_low_t *input, tran_low_t *output) {
  tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;

  tran_low_t x0 = input[0];
  tran_low_t x1 = input[1];
  tran_low_t x2 = input[2];
  tran_low_t x3 = input[3];

  if (!(x0 | x1 | x2 | x3)) {
    output[0] = output[1] = output[2] = output[3] = 0;
    return;
  }

  s0 = sinpi_1_9 * x0;
  s1 = sinpi_2_9 * x0;
  s2 = sinpi_3_9 * x1;
  s3 = sinpi_4_9 * x2;
  s4 = sinpi_1_9 * x2;
  s5 = sinpi_2_9 * x3;
  s6 = sinpi_4_9 * x3;
  s7 = WRAPLOW(x0 - x2 + x3);

  s0 = s0 + s3 + s5;
  s1 = s1 - s4 - s6;
  s3 = s2;
  s2 = sinpi_3_9 * s7;

  // 1-D transform scaling factor is sqrt(2).
  // The overall dynamic range is 14b (input) + 14b (multiplication scaling)
  // + 1b (addition) = 29b.
  // Hence the output bit depth is 15b.
  output[0] = WRAPLOW(dct_const_round_shift(s0 + s3));
  output[1] = WRAPLOW(dct_const_round_shift(s1 + s3));
  output[2] = WRAPLOW(dct_const_round_shift(s2));
  output[3] = WRAPLOW(dct_const_round_shift(s0 + s1 - s3));
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
