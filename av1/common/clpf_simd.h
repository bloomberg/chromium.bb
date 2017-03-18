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

#include "./aom_dsp_rtcd.h"
#include "aom_ports/mem.h"
#include "aom_ports/bitops.h"
#include "av1/common/clpf_simd_kernel.h"

// Process blocks of width 8, two lines at a time, 8 bit.
static void clpf_block8(const uint8_t *src, uint8_t *dst, int sstride,
                        int dstride, int x0, int y0, int sizey,
                        BOUNDARY_TYPE bt, unsigned int strength,
                        unsigned int dmp) {
  const int bottom = bt & TILE_BOTTOM_BOUNDARY ? sizey - 2 : -1;
  const int right = !(bt & TILE_RIGHT_BOUNDARY);
  const int left = !(bt & TILE_LEFT_BOUNDARY);
  const int top = bt & TILE_ABOVE_BOUNDARY ? 0 : -1;
  DECLARE_ALIGNED(16, static const uint64_t,
                  c_shuff[]) = { 0x0504030201000000LL, 0x0d0c0b0a09080808LL };
  DECLARE_ALIGNED(16, static const uint64_t,
                  d_shuff[]) = { 0x0605040302010000LL, 0x0e0d0c0b0a090808LL };
  DECLARE_ALIGNED(16, static const uint64_t,
                  e_shuff[]) = { 0x0707060504030201LL, 0x0f0f0e0d0c0b0a09LL };
  DECLARE_ALIGNED(16, static const uint64_t,
                  f_shuff[]) = { 0x0707070605040302LL, 0x0f0f0f0e0d0c0b0aLL };
  int y;

  dst += x0 + y0 * dstride;
  src += x0 + y0 * sstride;

  for (y = 0; y < sizey; y += 2) {
    const v64 l1 = v64_load_aligned(src);
    const v64 l2 = v64_load_aligned(src + sstride);
    const v64 l3 = v64_load_aligned(src - (y != top) * sstride);
    const v64 l4 = v64_load_aligned(src + ((y != bottom) + 1) * sstride);
    v128 o = v128_from_v64(l1, l2);
    const v128 a =
        v128_from_v64(v64_load_aligned(src - 2 * (y != top) * sstride), l3);
    const v128 b = v128_from_v64(l3, l1);
    const v128 g = v128_from_v64(l2, l4);
    const v128 h = v128_from_v64(
        l4, v64_load_aligned(src + (2 * (y != bottom) + 1) * sstride));
    v128 c, d, e, f;

    if (left) {
      c = v128_from_v64(v64_load_unaligned(src - 2),
                        v64_load_unaligned(src - 2 + sstride));
      d = v128_from_v64(v64_load_unaligned(src - 1),
                        v64_load_unaligned(src - 1 + sstride));
    } else {  // Left clipping
      c = v128_shuffle_8(o, v128_load_aligned(c_shuff));
      d = v128_shuffle_8(o, v128_load_aligned(d_shuff));
    }
    if (right) {
      e = v128_from_v64(v64_load_unaligned(src + 1),
                        v64_load_unaligned(src + 1 + sstride));
      f = v128_from_v64(v64_load_unaligned(src + 2),
                        v64_load_unaligned(src + 2 + sstride));
    } else {  // Right clipping
      e = v128_shuffle_8(o, v128_load_aligned(e_shuff));
      f = v128_shuffle_8(o, v128_load_aligned(f_shuff));
    }

    o = calc_delta(o, a, b, c, d, e, f, g, h, strength, dmp);
    v64_store_aligned(dst, v128_high_v64(o));
    v64_store_aligned(dst + dstride, v128_low_v64(o));
    src += sstride * 2;
    dst += dstride * 2;
  }
}

