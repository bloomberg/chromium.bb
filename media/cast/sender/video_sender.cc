// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/video_sender.h"

#include <algorithm>
#include <cstring>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/cast_defines.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/sender/external_video_encoder.h"
#include "media/cast/sender/video_encoder_impl.h"

namespace media {
namespace cast {

namespace {

// The following two constants are used to adjust the target
// playout delay (when allowed). They were calculated using
// a combination of cast_benchmark runs and manual testing.
//
// This is how many round trips we think we need on the network.
const int kRoundTripsNeeded = 4;
// This is an estimate of all the the constant time needed independent of
// network quality (e.g., additional time that accounts for encode and decode
// time).
const int kConstantTimeMs = 75;

}  // namespace

// Note, we use a fixed bitrate value when external video encoder is used.
// Some hardware encoder shows bad behavior if we set the bitrate too
// frequently, e.g. quality drop, not abiding by target bitrate, etc.
// See details: crbug.com/392086.
VideoSender::VideoSender(
    scoped_refptr<CastEnvironment> cast_environment,
    const VideoSenderConfig& video_config,
    const CastInitializationCallback& initialization_cb,
    const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
    const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb,
    CastTransportSender* const transport_sender,
    const PlayoutDelayChangeCB& playout_delay_change_cb)
    : FrameSender(
        cast_environment,
        false,
        transport_sender,
        base::TimeDelta::FromMilliseconds(video_config.rtcp_interval),
        kVideoFrequency,
        video_config.ssrc,
        video_config.max_frame_rate,
        video_config.min_playout_delay,
        video_config.max_playout_delay,
        NewFixedCongestionControl(
            (video_config.min_bitrate + video_config.max_bitrate) / 2)),
      frames_in_encoder_(0),
      last_bitrate_(0),
      playout_delay_change_cb_(playout_delay_change_cb),
      weak_factory_(this) {
  cast_initialization_status_ = STATUS_VIDEO_UNINITIALIZED;
  VLOG(1) << "max_unacked_frames is " << max_unacked_frames_
          << " for target_playout_delay="
          << target_playout_delay_.InMilliseconds() << " ms"
          << " and max_frame_rate=" << video_config.max_frame_rate;
  DCHECK_GT(max_unacked_frames_, 0);

  if (video_config.use_external_encoder) {
    video_encoder_.reset(new ExternalVideoEncoder(
        cast_environment,
        video_config,
        base::Bind(&VideoSender::OnEncoderInitialized,
                   weak_factory_.GetWeakPtr(), initialization_cb),
        create_vea_cb,
        create_video_encode_mem_cb));
  } else {
    // Software encoder is initialized immediately.
    congestion_control_.reset(
        NewAdaptiveCongestionControl(cast_environment->Clock(),
                                     video_config.max_bitrate,
                                     video_config.min_bitrate,
                                     max_unacked_frames_));
    video_encoder_.reset(new VideoEncoderImpl(
        cast_environment, video_config, max_unacked_frames_));
    cast_initialization_status_ = STATUS_VIDEO_INITIALIZED;
  }

  if (cast_initialization_status_ == STATUS_VIDEO_INITIALIZED) {
    cast_environment->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(initialization_cb, cast_initialization_status_));
  }

  media::cast::CastTransportRtpConfig transport_config;
  transport_config.ssrc = video_config.ssrc;
  transport_config.feedback_ssrc = video_config.incoming_feedback_ssrc;
  transport_config.rtp_payload_type = video_config.rtp_payload_type;
  transport_config.aes_key = video_config.aes_key;
  transport_config.aes_iv_mask = video_config.aes_iv_mask;

