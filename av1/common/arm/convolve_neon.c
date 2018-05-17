/*
 *
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <arm_neon.h>

#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "av1/common/arm/mem_neon.h"
#include "av1/common/arm/transpose_neon.h"

static INLINE int16x4_t convolve8_4x4(const int16x4_t s0, const int16x4_t s1,
                                      const int16x4_t s2, const int16x4_t s3,
                                      const int16x4_t s4, const int16x4_t s5,
                                      const int16x4_t s6, const int16x4_t s7,
                                      const int16_t *filter) {
  int16x4_t sum;

  sum = vmul_n_s16(s0, filter[0]);
  sum = vmla_n_s16(sum, s1, filter[1]);
  sum = vmla_n_s16(sum, s2, filter[2]);
  sum = vmla_n_s16(sum, s5, filter[5]);
  sum = vmla_n_s16(sum, s6, filter[6]);
  sum = vmla_n_s16(sum, s7, filter[7]);
  /* filter[3] can take a max value of 128. So the max value of the result :
   * 128*255 + sum > 16 bits
   */
  sum = vqadd_s16(sum, vmul_n_s16(s3, filter[3]));
  sum = vqadd_s16(sum, vmul_n_s16(s4, filter[4]));

  return sum;
}

static INLINE uint8x8_t convolve8_horiz_8x8(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16_t *filter,
    const int16x8_t shift_round_0, const int16x8_t shift_by_bits) {
  int16x8_t sum;

  sum = vmulq_n_s16(s0, filter[0]);
  sum = vmlaq_n_s16(sum, s1, filter[1]);
  sum = vmlaq_n_s16(sum, s2, filter[2]);
  sum = vmlaq_n_s16(sum, s5, filter[5]);
  sum = vmlaq_n_s16(sum, s6, filter[6]);
  sum = vmlaq_n_s16(sum, s7, filter[7]);
  /* filter[3] can take a max value of 128. So the max value of the result :
   * 128*255 + sum > 16 bits
   */
  sum = vqaddq_s16(sum, vmulq_n_s16(s3, filter[3]));
  sum = vqaddq_s16(sum, vmulq_n_s16(s4, filter[4]));

  sum = vqrshlq_s16(sum, shift_round_0);
  sum = vqrshlq_s16(sum, shift_by_bits);

  return vqmovun_s16(sum);
}

static INLINE uint8x8_t convolve8_vert_8x4(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16_t *filter) {
  int16x8_t sum;

  sum = vmulq_n_s16(s0, filter[0]);
  sum = vmlaq_n_s16(sum, s1, filter[1]);
  sum = vmlaq_n_s16(sum, s2, filter[2]);
  sum = vmlaq_n_s16(sum, s5, filter[5]);
  sum = vmlaq_n_s16(sum, s6, filter[6]);
  sum = vmlaq_n_s16(sum, s7, filter[7]);
  /* filter[3] can take a max value of 128. So the max value of the result :
   * 128*255 + sum > 16 bits
   */
  sum = vqaddq_s16(sum, vmulq_n_s16(s3, filter[3]));
  sum = vqaddq_s16(sum, vmulq_n_s16(s4, filter[4]));

  return vqrshrun_n_s16(sum, FILTER_BITS);
}

