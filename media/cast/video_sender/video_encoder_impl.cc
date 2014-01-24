// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/video_sender/video_encoder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_defines.h"
#include "media/cast/video_sender/video_encoder_impl.h"

namespace media {
namespace cast {

void LogFrameEncodedEvent(const base::TimeTicks& now,
                          CastEnvironment* const cast_environment,
                          const base::TimeTicks& capture_time) {
  cast_environment->Logging()->InsertFrameEvent(now, kVideoFrameEncoded,
      GetVideoRtpTimestamp(capture_time), kFrameIdUnknown);
}

VideoEncoderImpl::VideoEncoderImpl(
    scoped_refptr<CastEnvironment> cast_environment,
    const VideoSenderConfig& video_config,
    uint8 max_unacked_frames)
    : video_config_(video_config),
      cast_environment_(cast_environment),
      skip_next_frame_(false),
      skip_count_(0) {
  if (video_config.codec == transport::kVp8) {
    vp8_encoder_.reset(new Vp8Encoder(video_config, max_unacked_frames));
  } else {
    DCHECK(false) << "Invalid config";  // Codec not supported.
  }

  dynamic_config_.key_frame_requested = false;
  dynamic_config_.latest_frame_id_to_reference = kStartFrameId;
  dynamic_config_.bit_rate = video_config.start_bitrate;
}

VideoEncoderImpl::~VideoEncoderImpl() {}

bool VideoEncoderImpl::EncodeVideoFrame(
    const scoped_refptr<media::VideoFrame>& video_frame,
    const base::TimeTicks& capture_time,
    const FrameEncodedCallback& frame_encoded_callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (video_config_.codec != transport::kVp8) return false;

  if (skip_next_frame_) {
    ++skip_count_;
    skip_next_frame_ = false;
    VLOG(1) << "Skip encoding frame";
    return false;
  }

  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  cast_environment_->Logging()->InsertFrameEvent(now, kVideoFrameSentToEncoder,
      GetVideoRtpTimestamp(capture_time), kFrameIdUnknown);
  cast_environment_->PostTask(CastEnvironment::VIDEO_ENCODER, FROM_HERE,
      base::Bind(&VideoEncoderImpl::EncodeVideoFrameEncoderThread,
                 base::Unretained(this), video_frame, capture_time,
                 dynamic_config_, frame_encoded_callback));

  dynamic_config_.key_frame_requested = false;
  return true;
}

void VideoEncoderImpl::EncodeVideoFrameEncoderThread(
    const scoped_refptr<media::VideoFrame>& video_frame,
    const base::TimeTicks& capture_time,
    const CodecDynamicConfig& dynamic_config,
    const FrameEncodedCallback& frame_encoded_callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::VIDEO_ENCODER));
  if (dynamic_config.key_frame_requested) {
    vp8_encoder_->GenerateKeyFrame();
  }
  vp8_encoder_->LatestFrameIdToReference(
      dynamic_config.latest_frame_id_to_reference);
  vp8_encoder_->UpdateRates(dynamic_config.bit_rate);

  scoped_ptr<transport::EncodedVideoFrame> encoded_frame(
      new transport::EncodedVideoFrame());
  bool retval = vp8_encoder_->Encode(video_frame, encoded_frame.get());

  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
      base::Bind(LogFrameEncodedEvent, now, cast_environment_, capture_time));

  if (!retval) {
    VLOG(1) << "Encoding failed";
    return;
  }
  if (encoded_frame->data.size() <= 0) {
    VLOG(1) << "Encoding resulted in an empty frame";
    return;
  }
  cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
      base::Bind(frame_encoded_callback,
          base::Passed(&encoded_frame), capture_time));
}

// Inform the encoder about the new target bit rate.
void VideoEncoderImpl::SetBitRate(int new_bit_rate) {
  dynamic_config_.bit_rate = new_bit_rate;
}

// Inform the encoder to not encode the next frame.
void VideoEncoderImpl::SkipNextFrame(bool skip_next_frame) {
  skip_next_frame_ = skip_next_frame;
}

// Inform the encoder to encode the next frame as a key frame.
void VideoEncoderImpl::GenerateKeyFrame() {
  dynamic_config_.key_frame_requested = true;
}

// Inform the encoder to only reference frames older or equal to frame_id;
void VideoEncoderImpl::LatestFrameIdToReference(uint32 frame_id) {
  dynamic_config_.latest_frame_id_to_reference = frame_id;
}

int VideoEncoderImpl::NumberOfSkippedFrames() const {
  return skip_count_;
}

}  //  namespace cast
}  //  namespace media
