// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/key_systems_cast.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chromecast/media/base/key_systems_common.h"
#include "components/cdm/renderer/widevine_key_system_properties.h"
#include "media/base/eme_constants.h"
#include "media/base/key_system_properties.h"

#include "widevine_cdm_version.h" // In SHARED_INTERMEDIATE_DIR.

using ::media::EmeConfigRule;
using ::media::EmeFeatureSupport;
using ::media::EmeInitDataType;
using ::media::EmeMediaType;
using ::media::EmeRobustness;
using ::media::EmeSessionTypeSupport;
using ::media::SupportedCodecs;

namespace chromecast {
namespace shell {
namespace {

#if defined(PLAYREADY_CDM_AVAILABLE)
class PlayReadyKeySystemProperties : public ::media::KeySystemProperties {
 public:
  std::string GetKeySystemName() const override {
    return media::kChromecastPlayreadyKeySystem;
  }

  bool IsSupportedInitDataType(EmeInitDataType init_data_type) const override {
    return init_data_type == EmeInitDataType::CENC;
  }

  SupportedCodecs GetSupportedCodecs() const override {
    return ::media::EME_CODEC_MP4_AAC | ::media::EME_CODEC_MP4_AVC1;
  }

  EmeConfigRule GetRobustnessConfigRule(
      EmeMediaType media_type,
      const std::string& requested_robustness) const override {
    return requested_robustness.empty() ? EmeConfigRule::SUPPORTED
                                        : EmeConfigRule::NOT_SUPPORTED;
  }

  EmeSessionTypeSupport GetPersistentLicenseSessionSupport() const override {
#if defined(OS_ANDROID)
    return EmeSessionTypeSupport::NOT_SUPPORTED;
#else
    return EmeSessionTypeSupport::SUPPORTED;
#endif
  }

  EmeSessionTypeSupport GetPersistentReleaseMessageSessionSupport()
      const override {
    return EmeSessionTypeSupport::NOT_SUPPORTED;
  }

  EmeFeatureSupport GetPersistentStateSupport() const override {
    return EmeFeatureSupport::ALWAYS_ENABLED;
  }
  EmeFeatureSupport GetDistinctiveIdentifierSupport() const override {
    return EmeFeatureSupport::ALWAYS_ENABLED;
  }
};
#endif  // PLAYREADY_CDM_AVAILABLE

}  // namespace

void AddChromecastKeySystems(
    std::vector<std::unique_ptr<::media::KeySystemProperties>>*
        key_systems_properties) {
#if defined(PLAYREADY_CDM_AVAILABLE)
  key_systems_properties->emplace_back(new PlayReadyKeySystemProperties());
#endif  // defined(PLAYREADY_CDM_AVAILABLE)

#if defined(WIDEVINE_CDM_AVAILABLE)
  ::media::SupportedCodecs codecs =
      ::media::EME_CODEC_MP4_AAC | ::media::EME_CODEC_MP4_AVC1 |
      ::media::EME_CODEC_WEBM_VP8 | ::media::EME_CODEC_WEBM_VP9;
  key_systems_properties->emplace_back(new cdm::WidevineKeySystemProperties(
      codecs,  // Regular codecs.
#if defined(OS_ANDROID)
      codecs,  // Hardware-secure codecs.
#endif
      EmeRobustness::HW_SECURE_ALL,          // Max audio robustness.
      EmeRobustness::HW_SECURE_ALL,          // Max video robustness.
      EmeSessionTypeSupport::NOT_SUPPORTED,  // persistent-license.
      EmeSessionTypeSupport::NOT_SUPPORTED,  // persistent-release-message.
      // Note: On Chromecast, all CDMs may have persistent state.
      EmeFeatureSupport::ALWAYS_ENABLED,    // Persistent state.
      EmeFeatureSupport::ALWAYS_ENABLED));  // Distinctive identifier.
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
}

}  // namespace shell
}  // namespace chromecast
