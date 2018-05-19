/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>
#include <assert.h>

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "aom_dsp/txfm_common.h"
#include "aom_ports/mem.h"
#include "av1/common/common.h"
#include "av1/common/arm/convolve_neon.h"
#include "av1/common/arm/mem_neon.h"
#include "av1/common/arm/transpose_neon.h"

static INLINE void compute_avg_4x4(
    uint16x4_t res0, uint16x4_t res1, uint16x4_t res2, uint16x4_t res3,
    uint16x4_t d0, uint16x4_t d1, uint16x4_t d2, uint16x4_t d3,
    const uint16_t fwd_offset, const uint16_t bck_offset,
    const int16x4_t sub_const_vec, const int16_t round_bits,
    const int32_t use_jnt_comp_avg, uint8x8_t *t0, uint8x8_t *t1) {
  int16x4_t tmp0, tmp1, tmp2, tmp3;
  uint32x4_t sum0, sum1, sum2, sum3;

  int32x4_t dst0, dst1, dst2, dst3;
  int16x8_t tmp4, tmp5;
  const int16x8_t zero = vdupq_n_s16(0);

  if (use_jnt_comp_avg) {
    int32_t dist_prec_bits = -DIST_PRECISION_BITS;
    const int32x4_t round_bits_vec = vdupq_n_s32((int32_t)(-round_bits));
    const int32x4_t const_vec = vmovl_s16(sub_const_vec);
    const int32x4_t dist_prec_bit_vec = vdupq_n_s32(dist_prec_bits);

    sum0 = vmull_n_u16(res0, fwd_offset);
    sum0 = vmlal_n_u16(sum0, d0, bck_offset);
    sum1 = vmull_n_u16(res1, fwd_offset);
    sum1 = vmlal_n_u16(sum1, d1, bck_offset);
    sum2 = vmull_n_u16(res2, fwd_offset);
    sum2 = vmlal_n_u16(sum2, d2, bck_offset);
    sum3 = vmull_n_u16(res3, fwd_offset);
    sum3 = vmlal_n_u16(sum3, d3, bck_offset);

    sum0 = vreinterpretq_u32_s32(
        vshlq_s32(vreinterpretq_s32_u32(sum0), dist_prec_bit_vec));
    sum1 = vreinterpretq_u32_s32(
        vshlq_s32(vreinterpretq_s32_u32(sum1), dist_prec_bit_vec));
    sum2 = vreinterpretq_u32_s32(
        vshlq_s32(vreinterpretq_s32_u32(sum2), dist_prec_bit_vec));
    sum3 = vreinterpretq_u32_s32(
        vshlq_s32(vreinterpretq_s32_u32(sum3), dist_prec_bit_vec));

    dst0 = vsubq_s32(vreinterpretq_s32_u32(sum0), const_vec);
    dst1 = vsubq_s32(vreinterpretq_s32_u32(sum1), const_vec);
    dst2 = vsubq_s32(vreinterpretq_s32_u32(sum2), const_vec);
    dst3 = vsubq_s32(vreinterpretq_s32_u32(sum3), const_vec);

    dst0 = vqrshlq_s32(dst0, round_bits_vec);
    dst1 = vqrshlq_s32(dst1, round_bits_vec);
    dst2 = vqrshlq_s32(dst2, round_bits_vec);
    dst3 = vqrshlq_s32(dst3, round_bits_vec);

    tmp0 = vqmovn_s32(dst0);
    tmp1 = vqmovn_s32(dst1);
    tmp2 = vqmovn_s32(dst2);
    tmp3 = vqmovn_s32(dst3);
    tmp4 = vcombine_s16(tmp0, tmp1);
    tmp5 = vcombine_s16(tmp2, tmp3);
    tmp4 = vmaxq_s16(tmp4, zero);
    tmp5 = vmaxq_s16(tmp5, zero);

    *t0 = vqmovn_u16(vreinterpretq_u16_s16(tmp4));
    *t1 = vqmovn_u16(vreinterpretq_u16_s16(tmp5));
  } else {
    const int16x4_t round_bits_vec = vdup_n_s16(-round_bits);
    tmp0 = vhadd_s16(vreinterpret_s16_u16(res0), vreinterpret_s16_u16(d0));
    tmp1 = vhadd_s16(vreinterpret_s16_u16(res1), vreinterpret_s16_u16(d1));
    tmp2 = vhadd_s16(vreinterpret_s16_u16(res2), vreinterpret_s16_u16(d2));
    tmp3 = vhadd_s16(vreinterpret_s16_u16(res3), vreinterpret_s16_u16(d3));

    tmp0 = vsub_s16(tmp0, sub_const_vec);
    tmp1 = vsub_s16(tmp1, sub_const_vec);
    tmp2 = vsub_s16(tmp2, sub_const_vec);
    tmp3 = vsub_s16(tmp3, sub_const_vec);

    tmp0 = vqrshl_s16(tmp0, round_bits_vec);
    tmp1 = vqrshl_s16(tmp1, round_bits_vec);
    tmp2 = vqrshl_s16(tmp2, round_bits_vec);
    tmp3 = vqrshl_s16(tmp3, round_bits_vec);

    tmp4 = vcombine_s16(tmp0, tmp1);
    tmp5 = vcombine_s16(tmp2, tmp3);
    tmp4 = vmaxq_s16(tmp4, zero);
    tmp5 = vmaxq_s16(tmp5, zero);

    *t0 = vqmovn_u16(vreinterpretq_u16_s16(tmp4));
    *t1 = vqmovn_u16(vreinterpretq_u16_s16(tmp5));
  }
}

