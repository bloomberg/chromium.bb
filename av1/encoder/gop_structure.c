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

#include <stdint.h>

#include "config/aom_config.h"
#include "config/aom_scale_rtcd.h"

#include "aom/aom_codec.h"
#include "aom/aom_encoder.h"

#include "aom_ports/system_state.h"

#include "av1/common/onyxc_int.h"

#include "av1/encoder/encoder.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/gop_structure.h"

static void set_multi_layer_params(GF_GROUP *const gf_group, int l, int r,
                                   int *frame_ind, int arf_ind, int level) {
  if (r - l < 4) {
    while (++l < r) {
      // leaf nodes, not a look-ahead frame
      gf_group->update_type[*frame_ind] = LF_UPDATE;
      gf_group->arf_src_offset[*frame_ind] = 0;
      gf_group->arf_pos_in_gf[*frame_ind] = 0;
      gf_group->arf_update_idx[*frame_ind] = arf_ind;
      gf_group->pyramid_level[*frame_ind] = 0;
      ++gf_group->pyramid_lvl_nodes[0];
      ++(*frame_ind);
    }
  } else {
    int m = (l + r) / 2;
    int arf_pos_in_gf = *frame_ind;

    gf_group->update_type[*frame_ind] = INTNL_ARF_UPDATE;
    gf_group->arf_src_offset[*frame_ind] = m - l - 1;
    gf_group->arf_pos_in_gf[*frame_ind] = 0;
    gf_group->arf_update_idx[*frame_ind] = 1;  // mark all internal ARF 1
    gf_group->pyramid_level[*frame_ind] = level;
    ++gf_group->pyramid_lvl_nodes[level];
    ++(*frame_ind);

    // set parameters for frames displayed before this frame
    set_multi_layer_params(gf_group, l, m, frame_ind, 1, level - 1);

    // for overlay frames, we need to record the position of its corresponding
    // arf frames for bit allocation
    gf_group->update_type[*frame_ind] = INTNL_OVERLAY_UPDATE;
    gf_group->arf_src_offset[*frame_ind] = 0;
    gf_group->arf_pos_in_gf[*frame_ind] = arf_pos_in_gf;
    gf_group->arf_update_idx[*frame_ind] = 1;
    gf_group->pyramid_level[*frame_ind] = 0;
    ++(*frame_ind);

    // set parameters for frames displayed after this frame
    set_multi_layer_params(gf_group, m, r, frame_ind, arf_ind, level - 1);
  }
}

static INLINE unsigned char get_pyramid_height(int pyramid_width) {
  assert(pyramid_width <= MAX_GF_INTERVAL && pyramid_width >= MIN_GF_INTERVAL &&
         "invalid gf interval for pyramid structure");

  return pyramid_width > 12 ? 4 : (pyramid_width > 6 ? 3 : 2);
}

static int construct_multi_layer_gf_structure(GF_GROUP *const gf_group,
                                              const int gf_interval) {
  int frame_index = 0;
  gf_group->pyramid_height = get_pyramid_height(gf_interval);

  assert(gf_group->pyramid_height <= MAX_PYRAMID_LVL);

  av1_zero_array(gf_group->pyramid_lvl_nodes, MAX_PYRAMID_LVL);

  // At the beginning of each GF group it will be a key or overlay frame,
  gf_group->update_type[frame_index] = OVERLAY_UPDATE;
  gf_group->arf_src_offset[frame_index] = 0;
  gf_group->arf_pos_in_gf[frame_index] = 0;
  gf_group->arf_update_idx[frame_index] = 0;
  gf_group->pyramid_level[frame_index] = 0;
  ++frame_index;

  // ALT0
  gf_group->update_type[frame_index] = ARF_UPDATE;
  gf_group->arf_src_offset[frame_index] = gf_interval - 1;
  gf_group->arf_pos_in_gf[frame_index] = 0;
  gf_group->arf_update_idx[frame_index] = 0;
  gf_group->pyramid_level[frame_index] = gf_group->pyramid_height;
  ++frame_index;

  // set parameters for the rest of the frames
  set_multi_layer_params(gf_group, 0, gf_interval, &frame_index, 0,
                         gf_group->pyramid_height - 1);
  return frame_index;
}

