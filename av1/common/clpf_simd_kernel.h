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

#ifndef AV1_COMMON_CLPF_SIMD_KERNEL_H_
#define AV1_COMMON_CLPF_SIMD_KERNEL_H_

#include "aom_dsp/aom_simd.h"

// sign(a - b) * max(0, abs(a - b) - max(0, abs(a - b) -
// strength + (abs(a - b) >> (5 - log2(s)))))
SIMD_INLINE v128 constrain(v128 a, v128 b, unsigned int strength,
                           unsigned int damping) {
  const v128 diff = v128_sub_8(v128_max_u8(a, b), v128_min_u8(a, b));
  const v128 sign = v128_cmpeq_8(v128_min_u8(a, b), a);  // -(a <= b)
  const v128 s = v128_ssub_u8(v128_dup_8(strength),
                              v128_shr_u8(diff, damping - get_msb(strength)));
  return v128_sub_8(v128_xor(sign, v128_ssub_u8(diff, v128_ssub_u8(diff, s))),
                    sign);
}

// delta = 1/16 * constrain(a, x, s) + 3/16 * constrain(b, x, s) +
//         1/16 * constrain(c, x, s) + 3/16 * constrain(d, x, s) +
//         3/16 * constrain(e, x, s) + 1/16 * constrain(f, x, s) +
//         3/16 * constrain(g, x, s) + 1/16 * constrain(h, x, s)
SIMD_INLINE v128 calc_delta(v128 x, v128 a, v128 b, v128 c, v128 d, v128 e,
                            v128 f, v128 g, v128 h, unsigned int s,
                            unsigned int dmp) {
  const v128 bdeg =
      v128_add_8(v128_add_8(constrain(b, x, s, dmp), constrain(d, x, s, dmp)),
                 v128_add_8(constrain(e, x, s, dmp), constrain(g, x, s, dmp)));
  const v128 delta = v128_add_8(
      v128_add_8(v128_add_8(constrain(a, x, s, dmp), constrain(c, x, s, dmp)),
                 v128_add_8(constrain(f, x, s, dmp), constrain(h, x, s, dmp))),
      v128_add_8(v128_add_8(bdeg, bdeg), bdeg));
  return v128_add_8(
      x, v128_shr_s8(
             v128_add_8(v128_dup_8(8),
                        v128_add_8(delta, v128_cmplt_s8(delta, v128_zero()))),
             4));
}

#endif
