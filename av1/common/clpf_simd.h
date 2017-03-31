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

#include "./av1_rtcd.h"
#include "aom_ports/mem.h"
#include "aom_ports/bitops.h"
#include "av1/common/clpf_simd_kernel.h"

// Process blocks of width 8, two lines at a time, 8 bit.
static void clpf_block8(uint8_t *dst, const uint16_t *src, int dstride,
                        int sstride, int sizey, unsigned int strength,
                        unsigned int dmp) {
  int y;

  for (y = 0; y < sizey; y += 2) {
    const v128 l1 = v128_load_aligned(src);
    const v128 l2 = v128_load_aligned(src + sstride);
    const v128 l3 = v128_load_aligned(src - sstride);
    const v128 l4 = v128_load_aligned(src + 2 * sstride);
    const v128 a = v128_pack_s16_u8(v128_load_aligned(src - 2 * sstride), l3);
    const v128 b = v128_pack_s16_u8(l3, l1);
    const v128 g = v128_pack_s16_u8(l2, l4);
    const v128 h = v128_pack_s16_u8(l4, v128_load_aligned(src + 3 * sstride));
    const v128 c = v128_pack_s16_u8(v128_load_unaligned(src - 2),
                                    v128_load_unaligned(src - 2 + sstride));
    const v128 d = v128_pack_s16_u8(v128_load_unaligned(src - 1),
                                    v128_load_unaligned(src - 1 + sstride));
    const v128 e = v128_pack_s16_u8(v128_load_unaligned(src + 1),
                                    v128_load_unaligned(src + 1 + sstride));
    const v128 f = v128_pack_s16_u8(v128_load_unaligned(src + 2),
                                    v128_load_unaligned(src + 2 + sstride));
    const v128 o = calc_delta(v128_pack_s16_u8(l1, l2), a, b, c, d, e, f, g, h,
                              strength, dmp);

    v64_store_aligned(dst, v128_high_v64(o));
    v64_store_aligned(dst + dstride, v128_low_v64(o));
    src += sstride * 2;
    dst += dstride * 2;
  }
}

// Process blocks of width 4, four lines at a time, 8 bit.
static void clpf_block4(uint8_t *dst, const uint16_t *src, int dstride,
                        int sstride, int sizey, unsigned int strength,
                        unsigned int dmp) {
  int y;

  for (y = 0; y < sizey; y += 4) {
    const v64 l0 = v64_load_aligned(src - 2 * sstride);
    const v64 l1 = v64_load_aligned(src - sstride);
    const v64 l2 = v64_load_aligned(src);
    const v64 l3 = v64_load_aligned(src + sstride);
    const v64 l4 = v64_load_aligned(src + 2 * sstride);
    const v64 l5 = v64_load_aligned(src + 3 * sstride);
    const v64 l6 = v64_load_aligned(src + 4 * sstride);
    const v64 l7 = v64_load_aligned(src + 5 * sstride);
    const v128 a =
        v128_pack_s16_u8(v128_from_v64(l0, l1), v128_from_v64(l2, l3));
    const v128 b =
        v128_pack_s16_u8(v128_from_v64(l1, l2), v128_from_v64(l3, l4));
    const v128 g =
        v128_pack_s16_u8(v128_from_v64(l3, l4), v128_from_v64(l5, l6));
    const v128 h =
        v128_pack_s16_u8(v128_from_v64(l4, l5), v128_from_v64(l6, l7));
    const v128 c = v128_pack_s16_u8(
        v128_from_v64(v64_load_unaligned(src - 2),
                      v64_load_unaligned(src + sstride - 2)),
        v128_from_v64(v64_load_unaligned(src + 2 * sstride - 2),
                      v64_load_unaligned(src + 3 * sstride - 2)));
    const v128 d = v128_pack_s16_u8(
        v128_from_v64(v64_load_unaligned(src - 1),
                      v64_load_unaligned(src + sstride - 1)),
        v128_from_v64(v64_load_unaligned(src + 2 * sstride - 1),
                      v64_load_unaligned(src + 3 * sstride - 1)));
    const v128 e = v128_pack_s16_u8(
        v128_from_v64(v64_load_unaligned(src + 1),
                      v64_load_unaligned(src + sstride + 1)),
        v128_from_v64(v64_load_unaligned(src + 2 * sstride + 1),
                      v64_load_unaligned(src + 3 * sstride + 1)));
    const v128 f = v128_pack_s16_u8(
        v128_from_v64(v64_load_unaligned(src + 2),
                      v64_load_unaligned(src + sstride + 2)),
        v128_from_v64(v64_load_unaligned(src + 2 * sstride + 2),
                      v64_load_unaligned(src + 3 * sstride + 2)));

    const v128 o = calc_delta(
        v128_pack_s16_u8(v128_from_v64(l2, l3), v128_from_v64(l4, l5)), a, b, c,
        d, e, f, g, h, strength, dmp);

    u32_store_aligned(dst, v128_low_u32(v128_shr_n_byte(o, 12)));
    u32_store_aligned(dst + dstride, v128_low_u32(v128_shr_n_byte(o, 8)));
    u32_store_aligned(dst + 2 * dstride, v128_low_u32(v128_shr_n_byte(o, 4)));
    u32_store_aligned(dst + 3 * dstride, v128_low_u32(o));

    dst += 4 * dstride;
    src += 4 * sstride;
  }
}