// As above, but with no clipping tests
static void clpf_block8_noclip(const uint8_t *src, uint8_t *dst, int sstride,
                               int dstride, int x0, int y0, int sizey,
                               unsigned int strength, unsigned int dmp) {
  int y;

  dst += x0 + y0 * dstride;
  src += x0 + y0 * sstride;

  for (y = 0; y < sizey; y += 2) {
    const v64 l1 = v64_load_aligned(src);
    const v64 l2 = v64_load_aligned(src + sstride);
    const v64 l3 = v64_load_aligned(src - sstride);
    const v64 l4 = v64_load_aligned(src + 2 * sstride);
    const v128 a = v128_from_v64(v64_load_aligned(src - 2 * sstride), l3);
    const v128 b = v128_from_v64(l3, l1);
    const v128 g = v128_from_v64(l2, l4);
    const v128 h = v128_from_v64(l4, v64_load_aligned(src + 3 * sstride));
    const v128 c = v128_from_v64(v64_load_unaligned(src - 2),
                                 v64_load_unaligned(src - 2 + sstride));
    const v128 d = v128_from_v64(v64_load_unaligned(src - 1),
                                 v64_load_unaligned(src - 1 + sstride));
    const v128 e = v128_from_v64(v64_load_unaligned(src + 1),
                                 v64_load_unaligned(src + 1 + sstride));
    const v128 f = v128_from_v64(v64_load_unaligned(src + 2),
                                 v64_load_unaligned(src + 2 + sstride));
    const v128 o = calc_delta(v128_from_v64(l1, l2), a, b, c, d, e, f, g, h,
                              strength, dmp);

    v64_store_aligned(dst, v128_high_v64(o));
    v64_store_aligned(dst + dstride, v128_low_v64(o));
    src += sstride * 2;
    dst += dstride * 2;
  }
}

// Process blocks of width 4, four lines at a time, 8 bit.
static void clpf_block4(const uint8_t *src, uint8_t *dst, int sstride,
                        int dstride, int x0, int y0, int sizey,
                        BOUNDARY_TYPE bt, unsigned int strength,
                        unsigned int dmp) {
  const int right = !(bt & TILE_RIGHT_BOUNDARY);
  const int bottom = bt & TILE_BOTTOM_BOUNDARY ? sizey - 4 : -1;
  const int left = !(bt & TILE_LEFT_BOUNDARY);
  const int top = bt & TILE_ABOVE_BOUNDARY ? 0 : -1;

  DECLARE_ALIGNED(16, static const uint64_t,
                  c_shuff[]) = { 0x0504040401000000LL, 0x0d0c0c0c09080808LL };
  DECLARE_ALIGNED(16, static const uint64_t,
                  d_shuff[]) = { 0x0605040402010000LL, 0x0e0d0c0c0a090808LL };
  DECLARE_ALIGNED(16, static const uint64_t,
                  e_shuff[]) = { 0x0707060503030201LL, 0x0f0f0e0d0b0b0a09LL };
  DECLARE_ALIGNED(16, static const uint64_t,
                  f_shuff[]) = { 0x0707070603030302LL, 0x0f0f0f0e0b0b0b0aLL };
  int y;

  dst += x0 + y0 * dstride;
  src += x0 + y0 * sstride;

  for (y = 0; y < sizey; y += 4) {
    const uint32_t l0 = u32_load_aligned(src - 2 * (y != top) * sstride);
    const uint32_t l1 = u32_load_aligned(src - (y != top) * sstride);
    const uint32_t l2 = u32_load_aligned(src);
    const uint32_t l3 = u32_load_aligned(src + sstride);
    const uint32_t l4 = u32_load_aligned(src + 2 * sstride);
    const uint32_t l5 = u32_load_aligned(src + 3 * sstride);
    const uint32_t l6 = u32_load_aligned(src + ((y != bottom) + 3) * sstride);
    const uint32_t l7 =
        u32_load_aligned(src + (2 * (y != bottom) + 3) * sstride);
    v128 o = v128_from_32(l2, l3, l4, l5);
    const v128 a = v128_from_32(l0, l1, l2, l3);
    const v128 b = v128_from_32(l1, l2, l3, l4);
    const v128 g = v128_from_32(l3, l4, l5, l6);
    const v128 h = v128_from_32(l4, l5, l6, l7);
    v128 c, d, e, f;

    if (left) {
      c = v128_from_32(u32_load_unaligned(src - 2),
                       u32_load_unaligned(src + sstride - 2),
                       u32_load_unaligned(src + 2 * sstride - 2),
                       u32_load_unaligned(src + 3 * sstride - 2));
      d = v128_from_32(u32_load_unaligned(src - 1),
                       u32_load_unaligned(src + sstride - 1),
                       u32_load_unaligned(src + 2 * sstride - 1),
                       u32_load_unaligned(src + 3 * sstride - 1));
    } else {  // Left clipping
      c = v128_shuffle_8(o, v128_load_aligned(c_shuff));
      d = v128_shuffle_8(o, v128_load_aligned(d_shuff));
    }
    if (right) {
      e = v128_from_32(u32_load_unaligned(src + 1),
                       u32_load_unaligned(src + sstride + 1),
                       u32_load_unaligned(src + 2 * sstride + 1),
                       u32_load_unaligned(src + 3 * sstride + 1));
      f = v128_from_32(u32_load_unaligned(src + 2),
                       u32_load_unaligned(src + sstride + 2),
                       u32_load_unaligned(src + 2 * sstride + 2),
                       u32_load_unaligned(src + 3 * sstride + 2));
    } else {  // Right clipping
      e = v128_shuffle_8(o, v128_load_aligned(e_shuff));
      f = v128_shuffle_8(o, v128_load_aligned(f_shuff));
    }

    o = calc_delta(o, a, b, c, d, e, f, g, h, strength, dmp);
    u32_store_aligned(dst, v128_low_u32(v128_shr_n_byte(o, 12)));
    u32_store_aligned(dst + dstride, v128_low_u32(v128_shr_n_byte(o, 8)));
    u32_store_aligned(dst + 2 * dstride, v128_low_u32(v128_shr_n_byte(o, 4)));
    u32_store_aligned(dst + 3 * dstride, v128_low_u32(o));

    dst += 4 * dstride;
    src += 4 * sstride;
  }
}

