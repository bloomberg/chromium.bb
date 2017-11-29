// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/video_codec_factory.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/media/gpu/rtc_video_decoder_factory.h"
#include "content/renderer/media/gpu/rtc_video_encoder_factory.h"
#include "third_party/webrtc/media/base/codec.h"
#include "third_party/webrtc/media/engine/internaldecoderfactory.h"
#include "third_party/webrtc/media/engine/internalencoderfactory.h"
#include "third_party/webrtc/media/engine/simulcast_encoder_adapter.h"
#include "third_party/webrtc/media/engine/videodecodersoftwarefallbackwrapper.h"
#include "third_party/webrtc/media/engine/videoencodersoftwarefallbackwrapper.h"
#include "third_party/webrtc/media/engine/vp8_encoder_simulcast_proxy.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

namespace content {

namespace {

bool IsFormatSupported(
    const std::vector<webrtc::SdpVideoFormat>& supported_formats,
    const webrtc::SdpVideoFormat& format) {
  for (const webrtc::SdpVideoFormat& supported_format : supported_formats) {
    if (cricket::IsSameCodec(format.name, format.parameters,
                             supported_format.name,
                             supported_format.parameters)) {
      return true;
    }
  }
  return false;
}

template <typename Factory>
bool IsFormatSupported(Factory* factory, const webrtc::SdpVideoFormat& format) {
  return factory && IsFormatSupported(factory->GetSupportedFormats(), format);
}

// Merge |formats1| and |formats2|, but avoid adding duplicate formats.
std::vector<webrtc::SdpVideoFormat> MergeFormats(
    std::vector<webrtc::SdpVideoFormat> formats1,
    std::vector<webrtc::SdpVideoFormat> formats2) {
  for (const webrtc::SdpVideoFormat& format : formats2) {
    // Don't add same format twice.
    if (!IsFormatSupported(formats1, format))
      formats1.push_back(format);
  }
  return formats1;
}

std::unique_ptr<webrtc::VideoDecoder> CreateDecoder(
    webrtc::VideoDecoderFactory* factory,
    const webrtc::SdpVideoFormat& format) {
  return IsFormatSupported(factory, format)
             ? factory->CreateVideoDecoder(format)
             : nullptr;
}

template <typename SoftwareWrapperClass, typename CoderClass>
std::unique_ptr<CoderClass> Wrap(std::unique_ptr<CoderClass> internal_coder,
                                 std::unique_ptr<CoderClass> external_coder) {
  if (internal_coder && external_coder) {
    return base::MakeUnique<SoftwareWrapperClass>(std::move(internal_coder),
                                                  std::move(external_coder));
  }
  return external_coder ? std::move(external_coder) : std::move(internal_coder);
}

// This class combines an external factory with the internal factory and adds
// internal SW codecs, simulcast, and SW fallback wrappers.
class EncoderAdapter : public webrtc::VideoEncoderFactory {
 public:
  explicit EncoderAdapter(
      std::unique_ptr<webrtc::VideoEncoderFactory> external_encoder_factory)
      : external_encoder_factory_(std::move(external_encoder_factory)) {}

