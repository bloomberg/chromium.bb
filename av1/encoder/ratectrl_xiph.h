/*
 * Copyright (c) 2001-2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#if !defined(_ratectrl_xiph_H)
#define _ratectrl_xiph_H (1)

#include "av1/encoder/ratectrl.h"

/*Frame types.*/
#define OD_I_FRAME (0)
#define OD_P_FRAME (1)
#define OD_GOLDEN_P_FRAME (2)
#define OD_ALTREF_P_FRAME (3)

#define OD_FRAME_NSUBTYPES (OD_ALTREF_P_FRAME + 1)

/* Periodic boost (in between golden frames) strength - lower is more */
#define OD_PERIODIC_BOOST_DIV (10)

/* Constants for frame QP modulation <- tweak these
 * Adjusts how the rate control system decides the quantizers per frame
 * (sub)type */
#define OD_MQP_I (0.98)
#define OD_MQP_P (1.06)
#define OD_MQP_GP (0.99)
#define OD_MQP_AP (0.92)
#define OD_DQP_I (-2)
#define OD_DQP_P (0)
#define OD_DQP_GP (-2)
#define OD_DQP_AP (-2)

/*Fractional_coded_quantizer ~=
   log2(quantizer / (1 << OD_COEFF_SHIFT))*6.307 + 6.235*/
/*Base/scale factor for linear quantizer to fractional coded quantizer
   conversion (6.307 * 2^12) */
#define OD_LOG_QUANTIZER_BASE_Q12 (0x0064EB)
/*Inverse of above scale factor.*/
#define OD_LOG_QUANTIZER_EXP_Q12 (0x000289)
/*Offset for linear quantizer to fractional coded quantizer
   conversion (6.235 * 2^45) */
#define OD_LOG_QUANTIZER_OFFSET_Q45 (0x0000C7851EB851ECLL)

/*A 2nd order low-pass Bessel follower.
  We use this for rate control because it has fast reaction time, but is
   critically damped.*/
typedef struct od_iir_bessel2 {
  int32_t c[2];
  int64_t g;
  int32_t x[2];
  int32_t y[2];
} od_iir_bessel2;

/*Rate control setup and working state information.*/
typedef struct od_rc_state {
  /* Image format */
  int frame_width;
  int frame_height;
  int bit_depth;

  /* Framerate */
  double framerate;
  /* Keyframe rate */
  int keyframe_rate;
  /* Golden frame period */
  int goldenframe_rate;
  /* Altref frame period */
  int altref_rate;
  /*The target bit-rate in bits per second.*/
  int64_t target_bitrate;
  /* Quality level for non-bitrate-targeting */
  int quality;
  /* Copied from oxcf->frame_periodic_boost */
  int periodic_boosts;
  /* Max Q */
  int maxq;
  /* Min Q */
  int minq;

  /* Actual returned quantizer */
  int target_quantizer;
  /*The full-precision, unmodulated quantizer upon which
    our modulated quantizers are based.*/
  int base_quantizer;

  /* Increments by 1 for each frame. */
  int64_t cur_frame;

  /* End of input flag */
  int end_of_input;
  /* Closed GOP flag */
  int closed_gop;
  /*The number of frames over which to distribute the reservoir usage.*/
  int reservoir_frame_delay;
  /*Will we drop frames to meet bitrate target?*/
  unsigned char drop_frames;
  /*Do we respect the maximum reservoir fullness?*/
  unsigned char cap_overflow;
  /*Can the reservoir go negative?*/
  unsigned char cap_underflow;
  /*Two-pass mode state.
    0 => 1-pass encoding.
    1 => 1st pass of 2-pass encoding.
    2 => 2nd pass of 2-pass encoding.*/
  int twopass_state;
  /*The log of the number of pixels in a frame in Q57 format.*/
  int64_t log_npixels;
  /*The target average bits per frame.*/
  int64_t bits_per_frame;
  /*The current bit reservoir fullness (bits available to be used).*/
  int64_t reservoir_fullness;
  /*The target buffer fullness.
    This is where we'd like to be by the last keyframe the appears in the next
     buf_delay frames.*/
  int64_t reservoir_target;
  /*The maximum buffer fullness (total size of the buffer).*/
  int64_t reservoir_max;
  /*The log of estimated scale factor for the rate model in Q57 format.*/
  int64_t log_scale[OD_FRAME_NSUBTYPES];
  /*The exponent used in the rate model in Q8 format.*/
  unsigned exp[OD_FRAME_NSUBTYPES];
  /*The log of an estimated scale factor used to obtain the real framerate, for
     VFR sources or, e.g., 12 fps content doubled to 24 fps, etc.*/
  int64_t log_drop_scale[OD_FRAME_NSUBTYPES];
  /*The total drop count from the previous frame.*/
  uint32_t prev_drop_count[OD_FRAME_NSUBTYPES];
  /*Second-order lowpass filters to track scale and VFR/drops.*/
  od_iir_bessel2 scalefilter[OD_FRAME_NSUBTYPES];
  od_iir_bessel2 vfrfilter[OD_FRAME_NSUBTYPES];
  int frame_count[OD_FRAME_NSUBTYPES];
  int inter_p_delay;
  int inter_delay_target;
  /*The total accumulated estimation bias.*/
  int64_t rate_bias;
} od_rc_state;

int od_enc_rc_init(od_rc_state *rc, int64_t bitrate, int delay_ms);

int od_enc_rc_select_quantizers_and_lambdas(od_rc_state *rc,
                                            int is_golden_frame,
                                            int is_altref_frame, int frame_type,
                                            int *bottom_idx, int *top_idx);

/* Returns 1 if the frame should be dropped */
int od_enc_rc_update_state(od_rc_state *rc, int64_t bits, int is_golden_frame,
                           int is_altref_frame, int frame_type, int droppable);

int od_frame_type(od_rc_state *rc, int64_t coding_frame_count, int *is_golden,
                  int *is_altref, int64_t *ip_count);

int od_enc_rc_resize(od_rc_state *rc);

#endif