// As above, but with no clipping tests
static void clpf_block4_noclip(const uint8_t *src, uint8_t *dst, int sstride,
                               int dstride, int x0, int y0, int sizey,
                               unsigned int strength, unsigned int dmp) {
  int y;

  dst += x0 + y0 * dstride;
  src += x0 + y0 * sstride;

  for (y = 0; y < sizey; y += 4) {
    const uint32_t l0 = u32_load_aligned(src - 2 * sstride);
    const uint32_t l1 = u32_load_aligned(src - sstride);
    const uint32_t l2 = u32_load_aligned(src);
    const uint32_t l3 = u32_load_aligned(src + sstride);
    const uint32_t l4 = u32_load_aligned(src + 2 * sstride);
    const uint32_t l5 = u32_load_aligned(src + 3 * sstride);
    const uint32_t l6 = u32_load_aligned(src + 4 * sstride);
    const uint32_t l7 = u32_load_aligned(src + 5 * sstride);
    const v128 a = v128_from_32(l0, l1, l2, l3);
    const v128 b = v128_from_32(l1, l2, l3, l4);
    const v128 g = v128_from_32(l3, l4, l5, l6);
    const v128 h = v128_from_32(l4, l5, l6, l7);
    const v128 c = v128_from_32(u32_load_unaligned(src - 2),
                                u32_load_unaligned(src + sstride - 2),
                                u32_load_unaligned(src + 2 * sstride - 2),
                                u32_load_unaligned(src + 3 * sstride - 2));
    const v128 d = v128_from_32(u32_load_unaligned(src - 1),
                                u32_load_unaligned(src + sstride - 1),
                                u32_load_unaligned(src + 2 * sstride - 1),
                                u32_load_unaligned(src + 3 * sstride - 1));
    const v128 e = v128_from_32(u32_load_unaligned(src + 1),
                                u32_load_unaligned(src + sstride + 1),
                                u32_load_unaligned(src + 2 * sstride + 1),
                                u32_load_unaligned(src + 3 * sstride + 1));
    const v128 f = v128_from_32(u32_load_unaligned(src + 2),
                                u32_load_unaligned(src + sstride + 2),
                                u32_load_unaligned(src + 2 * sstride + 2),
                                u32_load_unaligned(src + 3 * sstride + 2));

    const v128 o = calc_delta(v128_from_32(l2, l3, l4, l5), a, b, c, d, e, f, g,
                              h, strength, dmp);

    u32_store_aligned(dst, v128_low_u32(v128_shr_n_byte(o, 12)));
    u32_store_aligned(dst + dstride, v128_low_u32(v128_shr_n_byte(o, 8)));
    u32_store_aligned(dst + 2 * dstride, v128_low_u32(v128_shr_n_byte(o, 4)));
    u32_store_aligned(dst + 3 * dstride, v128_low_u32(o));

    dst += 4 * dstride;
    src += 4 * sstride;
  }
}

