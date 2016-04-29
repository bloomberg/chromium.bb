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
const char kErrorUnableSendMedia[] = "Unable to send media";

}  // namespace

WiFiDisplayMediaPipeline::WiFiDisplayMediaPipeline(
    wds::SessionType type,
    const WiFiDisplayVideoEncoder::InitParameters& video_parameters,
    const wds::AudioCodec& audio_codec,
    const std::string& sink_ip_address,
    const std::pair<int, int>& sink_rtp_ports,
    const RegisterMediaServiceCallback& service_callback,
    const ErrorCallback& error_callback)
  : type_(type),
    video_parameters_(video_parameters),
    audio_codec_(audio_codec),
    sink_ip_address_(sink_ip_address),
    sink_rtp_ports_(sink_rtp_ports),
    service_callback_(service_callback),
    error_callback_(error_callback),
    weak_factory_(this) {
}

// static
std::unique_ptr<WiFiDisplayMediaPipeline> WiFiDisplayMediaPipeline::Create(
    wds::SessionType type,
    const WiFiDisplayVideoEncoder::InitParameters& video_parameters,
    const wds::AudioCodec& audio_codec,
    const std::string& sink_ip_address,
    const std::pair<int, int>& sink_rtp_ports,
    const RegisterMediaServiceCallback& service_callback,
    const ErrorCallback& error_callback) {
  return std::unique_ptr<WiFiDisplayMediaPipeline>(
      new WiFiDisplayMediaPipeline(type,
                                   video_parameters,
                                   audio_codec,
                                   sink_ip_address,
                                   sink_rtp_ports,
                                   service_callback,
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

enum WiFiDisplayMediaPipeline::InitializationStep : unsigned {
  INITIALIZE_FIRST,
  INITIALIZE_VIDEO_ENCODER = INITIALIZE_FIRST,
  INITIALIZE_MEDIA_PACKETIZER,
  INITIALIZE_MEDIA_SERVICE,
  INITIALIZE_LAST = INITIALIZE_MEDIA_SERVICE
};

void WiFiDisplayMediaPipeline::Initialize(
    const InitCompletionCallback& callback) {
  DCHECK(!video_encoder_ && !packetizer_);
  OnInitialize(callback, INITIALIZE_FIRST, true);
}

void WiFiDisplayMediaPipeline::OnInitialize(
    const InitCompletionCallback& callback,
    InitializationStep current_step,
    bool success) {
  if (!success) {
    callback.Run(false);
    return;
  }

  InitStepCompletionCallback init_step_callback;
  if (current_step < INITIALIZE_LAST) {
    InitializationStep next_step =
        static_cast<InitializationStep>(current_step + 1u);
    init_step_callback = base::Bind(&WiFiDisplayMediaPipeline::OnInitialize,
                               weak_factory_.GetWeakPtr(), callback, next_step);
  }

  switch (current_step) {
    case INITIALIZE_VIDEO_ENCODER:
      DCHECK(!video_encoder_);
      if (type_ & wds::VideoSession) {
        auto result_callback =
            base::Bind(&WiFiDisplayMediaPipeline::OnVideoEncoderCreated,
                       weak_factory_.GetWeakPtr(), init_step_callback);
        WiFiDisplayVideoEncoder::Create(video_parameters_, result_callback);
      } else {
        init_step_callback.Run(true);
      }
      break;
    case INITIALIZE_MEDIA_PACKETIZER:
      DCHECK(!packetizer_);
      CreateMediaPacketizer();
      init_step_callback.Run(true);
      break;
    case INITIALIZE_MEDIA_SERVICE:
      service_callback_.Run(
          mojo::GetProxy(&media_service_),
          base::Bind(&WiFiDisplayMediaPipeline::OnMediaServiceRegistered,
                     weak_factory_.GetWeakPtr(), callback));
      break;
  }
}

void WiFiDisplayMediaPipeline::CreateMediaPacketizer() {
  DCHECK(!packetizer_);
  std::vector<WiFiDisplayElementaryStreamInfo> stream_infos;

  if (type_ & wds::VideoSession) {
    DCHECK(video_encoder_);
    stream_infos.push_back(video_encoder_->CreateElementaryStreamInfo());
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
      base::TimeDelta::FromMilliseconds(200), stream_infos,
      base::Bind(&WiFiDisplayMediaPipeline::OnPacketizedMediaDatagramPacket,
                 base::Unretained(this))));
}

void WiFiDisplayMediaPipeline::OnVideoEncoderCreated(
    const InitStepCompletionCallback& callback,
    scoped_refptr<WiFiDisplayVideoEncoder> video_encoder) {
  DCHECK(!video_encoder_);

  if (!video_encoder) {
    callback.Run(false);
    return;
  }

  video_encoder_ = std::move(video_encoder);
  auto encoded_callback = base::Bind(
      &WiFiDisplayMediaPipeline::OnEncodedVideoFrame,
      weak_factory_.GetWeakPtr());
  auto error_callback = base::Bind(error_callback_, kErrorVideoEncoderError);
  video_encoder_->SetCallbacks(encoded_callback, error_callback);

  callback.Run(true);
}

void WiFiDisplayMediaPipeline::OnMediaServiceRegistered(
    const InitCompletionCallback& callback) {
  DCHECK(media_service_);
  auto error_callback = base::Bind(error_callback_, kErrorUnableSendMedia);
  media_service_.set_connection_error_handler(error_callback);
  media_service_->SetDesinationPoint(
      sink_ip_address_,
      static_cast<int32_t>(sink_rtp_ports_.first),
      callback);
}

void WiFiDisplayMediaPipeline::OnEncodedVideoFrame(
    std::unique_ptr<WiFiDisplayEncodedFrame> frame) {
  DCHECK(packetizer_);
  if (!packetizer_->EncodeElementaryStreamUnit(0u, frame->bytes(),
                                               frame->size(), frame->key_frame,
                                               frame->pts, frame->dts, true)) {
    DVLOG(1) << "Couldn't write video mpegts packet";
  }
}

bool WiFiDisplayMediaPipeline::OnPacketizedMediaDatagramPacket(
    WiFiDisplayMediaDatagramPacket media_datagram_packet) {
  DCHECK(media_service_);
  media_service_->SendMediaPacket(std::move(media_datagram_packet));
  return true;
}

}  // namespace extensions