#define CHECK_GF_PARAMETER 0
#if CHECK_GF_PARAMETER
void check_frame_params(GF_GROUP *const gf_group, int gf_interval,
                        int frame_nums) {
  static const char *update_type_strings[] = {
    "KF_UPDATE",          "LF_UPDATE",      "GF_UPDATE",
    "ARF_UPDATE",         "OVERLAY_UPDATE", "BRF_UPDATE",
    "LAST_BIPRED_UPDATE", "BIPRED_UPDATE",  "INTNL_OVERLAY_UPDATE",
    "INTNL_ARF_UPDATE"
  };
  FILE *fid = fopen("GF_PARAMS.txt", "a");

  fprintf(fid, "\n{%d}\n", gf_interval);
  for (int i = 0; i <= frame_nums; ++i) {
    fprintf(fid, "%s %d %d %d %d\n",
            update_type_strings[gf_group->update_type[i]],
            gf_group->arf_src_offset[i], gf_group->arf_pos_in_gf[i],
            gf_group->arf_update_idx[i], gf_group->pyramid_level[i]);
  }

  fprintf(fid, "number of nodes in each level: \n");
  for (int i = 0; i < gf_group->pyramid_height; ++i) {
    fprintf(fid, "lvl %d: %d ", i, gf_group->pyramid_lvl_nodes[i]);
  }
  fprintf(fid, "\n");
  fclose(fid);
}
#endif  // CHECK_GF_PARAMETER

static int update_type_2_rf_level(FRAME_UPDATE_TYPE update_type) {
  // Derive rf_level from update_type
  switch (update_type) {
    case LF_UPDATE: return INTER_NORMAL;
    case ARF_UPDATE: return GF_ARF_STD;
    case OVERLAY_UPDATE: return INTER_NORMAL;
    case BRF_UPDATE: return GF_ARF_LOW;
    case LAST_BIPRED_UPDATE: return INTER_NORMAL;
    case BIPRED_UPDATE: return INTER_NORMAL;
    case INTNL_ARF_UPDATE: return GF_ARF_LOW;
    case INTNL_OVERLAY_UPDATE: return INTER_NORMAL;
    default: return INTER_NORMAL;
  }
}

static void define_pyramid_gf_group_structure(
    AV1_COMP *cpi, const EncodeFrameParams *const frame_params) {
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;
  GF_GROUP *const gf_group = &twopass->gf_group;
  const int key_frame = frame_params->frame_type == KEY_FRAME;

  assert(rc->baseline_gf_interval >= MIN_GF_INTERVAL &&
         rc->baseline_gf_interval <=
             get_max_gf_length(cpi->oxcf.gf_max_pyr_height));
  assert(cpi->oxcf.gf_max_pyr_height >= MIN_PYRAMID_LVL &&
         cpi->oxcf.gf_max_pyr_height <= MAX_PYRAMID_LVL);

  const int gf_update_frames =
      construct_multi_layer_gf_structure(gf_group, rc->baseline_gf_interval);
  int frame_index;

  cpi->num_extra_arfs = 0;

  for (frame_index = 0; frame_index < gf_update_frames; ++frame_index) {
    // Set unused variables to default values
    gf_group->brf_src_offset[frame_index] = 0;

    // Special handle for the first frame for assigning update_type
    if (frame_index == 0) {
      // For key frames the frame target rate is already set and it
      // is also the golden frame.
      if (key_frame) {
        gf_group->update_type[frame_index] = KF_UPDATE;
        continue;
      }

      if (rc->source_alt_ref_active) {
        gf_group->update_type[frame_index] = OVERLAY_UPDATE;
      } else {
        gf_group->update_type[frame_index] = GF_UPDATE;
      }
    } else {
      if (gf_group->update_type[frame_index] == INTNL_ARF_UPDATE)
        ++cpi->num_extra_arfs;
    }

    // Assign rf level based on update type
    gf_group->rf_level[frame_index] =
        update_type_2_rf_level(gf_group->update_type[frame_index]);
  }

  // NOTE: We need to configure the frame at the end of the sequence + 1 that
  //       will be the start frame for the next group. Otherwise prior to the
  //       call to av1_get_second_pass_params() the data will be undefined.
  if (rc->source_alt_ref_pending) {
    gf_group->update_type[frame_index] = OVERLAY_UPDATE;
    gf_group->rf_level[frame_index] = INTER_NORMAL;
  } else {
    gf_group->update_type[frame_index] = GF_UPDATE;
    gf_group->rf_level[frame_index] = GF_ARF_STD;
  }

  gf_group->brf_src_offset[frame_index] = 0;
  gf_group->arf_update_idx[frame_index] = 0;
  // This value is only used for INTNL_OVERLAY_UPDATE
  gf_group->arf_pos_in_gf[frame_index] = 0;

#ifdef CHCEK_GF_PARAMETER
  check_frame_params(gf_group, rc->baseline_gf_interval, gf_update_frames);
#endif
}