static INLINE void jnt_convolve_2d_horiz_neon(
    const uint8_t *src, int src_stride, int16_t *im_block, const int im_stride,
    int16_t *x_filter_tmp, const int im_h, int w, const int round_0) {
  const int bd = 8;
  const uint8_t *s;
  int16_t *dst_ptr;
  int dst_stride;
  int width, height;
  uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7;

  dst_ptr = im_block;
  dst_stride = im_stride;
  height = im_h;
  width = w;

  if (w == 4) {
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;
    int16x8_t tt0, tt1, tt2, tt3;

    const int16x4_t horiz_const = vdup_n_s16((1 << (bd + FILTER_BITS - 2)));
    const int16x4_t shift_round_0 = vdup_n_s16(-(round_0));

    do {
      s = src;
      __builtin_prefetch(s + 0 * src_stride);
      __builtin_prefetch(s + 1 * src_stride);
      __builtin_prefetch(s + 2 * src_stride);
      __builtin_prefetch(s + 3 * src_stride);

      load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);
      tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      tt1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      tt2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      tt3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s0 = vget_low_s16(tt0);
      s1 = vget_low_s16(tt1);
      s2 = vget_low_s16(tt2);
      s3 = vget_low_s16(tt3);
      s4 = vget_high_s16(tt0);
      s5 = vget_high_s16(tt1);
      s6 = vget_high_s16(tt2);
      __builtin_prefetch(dst_ptr + 0 * dst_stride);
      __builtin_prefetch(dst_ptr + 1 * dst_stride);
      __builtin_prefetch(dst_ptr + 2 * dst_stride);
      __builtin_prefetch(dst_ptr + 3 * dst_stride);
      s += 7;

      load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);
      tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      tt1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      tt2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      tt3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s7 = vget_low_s16(tt0);
      s8 = vget_low_s16(tt1);
      s9 = vget_low_s16(tt2);
      s10 = vget_low_s16(tt3);

      d0 = convolve8_4x4_s16(s0, s1, s2, s3, s4, s5, s6, s7, x_filter_tmp,
                             horiz_const, shift_round_0);
      d1 = convolve8_4x4_s16(s1, s2, s3, s4, s5, s6, s7, s8, x_filter_tmp,
                             horiz_const, shift_round_0);
      d2 = convolve8_4x4_s16(s2, s3, s4, s5, s6, s7, s8, s9, x_filter_tmp,
                             horiz_const, shift_round_0);
      d3 = convolve8_4x4_s16(s3, s4, s5, s6, s7, s8, s9, s10, x_filter_tmp,
                             horiz_const, shift_round_0);

      transpose_s16_4x4d(&d0, &d1, &d2, &d3);

      vst1_s16((dst_ptr + 0 * dst_stride), d0);
      vst1_s16((dst_ptr + 1 * dst_stride), d1);
      vst1_s16((dst_ptr + 2 * dst_stride), d2);
      vst1_s16((dst_ptr + 3 * dst_stride), d3);

      src += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 0);
  } else {
    int16_t *d_tmp;
    int16x8_t s11, s12, s13, s14;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
    int16x8_t res0, res1, res2, res3, res4, res5, res6, res7;

    const int16x8_t horiz_const = vdupq_n_s16((1 << (bd + FILTER_BITS - 2)));
    const int16x8_t shift_round_0 = vdupq_n_s16(-(round_0));

    do {
      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);
      __builtin_prefetch(src + 4 * src_stride);
      __builtin_prefetch(src + 5 * src_stride);
      __builtin_prefetch(src + 6 * src_stride);
      __builtin_prefetch(src + 7 * src_stride);
      load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
      transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
      s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
      s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

      width = w;
      s = src + 7;
      d_tmp = dst_ptr;
      __builtin_prefetch(dst_ptr + 0 * dst_stride);
      __builtin_prefetch(dst_ptr + 1 * dst_stride);
      __builtin_prefetch(dst_ptr + 2 * dst_stride);
      __builtin_prefetch(dst_ptr + 3 * dst_stride);
      __builtin_prefetch(dst_ptr + 4 * dst_stride);
      __builtin_prefetch(dst_ptr + 5 * dst_stride);
      __builtin_prefetch(dst_ptr + 6 * dst_stride);
      __builtin_prefetch(dst_ptr + 7 * dst_stride);

      do {
        load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s10 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s11 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s12 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s13 = vreinterpretq_s16_u16(vmovl_u8(t6));
        s14 = vreinterpretq_s16_u16(vmovl_u8(t7));

        res0 = convolve8_8x8_s16(s0, s1, s2, s3, s4, s5, s6, s7, x_filter_tmp,
                                 horiz_const, shift_round_0);
        res1 = convolve8_8x8_s16(s1, s2, s3, s4, s5, s6, s7, s8, x_filter_tmp,
                                 horiz_const, shift_round_0);
        res2 = convolve8_8x8_s16(s2, s3, s4, s5, s6, s7, s8, s9, x_filter_tmp,
                                 horiz_const, shift_round_0);
        res3 = convolve8_8x8_s16(s3, s4, s5, s6, s7, s8, s9, s10, x_filter_tmp,
                                 horiz_const, shift_round_0);
        res4 = convolve8_8x8_s16(s4, s5, s6, s7, s8, s9, s10, s11, x_filter_tmp,
                                 horiz_const, shift_round_0);
        res5 = convolve8_8x8_s16(s5, s6, s7, s8, s9, s10, s11, s12,
                                 x_filter_tmp, horiz_const, shift_round_0);
        res6 = convolve8_8x8_s16(s6, s7, s8, s9, s10, s11, s12, s13,
                                 x_filter_tmp, horiz_const, shift_round_0);
        res7 = convolve8_8x8_s16(s7, s8, s9, s10, s11, s12, s13, s14,
                                 x_filter_tmp, horiz_const, shift_round_0);

        transpose_s16_8x8(&res0, &res1, &res2, &res3, &res4, &res5, &res6,
                          &res7);

        store_s16_8x8(d_tmp, dst_stride, res0, res1, res2, res3, res4, res5,
                      res6, res7);
        s0 = s8;
        s1 = s9;
        s2 = s10;
        s3 = s11;
        s4 = s12;
        s5 = s13;
        s6 = s14;
        s += 8;
        d_tmp += 8;
        width -= 8;
      } while (width > 0);
      src += 8 * src_stride;
      dst_ptr += 8 * dst_stride;
      height -= 8;
    } while (height > 0);
  }
}