static void clpf_hblock8(uint8_t *dst, const uint16_t *src, int dstride,
                         int sstride, int sizey, unsigned int strength,
                         unsigned int dmp) {
  int y;

  for (y = 0; y < sizey; y += 2) {
    const v128 l1 = v128_load_aligned(src);
    const v128 l2 = v128_load_aligned(src + sstride);
    const v128 a = v128_pack_s16_u8(v128_load_unaligned(src - 2),
                                    v128_load_unaligned(src - 2 + sstride));
    const v128 b = v128_pack_s16_u8(v128_load_unaligned(src - 1),
                                    v128_load_unaligned(src - 1 + sstride));
    const v128 c = v128_pack_s16_u8(v128_load_unaligned(src + 1),
                                    v128_load_unaligned(src + 1 + sstride));
    const v128 d = v128_pack_s16_u8(v128_load_unaligned(src + 2),
                                    v128_load_unaligned(src + 2 + sstride));
    const v128 o =
        calc_hdelta(v128_pack_s16_u8(l1, l2), a, b, c, d, strength, dmp);

    v64_store_aligned(dst, v128_high_v64(o));
    v64_store_aligned(dst + dstride, v128_low_v64(o));
    src += sstride * 2;
    dst += dstride * 2;
  }
}

// Process blocks of width 4, four lines at a time, 8 bit.
static void clpf_hblock4(uint8_t *dst, const uint16_t *src, int dstride,
                         int sstride, int sizey, unsigned int strength,
                         unsigned int dmp) {
  int y;

  for (y = 0; y < sizey; y += 4) {
    const v64 l0 = v64_load_aligned(src);
    const v64 l1 = v64_load_aligned(src + sstride);
    const v64 l2 = v64_load_aligned(src + 2 * sstride);
    const v64 l3 = v64_load_aligned(src + 3 * sstride);
    const v128 a = v128_pack_s16_u8(
        v128_from_v64(v64_load_unaligned(src - 2),
                      v64_load_unaligned(src + sstride - 2)),
        v128_from_v64(v64_load_unaligned(src + 2 * sstride - 2),
                      v64_load_unaligned(src + 3 * sstride - 2)));
    const v128 b = v128_pack_s16_u8(
        v128_from_v64(v64_load_unaligned(src - 1),
                      v64_load_unaligned(src + sstride - 1)),
        v128_from_v64(v64_load_unaligned(src + 2 * sstride - 1),
                      v64_load_unaligned(src + 3 * sstride - 1)));
    const v128 c = v128_pack_s16_u8(
        v128_from_v64(v64_load_unaligned(src + 1),
                      v64_load_unaligned(src + sstride + 1)),
        v128_from_v64(v64_load_unaligned(src + 2 * sstride + 1),
                      v64_load_unaligned(src + 3 * sstride + 1)));
    const v128 d = v128_pack_s16_u8(
        v128_from_v64(v64_load_unaligned(src + 2),
                      v64_load_unaligned(src + sstride + 2)),
        v128_from_v64(v64_load_unaligned(src + 2 * sstride + 2),
                      v64_load_unaligned(src + 3 * sstride + 2)));

    const v128 o = calc_hdelta(
        v128_pack_s16_u8(v128_from_v64(l0, l1), v128_from_v64(l2, l3)), a, b, c,
        d, strength, dmp);

    u32_store_aligned(dst, v128_low_u32(v128_shr_n_byte(o, 12)));
    u32_store_aligned(dst + dstride, v128_low_u32(v128_shr_n_byte(o, 8)));
    u32_store_aligned(dst + 2 * dstride, v128_low_u32(v128_shr_n_byte(o, 4)));
    u32_store_aligned(dst + 3 * dstride, v128_low_u32(o));

    dst += 4 * dstride;
    src += 4 * sstride;
  }
}

