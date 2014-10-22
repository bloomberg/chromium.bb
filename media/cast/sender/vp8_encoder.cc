// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/vp8_encoder.h"

#include "base/logging.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_defines.h"
#include "media/cast/net/cast_transport_config.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8cx.h"

namespace media {
namespace cast {

namespace {

// After a pause in the video stream, what is the maximum duration amount to
// pass to the encoder for the next frame (in terms of 1/max_fps sized periods)?
// This essentially controls the encoded size of the first frame that follows a
// pause in the video stream.
const int kRestartFramePeriods = 3;

}  // namespace

Vp8Encoder::Vp8Encoder(const VideoSenderConfig& video_config)
    : cast_config_(video_config),
      use_multiple_video_buffers_(
          cast_config_.max_number_of_video_buffers_used ==
          kNumberOfVp8VideoBuffers),
      raw_image_(nullptr),
      key_frame_requested_(true),
      last_encoded_frame_id_(kStartFrameId),
      last_acked_frame_id_(kStartFrameId),
      undroppable_frames_(0) {
  config_.g_timebase.den = 0;  // Not initialized.

  // VP8 have 3 buffers available for prediction, with
  // max_number_of_video_buffers_used set to 1 we maximize the coding efficiency
  // however in this mode we can not skip frames in the receiver to catch up
  // after a temporary network outage; with max_number_of_video_buffers_used
  // set to 3 we allow 2 frames to be skipped by the receiver without error
  // propagation.
  DCHECK(cast_config_.max_number_of_video_buffers_used == 1 ||
         cast_config_.max_number_of_video_buffers_used ==
             kNumberOfVp8VideoBuffers)
      << "Invalid argument";

  thread_checker_.DetachFromThread();
}

Vp8Encoder::~Vp8Encoder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (is_initialized())
    vpx_codec_destroy(&encoder_);
  vpx_img_free(raw_image_);
}

void Vp8Encoder::Initialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!is_initialized());

  // Creating a wrapper to the image - setting image data to NULL. Actual
  // pointer will be set during encode. Setting align to 1, as it is
  // meaningless (actual memory is not allocated).
  raw_image_ = vpx_img_wrap(
      NULL, VPX_IMG_FMT_I420, cast_config_.width, cast_config_.height, 1, NULL);

  for (int i = 0; i < kNumberOfVp8VideoBuffers; ++i) {
    buffer_state_[i].frame_id = kStartFrameId;
    buffer_state_[i].state = kBufferStartState;
  }

  // Populate encoder configuration with default values.
  if (vpx_codec_enc_config_default(vpx_codec_vp8_cx(), &config_, 0)) {
    NOTREACHED() << "Invalid return value";
    config_.g_timebase.den = 0;  // Do not call vpx_codec_destroy() in dtor.
    return;
  }

  config_.g_threads = cast_config_.number_of_encode_threads;
  config_.g_w = cast_config_.width;
  config_.g_h = cast_config_.height;
  // Set the timebase to match that of base::TimeDelta.
  config_.g_timebase.num = 1;
  config_.g_timebase.den = base::Time::kMicrosecondsPerSecond;
  if (use_multiple_video_buffers_) {
    // We must enable error resilience when we use multiple buffers, due to
    // codec requirements.
    config_.g_error_resilient = 1;
  }
  config_.g_pass = VPX_RC_ONE_PASS;
  config_.g_lag_in_frames = 0;  // Immediate data output for each frame.

  // Rate control settings.
  config_.rc_dropframe_thresh = 0;  // The encoder may not drop any frames.
  config_.rc_resize_allowed = 0;  // TODO(miu): Why not?  Investigate this.
  config_.rc_end_usage = VPX_CBR;
  config_.rc_target_bitrate = cast_config_.start_bitrate / 1000;  // In kbit/s.
  config_.rc_min_quantizer = cast_config_.min_qp;
  config_.rc_max_quantizer = cast_config_.max_qp;
  // TODO(miu): Revisit these now that the encoder is being successfully
  // micro-managed.
  config_.rc_undershoot_pct = 100;
  config_.rc_overshoot_pct = 15;
  // TODO(miu): Document why these rc_buf_*_sz values were chosen and/or
  // research for better values.  Should they be computed from the target
  // playout delay?
  config_.rc_buf_initial_sz = 500;
  config_.rc_buf_optimal_sz = 600;
  config_.rc_buf_sz = 1000;

  config_.kf_mode = VPX_KF_DISABLED;

  vpx_codec_flags_t flags = 0;
  if (vpx_codec_enc_init(&encoder_, vpx_codec_vp8_cx(), &config_, flags)) {
    NOTREACHED() << "vpx_codec_enc_init() failed.";
    config_.g_timebase.den = 0;  // Do not call vpx_codec_destroy() in dtor.
    return;
  }

  // Raise the threshold for considering macroblocks as static.  The default is
  // zero, so this setting makes the encoder less sensitive to motion.  This
  // lowers the probability of needing to utilize more CPU to search for motion
  // vectors.
  vpx_codec_control(&encoder_, VP8E_SET_STATIC_THRESHOLD, 1);

  // Improve quality by enabling sets of codec features that utilize more CPU.
  // The default is zero, with increasingly more CPU to be used as the value is
  // more negative.
  // TODO(miu): Document why this value was chosen and expected behaviors.
  // Should this be dynamic w.r.t. hardware performance?
  vpx_codec_control(&encoder_, VP8E_SET_CPUUSED, -6);
}

