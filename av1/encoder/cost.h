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

#ifndef VP10_ENCODER_COST_H_
#define VP10_ENCODER_COST_H_

#include "aom_dsp/prob.h"
#include "aom/vpx_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const uint16_t vp10_prob_cost[257];

// The factor to scale from cost in bits to cost in vp10_prob_cost units.
#define VP9_PROB_COST_SHIFT 9

#define vp10_cost_zero(prob) (vp10_prob_cost[prob])

#define vp10_cost_one(prob) vp10_cost_zero(256 - (prob))

#define vp10_cost_bit(prob, bit) vp10_cost_zero((bit) ? 256 - (prob) : (prob))

static INLINE unsigned int cost_branch256(const unsigned int ct[2],
                                          vpx_prob p) {
  return ct[0] * vp10_cost_zero(p) + ct[1] * vp10_cost_one(p);
}

static INLINE int treed_cost(vpx_tree tree, const vpx_prob *probs, int bits,
                             int len) {
  int cost = 0;
  vpx_tree_index i = 0;

  do {
    const int bit = (bits >> --len) & 1;
    cost += vp10_cost_bit(probs[i >> 1], bit);
    i = tree[i + bit];
  } while (len);

  return cost;
}

void vp10_cost_tokens(int *costs, const vpx_prob *probs, vpx_tree tree);
void vp10_cost_tokens_skip(int *costs, const vpx_prob *probs, vpx_tree tree);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_ENCODER_COST_H_
