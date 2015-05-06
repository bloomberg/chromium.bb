// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_RENDERER_WIDEVINE_KEY_SYSTEMS_H_
#define COMPONENTS_CDM_RENDERER_WIDEVINE_KEY_SYSTEMS_H_

#include <vector>

#include "build/build_config.h"
#include "media/base/key_system_info.h"

namespace cdm {

enum WidevineCdmType {
  WIDEVINE,
#if defined(OS_ANDROID)
  WIDEVINE_HR_NON_COMPOSITING,
#endif  // defined(OS_ANDROID)
};

void AddWidevineWithCodecs(
    WidevineCdmType widevine_cdm_type,
    media::SupportedCodecs supported_codecs,
#if defined(OS_ANDROID)
    media::SupportedCodecs supported_secure_codecs,
#endif  // defined(OS_ANDROID)
    media::EmeRobustness max_audio_robustness,
    media::EmeRobustness max_video_robustness,
    media::EmeSessionTypeSupport persistent_license_support,
    media::EmeSessionTypeSupport persistent_release_message_support,
    media::EmeFeatureSupport persistent_state_support,
    media::EmeFeatureSupport distinctive_identifier_support,
    std::vector<media::KeySystemInfo>* concrete_key_systems);

}  // namespace cdm

#endif  // COMPONENTS_CDM_RENDERER_WIDEVINE_KEY_SYSTEMS_H_
