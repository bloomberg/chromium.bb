/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_ENCODER_GOP_STRUCTURE_H_
#define AOM_AV1_ENCODER_GOP_STRUCTURE_H_

#include "av1/common/onyxc_int.h"
#include "av1/encoder/ratectrl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Given the maximum allowed height of the pyramid structure, return the maximum
// GF length supported by the same.
static INLINE int get_max_gf_length(int max_pyr_height) {
  // We allow a frame to have at most two left/right descendants before changing
  // them into to a subtree, i.e., we allow the following structure:
  /*                    OUT_OF_ORDER_FRAME
                       / /              \ \
  (two left children) F F                F F (two right children) */
  // For example, the max gf size supported by 4 layer structure is:
  // 1 (KEY/OVERLAY) + 1 + 2 + 4 + 16 (two children on both side of their
  // parent)
  switch (max_pyr_height) {
    case 2: return 6;   // = 1 (KEY/OVERLAY) + 1 + 4
    case 3: return 12;  // = 1 (KEY/OVERLAY) + 1 + 2 + 8
    case 4: return 24;  // = 1 (KEY/OVERLAY) + 1 + 2 + 4 + 16
    case 1:
      return MAX_GF_INTERVAL;  // Special case: uses the old pyramid structure.
    default: assert(0 && "Invalid max_pyr_height"); return -1;
  }
}

struct AV1_COMP;
struct EncodeFrameParams;

// Set up the Group-Of-Pictures structure for this GF_GROUP.  This involves
// deciding where to place the various FRAME_UPDATE_TYPEs (e.g. ARF, BRF,
// OVERLAY, LAST) in the group.  It does this primarily by setting the contents
// of cpi->twopass.gf_group.update_type[].
void av1_gop_setup_structure(
    struct AV1_COMP *cpi, const struct EncodeFrameParams *const frame_params);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_GOP_STRUCTURE_H_