void Vp8Encoder::Encode(const scoped_refptr<media::VideoFrame>& video_frame,
                        const base::TimeTicks& reference_time,
                        EncodedFrame* encoded_frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(encoded_frame);

  CHECK(is_initialized());  // No illegal reference to |config_| or |encoder_|.

  // Image in vpx_image_t format.
  // Input image is const. VP8's raw image is not defined as const.
  raw_image_->planes[VPX_PLANE_Y] =
      const_cast<uint8*>(video_frame->data(VideoFrame::kYPlane));
  raw_image_->planes[VPX_PLANE_U] =
      const_cast<uint8*>(video_frame->data(VideoFrame::kUPlane));
  raw_image_->planes[VPX_PLANE_V] =
      const_cast<uint8*>(video_frame->data(VideoFrame::kVPlane));

  raw_image_->stride[VPX_PLANE_Y] = video_frame->stride(VideoFrame::kYPlane);
  raw_image_->stride[VPX_PLANE_U] = video_frame->stride(VideoFrame::kUPlane);
  raw_image_->stride[VPX_PLANE_V] = video_frame->stride(VideoFrame::kVPlane);

  uint32 latest_frame_id_to_reference;
  Vp8Buffers buffer_to_update;
  vpx_codec_flags_t flags = 0;
  if (key_frame_requested_) {
    flags = VPX_EFLAG_FORCE_KF;
    // Self reference.
    latest_frame_id_to_reference = last_encoded_frame_id_ + 1;
    // We can pick any buffer as buffer_to_update since we update
    // them all.
    buffer_to_update = kLastBuffer;
  } else {
    // Reference all acked frames (buffers).
    latest_frame_id_to_reference = GetCodecReferenceFlags(&flags);
    buffer_to_update = GetNextBufferToUpdate();
    GetCodecUpdateFlags(buffer_to_update, &flags);
  }

  // The frame duration given to the VP8 codec affects a number of important
  // behaviors, including: per-frame bandwidth, CPU time spent encoding,
  // temporal quality trade-offs, and key/golden/alt-ref frame generation
  // intervals.  Use the actual amount of time between the current and previous
  // frames as a prediction for the next frame's duration, but bound the
  // prediction to account for the fact that the frame rate can be highly
  // variable, including long pauses in the video stream.
  const base::TimeDelta minimum_frame_duration =
      base::TimeDelta::FromSecondsD(1.0 / cast_config_.max_frame_rate);
  const base::TimeDelta maximum_frame_duration =
      base::TimeDelta::FromSecondsD(static_cast<double>(kRestartFramePeriods) /
                                        cast_config_.max_frame_rate);
  const base::TimeDelta last_frame_duration =
      video_frame->timestamp() - last_frame_timestamp_;
  const base::TimeDelta predicted_frame_duration =
      std::max(minimum_frame_duration,
               std::min(maximum_frame_duration, last_frame_duration));
  last_frame_timestamp_ = video_frame->timestamp();

  // Encode the frame.  The presentation time stamp argument here is fixed to
  // zero to force the encoder to base its single-frame bandwidth calculations
  // entirely on |predicted_frame_duration| and the target bitrate setting being
  // micro-managed via calls to UpdateRates().
  CHECK_EQ(vpx_codec_encode(&encoder_,
                            raw_image_,
                            0,
                            predicted_frame_duration.InMicroseconds(),
                            flags,
                            VPX_DL_REALTIME),
           VPX_CODEC_OK)
      << "BUG: Invalid arguments passed to vpx_codec_encode().";

  // Pull data from the encoder, populating a new EncodedFrame.
  encoded_frame->frame_id = ++last_encoded_frame_id_;
  const vpx_codec_cx_pkt_t* pkt = NULL;
  vpx_codec_iter_t iter = NULL;
  while ((pkt = vpx_codec_get_cx_data(&encoder_, &iter)) != NULL) {
    if (pkt->kind != VPX_CODEC_CX_FRAME_PKT)
      continue;
    if (pkt->data.frame.flags & VPX_FRAME_IS_KEY) {
      // TODO(hubbe): Replace "dependency" with a "bool is_key_frame".
      encoded_frame->dependency = EncodedFrame::KEY;
      encoded_frame->referenced_frame_id = encoded_frame->frame_id;
    } else {
      encoded_frame->dependency = EncodedFrame::DEPENDENT;
      // Frame dependencies could theoretically be relaxed by looking for the
      // VPX_FRAME_IS_DROPPABLE flag, but in recent testing (Oct 2014), this
      // flag never seems to be set.
      encoded_frame->referenced_frame_id = latest_frame_id_to_reference;
    }
    encoded_frame->rtp_timestamp =
        TimeDeltaToRtpDelta(video_frame->timestamp(), kVideoFrequency);
    encoded_frame->reference_time = reference_time;
    encoded_frame->data.assign(
        static_cast<const uint8*>(pkt->data.frame.buf),
        static_cast<const uint8*>(pkt->data.frame.buf) + pkt->data.frame.sz);
    break;  // Done, since all data is provided in one CX_FRAME_PKT packet.
  }
  DCHECK(!encoded_frame->data.empty())
      << "BUG: Encoder must provide data since lagged encoding is disabled.";

  DVLOG(2) << "VP8 encoded frame_id " << encoded_frame->frame_id
           << ", sized:" << encoded_frame->data.size();

  if (encoded_frame->dependency == EncodedFrame::KEY) {
    key_frame_requested_ = false;

    for (int i = 0; i < kNumberOfVp8VideoBuffers; ++i) {
      buffer_state_[i].state = kBufferSent;
      buffer_state_[i].frame_id = encoded_frame->frame_id;
    }
  } else {
    if (buffer_to_update != kNoBuffer) {
      buffer_state_[buffer_to_update].state = kBufferSent;
      buffer_state_[buffer_to_update].frame_id = encoded_frame->frame_id;
    }
  }
}

