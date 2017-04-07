// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/media/cast_media_client.h"

#include "chromecast/media/base/media_caps.h"
#include "chromecast/media/base/media_codec_support.h"
#include "chromecast/media/base/supported_codec_profile_levels_memo.h"
#include "chromecast/public/media/media_capabilities_shlib.h"

// TODO(servolk): Is there a better way to override just IsSupportedVideoConfig
// without duplicating content::RenderMediaClient implementation?
// For now use this to allow access to the ::media::GetMediaClient.
namespace media {
MEDIA_EXPORT MediaClient* GetMediaClient();
}

namespace chromecast {
namespace media {

void CastMediaClient::Initialize(
    SupportedCodecProfileLevelsMemo* supported_profiles) {
  ::media::SetMediaClient(
      new CastMediaClient(::media::GetMediaClient(), supported_profiles));
}

CastMediaClient::CastMediaClient(
    ::media::MediaClient* content_media_client,
    SupportedCodecProfileLevelsMemo* supported_profiles) {
  // Ensure that CastMediaClient gets initialized after the
  // content::RenderMediaClient.
  DCHECK(content_media_client);
  content_media_client_ = content_media_client;
  supported_profiles_ = supported_profiles;
}

CastMediaClient::~CastMediaClient() {}

void CastMediaClient::AddKeySystemsInfoForUMA(
    std::vector<::media::KeySystemInfoForUMA>* key_systems_info_for_uma) {
  content_media_client_->AddKeySystemsInfoForUMA(key_systems_info_for_uma);
}

bool CastMediaClient::IsKeySystemsUpdateNeeded() {
  return content_media_client_->IsKeySystemsUpdateNeeded();
}

void CastMediaClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<::media::KeySystemProperties>>*
        key_systems_properties) {
  content_media_client_->AddSupportedKeySystems(key_systems_properties);
}

void CastMediaClient::RecordRapporURL(const std::string& metric,
                                      const GURL& url) {
  content_media_client_->RecordRapporURL(metric, url);
}

// static
bool CastMediaClient::IsSupportedVideoConfig(
    const ::media::VideoConfig& config) {
// TODO(servolk): make use of eotf.
#if defined(OS_ANDROID)
  return supported_profiles_->IsSupportedVideoConfig(
      ToCastVideoCodec(config.codec, config.profile),
      ToCastVideoProfile(config.profile), config.level);
#else
  return MediaCapabilitiesShlib::IsSupportedVideoConfig(
      ToCastVideoCodec(config.codec, config.profile),
      ToCastVideoProfile(config.profile), config.level);
#endif
}

bool CastMediaClient::IsSupportedAudioConfig(
    const ::media::AudioConfig& config) {
#if defined(OS_ANDROID)
  AudioCodec codec = ToCastAudioCodec(config.codec);

  // No ATV device we know of has (E)AC3 decoder, so it relies on the audio sink
  // device.
  if (codec == kCodecEAC3)
    return MediaCapabilities::HdmiSinkSupportsEAC3();
  if (codec == kCodecAC3)
    return MediaCapabilities::HdmiSinkSupportsAC3();

  // TODO(sanfin): Implement this for Android.
  return true;
#else
  AudioCodec codec = ToCastAudioCodec(config.codec);
  // Cast platform implements software decoding of Opus and FLAC, so only PCM
  // support is necessary in order to support Opus and FLAC.
  if (codec == kCodecOpus || codec == kCodecFLAC)
    codec = kCodecPCM;

  // If HDMI sink supports AC3/EAC3 codecs then we don't need the vendor backend
  // to support these codec directly.
  if (codec == kCodecEAC3 && MediaCapabilities::HdmiSinkSupportsEAC3())
    return true;
  if (codec == kCodecAC3 && MediaCapabilities::HdmiSinkSupportsAC3())
    return true;

  AudioConfig cast_audio_config;
  cast_audio_config.codec = codec;
  return MediaCapabilitiesShlib::IsSupportedAudioConfig(cast_audio_config);
#endif
}

}  // namespace media
}  // namespace chromecast