  transport_sender->InitializeVideo(
      transport_config,
      base::Bind(&VideoSender::OnReceivedCastFeedback,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&VideoSender::OnMeasuredRoundTripTime,
                 weak_factory_.GetWeakPtr()));
}

VideoSender::~VideoSender() {
}

void VideoSender::InsertRawVideoFrame(
    const scoped_refptr<media::VideoFrame>& video_frame,
    const base::TimeTicks& capture_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (cast_initialization_status_ != STATUS_VIDEO_INITIALIZED) {
    NOTREACHED();
    return;
  }
  DCHECK(video_encoder_.get()) << "Invalid state";

  RtpTimestamp rtp_timestamp = GetVideoRtpTimestamp(capture_time);
  cast_environment_->Logging()->InsertFrameEvent(
      capture_time, FRAME_CAPTURE_BEGIN, VIDEO_EVENT,
      rtp_timestamp, kFrameIdUnknown);
  cast_environment_->Logging()->InsertFrameEvent(
      cast_environment_->Clock()->NowTicks(),
      FRAME_CAPTURE_END, VIDEO_EVENT,
      rtp_timestamp,
      kFrameIdUnknown);

  // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
  TRACE_EVENT_INSTANT2(
      "cast_perf_test", "InsertRawVideoFrame",
      TRACE_EVENT_SCOPE_THREAD,
      "timestamp", capture_time.ToInternalValue(),
      "rtp_timestamp", rtp_timestamp);

  // Drop the frame if its reference timestamp is not an increase over the last
  // frame's.  This protects: 1) the duration calculations that assume
  // timestamps are monotonically non-decreasing, and 2) assumptions made deeper
  // in the implementation where each frame's RTP timestamp needs to be unique.
  if (!last_enqueued_frame_reference_time_.is_null() &&
      capture_time <= last_enqueued_frame_reference_time_) {
    VLOG(1) << "Dropping video frame: Reference time did not increase.";
    return;
  }

  // Two video frames are needed to compute the exact media duration added by
  // the next frame.  If there are no frames in the encoder, compute a guess
  // based on the configured |max_frame_rate_|.  Any error introduced by this
  // guess will be eliminated when |duration_in_encoder_| is updated in
  // OnEncodedVideoFrame().
  const base::TimeDelta duration_added_by_next_frame = frames_in_encoder_ > 0 ?
      capture_time - last_enqueued_frame_reference_time_ :
      base::TimeDelta::FromSecondsD(1.0 / max_frame_rate_);

  if (ShouldDropNextFrame(duration_added_by_next_frame)) {
    base::TimeDelta new_target_delay = std::min(
        current_round_trip_time_ * kRoundTripsNeeded +
        base::TimeDelta::FromMilliseconds(kConstantTimeMs),
        max_playout_delay_);
    if (new_target_delay > target_playout_delay_) {
      VLOG(1) << "New target delay: " << new_target_delay.InMilliseconds();
      playout_delay_change_cb_.Run(new_target_delay);
    }
    return;
  }

  uint32 bitrate = congestion_control_->GetBitrate(
        capture_time + target_playout_delay_, target_playout_delay_);
  if (bitrate != last_bitrate_) {
    video_encoder_->SetBitRate(bitrate);
    last_bitrate_ = bitrate;
  }

  if (video_encoder_->EncodeVideoFrame(
          video_frame,
          capture_time,
          base::Bind(&VideoSender::OnEncodedVideoFrame,
                     weak_factory_.GetWeakPtr(),
                     bitrate))) {
    frames_in_encoder_++;
    duration_in_encoder_ += duration_added_by_next_frame;
    last_enqueued_frame_reference_time_ = capture_time;
  } else {
    VLOG(1) << "Encoder rejected a frame.  Skipping...";
  }
}

int VideoSender::GetNumberOfFramesInEncoder() const {
  return frames_in_encoder_;
}

base::TimeDelta VideoSender::GetInFlightMediaDuration() const {
  if (GetUnacknowledgedFrameCount() > 0) {
    const uint32 oldest_unacked_frame_id = latest_acked_frame_id_ + 1;
    return last_enqueued_frame_reference_time_ -
        GetRecordedReferenceTime(oldest_unacked_frame_id);
  } else {
    return duration_in_encoder_;
  }
}

void VideoSender::OnAck(uint32 frame_id) {
  video_encoder_->LatestFrameIdToReference(frame_id);
}

void VideoSender::OnEncoderInitialized(
    const CastInitializationCallback& initialization_cb,
    CastInitializationStatus status) {
  cast_initialization_status_ = status;
  initialization_cb.Run(status);
}

void VideoSender::OnEncodedVideoFrame(
    int encoder_bitrate,
    scoped_ptr<EncodedFrame> encoded_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  frames_in_encoder_--;
  DCHECK_GE(frames_in_encoder_, 0);

  duration_in_encoder_ =
      last_enqueued_frame_reference_time_ - encoded_frame->reference_time;

  SendEncodedFrame(encoder_bitrate, encoded_frame.Pass());
}

}  // namespace cast
}  // namespace media
