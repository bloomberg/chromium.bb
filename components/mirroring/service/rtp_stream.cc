// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/rtp_stream.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/sender/audio_sender.h"
#include "media/cast/sender/video_sender.h"

using media::cast::FrameSenderConfig;
using media::cast::RtpPayloadType;

namespace mirroring {

namespace {

// The maximum time since the last video frame was received from the video
// source, before requesting refresh frames.
constexpr base::TimeDelta kRefreshInterval =
    base::TimeDelta::FromMilliseconds(250);

// The maximum number of refresh video frames to request/receive.  After this
// limit (60 * 250ms = 15 seconds), refresh frame requests will stop being made.
constexpr int kMaxConsecutiveRefreshFrames = 60;

FrameSenderConfig DefaultOpusConfig() {
  FrameSenderConfig config;
  config.rtp_payload_type = RtpPayloadType::AUDIO_OPUS;
  config.sender_ssrc = 1;
  config.receiver_ssrc = 2;
  config.rtp_timebase = media::cast::kDefaultAudioSamplingRate;
  config.channels = 2;
  config.min_bitrate = config.max_bitrate = config.start_bitrate =
      media::cast::kDefaultAudioEncoderBitrate;
  config.max_frame_rate = 100;  // 10 ms audio frames
  config.codec = media::cast::CODEC_AUDIO_OPUS;
  return config;
}

FrameSenderConfig DefaultVp8Config() {
  FrameSenderConfig config;
  config.rtp_payload_type = RtpPayloadType::VIDEO_VP8;
  config.sender_ssrc = 11;
  config.receiver_ssrc = 12;
  config.rtp_timebase = media::cast::kVideoFrequency;
  config.channels = 1;
  config.max_bitrate = media::cast::kDefaultMaxVideoBitrate;
  config.min_bitrate = media::cast::kDefaultMinVideoBitrate;
  config.max_frame_rate = media::cast::kDefaultMaxFrameRate;
  config.codec = media::cast::CODEC_VIDEO_VP8;
  return config;
}

FrameSenderConfig DefaultH264Config() {
  FrameSenderConfig config;
  config.rtp_payload_type = RtpPayloadType::VIDEO_H264;
  config.sender_ssrc = 11;
  config.receiver_ssrc = 12;
  config.rtp_timebase = media::cast::kVideoFrequency;
  config.channels = 1;
  config.max_bitrate = media::cast::kDefaultMaxVideoBitrate;
  config.min_bitrate = media::cast::kDefaultMinVideoBitrate;
  config.max_frame_rate = media::cast::kDefaultMaxFrameRate;
  config.codec = media::cast::CODEC_VIDEO_H264;
  return config;
}

bool IsHardwareVP8EncodingSupported(RtpStreamClient* client) {
  // Query for hardware VP8 encoder support.
  const std::vector<media::VideoEncodeAccelerator::SupportedProfile>
      vea_profiles = client->GetSupportedVideoEncodeAcceleratorProfiles();
  for (const auto& vea_profile : vea_profiles) {
    if (vea_profile.profile >= media::VP8PROFILE_MIN &&
        vea_profile.profile <= media::VP8PROFILE_MAX) {
      return true;
    }
  }
  return false;
}

bool IsHardwareH264EncodingSupported(RtpStreamClient* client) {
// Query for hardware H.264 encoder support.
//
// TODO(miu): Look into why H.264 hardware encoder on MacOS is broken.
// http://crbug.com/596674
// TODO(emircan): Look into HW encoder initialization issues on Win.
// https://crbug.com/636064
#if !defined(OS_MACOSX) && !defined(OS_WIN)
  const std::vector<media::VideoEncodeAccelerator::SupportedProfile>
      vea_profiles = client->GetSupportedVideoEncodeAcceleratorProfiles();
  for (const auto& vea_profile : vea_profiles) {
    if (vea_profile.profile >= media::H264PROFILE_MIN &&
        vea_profile.profile <= media::H264PROFILE_MAX) {
      return true;
    }
  }
#endif  // !defined(OS_MACOSX) && !defined(OS_WIN)
  return false;
}

}  // namespace

VideoRtpStream::VideoRtpStream(
    std::unique_ptr<media::cast::VideoSender> video_sender,
    base::WeakPtr<RtpStreamClient> client)
    : video_sender_(std::move(video_sender)),
      client_(client),
      consecutive_refresh_count_(0),
      expecting_a_refresh_frame_(false),
      weak_factory_(this) {
  DCHECK(video_sender_);
  DCHECK(client);

  refresh_timer_.Start(FROM_HERE, kRefreshInterval,
                       base::BindRepeating(&VideoRtpStream::OnRefreshTimerFired,
                                           weak_factory_.GetWeakPtr()));
}

VideoRtpStream::~VideoRtpStream() {}

// static
std::vector<FrameSenderConfig> VideoRtpStream::GetSupportedConfigs(
    RtpStreamClient* client) {
  std::vector<FrameSenderConfig> supported_configs;
  // Prefer VP8 over H.264 for hardware encoder.
  if (IsHardwareVP8EncodingSupported(client))
    supported_configs.push_back(DefaultVp8Config());
  if (IsHardwareH264EncodingSupported(client))
    supported_configs.push_back(DefaultH264Config());

  // Propose the default software VP8 encoder, if no hardware encoders are
  // available.
  if (supported_configs.empty())
    supported_configs.push_back(DefaultVp8Config());

  return supported_configs;
}

void VideoRtpStream::InsertVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame) {
  DCHECK(client_);
  base::TimeTicks reference_time;
  if (!video_frame->metadata()->GetTimeTicks(
          media::VideoFrameMetadata::REFERENCE_TIME, &reference_time)) {
    client_->OnError("Missing REFERENCE_TIME.");
    return;
  }
  DCHECK(!reference_time.is_null());
  if (expecting_a_refresh_frame_) {
    // There is uncertainty as to whether the video frame was in response to a
    // refresh request.  However, if it was not, more video frames will soon
    // follow, and before the refresh timer can fire again.  Thus, the
    // behavior resulting from this logic will be correct.
    expecting_a_refresh_frame_ = false;
  } else {
    consecutive_refresh_count_ = 0;
    // The following re-starts the timer, scheduling it to fire at
    // kRefreshInterval from now.
    refresh_timer_.Reset();
  }

  if (!(video_frame->format() == media::PIXEL_FORMAT_I420 ||
        video_frame->format() == media::PIXEL_FORMAT_YV12 ||
        video_frame->format() == media::PIXEL_FORMAT_I420A)) {
    client_->OnError("Incompatible video frame format.");
    return;
  }
  video_sender_->InsertRawVideoFrame(std::move(video_frame), reference_time);
}