void SIMD_FUNC(aom_clpf_block)(const uint8_t *src, uint8_t *dst, int sstride,
                               int dstride, int x0, int y0, int sizex,
                               int sizey, unsigned int strength,
                               BOUNDARY_TYPE bt, unsigned int dmp) {
  if ((sizex != 4 && sizex != 8) || ((sizey & 3) && sizex == 4)) {
    // Fallback to C for odd sizes:
    // * block widths not 4 or 8
    // * block heights not a multiple of 4 if the block width is 4
    aom_clpf_block_c(src, dst, sstride, dstride, x0, y0, sizex, sizey, strength,
                     bt, dmp);
  } else {
    if (bt)
      (sizex == 4 ? clpf_block4 : clpf_block8)(src, dst, sstride, dstride, x0,
                                               y0, sizey, bt, strength, dmp);
    else
      (sizex == 4 ? clpf_block4_noclip : clpf_block8_noclip)(
          src, dst, sstride, dstride, x0, y0, sizey, strength, dmp);
  }
}

#if defined(CONFIG_AOM_HIGHBITDEPTH)
// sign(a - b) * max(0, abs(a - b) - max(0, abs(a - b) -
// strength + (abs(a - b) >> (dmp - log2(s)))))
SIMD_INLINE v128 constrain_hbd(v128 a, v128 b, unsigned int strength,
                               unsigned int dmp) {
  const v128 diff = v128_sub_16(v128_max_s16(a, b), v128_min_s16(a, b));
  const v128 sign = v128_cmpeq_16(v128_min_s16(a, b), a);  // -(a <= b)
  const v128 zero = v128_zero();
  const v128 s = v128_max_s16(
      zero, v128_sub_16(v128_dup_16(strength),
                        v128_shr_u16(diff, dmp - get_msb(strength))));
  return v128_sub_16(
      v128_xor(sign,
               v128_max_s16(
                   zero, v128_sub_16(
                             diff, v128_max_s16(zero, v128_sub_16(diff, s))))),
      sign);
}

// delta = 1/16 * constrain(a, x, s, dmp) + 3/16 * constrain(b, x, s, dmp) +
//         1/16 * constrain(c, x, s, dmp) + 3/16 * constrain(d, x, s, dmp) +
//         3/16 * constrain(e, x, s, dmp) + 1/16 * constrain(f, x, s, dmp) +
//         3/16 * constrain(g, x, s, dmp) + 1/16 * constrain(h, x, s, dmp)
SIMD_INLINE v128 calc_delta_hbd(v128 x, v128 a, v128 b, v128 c, v128 d, v128 e,
                                v128 f, v128 g, v128 h, unsigned int s,
                                unsigned int dmp) {
  const v128 bdeg = v128_add_16(
      v128_add_16(constrain_hbd(b, x, s, dmp), constrain_hbd(d, x, s, dmp)),
      v128_add_16(constrain_hbd(e, x, s, dmp), constrain_hbd(g, x, s, dmp)));
  const v128 delta = v128_add_16(
      v128_add_16(
          v128_add_16(constrain_hbd(a, x, s, dmp), constrain_hbd(c, x, s, dmp)),
          v128_add_16(constrain_hbd(f, x, s, dmp),
                      constrain_hbd(h, x, s, dmp))),
      v128_add_16(v128_add_16(bdeg, bdeg), bdeg));
  return v128_add_16(
      x,
      v128_shr_s16(
          v128_add_16(v128_dup_16(8),
                      v128_add_16(delta, v128_cmplt_s16(delta, v128_zero()))),
          4));
}