void SIMD_FUNC(aom_clpf_block)(uint8_t *dst, const uint16_t *src, int dstride,
                               int sstride, int sizex, int sizey,
                               unsigned int strength, unsigned int dmp) {
  if ((sizex != 4 && sizex != 8) || ((sizey & 3) && sizex == 4)) {
    // Fallback to C for odd sizes:
    // * block widths not 4 or 8
    // * block heights not a multiple of 4 if the block width is 4
    aom_clpf_block_c(dst, src, dstride, sstride, sizex, sizey, strength, dmp);
  } else {
    (sizex == 4 ? clpf_block4 : clpf_block8)(dst, src, dstride, sstride, sizey,
                                             strength, dmp);
  }
}

void SIMD_FUNC(aom_clpf_hblock)(uint8_t *dst, const uint16_t *src, int dstride,
                                int sstride, int sizex, int sizey,
                                unsigned int strength, unsigned int dmp) {
  if ((sizex != 4 && sizex != 8) || ((sizey & 3) && sizex == 4)) {
    // Fallback to C for odd sizes:
    // * block widths not 4 or 8
    // * block heights not a multiple of 4 if the block width is 4
    aom_clpf_hblock_c(dst, src, dstride, sstride, sizex, sizey, strength, dmp);
  } else {
    (sizex == 4 ? clpf_hblock4 : clpf_hblock8)(dst, src, dstride, sstride,
                                               sizey, strength, dmp);
  }
}