static INLINE void jnt_convolve_2d_vert_neon(
    int16_t *im_block, const int im_stride, uint8_t *dst8, int dst8_stride,
    ConvolveParams *conv_params, const int16_t *y_filter, int h, int w) {
  uint8_t *dst_u8_ptr, *d_u8;
  CONV_BUF_TYPE *dst_ptr, *dst;
  int16_t *src_ptr, *s;
  uint16_t *d;

  const int bd = 8;
  int height;
  int dst_stride = conv_params->dst_stride;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int16_t sub_const = (1 << (offset_bits - conv_params->round_1)) +
                            (1 << (offset_bits - conv_params->round_1 - 1));

  const int16_t round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const int offset = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int32x4_t round_shift_vec = vdupq_n_s32(-(conv_params->round_1));
  const int32x4_t offset_const = vdupq_n_s32(1 << offset);
  const int16x4_t sub_const_vec = vdup_n_s16(sub_const);
  const uint16_t fwd_offset = conv_params->fwd_offset;
  const uint16_t bck_offset = conv_params->bck_offset;
  const int do_average = conv_params->do_average;
  const int use_jnt_comp_avg = conv_params->use_jnt_comp_avg;

  int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
  uint16x4_t res4, res5, res6, res7;
  uint16x4_t d0, d1, d2, d3;
  uint8x8_t t0, t1;

  dst = conv_params->dst;
  src_ptr = im_block;
  dst_u8_ptr = dst8;
  dst_ptr = dst;
  height = h;

  do {
    d = dst_ptr;
    d_u8 = dst_u8_ptr;
    s = src_ptr;
    height = h;

    __builtin_prefetch(s + 0 * im_stride);
    __builtin_prefetch(s + 1 * im_stride);
    __builtin_prefetch(s + 2 * im_stride);
    __builtin_prefetch(s + 3 * im_stride);
    __builtin_prefetch(s + 4 * im_stride);
    __builtin_prefetch(s + 5 * im_stride);
    __builtin_prefetch(s + 6 * im_stride);
    __builtin_prefetch(s + 7 * im_stride);

    load_s16_4x8(s, im_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);
    s += (7 * im_stride);

    do {
      load_s16_4x4(s, im_stride, &s7, &s8, &s9, &s10);
      s += (im_stride << 2);

      __builtin_prefetch(d + 0 * dst_stride);
      __builtin_prefetch(d + 1 * dst_stride);
      __builtin_prefetch(d + 2 * dst_stride);
      __builtin_prefetch(d + 3 * dst_stride);

      __builtin_prefetch(d_u8 + 4 * dst8_stride);
      __builtin_prefetch(d_u8 + 5 * dst8_stride);
      __builtin_prefetch(d_u8 + 6 * dst8_stride);
      __builtin_prefetch(d_u8 + 7 * dst8_stride);

      d0 = convolve8_4x4_s32(s0, s1, s2, s3, s4, s5, s6, s7, y_filter,
                             round_shift_vec, offset_const);
      d1 = convolve8_4x4_s32(s1, s2, s3, s4, s5, s6, s7, s8, y_filter,
                             round_shift_vec, offset_const);
      d2 = convolve8_4x4_s32(s2, s3, s4, s5, s6, s7, s8, s9, y_filter,
                             round_shift_vec, offset_const);
      d3 = convolve8_4x4_s32(s3, s4, s5, s6, s7, s8, s9, s10, y_filter,
                             round_shift_vec, offset_const);

      if (do_average) {
        load_u16_4x4(d, dst_stride, &res4, &res5, &res6, &res7);
        d += (dst_stride << 2);

        compute_avg_4x4(res4, res5, res6, res7, d0, d1, d2, d3, fwd_offset,
                        bck_offset, sub_const_vec, round_bits, use_jnt_comp_avg,
                        &t0, &t1);

        vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(t0),
                      0);  // 00 01 02 03
        d_u8 += dst8_stride;
        vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(t0),
                      1);  // 10 11 12 13
        d_u8 += dst8_stride;
        vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(t1),
                      0);  // 20 21 22 23
        d_u8 += dst8_stride;
        vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(t1),
                      1);  // 30 31 32 33
        d_u8 += dst8_stride;

      } else {
        store_u16_4x4(d, dst_stride, d0, d1, d2, d3);
        d += (dst_stride << 2);
      }
      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      height -= 4;
    } while (height > 0);
    src_ptr += 4;
    dst_ptr += 4;
    dst_u8_ptr += 4;
    w -= 4;
  } while (w > 0);
}