static void calc_delta_hbd4(v128 o, v128 a, v128 b, v128 c, v128 d, v128 e,
                            v128 f, v128 g, v128 h, uint16_t *dst,
                            unsigned int s, unsigned int dmp, int dstride) {
  o = calc_delta_hbd(o, a, b, c, d, e, f, g, h, s, dmp);
  v64_store_aligned(dst, v128_high_v64(o));
  v64_store_aligned(dst + dstride, v128_low_v64(o));
}

static void calc_delta_hbd8(v128 o, v128 a, v128 b, v128 c, v128 d, v128 e,
                            v128 f, v128 g, v128 h, uint16_t *dst,
                            unsigned int s, unsigned int dmp) {
  v128_store_aligned(dst, calc_delta_hbd(o, a, b, c, d, e, f, g, h, s, dmp));
}

// Process blocks of width 4, two lines at time.
SIMD_INLINE void clpf_block_hbd4(const uint16_t *src, uint16_t *dst,
                                 int sstride, int dstride, int x0, int y0,
                                 int sizey, unsigned int strength,
                                 BOUNDARY_TYPE bt, unsigned int dmp) {
  const int right = !(bt & TILE_RIGHT_BOUNDARY);
  const int bottom = bt & TILE_BOTTOM_BOUNDARY ? sizey - 2 : -1;
  const int left = !(bt & TILE_LEFT_BOUNDARY);
  const int top = bt & TILE_ABOVE_BOUNDARY ? 0 : -1;

  DECLARE_ALIGNED(16, static const uint64_t,
                  c_shuff[]) = { 0x0302010001000100LL, 0x0b0a090809080908LL };
  DECLARE_ALIGNED(16, static const uint64_t,
                  d_shuff[]) = { 0x0504030201000100LL, 0x0d0c0b0a09080908LL };
  DECLARE_ALIGNED(16, static const uint64_t,
                  e_shuff[]) = { 0x0706070605040302LL, 0x0f0e0f0e0d0c0b0aLL };
  DECLARE_ALIGNED(16, static const uint64_t,
                  f_shuff[]) = { 0x0706070607060504LL, 0x0f0e0f0e0f0e0d0cLL };
  int y;

  dst += x0 + y0 * dstride;
  src += x0 + y0 * sstride;

  for (y = 0; y < sizey; y += 2) {
    const v64 l1 = v64_load_aligned(src);
    const v64 l2 = v64_load_aligned(src + sstride);
    const v64 l3 = v64_load_aligned(src - (y != top) * sstride);
    const v64 l4 = v64_load_aligned(src + ((y != bottom) + 1) * sstride);
    v128 o = v128_from_v64(l1, l2);
    const v128 a =
        v128_from_v64(v64_load_aligned(src - 2 * (y != top) * sstride), l3);
    const v128 b = v128_from_v64(l3, l1);
    const v128 g = v128_from_v64(l2, l4);
    const v128 h = v128_from_v64(
        l4, v64_load_aligned(src + (2 * (y != bottom) + 1) * sstride));
    v128 c, d, e, f;

    if (left) {
      c = v128_from_v64(v64_load_unaligned(src - 2),
                        v64_load_unaligned(src - 2 + sstride));
      d = v128_from_v64(v64_load_unaligned(src - 1),
                        v64_load_unaligned(src - 1 + sstride));
    } else {  // Left clipping
      c = v128_shuffle_8(o, v128_load_aligned(c_shuff));
      d = v128_shuffle_8(o, v128_load_aligned(d_shuff));
    }
    if (right) {
      e = v128_from_v64(v64_load_unaligned(src + 1),
                        v64_load_unaligned(src + 1 + sstride));
      f = v128_from_v64(v64_load_unaligned(src + 2),
                        v64_load_unaligned(src + 2 + sstride));
    } else {  // Right clipping
      e = v128_shuffle_8(o, v128_load_aligned(e_shuff));
      f = v128_shuffle_8(o, v128_load_aligned(f_shuff));
    }
    calc_delta_hbd4(o, a, b, c, d, e, f, g, h, dst, strength, dmp, dstride);
    src += sstride * 2;
    dst += dstride * 2;
  }
}

