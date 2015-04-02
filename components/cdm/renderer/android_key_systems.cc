// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/renderer/android_key_systems.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "components/cdm/common/cdm_messages_android.h"
#include "components/cdm/renderer/widevine_key_systems.h"
#include "content/public/renderer/render_thread.h"
#include "media/base/eme_constants.h"
#include "media/base/media_switches.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

using media::EmeRobustness;
using media::KeySystemInfo;
using media::SupportedCodecs;

namespace cdm {

static SupportedKeySystemResponse QueryKeySystemSupport(
    const std::string& key_system) {
  SupportedKeySystemRequest request;
  SupportedKeySystemResponse response;

  request.key_system = key_system;
  request.codecs = media::EME_CODEC_ALL;
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_QueryKeySystemSupport(request, &response));
  DCHECK(!(response.compositing_codecs & ~media::EME_CODEC_ALL))
      << "unrecognized codec";
  DCHECK(!(response.non_compositing_codecs & ~media::EME_CODEC_ALL))
      << "unrecognized codec";
  return response;
}

void AddAndroidWidevine(std::vector<KeySystemInfo>* concrete_key_systems,
                        bool is_non_compositing_supported) {
  SupportedKeySystemResponse response = QueryKeySystemSupport(
      kWidevineKeySystem);

  // When creating the WIDEVINE key system, BrowserCdmFactoryAndroid configures
  // the CDM's security level based on a pref. Therefore the supported
  // codec/robustenss combinations depend on that pref, represented by
  // |bool is_non_compositing_supported|.
  // TODO(sandersd): For unprefixed, set the security level based on the
  // requested robustness instead of the flag. http://crbug.com/467779
  // We should also stop using the term "non_compositing."
  SupportedCodecs codecs = response.compositing_codecs;
  EmeRobustness max_audio_robustness = EmeRobustness::SW_SECURE_CRYPTO;
  EmeRobustness max_video_robustness = EmeRobustness::SW_SECURE_CRYPTO;
  if (is_non_compositing_supported) {
    codecs = response.non_compositing_codecs;
    max_audio_robustness = EmeRobustness::HW_SECURE_CRYPTO;
    max_video_robustness = EmeRobustness::HW_SECURE_ALL;
  }
  if (codecs != media::EME_CODEC_NONE) {
    AddWidevineWithCodecs(
        WIDEVINE,
        codecs,
        max_audio_robustness,
        max_video_robustness,
        media::EME_SESSION_TYPE_NOT_SUPPORTED,  // persistent-license.
        media::EME_SESSION_TYPE_NOT_SUPPORTED,  // persistent-release-message.
        media::EME_FEATURE_NOT_SUPPORTED,       // Persistent state.
        media::EME_FEATURE_ALWAYS_ENABLED,      // Distinctive identifier.
        concrete_key_systems);
  }

  // For compatibility with the prefixed API, register a separate L1 key system.
  // When creating the WIDEVINE_HR_NON_COMPOSITING key system,
  // BrowserCdmFactoryAndroid does not configure the CDM's security level (that
  // is, leaves it as L1); therefore only secure codecs are supported.
  // TODO(ddorwin): Remove with unprefixed. http://crbug.com/249976
  if (response.non_compositing_codecs != media::EME_CODEC_NONE) {
    AddWidevineWithCodecs(
        WIDEVINE_HR_NON_COMPOSITING,
        response.non_compositing_codecs,
        EmeRobustness::HW_SECURE_CRYPTO,        // Max audio robustness.
        EmeRobustness::HW_SECURE_ALL,           // Max video robustness.
        media::EME_SESSION_TYPE_NOT_SUPPORTED,  // persistent-license.
        media::EME_SESSION_TYPE_NOT_SUPPORTED,  // persistent-release-message.
        media::EME_FEATURE_NOT_SUPPORTED,       // Persistent state.
        media::EME_FEATURE_ALWAYS_ENABLED,      // Distinctive identifier.
        concrete_key_systems);
  }
}

void AddAndroidPlatformKeySystems(
    std::vector<KeySystemInfo>* concrete_key_systems) {
  std::vector<std::string> key_system_names;
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_GetPlatformKeySystemNames(&key_system_names));

  for (std::vector<std::string>::const_iterator it = key_system_names.begin();
       it != key_system_names.end(); ++it) {
    SupportedKeySystemResponse response = QueryKeySystemSupport(*it);
    if (response.compositing_codecs != media::EME_CODEC_NONE) {
      KeySystemInfo info;
      info.key_system = *it;
      info.supported_codecs = response.compositing_codecs;
      // Here we assume that support for a container implies support for the
      // associated initialization data type. KeySystems handles validating
      // |init_data_type| x |container| pairings.
      if (response.compositing_codecs & media::EME_CODEC_WEBM_ALL)
        info.supported_init_data_types |= media::kInitDataTypeMaskWebM;
#if defined(USE_PROPRIETARY_CODECS)
      if (response.compositing_codecs & media::EME_CODEC_MP4_ALL)
        info.supported_init_data_types |= media::kInitDataTypeMaskCenc;
#endif  // defined(USE_PROPRIETARY_CODECS)
      info.max_audio_robustness = EmeRobustness::EMPTY;
      info.max_video_robustness = EmeRobustness::EMPTY;
      // Assume the worst case (from a user point of view).
      info.persistent_license_support = media::EME_SESSION_TYPE_NOT_SUPPORTED;
      info.persistent_release_message_support =
          media::EME_SESSION_TYPE_NOT_SUPPORTED;
      info.persistent_state_support = media::EME_FEATURE_ALWAYS_ENABLED;
      info.distinctive_identifier_support = media::EME_FEATURE_ALWAYS_ENABLED;
      concrete_key_systems->push_back(info);
    }
  }
}

}  // namespace cdm
