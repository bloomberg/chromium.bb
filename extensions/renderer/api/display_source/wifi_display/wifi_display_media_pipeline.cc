// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/display_source/wifi_display/wifi_display_media_pipeline.h"

#include "base/logging.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/video_encode_accelerator.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_elementary_stream_descriptor.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_elementary_stream_info.h"

namespace extensions {

namespace {

const char kErrorVideoEncoderError[] = "Unrepairable video encoder error";

}  // namespace

WiFiDisplayMediaPipeline::WiFiDisplayMediaPipeline(
    wds::SessionType type,
    const WiFiDisplayVideoEncoder::InitParameters& video_parameters,
    const wds::AudioCodec& audio_codec,
    const ErrorCallback& error_callback)
  : type_(type),
    video_parameters_(video_parameters),
    audio_codec_(audio_codec),
    error_callback_(error_callback),
    weak_factory_(this) {
}

// static
std::unique_ptr<WiFiDisplayMediaPipeline> WiFiDisplayMediaPipeline::Create(
    wds::SessionType type,
    const WiFiDisplayVideoEncoder::InitParameters& video_parameters,
    const wds::AudioCodec& audio_codec,
    const ErrorCallback& error_callback) {
  return std::unique_ptr<WiFiDisplayMediaPipeline>(
      new WiFiDisplayMediaPipeline(type,
                                   video_parameters,
                                   audio_codec,
                                   error_callback));
}

WiFiDisplayMediaPipeline::~WiFiDisplayMediaPipeline() {
}

void WiFiDisplayMediaPipeline::InsertRawVideoFrame(
    const scoped_refptr<media::VideoFrame>& video_frame,
    base::TimeTicks reference_time) {
  DCHECK(video_encoder_);
  video_encoder_->InsertRawVideoFrame(video_frame, reference_time);
}

void WiFiDisplayMediaPipeline::RequestIDRPicture() {
  DCHECK(video_encoder_);
  video_encoder_->RequestIDRPicture();
}

void WiFiDisplayMediaPipeline::Initialize(
    const InitCompletionCallback& callback) {
  DCHECK(init_completion_callback_.is_null());
  init_completion_callback_ = callback;

  CreateMediaPacketizer();

  if (type_ & wds::VideoSession) {
    CreateVideoEncoder();
    return;
  }

  init_completion_callback_.Run(true);
}

void WiFiDisplayMediaPipeline::CreateVideoEncoder() {
  DCHECK(!video_encoder_);
  auto result_callback =
      base::Bind(&WiFiDisplayMediaPipeline::OnVideoEncoderCreated,
                 weak_factory_.GetWeakPtr());
  WiFiDisplayVideoEncoder::Create(video_parameters_,
                                  result_callback);
}

void WiFiDisplayMediaPipeline::CreateMediaPacketizer() {
  DCHECK(!packetizer_);
  std::vector<WiFiDisplayElementaryStreamInfo> stream_infos;
  if (type_ & wds::VideoSession) {
    std::vector<WiFiDisplayElementaryStreamDescriptor> descriptors;
    descriptors.emplace_back(
        WiFiDisplayElementaryStreamDescriptor::AVCTimingAndHRD::Create());
    stream_infos.emplace_back(WiFiDisplayElementaryStreamInfo::VIDEO_H264,
                              std::move(descriptors));
  }

  if (type_ & wds::AudioSession) {
    using LPCMAudioStreamDescriptor =
        WiFiDisplayElementaryStreamDescriptor::LPCMAudioStream;
    std::vector<WiFiDisplayElementaryStreamDescriptor> descriptors;
    descriptors.emplace_back(
        LPCMAudioStreamDescriptor::Create(
            audio_codec_.modes.test(wds::LPCM_48K_16B_2CH)
                ? LPCMAudioStreamDescriptor::SAMPLING_FREQUENCY_48K
                : LPCMAudioStreamDescriptor::SAMPLING_FREQUENCY_44_1K,
            LPCMAudioStreamDescriptor::BITS_PER_SAMPLE_16,
            false,  // emphasis_flag
            LPCMAudioStreamDescriptor::NUMBER_OF_CHANNELS_STEREO));
    stream_infos.emplace_back(WiFiDisplayElementaryStreamInfo::AUDIO_LPCM,
                              std::move(descriptors));
  }

  packetizer_.reset(new WiFiDisplayMediaPacketizer(
      base::TimeDelta::FromMilliseconds(200),
      std::move(stream_infos),
      base::Bind(&WiFiDisplayMediaPipeline::OnPacketizedMediaDatagramPacket,
                 base::Unretained(this))));
}

void WiFiDisplayMediaPipeline::OnVideoEncoderCreated(
    scoped_refptr<WiFiDisplayVideoEncoder> video_encoder) {
  if (!video_encoder) {
    init_completion_callback_.Run(false);
    return;
  }
  video_encoder_ = std::move(video_encoder);
  auto encoded_callback = base::Bind(
      &WiFiDisplayMediaPipeline::OnEncodedVideoFrame,
      weak_factory_.GetWeakPtr());
  auto error_callback = base::Bind(error_callback_, kErrorVideoEncoderError);
  video_encoder_->SetCallbacks(encoded_callback, error_callback);
  init_completion_callback_.Run(true);
}

void WiFiDisplayMediaPipeline::OnEncodedVideoFrame(
    const WiFiDisplayEncodedFrame& frame) {
  DCHECK(packetizer_);
  if (!packetizer_->EncodeElementaryStreamUnit(
      0u, frame.bytes(), frame.data.size(), frame.key_frame, frame.pts,
          frame.dts, true)) {
    DVLOG(1) << "Couldn't write video mpegts packet";
  }
}

bool WiFiDisplayMediaPipeline::OnPacketizedMediaDatagramPacket(
    WiFiDisplayMediaDatagramPacket media_datagram_packet) {
  NOTIMPLEMENTED();
  return true;
}

}  // namespace extensions
