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

// Note, we use a fixed bitrate value when external video encoder is used.
// Some hardware encoder shows bad behavior if we set the bitrate too
// frequently, e.g. quality drop, not abiding by target bitrate, etc.
// See details: crbug.com/392086.
VideoSender::VideoSender(
    scoped_refptr<CastEnvironment> cast_environment,
    const VideoSenderConfig& video_config,
    const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
    const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb,
    CastTransportSender* const transport_sender)
    : FrameSender(
        cast_environment,
        false,
        transport_sender,
        base::TimeDelta::FromMilliseconds(video_config.rtcp_interval),
        kVideoFrequency,
        video_config.ssrc,
        video_config.max_frame_rate,
        video_config.target_playout_delay,
        NewFixedCongestionControl(
            (video_config.min_bitrate + video_config.max_bitrate) / 2)),
      last_bitrate_(0),
      weak_factory_(this) {
  cast_initialization_status_ = STATUS_VIDEO_UNINITIALIZED;
  VLOG(1) << "max_unacked_frames is " << max_unacked_frames_
          << " for target_playout_delay="
          << target_playout_delay_.InMilliseconds() << " ms"
          << " and max_frame_rate=" << video_config.max_frame_rate;
  DCHECK_GT(max_unacked_frames_, 0);

  if (video_config.use_external_encoder) {
    video_encoder_.reset(new ExternalVideoEncoder(cast_environment,
                                                  video_config,
                                                  create_vea_cb,
                                                  create_video_encode_mem_cb));
  } else {
    congestion_control_.reset(
        NewAdaptiveCongestionControl(cast_environment->Clock(),
                                     video_config.max_bitrate,
                                     video_config.min_bitrate,
                                     max_unacked_frames_));
    video_encoder_.reset(new VideoEncoderImpl(
        cast_environment, video_config, max_unacked_frames_));
  }
  cast_initialization_status_ = STATUS_VIDEO_INITIALIZED;

  media::cast::CastTransportRtpConfig transport_config;
  transport_config.ssrc = video_config.ssrc;
  transport_config.feedback_ssrc = video_config.incoming_feedback_ssrc;
  transport_config.rtp_payload_type = video_config.rtp_payload_type;
  transport_config.stored_frames = max_unacked_frames_;
  transport_config.aes_key = video_config.aes_key;
  transport_config.aes_iv_mask = video_config.aes_iv_mask;

  transport_sender->InitializeVideo(
      transport_config,
      base::Bind(&VideoSender::OnReceivedCastFeedback,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&VideoSender::OnReceivedRtt, weak_factory_.GetWeakPtr()));
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

  if (ShouldDropNextFrame(capture_time)) {
    VLOG(1) << "Dropping frame due to too many frames currently in-flight.";
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
          base::Bind(&FrameSender::SendEncodedFrame,
                     weak_factory_.GetWeakPtr(),
                     bitrate))) {
    frames_in_encoder_++;
  } else {
    VLOG(1) << "Encoder rejected a frame.  Skipping...";
  }
}

void VideoSender::OnAck(uint32 frame_id) {
  video_encoder_->LatestFrameIdToReference(frame_id);
}

}  // namespace cast
}  // namespace media
