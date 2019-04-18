// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediacapabilitiesclient_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "media/base/cdm_config.h"
#include "media/base/key_system_names.h"
#include "media/base/mime_util.h"
#include "media/base/supported_types.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/blink/webcontentdecryptionmoduleaccess_impl.h"
#include "media/mojo/interfaces/media_types.mojom.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/platform/modules/media_capabilities/web_media_capabilities_info.h"
#include "third_party/blink/public/platform/modules/media_capabilities/web_media_decoding_configuration.h"
#include "third_party/blink/public/platform/modules/media_capabilities/web_video_configuration.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scoped_web_callbacks.h"

namespace media {

void BindToHistoryService(mojom::VideoDecodePerfHistoryPtr* history_ptr) {
  DVLOG(2) << __func__;
  blink::Platform* platform = blink::Platform::Current();
  service_manager::Connector* connector = platform->GetConnector();

  connector->BindInterface(platform->GetBrowserServiceName(),
                           mojo::MakeRequest(history_ptr));
}

bool CheckVideoSupport(const blink::WebVideoConfiguration& video_config,
                       VideoCodecProfile* out_video_profile) {
  bool video_supported = false;
  VideoCodec video_codec = kUnknownVideoCodec;
  uint8_t video_level = 0;
  VideoColorSpace video_color_space;
  bool is_video_codec_ambiguous = true;

  if (!ParseVideoCodecString(
          video_config.mime_type.Ascii(), video_config.codec.Ascii(),
          &is_video_codec_ambiguous, &video_codec, out_video_profile,
          &video_level, &video_color_space)) {
    DVLOG(2) << __func__ << " Failed to parse video contentType: "
             << video_config.mime_type.Ascii()
             << "; codecs=" << video_config.codec.Ascii();
    video_supported = false;
  } else if (is_video_codec_ambiguous) {
    DVLOG(2) << __func__ << " Invalid (ambiguous) video codec string:"
             << video_config.codec.Ascii();
    video_supported = false;
  } else {
    video_supported = IsSupportedVideoType(
        {video_codec, *out_video_profile, video_level, video_color_space});
  }

  return video_supported;
}

WebMediaCapabilitiesClientImpl::WebMediaCapabilitiesClientImpl() = default;

WebMediaCapabilitiesClientImpl::~WebMediaCapabilitiesClientImpl() = default;

namespace {
void VideoPerfInfoCallback(
    blink::ScopedWebCallbacks<blink::WebMediaCapabilitiesDecodingInfoCallbacks>
        scoped_callbacks,
    std::unique_ptr<blink::WebMediaCapabilitiesDecodingInfo> info,
    bool is_smooth,
    bool is_power_efficient) {
  DCHECK(info->supported);
  info->smooth = is_smooth;
  info->power_efficient = is_power_efficient;

  scoped_callbacks.PassCallbacks()->OnSuccess(std::move(info));
}

void OnGetPerfInfoError(
    std::unique_ptr<blink::WebMediaCapabilitiesDecodingInfoCallbacks>
        callbacks) {
  callbacks->OnError();
}
}  // namespace

void WebMediaCapabilitiesClientImpl::DecodingInfo(
    const blink::WebMediaDecodingConfiguration& configuration,
    std::unique_ptr<blink::WebContentDecryptionModuleAccess> cdm_access,
    std::unique_ptr<blink::WebMediaCapabilitiesDecodingInfoCallbacks>
        callbacks) {
  std::unique_ptr<blink::WebMediaCapabilitiesDecodingInfo> info(
      new blink::WebMediaCapabilitiesDecodingInfo());

  DCHECK(configuration.video_configuration);
  const blink::WebVideoConfiguration& video_config =
      configuration.video_configuration.value();
  VideoCodecProfile video_profile = VIDEO_CODEC_PROFILE_UNKNOWN;

  // TODO(chcunningham): Skip the call to IsSupportedVideoType() when we already
  // have a |cdm_access|. In this case, we know the codec is supported, but we
  // still need to get the profile and validate the mime type is allowed (not
  // ambiguous) by MediaCapapbilities.
  bool video_supported = CheckVideoSupport(video_config, &video_profile);

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

  std::string key_system = "";
  bool use_hw_secure_codecs = false;
  if (cdm_access) {
    WebContentDecryptionModuleAccessImpl* cdm_access_impl =
        WebContentDecryptionModuleAccessImpl::From(cdm_access.get());

    key_system = cdm_access_impl->GetKeySystem().Ascii();
    use_hw_secure_codecs = cdm_access_impl->GetCdmConfig().use_hw_secure_codecs;

    // EME is supported! Provide the MediaKeySystemAccess.
    info->content_decryption_module_access = std::move(cdm_access);
  }

  mojom::PredictionFeaturesPtr features = mojom::PredictionFeatures::New(
      video_profile, gfx::Size(video_config.width, video_config.height),
      video_config.framerate, key_system, use_hw_secure_codecs);

  decode_history_ptr_->GetPerfInfo(
      std::move(features),
      base::BindOnce(
          &VideoPerfInfoCallback,
          blink::MakeScopedWebCallbacks(std::move(callbacks),
                                        base::BindOnce(&OnGetPerfInfoError)),
          std::move(info)));
}

void WebMediaCapabilitiesClientImpl::BindVideoDecodePerfHistoryForTests(
    mojom::VideoDecodePerfHistoryPtr decode_history_ptr) {
  decode_history_ptr_ = std::move(decode_history_ptr);
}

}  // namespace media