static void define_flat_gf_group_structure(
    AV1_COMP *cpi, const EncodeFrameParams *const frame_params) {
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;
  GF_GROUP *const gf_group = &twopass->gf_group;
  int i;
  int frame_index = 0;
  const int key_frame = frame_params->frame_type == KEY_FRAME;

  // The use of bi-predictive frames are only enabled when following 3
  // conditions are met:
  // (1) ALTREF is enabled;
  // (2) The bi-predictive group interval is at least 2; and
  // (3) The bi-predictive group interval is strictly smaller than the
  //     golden group interval.
  const int is_bipred_enabled =
      cpi->extra_arf_allowed && rc->source_alt_ref_pending &&
      rc->bipred_group_interval &&
      rc->bipred_group_interval <=
          (rc->baseline_gf_interval - rc->source_alt_ref_pending);
  int bipred_group_end = 0;
  int bipred_frame_index = 0;

  const unsigned char ext_arf_interval =
      (unsigned char)(rc->baseline_gf_interval / (cpi->num_extra_arfs + 1) - 1);
  int which_arf = cpi->num_extra_arfs;
  int subgroup_interval[MAX_EXT_ARFS + 1];
  int is_sg_bipred_enabled = is_bipred_enabled;
  int accumulative_subgroup_interval = 0;

  // For key frames the frame target rate is already set and it
  // is also the golden frame.
  // === [frame_index == 0] ===
  if (!key_frame) {
    if (rc->source_alt_ref_active) {
      gf_group->update_type[frame_index] = OVERLAY_UPDATE;
      gf_group->rf_level[frame_index] = INTER_NORMAL;
    } else {
      gf_group->update_type[frame_index] = GF_UPDATE;
      gf_group->rf_level[frame_index] = GF_ARF_STD;
    }
    gf_group->arf_update_idx[frame_index] = 0;
  }

  gf_group->brf_src_offset[frame_index] = 0;

  frame_index++;

  bipred_frame_index++;

  // === [frame_index == 1] ===
  if (rc->source_alt_ref_pending) {
    gf_group->update_type[frame_index] = ARF_UPDATE;
    gf_group->rf_level[frame_index] = GF_ARF_STD;
    gf_group->arf_src_offset[frame_index] =
        (unsigned char)(rc->baseline_gf_interval - 1);

    gf_group->arf_update_idx[frame_index] = 0;

    gf_group->brf_src_offset[frame_index] = 0;
    // NOTE: "bidir_pred_frame_index" stays unchanged for ARF_UPDATE frames.

    // Work out the ARFs' positions in this gf group
    // NOTE(weitinglin): ALT_REFs' are indexed inversely, but coded in display
    // order (except for the original ARF). In the example of three ALT_REF's,
    // We index ALTREF's as: KEY ----- ALT2 ----- ALT1 ----- ALT0
    // but code them in the following order:
    // KEY-ALT0-ALT2 ----- OVERLAY2-ALT1 ----- OVERLAY1 ----- OVERLAY0
    //
    // arf_pos_for_ovrly[]: Position for OVERLAY
    // arf_pos_in_gf[]:     Position for ALTREF
    cpi->arf_pos_for_ovrly[0] = frame_index + cpi->num_extra_arfs +
                                gf_group->arf_src_offset[frame_index] + 1;
    for (i = 0; i < cpi->num_extra_arfs; ++i) {
      cpi->arf_pos_for_ovrly[i + 1] =
          frame_index + (cpi->num_extra_arfs - i) * (ext_arf_interval + 2);
      subgroup_interval[i] = cpi->arf_pos_for_ovrly[i] -
                             cpi->arf_pos_for_ovrly[i + 1] - (i == 0 ? 1 : 2);
    }
    subgroup_interval[cpi->num_extra_arfs] =
        cpi->arf_pos_for_ovrly[cpi->num_extra_arfs] - frame_index -
        (cpi->num_extra_arfs == 0 ? 1 : 2);

    ++frame_index;

    // Insert an extra ARF
    // === [frame_index == 2] ===
    if (cpi->num_extra_arfs) {
      gf_group->update_type[frame_index] = INTNL_ARF_UPDATE;
      gf_group->rf_level[frame_index] = GF_ARF_LOW;
      gf_group->arf_src_offset[frame_index] = ext_arf_interval;

      gf_group->arf_update_idx[frame_index] = which_arf;
      ++frame_index;
    }
    accumulative_subgroup_interval += subgroup_interval[cpi->num_extra_arfs];
  }

  const int normal_frames =
      rc->baseline_gf_interval - (key_frame || rc->source_alt_ref_pending);

  for (i = 0; i < normal_frames; ++i) {
    gf_group->arf_update_idx[frame_index] = which_arf;

    // If we are going to have ARFs, check whether we can have BWDREF in this
    // subgroup, and further, whether we can have ARF subgroup which contains
    // the BWDREF subgroup but contained within the GF group:
    //
    // GF group --> ARF subgroup --> BWDREF subgroup
    if (rc->source_alt_ref_pending) {
      is_sg_bipred_enabled =
          is_bipred_enabled &&
          (subgroup_interval[which_arf] > rc->bipred_group_interval);
    }

    // NOTE: BIDIR_PRED is only enabled when the length of the bi-predictive
    //       frame group interval is strictly smaller than that of the GOLDEN
    //       FRAME group interval.
    // TODO(zoeliu): Currently BIDIR_PRED is only enabled when alt-ref is on.
    if (is_sg_bipred_enabled && !bipred_group_end) {
      const int cur_brf_src_offset = rc->bipred_group_interval - 1;

      if (bipred_frame_index == 1) {
        // --- BRF_UPDATE ---
        gf_group->update_type[frame_index] = BRF_UPDATE;
        gf_group->rf_level[frame_index] = GF_ARF_LOW;
        gf_group->brf_src_offset[frame_index] = cur_brf_src_offset;
      } else if (bipred_frame_index == rc->bipred_group_interval) {
        // --- LAST_BIPRED_UPDATE ---
        gf_group->update_type[frame_index] = LAST_BIPRED_UPDATE;
        gf_group->rf_level[frame_index] = INTER_NORMAL;
        gf_group->brf_src_offset[frame_index] = 0;

        // Reset the bi-predictive frame index.
        bipred_frame_index = 0;
      } else {
        // --- BIPRED_UPDATE ---
        gf_group->update_type[frame_index] = BIPRED_UPDATE;
        gf_group->rf_level[frame_index] = INTER_NORMAL;
        gf_group->brf_src_offset[frame_index] = 0;
      }

      bipred_frame_index++;
      // Check whether the next bi-predictive frame group would entirely be
      // included within the current golden frame group.
      // In addition, we need to avoid coding a BRF right before an ARF.
      if (bipred_frame_index == 1 &&
          (i + 2 + cur_brf_src_offset) >= accumulative_subgroup_interval) {
        bipred_group_end = 1;
      }
    } else {
      gf_group->update_type[frame_index] = LF_UPDATE;
      gf_group->rf_level[frame_index] = INTER_NORMAL;
      gf_group->brf_src_offset[frame_index] = 0;
    }

    ++frame_index;

    // Check if we need to update the ARF.
    if (is_sg_bipred_enabled && cpi->num_extra_arfs && which_arf > 0 &&
        frame_index > cpi->arf_pos_for_ovrly[which_arf]) {
      --which_arf;
      accumulative_subgroup_interval += subgroup_interval[which_arf] + 1;

      // Meet the new subgroup; Reset the bipred_group_end flag.
      bipred_group_end = 0;
      // Insert another extra ARF after the overlay frame
      if (which_arf) {
        gf_group->update_type[frame_index] = INTNL_ARF_UPDATE;
        gf_group->rf_level[frame_index] = GF_ARF_LOW;
        gf_group->arf_src_offset[frame_index] = ext_arf_interval;

        gf_group->arf_update_idx[frame_index] = which_arf;
        ++frame_index;
      }
    }
  }

  // NOTE: We need to configure the frame at the end of the sequence + 1 that
  //       will be the start frame for the next group. Otherwise prior to the
  //       call to av1_get_second_pass_params() the data will be undefined.
  gf_group->arf_update_idx[frame_index] = 0;

  if (rc->source_alt_ref_pending) {
    gf_group->update_type[frame_index] = OVERLAY_UPDATE;
    gf_group->rf_level[frame_index] = INTER_NORMAL;

    cpi->arf_pos_in_gf[0] = 1;
    if (cpi->num_extra_arfs) {
      // Overwrite the update_type for extra-ARF's corresponding internal
      // OVERLAY's: Change from LF_UPDATE to INTNL_OVERLAY_UPDATE.
      for (i = cpi->num_extra_arfs; i > 0; --i) {
        cpi->arf_pos_in_gf[i] =
            (i == cpi->num_extra_arfs ? 2 : cpi->arf_pos_for_ovrly[i + 1] + 1);

        gf_group->update_type[cpi->arf_pos_for_ovrly[i]] = INTNL_OVERLAY_UPDATE;
        gf_group->rf_level[cpi->arf_pos_for_ovrly[i]] = INTER_NORMAL;
      }
    }
  } else {
    gf_group->update_type[frame_index] = GF_UPDATE;
    gf_group->rf_level[frame_index] = GF_ARF_STD;
  }

  gf_group->brf_src_offset[frame_index] = 0;
}

void av1_gop_setup_structure(AV1_COMP *cpi,
                             const EncodeFrameParams *const frame_params) {
  RATE_CONTROL *const rc = &cpi->rc;

  const int max_pyr_height = cpi->oxcf.gf_max_pyr_height;
  const int valid_pyramid_gf_length =
      max_pyr_height >= MIN_PYRAMID_LVL && max_pyr_height <= MAX_PYRAMID_LVL &&
      rc->baseline_gf_interval >= MIN_GF_INTERVAL &&
      rc->baseline_gf_interval <= get_max_gf_length(max_pyr_height);

  // Decide whether to use a flat or pyramid structure for this GF
  if (valid_pyramid_gf_length && rc->source_alt_ref_pending &&
      cpi->extra_arf_allowed > 0) {
    define_pyramid_gf_group_structure(cpi, frame_params);
    cpi->new_bwdref_update_rule = 1;
  } else {
    define_flat_gf_group_structure(cpi, frame_params);
    cpi->new_bwdref_update_rule = 0;
  }
}
