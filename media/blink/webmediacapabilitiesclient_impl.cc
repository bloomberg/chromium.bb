// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediacapabilitiesclient_impl.h"

#include "base/bind_helpers.h"
#include "media/base/audio_codecs.h"
#include "media/base/decode_capabilities.h"
#include "media/base/mime_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/modules/media_capabilities/WebAudioConfiguration.h"
#include "third_party/WebKit/public/platform/modules/media_capabilities/WebMediaCapabilitiesInfo.h"
#include "third_party/WebKit/public/platform/modules/media_capabilities/WebMediaConfiguration.h"
#include "third_party/WebKit/public/platform/modules/media_capabilities/WebVideoConfiguration.h"

namespace media {

void BindToHistoryService(mojom::VideoDecodePerfHistoryPtr* history_ptr) {
  blink::Platform* platform = blink::Platform::Current();
  service_manager::Connector* connector = platform->GetConnector();

  connector->BindInterface(platform->GetBrowserServiceName(),
                           mojo::MakeRequest(history_ptr));
}

WebMediaCapabilitiesClientImpl::WebMediaCapabilitiesClientImpl() = default;

WebMediaCapabilitiesClientImpl::~WebMediaCapabilitiesClientImpl() = default;

void VideoPerfInfoCallback(
    std::unique_ptr<blink::WebMediaCapabilitiesQueryCallbacks> callbacks,
    std::unique_ptr<blink::WebMediaCapabilitiesInfo> info,
    bool is_smooth,
    bool is_power_efficient) {
  DCHECK(info->supported);
  info->smooth = is_smooth;
  info->power_efficient = is_power_efficient;
  callbacks->OnSuccess(std::move(info));
}

void WebMediaCapabilitiesClientImpl::DecodingInfo(
    const blink::WebMediaConfiguration& configuration,
    std::unique_ptr<blink::WebMediaCapabilitiesQueryCallbacks> callbacks) {
  std::unique_ptr<blink::WebMediaCapabilitiesInfo> info(
      new blink::WebMediaCapabilitiesInfo());

  // TODO(chcunningham): split this up with helper methods.

  bool audio_supported = true;
  if (configuration.audio_configuration) {
    const blink::WebAudioConfiguration& audio_config =
        configuration.audio_configuration.value();
    AudioCodec audio_codec;
    bool is_audio_codec_ambiguous;

    if (!ParseAudioCodecString(audio_config.mime_type.Ascii(),
                               audio_config.codec.Ascii(),
                               &is_audio_codec_ambiguous, &audio_codec)) {
      // TODO(chcunningham): Replace this and other DVLOGs here with MEDIA_LOG.
      // MediaCapabilities may need its own tab in chrome://media-internals.
      DVLOG(2) << __func__ << " Failed to parse audio contentType: "
               << audio_config.mime_type.Ascii()
               << "; codecs=" << audio_config.codec.Ascii();
      audio_supported = false;
    } else if (is_audio_codec_ambiguous) {
      DVLOG(2) << __func__ << " Invalid (ambiguous) audio codec string:"
               << audio_config.codec.Ascii();
      audio_supported = false;
    } else {
      AudioConfig audio_config = {audio_codec};
      audio_supported = IsSupportedAudioConfig(audio_config);
    }
  }

  // No need to check video capabilities if video not included in configuration
  // or when audio is already known to be unsupported.
  if (!audio_supported || !configuration.video_configuration) {
    // Supported audio-only configurations are always considered smooth and
    // power efficient.
    info->supported = info->smooth = info->power_efficient = audio_supported;
    callbacks->OnSuccess(std::move(info));
    return;
  }

  // Audio is supported and video configuration is provided in the query; all
  // that remains is to check video support and performance.
  bool video_supported;
  DCHECK(audio_supported);
  DCHECK(configuration.video_configuration);
  const blink::WebVideoConfiguration& video_config =
      configuration.video_configuration.value();
  VideoCodec video_codec;
  VideoCodecProfile video_profile;
  uint8_t video_level;
  VideoColorSpace video_color_space;
  bool is_video_codec_ambiguous;

  if (!ParseVideoCodecString(
          video_config.mime_type.Ascii(), video_config.codec.Ascii(),
          &is_video_codec_ambiguous, &video_codec, &video_profile, &video_level,
          &video_color_space)) {
    DVLOG(2) << __func__ << " Failed to parse video contentType: "
             << video_config.mime_type.Ascii()
             << "; codecs=" << video_config.codec.Ascii();
    video_supported = false;
  } else if (is_video_codec_ambiguous) {
    DVLOG(2) << __func__ << " Invalid (ambiguous) video codec string:"
             << video_config.codec.Ascii();
    video_supported = false;
  } else {
    video_supported = IsSupportedVideoConfig(
        {video_codec, video_profile, video_level, video_color_space});
  }

  // Return early for unsupported configurations.
  if (!video_supported) {
    info->supported = info->smooth = info->power_efficient = video_supported;
    callbacks->OnSuccess(std::move(info));
    return;
  }

  // Video is supported! Check its performance history.
  info->supported = true;

  if (!decode_history_ptr_.is_bound())
    BindToHistoryService(&decode_history_ptr_);
  DCHECK(decode_history_ptr_.is_bound());

  decode_history_ptr_->GetPerfInfo(
      video_profile, gfx::Size(video_config.width, video_config.height),
      video_config.framerate,
      base::BindOnce(&VideoPerfInfoCallback, std::move(callbacks),
                     std::move(info)));
}

}  // namespace media