// sign(a - b) * max(0, abs(a - b) - max(0, abs(a - b) -
// strength + (abs(a - b) >> (dmp - log2(s)))))
SIMD_INLINE v128 constrain_hbd(v128 a, v128 b, unsigned int strength,
                               unsigned int dmp) {
  v128 diff = v128_sub_16(a, b);
  const v128 sign = v128_shr_n_s16(diff, 15);
  diff = v128_abs_s16(diff);
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

// delta = 1/16 * constrain(a, x, s, dmp) + 3/16 * constrain(b, x, s, dmp) +
//         3/16 * constrain(c, x, s, dmp) + 1/16 * constrain(d, x, s, dmp)
SIMD_INLINE v128 calc_hdelta_hbd(v128 x, v128 a, v128 b, v128 c, v128 d,
                                 unsigned int s, unsigned int dmp) {
  const v128 bc =
      v128_add_16(constrain_hbd(b, x, s, dmp), constrain_hbd(c, x, s, dmp));
  const v128 delta = v128_add_16(
      v128_add_16(constrain_hbd(a, x, s, dmp), constrain_hbd(d, x, s, dmp)),
      v128_add_16(v128_add_16(bc, bc), bc));
  return v128_add_16(
      x,
      v128_shr_s16(
          v128_add_16(v128_dup_16(4),
                      v128_add_16(delta, v128_cmplt_s16(delta, v128_zero()))),
          3));
}

static void calc_hdelta_hbd4(v128 o, v128 a, v128 b, v128 c, v128 d,
                             uint16_t *dst, unsigned int s, unsigned int dmp,
                             int dstride) {
  o = calc_hdelta_hbd(o, a, b, c, d, s, dmp);
  v64_store_aligned(dst, v128_high_v64(o));
  v64_store_aligned(dst + dstride, v128_low_v64(o));
}

static void calc_hdelta_hbd8(v128 o, v128 a, v128 b, v128 c, v128 d,
                             uint16_t *dst, unsigned int s, unsigned int dmp) {
  v128_store_aligned(dst, calc_hdelta_hbd(o, a, b, c, d, s, dmp));
}

// Process blocks of width 4, two lines at time.
SIMD_INLINE void clpf_block_hbd4(uint16_t *dst, const uint16_t *src,
                                 int dstride, int sstride, int sizey,
                                 unsigned int strength, unsigned int dmp) {
  int y;

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
SIMD_INLINE void clpf_block_hbd(uint16_t *dst, const uint16_t *src, int dstride,
                                int sstride, int sizey, unsigned int strength,
                                unsigned int dmp) {
  int y;

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

// Process blocks of width 4, horizontal filter, two lines at time.
SIMD_INLINE void clpf_hblock_hbd4(uint16_t *dst, const uint16_t *src,
                                  int dstride, int sstride, int sizey,
                                  unsigned int strength, unsigned int dmp) {
  int y;

  for (y = 0; y < sizey; y += 2) {
    const v128 a = v128_from_v64(v64_load_unaligned(src - 2),
                                 v64_load_unaligned(src - 2 + sstride));
    const v128 b = v128_from_v64(v64_load_unaligned(src - 1),
                                 v64_load_unaligned(src - 1 + sstride));
    const v128 c = v128_from_v64(v64_load_unaligned(src + 1),
                                 v64_load_unaligned(src + 1 + sstride));
    const v128 d = v128_from_v64(v64_load_unaligned(src + 2),
                                 v64_load_unaligned(src + 2 + sstride));

    calc_hdelta_hbd4(v128_from_v64(v64_load_unaligned(src),
                                   v64_load_unaligned(src + sstride)),
                     a, b, c, d, dst, strength, dmp, dstride);
    src += sstride * 2;
    dst += dstride * 2;
  }
}

// Process blocks of width 8, horizontal filter, two lines at time.
SIMD_INLINE void clpf_hblock_hbd(uint16_t *dst, const uint16_t *src,
                                 int dstride, int sstride, int sizey,
                                 unsigned int strength, unsigned int dmp) {
  int y;

  for (y = 0; y < sizey; y++) {
    const v128 o = v128_load_aligned(src);
    const v128 a = v128_load_unaligned(src - 2);
    const v128 b = v128_load_unaligned(src - 1);
    const v128 c = v128_load_unaligned(src + 1);
    const v128 d = v128_load_unaligned(src + 2);

    calc_hdelta_hbd8(o, a, b, c, d, dst, strength, dmp);
    src += sstride;
    dst += dstride;
  }
}

void SIMD_FUNC(aom_clpf_block_hbd)(uint16_t *dst, const uint16_t *src,
                                   int dstride, int sstride, int sizex,
                                   int sizey, unsigned int strength,
                                   unsigned int dmp) {
  if ((sizex != 4 && sizex != 8) || ((sizey & 1) && sizex == 4)) {
    // Fallback to C for odd sizes:
    // * block width not 4 or 8
    // * block heights not a multiple of 2 if the block width is 4
    aom_clpf_block_hbd_c(dst, src, dstride, sstride, sizex, sizey, strength,
                         dmp);
  } else {
    (sizex == 4 ? clpf_block_hbd4 : clpf_block_hbd)(dst, src, dstride, sstride,
                                                    sizey, strength, dmp);
  }
}

void SIMD_FUNC(aom_clpf_hblock_hbd)(uint16_t *dst, const uint16_t *src,
                                    int dstride, int sstride, int sizex,
                                    int sizey, unsigned int strength,
                                    unsigned int dmp) {
  if ((sizex != 4 && sizex != 8) || ((sizey & 1) && sizex == 4)) {
    // Fallback to C for odd sizes:
    // * block width not 4 or 8
    // * block heights not a multiple of 2 if the block width is 4
    aom_clpf_hblock_hbd_c(dst, src, dstride, sstride, sizex, sizey, strength,
                          dmp);
  } else {
    (sizex == 4 ? clpf_hblock_hbd4 : clpf_hblock_hbd)(
        dst, src, dstride, sstride, sizey, strength, dmp);
  }
}