  webrtc::VideoEncoderFactory::CodecInfo QueryVideoEncoder(
      const webrtc::SdpVideoFormat& format) const override {
    const webrtc::VideoEncoderFactory* factory =
        IsFormatSupported(external_encoder_factory_.get(), format)
            ? external_encoder_factory_.get()
            : &internal_encoder_factory_;
    return factory->QueryVideoEncoder(format);
  }

  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& format) override {
    std::unique_ptr<webrtc::VideoEncoder> internal_encoder;
    if (IsFormatSupported(internal_encoder_factory_.GetSupportedFormats(),
                          format)) {
      internal_encoder =
          cricket::CodecNamesEq(format.name.c_str(), cricket::kVp8CodecName)
              ? base::MakeUnique<webrtc::VP8EncoderSimulcastProxy>(
                    &internal_encoder_factory_)
              : internal_encoder_factory_.CreateVideoEncoder(format);
    }

    std::unique_ptr<webrtc::VideoEncoder> external_encoder;
    if (IsFormatSupported(external_encoder_factory_.get(), format)) {
      external_encoder =
          cricket::CodecNamesEq(format.name.c_str(), cricket::kVp8CodecName)
              ? base::MakeUnique<webrtc::SimulcastEncoderAdapter>(
                    external_encoder_factory_.get())
              : external_encoder_factory_->CreateVideoEncoder(format);
    }

    return Wrap<webrtc::VideoEncoderSoftwareFallbackWrapper>(
        std::move(internal_encoder), std::move(external_encoder));
  }

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    std::vector<webrtc::SdpVideoFormat> internal_formats =
        internal_encoder_factory_.GetSupportedFormats();
    return external_encoder_factory_
               ? MergeFormats(internal_formats,
                              external_encoder_factory_->GetSupportedFormats())
               : internal_formats;
  }

 private:
  webrtc::InternalEncoderFactory internal_encoder_factory_;
  const std::unique_ptr<webrtc::VideoEncoderFactory> external_encoder_factory_;
};

// This class combines an external factory with the internal factory and adds
// internal SW codecs and SW fallback wrappers.
class DecoderAdapter : public webrtc::VideoDecoderFactory {
 public:
  explicit DecoderAdapter(
      std::unique_ptr<webrtc::VideoDecoderFactory> external_decoder_factory)
      : external_decoder_factory_(std::move(external_decoder_factory)) {}

  std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(
      const webrtc::SdpVideoFormat& format) override {
    std::unique_ptr<webrtc::VideoDecoder> internal_decoder =
        CreateDecoder(&internal_decoder_factory_, format);

    std::unique_ptr<webrtc::VideoDecoder> external_decoder =
        CreateDecoder(external_decoder_factory_.get(), format);

    return Wrap<webrtc::VideoDecoderSoftwareFallbackWrapper>(
        std::move(internal_decoder), std::move(external_decoder));
  }

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    std::vector<webrtc::SdpVideoFormat> internal_formats =
        internal_decoder_factory_.GetSupportedFormats();
    return external_decoder_factory_
               ? MergeFormats(internal_formats,
                              external_decoder_factory_->GetSupportedFormats())
               : internal_formats;
  }

 private:
  webrtc::InternalDecoderFactory internal_decoder_factory_;
  const std::unique_ptr<webrtc::VideoDecoderFactory> external_decoder_factory_;
};

}  // namespace

std::unique_ptr<webrtc::VideoEncoderFactory> CreateWebrtcVideoEncoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  std::unique_ptr<webrtc::VideoEncoderFactory> encoder_factory;

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (gpu_factories && gpu_factories->IsGpuVideoAcceleratorEnabled() &&
      !cmd_line->HasSwitch(switches::kDisableWebRtcHWEncoding)) {
    encoder_factory.reset(new RTCVideoEncoderFactory(gpu_factories));
  }

#if defined(OS_ANDROID)
  if (!media::MediaCodecUtil::SupportsSetParameters())
    encoder_factory.reset();
#endif

  return base::MakeUnique<EncoderAdapter>(std::move(encoder_factory));
}

std::unique_ptr<webrtc::VideoDecoderFactory> CreateWebrtcVideoDecoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  std::unique_ptr<webrtc::VideoDecoderFactory> decoder_factory;

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (gpu_factories && gpu_factories->IsGpuVideoAcceleratorEnabled() &&
      !cmd_line->HasSwitch(switches::kDisableWebRtcHWDecoding)) {
    decoder_factory.reset(new RTCVideoDecoderFactory(gpu_factories));
  }

  return base::MakeUnique<DecoderAdapter>(std::move(decoder_factory));
}

}  // namespace content