void av1_convolve_x_sr_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            InterpFilterParams *filter_params_x,
                            InterpFilterParams *filter_params_y,
                            const int subpel_x_q4, const int subpel_y_q4,
                            ConvolveParams *conv_params) {
  const uint8_t horiz_offset = filter_params_x->taps / 2 - 1;
  const uint8_t bits = FILTER_BITS - conv_params->round_0;

  (void)subpel_y_q4;
  (void)conv_params;
  (void)filter_params_y;

  uint8x8_t t0, t1, t2, t3;

  assert(bits >= 0);
  assert((FILTER_BITS - conv_params->round_1) >= 0 ||
         ((conv_params->round_0 + conv_params->round_1) == 2 * FILTER_BITS));

  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);

  const int16x8_t shift_round_0 = vdupq_n_s16(-conv_params->round_0);
  const int16x8_t shift_by_bits = vdupq_n_s16(-bits);

  src -= horiz_offset;

  if (h == 4) {
    uint8x8_t d01, d23;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;
    int16x8_t d01_temp, d23_temp;

    __builtin_prefetch(src + 0 * src_stride);
    __builtin_prefetch(src + 1 * src_stride);
    __builtin_prefetch(src + 2 * src_stride);
    __builtin_prefetch(src + 3 * src_stride);

    load_u8_8x4(src, src_stride, &t0, &t1, &t2, &t3);
    transpose_u8_8x4(&t0, &t1, &t2, &t3);

    s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
    s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    s5 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    s6 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    __builtin_prefetch(dst + 0 * dst_stride);
    __builtin_prefetch(dst + 1 * dst_stride);
    __builtin_prefetch(dst + 2 * dst_stride);
    __builtin_prefetch(dst + 3 * dst_stride);
    src += 7;

    do {
      load_u8_8x4(src, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);

      s7 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      s9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      s10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

      d0 = convolve8_4x4(s0, s1, s2, s3, s4, s5, s6, s7, x_filter);

      d1 = convolve8_4x4(s1, s2, s3, s4, s5, s6, s7, s8, x_filter);

      d2 = convolve8_4x4(s2, s3, s4, s5, s6, s7, s8, s9, x_filter);

      d3 = convolve8_4x4(s3, s4, s5, s6, s7, s8, s9, s10, x_filter);

      d01_temp = vqrshlq_s16(vcombine_s16(d0, d1), shift_round_0);
      d23_temp = vqrshlq_s16(vcombine_s16(d2, d3), shift_round_0);

      d01_temp = vqrshlq_s16(d01_temp, shift_by_bits);
      d23_temp = vqrshlq_s16(d23_temp, shift_by_bits);

      d01 = vqmovun_s16(d01_temp);
      d23 = vqmovun_s16(d23_temp);

      transpose_u8_4x4(&d01, &d23);

      if (w != 2) {
        vst1_lane_u32((uint32_t *)(dst + 0 * dst_stride),  // 00 01 02 03
                      vreinterpret_u32_u8(d01), 0);
        vst1_lane_u32((uint32_t *)(dst + 1 * dst_stride),  // 10 11 12 13
                      vreinterpret_u32_u8(d23), 0);
        vst1_lane_u32((uint32_t *)(dst + 2 * dst_stride),  // 20 21 22 23
                      vreinterpret_u32_u8(d01), 1);
        vst1_lane_u32((uint32_t *)(dst + 3 * dst_stride),  // 30 31 32 33
                      vreinterpret_u32_u8(d23), 1);
      } else {
        vst1_lane_u16((uint16_t *)(dst + 0 * dst_stride),  // 00 01
                      vreinterpret_u16_u8(d01), 0);
        vst1_lane_u16((uint16_t *)(dst + 1 * dst_stride),  // 10 11
                      vreinterpret_u16_u8(d23), 0);
        vst1_lane_u16((uint16_t *)(dst + 2 * dst_stride),  // 20 21
                      vreinterpret_u16_u8(d01), 2);
        vst1_lane_u16((uint16_t *)(dst + 3 * dst_stride),  // 30 31
                      vreinterpret_u16_u8(d23), 2);
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      src += 4;
      dst += 4;
      w -= 4;
    } while (w > 0);
  } else {
    int width;
    const uint8_t *s;
    uint8x8_t t4, t5, t6, t7;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;

    if (w <= 4) {
      do {
        load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

        load_u8_8x8(src + 7, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6,
                    &t7);
        src += 8 * src_stride;
        __builtin_prefetch(dst + 0 * dst_stride);
        __builtin_prefetch(dst + 1 * dst_stride);
        __builtin_prefetch(dst + 2 * dst_stride);
        __builtin_prefetch(dst + 3 * dst_stride);
        __builtin_prefetch(dst + 4 * dst_stride);
        __builtin_prefetch(dst + 5 * dst_stride);
        __builtin_prefetch(dst + 6 * dst_stride);
        __builtin_prefetch(dst + 7 * dst_stride);

        transpose_u8_4x8(&t0, &t1, &t2, &t3, t4, t5, t6, t7);

        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s10 = vreinterpretq_s16_u16(vmovl_u8(t3));

        __builtin_prefetch(src + 0 * src_stride);
        __builtin_prefetch(src + 1 * src_stride);
        __builtin_prefetch(src + 2 * src_stride);
        __builtin_prefetch(src + 3 * src_stride);
        __builtin_prefetch(src + 4 * src_stride);
        __builtin_prefetch(src + 5 * src_stride);
        __builtin_prefetch(src + 6 * src_stride);
        __builtin_prefetch(src + 7 * src_stride);
        t0 = convolve8_horiz_8x8(s0, s1, s2, s3, s4, s5, s6, s7, x_filter,
                                 shift_round_0, shift_by_bits);
        t1 = convolve8_horiz_8x8(s1, s2, s3, s4, s5, s6, s7, s8, x_filter,
                                 shift_round_0, shift_by_bits);
        t2 = convolve8_horiz_8x8(s2, s3, s4, s5, s6, s7, s8, s9, x_filter,
                                 shift_round_0, shift_by_bits);
        t3 = convolve8_horiz_8x8(s3, s4, s5, s6, s7, s8, s9, s10, x_filter,
                                 shift_round_0, shift_by_bits);

        transpose_u8_8x4(&t0, &t1, &t2, &t3);

        if ((w == 4) && (h > 4)) {
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t0),
                        0);  // 00 01 02 03
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t1),
                        0);  // 10 11 12 13
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t2),
                        0);  // 20 21 22 23
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t3),
                        0);  // 30 31 32 33
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t0),
                        1);  // 40 41 42 43
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t1),
                        1);  // 50 51 52 53
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t2),
                        1);  // 60 61 62 63
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t3),
                        1);  // 70 71 72 73
          dst += dst_stride;
        } else if ((w == 4) && (h == 2)) {
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t0),
                        0);  // 00 01 02 03
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t1),
                        0);  // 10 11 12 13
          dst += dst_stride;
        } else if ((w == 2) && (h > 4)) {
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t0), 0);  // 00 01
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t1), 0);  // 10 11
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t2), 0);  // 20 21
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t3), 0);  // 30 31
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t0), 2);  // 40 41
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t1), 2);  // 50 51
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t2), 2);  // 60 61
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t3), 2);  // 70 71
          dst += dst_stride;
        } else if ((w == 2) && (h == 2)) {
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t0), 0);  // 00 01
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t1), 0);  // 10 11
          dst += dst_stride;
        }
        h -= 8;
      } while (h > 0);
    } else {
      uint8_t *d;
      int16x8_t s11, s12, s13, s14;

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
        d = dst;
        __builtin_prefetch(dst + 0 * dst_stride);
        __builtin_prefetch(dst + 1 * dst_stride);
        __builtin_prefetch(dst + 2 * dst_stride);
        __builtin_prefetch(dst + 3 * dst_stride);
        __builtin_prefetch(dst + 4 * dst_stride);
        __builtin_prefetch(dst + 5 * dst_stride);
        __builtin_prefetch(dst + 6 * dst_stride);
        __builtin_prefetch(dst + 7 * dst_stride);

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

          t0 = convolve8_horiz_8x8(s0, s1, s2, s3, s4, s5, s6, s7, x_filter,
                                   shift_round_0, shift_by_bits);

          t1 = convolve8_horiz_8x8(s1, s2, s3, s4, s5, s6, s7, s8, x_filter,
                                   shift_round_0, shift_by_bits);

          t2 = convolve8_horiz_8x8(s2, s3, s4, s5, s6, s7, s8, s9, x_filter,
                                   shift_round_0, shift_by_bits);

          t3 = convolve8_horiz_8x8(s3, s4, s5, s6, s7, s8, s9, s10, x_filter,
                                   shift_round_0, shift_by_bits);

          t4 = convolve8_horiz_8x8(s4, s5, s6, s7, s8, s9, s10, s11, x_filter,
                                   shift_round_0, shift_by_bits);

          t5 = convolve8_horiz_8x8(s5, s6, s7, s8, s9, s10, s11, s12, x_filter,
                                   shift_round_0, shift_by_bits);

          t6 = convolve8_horiz_8x8(s6, s7, s8, s9, s10, s11, s12, s13, x_filter,
                                   shift_round_0, shift_by_bits);

          t7 = convolve8_horiz_8x8(s7, s8, s9, s10, s11, s12, s13, s14,
                                   x_filter, shift_round_0, shift_by_bits);

          transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
          if (h != 2) {
            store_u8_8x8(d, dst_stride, t0, t1, t2, t3, t4, t5, t6, t7);
          } else {
            store_row2_u8_8x8(d, dst_stride, t0, t1);
          }
          s0 = s8;
          s1 = s9;
          s2 = s10;
          s3 = s11;
          s4 = s12;
          s5 = s13;
          s6 = s14;
          s += 8;
          d += 8;
          width -= 8;
        } while (width > 0);
        src += 8 * src_stride;
        dst += 8 * dst_stride;
        h -= 8;
      } while (h > 0);
    }
  }
}

