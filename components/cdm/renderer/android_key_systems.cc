// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/renderer/android_key_systems.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "components/cdm/common/cdm_messages_android.h"
#include "components/cdm/renderer/widevine_key_systems.h"
#include "content/public/renderer/render_thread.h"
#include "media/base/eme_constants.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

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
  if (response.compositing_codecs != media::EME_CODEC_NONE) {
    AddWidevineWithCodecs(
        WIDEVINE,
        static_cast<SupportedCodecs>(response.compositing_codecs),
        media::EME_SESSION_TYPE_NOT_SUPPORTED,  // Persistent license.
        media::EME_SESSION_TYPE_NOT_SUPPORTED,  // Persistent release message.
        media::EME_FEATURE_NOT_SUPPORTED,       // Persistent state.
        media::EME_FEATURE_ALWAYS_ENABLED,      // Distinctive identifier.
        concrete_key_systems);
  }

  if (response.non_compositing_codecs != media::EME_CODEC_NONE) {
    // TODO(ddorwin): Remove with unprefixed. http://crbug.com/249976
    AddWidevineWithCodecs(
        WIDEVINE_HR_NON_COMPOSITING,
        static_cast<SupportedCodecs>(response.non_compositing_codecs),
        media::EME_SESSION_TYPE_NOT_SUPPORTED,  // Persistent license.
        media::EME_SESSION_TYPE_NOT_SUPPORTED,  // Persistent release message.
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
        info.supported_init_data_types |= media::EME_INIT_DATA_TYPE_WEBM;
#if defined(USE_PROPRIETARY_CODECS)
      if (response.compositing_codecs & media::EME_CODEC_MP4_ALL)
        info.supported_init_data_types |= media::EME_INIT_DATA_TYPE_CENC;
#endif  // defined(USE_PROPRIETARY_CODECS)
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