void av1_jnt_convolve_2d_neon(const uint8_t *src, int src_stride, uint8_t *dst8,
                              int dst8_stride, int w, int h,
                              InterpFilterParams *filter_params_x,
                              InterpFilterParams *filter_params_y,
                              const int subpel_x_q4, const int subpel_y_q4,
                              ConvolveParams *conv_params) {
  assert(!(w % 4));
  assert(!(h % 4));

  DECLARE_ALIGNED(16, int16_t,
                  im_block[(MAX_SB_SIZE + HORIZ_EXTRA_ROWS) * MAX_SB_SIZE]);

  const int im_h = h + filter_params_y->taps - 1;
  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = filter_params_y->taps / 2 - 1;
  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const int round_0 = conv_params->round_0 - 1;
  const uint8_t *src_ptr = src - vert_offset * src_stride - horiz_offset;
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);

  int16_t x_filter_tmp[8];
  int16x8_t filter_x_coef = vld1q_s16(x_filter);

  // filter coeffs are even, so downshifting by 1 to reduce intermediate
  // precision requirements.
  filter_x_coef = vshrq_n_s16(filter_x_coef, 1);
  vst1q_s16(&x_filter_tmp[0], filter_x_coef);

  jnt_convolve_2d_horiz_neon(src_ptr, src_stride, im_block, im_stride,
                             x_filter_tmp, im_h, w, round_0);

  jnt_convolve_2d_vert_neon(im_block, im_stride, dst8, dst8_stride, conv_params,
                            y_filter, h, w);
}