void av1_convolve_y_sr_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            InterpFilterParams *filter_params_x,
                            InterpFilterParams *filter_params_y,
                            const int subpel_x_q4, const int subpel_y_q4,
                            ConvolveParams *conv_params) {
  const int vert_offset = filter_params_y->taps / 2 - 1;

  src -= vert_offset * src_stride;

  (void)filter_params_x;
  (void)subpel_x_q4;
  (void)conv_params;

  assert(conv_params->round_0 <= FILTER_BITS);
  assert(((conv_params->round_0 + conv_params->round_1) <= (FILTER_BITS + 1)) ||
         ((conv_params->round_0 + conv_params->round_1) == (2 * FILTER_BITS)));

  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_y, subpel_y_q4 & SUBPEL_MASK);

  if (w <= 4) {
    uint8x8_t d01, d23;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;

    s0 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s1 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s2 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s3 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s4 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s5 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s6 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;

    do {
      s7 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;
      s8 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;
      s9 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;
      s10 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;

      __builtin_prefetch(dst + 0 * dst_stride);
      __builtin_prefetch(dst + 1 * dst_stride);
      __builtin_prefetch(dst + 2 * dst_stride);
      __builtin_prefetch(dst + 3 * dst_stride);
      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);
      d0 = convolve8_4x4(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);
      d1 = convolve8_4x4(s1, s2, s3, s4, s5, s6, s7, s8, y_filter);
      d2 = convolve8_4x4(s2, s3, s4, s5, s6, s7, s8, s9, y_filter);
      d3 = convolve8_4x4(s3, s4, s5, s6, s7, s8, s9, s10, y_filter);

      d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS);
      d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS);
      if ((w == 4) && (h != 2)) {
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d01),
                      0);  // 00 01 02 03
        dst += dst_stride;
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d01),
                      1);  // 10 11 12 13
        dst += dst_stride;
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d23),
                      0);  // 20 21 22 23
        dst += dst_stride;
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d23),
                      1);  // 30 31 32 33
        dst += dst_stride;
      } else if ((w == 4) && (h == 2)) {
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d01),
                      0);  // 00 01 02 03
        dst += dst_stride;
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d01),
                      1);  // 10 11 12 13
        dst += dst_stride;
      } else if ((w == 2) && (h != 2)) {
        vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(d01), 0);  // 00 01
        dst += dst_stride;
        vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(d01), 2);  // 10 11
        dst += dst_stride;
        vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(d23), 0);  // 20 21
        dst += dst_stride;
        vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(d23), 2);  // 30 31
        dst += dst_stride;
      } else if ((w == 2) && (h == 2)) {
        vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(d01), 0);  // 00 01
        dst += dst_stride;
        vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(d01), 2);  // 10 11
        dst += dst_stride;
      }
      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      h -= 4;
    } while (h > 0);
  } else {
    int height;
    const uint8_t *s;
    uint8_t *d;
    uint8x8_t t0, t1, t2, t3;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;

    do {
      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);
      __builtin_prefetch(src + 4 * src_stride);
      __builtin_prefetch(src + 5 * src_stride);
      __builtin_prefetch(src + 6 * src_stride);
      s = src;
      s0 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s1 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s2 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s3 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s4 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s5 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s6 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      d = dst;
      height = h;

      do {
        s7 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;
        s8 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;
        s9 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;
        s10 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;

        __builtin_prefetch(d + 0 * dst_stride);
        __builtin_prefetch(d + 1 * dst_stride);
        __builtin_prefetch(d + 2 * dst_stride);
        __builtin_prefetch(d + 3 * dst_stride);
        __builtin_prefetch(s + 0 * src_stride);
        __builtin_prefetch(s + 1 * src_stride);
        __builtin_prefetch(s + 2 * src_stride);
        __builtin_prefetch(s + 3 * src_stride);
        t0 = convolve8_vert_8x4(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);
        t1 = convolve8_vert_8x4(s1, s2, s3, s4, s5, s6, s7, s8, y_filter);
        t2 = convolve8_vert_8x4(s2, s3, s4, s5, s6, s7, s8, s9, y_filter);
        t3 = convolve8_vert_8x4(s3, s4, s5, s6, s7, s8, s9, s10, y_filter);
        if (h != 2) {
          vst1_u8(d, t0);
          d += dst_stride;
          vst1_u8(d, t1);
          d += dst_stride;
          vst1_u8(d, t2);
          d += dst_stride;
          vst1_u8(d, t3);
          d += dst_stride;
        } else {
          vst1_u8(d, t0);
          d += dst_stride;
          vst1_u8(d, t1);
          d += dst_stride;
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
      src += 8;
      dst += 8;
      w -= 8;
    } while (w > 0);
  }
}