uint32 Vp8Encoder::GetCodecReferenceFlags(vpx_codec_flags_t* flags) {
  if (!use_multiple_video_buffers_)
    return last_encoded_frame_id_;

  const uint32 kMagicFrameOffset = 512;
  // We set latest_frame_to_reference to an old frame so that
  // IsNewerFrameId will work correctly.
  uint32 latest_frame_to_reference =
      last_encoded_frame_id_ - kMagicFrameOffset;

  // Reference all acked frames.
  // TODO(hubbe): We may also want to allow references to the
  // last encoded frame, if that frame was assigned to a buffer.
  for (int i = 0; i < kNumberOfVp8VideoBuffers; ++i) {
    if (buffer_state_[i].state == kBufferAcked) {
      if (IsNewerFrameId(buffer_state_[i].frame_id,
                         latest_frame_to_reference)) {
        latest_frame_to_reference = buffer_state_[i].frame_id;
      }
    } else {
      switch (i) {
        case kAltRefBuffer:
          *flags |= VP8_EFLAG_NO_REF_ARF;
          break;
        case kGoldenBuffer:
          *flags |= VP8_EFLAG_NO_REF_GF;
          break;
        case kLastBuffer:
          *flags |= VP8_EFLAG_NO_REF_LAST;
          break;
      }
    }
  }

  if (latest_frame_to_reference ==
      last_encoded_frame_id_ - kMagicFrameOffset) {
    // We have nothing to reference, it's kind of like a key frame,
    // but doesn't reset buffers.
    latest_frame_to_reference = last_encoded_frame_id_ + 1;
  }

  return latest_frame_to_reference;
}