void VideoRtpStream::OnRefreshTimerFired() {
  ++consecutive_refresh_count_;
  if (consecutive_refresh_count_ >= kMaxConsecutiveRefreshFrames)
    refresh_timer_.Stop();  // Stop timer until receiving a non-refresh frame.

  DVLOG(1) << "CastVideoSink is requesting another refresh frame "
              "(consecutive count="
           << consecutive_refresh_count_ << ").";
  expecting_a_refresh_frame_ = true;
  client_->RequestRefreshFrame();
}

//------------------------------------------------------------------
// AudioRtpStream
//------------------------------------------------------------------

AudioRtpStream::AudioRtpStream(
    std::unique_ptr<media::cast::AudioSender> audio_sender,
    base::WeakPtr<RtpStreamClient> client)
    : audio_sender_(std::move(audio_sender)), client_(std::move(client)) {
  DCHECK(audio_sender_);
  DCHECK(client_);
}

AudioRtpStream::~AudioRtpStream() {}

// static
std::vector<FrameSenderConfig> AudioRtpStream::GetSupportedConfigs() {
  return {DefaultOpusConfig()};
}

void AudioRtpStream::InsertAudio(std::unique_ptr<media::AudioBus> audio_bus,
                                 base::TimeTicks capture_time) {
  audio_sender_->InsertAudio(std::move(audio_bus), capture_time);
}

}  // namespace mirroring