// As above, but with no clipping tests
SIMD_INLINE void clpf_block_hbd4_noclip(const uint16_t *src, uint16_t *dst,
                                        int sstride, int dstride, int x0,
                                        int y0, int sizey,
                                        unsigned int strength,
                                        unsigned int dmp) {
  int y;

  dst += x0 + y0 * dstride;
  src += x0 + y0 * sstride;

  for (y = 0; y < sizey; y += 2) {
    const v64 l1 = v64_load_aligned(src);
    const v64 l2 = v64_load_aligned(src + sstride);
    const v64 l3 = v64_load_aligned(src - sstride);
    const v64 l4 = v64_load_aligned(src + 2 * sstride);
    const v128 a = v128_from_v64(v64_load_aligned(src - 2 * sstride), l3);
    const v128 b = v128_from_v64(l3, l1);
    const v128 g = v128_from_v64(l2, l4);
    const v128 h = v128_from_v64(l4, v64_load_aligned(src + 3 * sstride));
    const v128 c = v128_from_v64(v64_load_unaligned(src - 2),
                                 v64_load_unaligned(src - 2 + sstride));
    const v128 d = v128_from_v64(v64_load_unaligned(src - 1),
                                 v64_load_unaligned(src - 1 + sstride));
    const v128 e = v128_from_v64(v64_load_unaligned(src + 1),
                                 v64_load_unaligned(src + 1 + sstride));
    const v128 f = v128_from_v64(v64_load_unaligned(src + 2),
                                 v64_load_unaligned(src + 2 + sstride));

    calc_delta_hbd4(v128_from_v64(l1, l2), a, b, c, d, e, f, g, h, dst,
                    strength, dmp, dstride);
    src += sstride * 2;
    dst += dstride * 2;
  }
}