Vp8Encoder::Vp8Buffers Vp8Encoder::GetNextBufferToUpdate() {
  if (!use_multiple_video_buffers_)
    return kNoBuffer;

  // The goal here is to make sure that we always keep one ACKed
  // buffer while trying to get an ACK for a newer buffer as we go.
  // Here are the rules for which buffer to select for update:
  // 1. If there is a buffer in state kStartState, use it.
  // 2. If there is a buffer other than the oldest buffer
  //    which is Acked, use the oldest buffer.
  // 3. If there are Sent buffers which are older than
  //    latest_acked_frame_, use the oldest one.
  // 4. If all else fails, just overwrite the newest buffer,
  //    but no more than 3 times in a row.
  //    TODO(hubbe): Figure out if 3 is optimal.
  // Note, rule 1-3 describe cases where there is a "free" buffer
  // that we can use. Rule 4 describes what happens when there is
  // no free buffer available.

  // Buffers, sorted from oldest frame to newest.
  Vp8Encoder::Vp8Buffers buffers[kNumberOfVp8VideoBuffers];

  for (int i = 0; i < kNumberOfVp8VideoBuffers; ++i) {
    Vp8Encoder::Vp8Buffers buffer = static_cast<Vp8Encoder::Vp8Buffers>(i);

    // Rule 1
    if (buffer_state_[buffer].state == kBufferStartState) {
      undroppable_frames_ = 0;
      return buffer;
    }
    buffers[buffer] = buffer;
  }

  // Sorting three elements with selection sort.
  for (int i = 0; i < kNumberOfVp8VideoBuffers - 1; i++) {
    for (int j = i + 1; j < kNumberOfVp8VideoBuffers; j++) {
      if (IsOlderFrameId(buffer_state_[buffers[j]].frame_id,
                         buffer_state_[buffers[i]].frame_id)) {
        std::swap(buffers[i], buffers[j]);
      }
    }
  }

  // Rule 2
  if (buffer_state_[buffers[1]].state == kBufferAcked ||
      buffer_state_[buffers[2]].state == kBufferAcked) {
    undroppable_frames_ = 0;
    return buffers[0];
  }

  // Rule 3
  for (int i = 0; i < kNumberOfVp8VideoBuffers; i++) {
    if (buffer_state_[buffers[i]].state == kBufferSent &&
        IsOlderFrameId(buffer_state_[buffers[i]].frame_id,
                       last_acked_frame_id_)) {
      undroppable_frames_ = 0;
      return buffers[i];
    }
  }

  // Rule 4
  if (undroppable_frames_ >= 3) {
    undroppable_frames_ = 0;
    return kNoBuffer;
  } else {
    undroppable_frames_++;
    return buffers[kNumberOfVp8VideoBuffers - 1];
  }
}

void Vp8Encoder::GetCodecUpdateFlags(Vp8Buffers buffer_to_update,
                                     vpx_codec_flags_t* flags) {
  if (!use_multiple_video_buffers_)
    return;

  // Update at most one buffer, except for key-frames.
  switch (buffer_to_update) {
    case kAltRefBuffer:
      *flags |= VP8_EFLAG_NO_UPD_GF;
      *flags |= VP8_EFLAG_NO_UPD_LAST;
      break;
    case kLastBuffer:
      *flags |= VP8_EFLAG_NO_UPD_GF;
      *flags |= VP8_EFLAG_NO_UPD_ARF;
      break;
    case kGoldenBuffer:
      *flags |= VP8_EFLAG_NO_UPD_ARF;
      *flags |= VP8_EFLAG_NO_UPD_LAST;
      break;
    case kNoBuffer:
      *flags |= VP8_EFLAG_NO_UPD_ARF;
      *flags |= VP8_EFLAG_NO_UPD_GF;
      *flags |= VP8_EFLAG_NO_UPD_LAST;
      *flags |= VP8_EFLAG_NO_UPD_ENTROPY;
      break;
  }
}

void Vp8Encoder::UpdateRates(uint32 new_bitrate) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!is_initialized())
    return;

  uint32 new_bitrate_kbit = new_bitrate / 1000;
  if (config_.rc_target_bitrate == new_bitrate_kbit)
    return;

  config_.rc_target_bitrate = new_bitrate_kbit;

  // Update encoder context.
  if (vpx_codec_enc_config_set(&encoder_, &config_)) {
    NOTREACHED() << "Invalid return value";
  }

  VLOG(1) << "VP8 new rc_target_bitrate: " << new_bitrate_kbit << " kbps";
}

void Vp8Encoder::LatestFrameIdToReference(uint32 frame_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!use_multiple_video_buffers_)
    return;

  VLOG(2) << "VP8 ok to reference frame:" << static_cast<int>(frame_id);
  for (int i = 0; i < kNumberOfVp8VideoBuffers; ++i) {
    if (frame_id == buffer_state_[i].frame_id) {
      buffer_state_[i].state = kBufferAcked;
      break;
    }
  }
  if (IsOlderFrameId(last_acked_frame_id_, frame_id)) {
    last_acked_frame_id_ = frame_id;
  }
}

void Vp8Encoder::GenerateKeyFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());
  key_frame_requested_ = true;
}

}  // namespace cast
}  // namespace media
