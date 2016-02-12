// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/key_systems_cast.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chromecast/media/base/key_systems_common.h"
#include "components/cdm/renderer/widevine_key_systems.h"
#include "media/base/eme_constants.h"

#include "widevine_cdm_version.h" // In SHARED_INTERMEDIATE_DIR.

using ::media::EmeFeatureSupport;
using ::media::EmeRobustness;
using ::media::EmeSessionTypeSupport;

namespace chromecast {
namespace shell {

void AddKeySystemWithCodecs(
    const std::string& key_system_name,
    std::vector<::media::KeySystemInfo>* key_systems_info) {
  ::media::KeySystemInfo info;
  info.key_system = key_system_name;
  info.supported_init_data_types = ::media::kInitDataTypeMaskCenc;
  info.supported_codecs =
      ::media::EME_CODEC_MP4_AAC | ::media::EME_CODEC_MP4_AVC1;
  info.max_audio_robustness = ::media::EmeRobustness::EMPTY;
  info.max_video_robustness = ::media::EmeRobustness::EMPTY;
#if defined(OS_ANDROID)
  info.persistent_license_support =
      ::media::EmeSessionTypeSupport::NOT_SUPPORTED;
#else
  info.persistent_license_support =
      ::media::EmeSessionTypeSupport::SUPPORTED;
#endif
  info.persistent_release_message_support =
      ::media::EmeSessionTypeSupport::NOT_SUPPORTED;
  info.persistent_state_support = ::media::EmeFeatureSupport::ALWAYS_ENABLED;
  info.distinctive_identifier_support =
      ::media::EmeFeatureSupport::ALWAYS_ENABLED;
  key_systems_info->push_back(info);
}

void AddChromecastKeySystems(
    std::vector<::media::KeySystemInfo>* key_systems_info) {
#if defined(WIDEVINE_CDM_AVAILABLE)
  ::media::SupportedCodecs codecs =
      ::media::EME_CODEC_MP4_AAC | ::media::EME_CODEC_MP4_AVC1 |
      ::media::EME_CODEC_WEBM_VP8 | ::media::EME_CODEC_WEBM_VP9;
  AddWidevineWithCodecs(
      cdm::WIDEVINE,
      codecs,                                // Regular codecs.
#if defined(OS_ANDROID)
      codecs,                                // Hardware-secure codecs.
#endif  // defined(OS_ANDROID)
      EmeRobustness::HW_SECURE_ALL,          // Max audio robustness.
      EmeRobustness::HW_SECURE_ALL,          // Max video robustness.
      EmeSessionTypeSupport::NOT_SUPPORTED,  // persistent-license.
      EmeSessionTypeSupport::NOT_SUPPORTED,  // persistent-release-message.
      // Note: On Chromecast, all CDMs may have persistent state.
      EmeFeatureSupport::ALWAYS_ENABLED,     // Persistent state.
      EmeFeatureSupport::ALWAYS_ENABLED,     // Distinctive identifier.
      key_systems_info);
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

#if defined(PLAYREADY_CDM_AVAILABLE)
  AddKeySystemWithCodecs(media::kChromecastPlayreadyKeySystem,
                         key_systems_info);
#endif  // defined(PLAYREADY_CDM_AVAILABLE)
}

}  // namespace shell
}  // namespace chromecast