// The most simple case.  Start here if you need to understand the functions.
SIMD_INLINE void clpf_block_hbd(const uint16_t *src, uint16_t *dst, int sstride,
                                int dstride, int x0, int y0, int sizey,
                                unsigned int strength, BOUNDARY_TYPE bt,
                                unsigned int dmp) {
  const int right = !(bt & TILE_RIGHT_BOUNDARY);
  const int left = !(bt & TILE_LEFT_BOUNDARY);
  const int ymin = -!(bt & TILE_ABOVE_BOUNDARY) * 2;
  const int ymax = sizey + !(bt & TILE_BOTTOM_BOUNDARY) * 2 - 1;

  DECLARE_ALIGNED(16, static const uint64_t,
                  c_shuff[]) = { 0x0302010001000100LL, 0x0b0a090807060504LL };
  DECLARE_ALIGNED(16, static const uint64_t,
                  d_shuff[]) = { 0x0504030201000100LL, 0x0d0c0b0a09080706LL };
  DECLARE_ALIGNED(16, static const uint64_t,
                  e_shuff[]) = { 0x0908070605040302LL, 0x0f0e0f0e0d0c0b0aLL };
  DECLARE_ALIGNED(16, static const uint64_t,
                  f_shuff[]) = { 0x0b0a090807060504LL, 0x0f0e0f0e0f0e0d0cLL };
  int y;

  dst += x0 + y0 * dstride;
  src += x0 + y0 * sstride;

  // Read 8 set of pixels at a time.  Clipping along upper and lower
  // edges is handled by reading the upper or lower line twice.
  // Clipping along the left and right edges is handled by shuffle
  // instructions doing shift and pad.
  for (y = 0; y < sizey; y++) {
    const v128 o = v128_load_aligned(src + y * sstride);
    const v128 a = v128_load_aligned(src + AOMMAX(ymin, y - 2) * sstride);
    const v128 b = v128_load_aligned(src + AOMMAX(ymin, y - 1) * sstride);
    const v128 g = v128_load_aligned(src + AOMMIN(ymax, y + 1) * sstride);
    const v128 h = v128_load_aligned(src + AOMMIN(ymax, y + 2) * sstride);
    v128 c, d, e, f;

    if (left) {
      c = v128_load_unaligned(src + y * sstride - 2);
      d = v128_load_unaligned(src + y * sstride - 1);
    } else {  // Left clipping
      c = v128_shuffle_8(o, v128_load_aligned(c_shuff));
      d = v128_shuffle_8(o, v128_load_aligned(d_shuff));
    }
    if (right) {
      e = v128_load_unaligned(src + y * sstride + 1);
      f = v128_load_unaligned(src + y * sstride + 2);
    } else {  // Right clipping
      e = v128_shuffle_8(o, v128_load_aligned(e_shuff));
      f = v128_shuffle_8(o, v128_load_aligned(f_shuff));
    }
    calc_delta_hbd8(o, a, b, c, d, e, f, g, h, dst, strength, dmp);
    dst += dstride;
  }
}

// As above, but with no clipping tests
SIMD_INLINE void clpf_block_hbd_noclip(const uint16_t *src, uint16_t *dst,
                                       int sstride, int dstride, int x0, int y0,
                                       int sizey, unsigned int strength,
                                       unsigned int dmp) {
  int y;

  dst += x0 + y0 * dstride;
  src += x0 + y0 * sstride;

  for (y = 0; y < sizey; y++) {
    const v128 o = v128_load_aligned(src);
    const v128 a = v128_load_aligned(src - 2 * sstride);
    const v128 b = v128_load_aligned(src - 1 * sstride);
    const v128 g = v128_load_aligned(src + sstride);
    const v128 h = v128_load_aligned(src + 2 * sstride);
    const v128 c = v128_load_unaligned(src - 2);
    const v128 d = v128_load_unaligned(src - 1);
    const v128 e = v128_load_unaligned(src + 1);
    const v128 f = v128_load_unaligned(src + 2);

    calc_delta_hbd8(o, a, b, c, d, e, f, g, h, dst, strength, dmp);
    src += sstride;
    dst += dstride;
  }
}

void SIMD_FUNC(aom_clpf_block_hbd)(const uint16_t *src, uint16_t *dst,
                                   int sstride, int dstride, int x0, int y0,
                                   int sizex, int sizey, unsigned int strength,
                                   BOUNDARY_TYPE bt, unsigned int dmp) {
  if ((sizex != 4 && sizex != 8) || ((sizey & 1) && sizex == 4)) {
    // Fallback to C for odd sizes:
    // * block width not 4 or 8
    // * block heights not a multiple of 2 if the block width is 4
    aom_clpf_block_hbd_c(src, dst, sstride, dstride, x0, y0, sizex, sizey,
                         strength, bt, dmp);
  } else {
    if (bt)
      (sizex == 4 ? clpf_block_hbd4 : clpf_block_hbd)(
          src, dst, sstride, dstride, x0, y0, sizey, strength, bt, dmp);
    else
      (sizex == 4 ? clpf_block_hbd4_noclip : clpf_block_hbd_noclip)(
          src, dst, sstride, dstride, x0, y0, sizey, strength, dmp);
  }
}
#endif
