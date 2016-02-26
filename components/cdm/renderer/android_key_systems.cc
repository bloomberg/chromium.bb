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

using media::EmeFeatureSupport;
using media::EmeRobustness;
using media::EmeSessionTypeSupport;
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

void AddAndroidWidevine(std::vector<KeySystemInfo>* concrete_key_systems) {
  SupportedKeySystemResponse response = QueryKeySystemSupport(
      kWidevineKeySystem);

  // Since we do not control the implementation of the MediaDrm API on Android,
  // we assume that it can and will make use of persistence even though no
  // persistence-based features are supported.

  if (response.compositing_codecs != media::EME_CODEC_NONE) {
    AddWidevineWithCodecs(
        response.compositing_codecs,           // Regular codecs.
        response.non_compositing_codecs,       // Hardware-secure codecs.
        EmeRobustness::HW_SECURE_CRYPTO,       // Max audio robustness.
        EmeRobustness::HW_SECURE_ALL,          // Max video robustness.
        EmeSessionTypeSupport::NOT_SUPPORTED,  // persistent-license.
        EmeSessionTypeSupport::NOT_SUPPORTED,  // persistent-release-message.
        EmeFeatureSupport::ALWAYS_ENABLED,     // Persistent state.
        EmeFeatureSupport::ALWAYS_ENABLED,     // Distinctive identifier.
        concrete_key_systems);
  } else {
    // It doesn't make sense to support secure codecs but not regular codecs.
    DCHECK(response.non_compositing_codecs == media::EME_CODEC_NONE);
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
      // Assume that platform key systems support no features but can and will
      // make use of persistence and identifiers.
      info.persistent_license_support =
          media::EmeSessionTypeSupport::NOT_SUPPORTED;
      info.persistent_release_message_support =
          media::EmeSessionTypeSupport::NOT_SUPPORTED;
      info.persistent_state_support = media::EmeFeatureSupport::ALWAYS_ENABLED;
      info.distinctive_identifier_support =
          media::EmeFeatureSupport::ALWAYS_ENABLED;
      concrete_key_systems->push_back(info);
    }
  }
}

}  // namespace cdm
