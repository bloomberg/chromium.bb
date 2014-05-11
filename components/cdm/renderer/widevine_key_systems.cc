// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/renderer/widevine_key_systems.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "components/cdm/common/cdm_messages_android.h"
#include "content/public/renderer/key_system_info.h"
#include "content/public/renderer/render_thread.h"

#include "widevine_cdm_version.h" // In SHARED_INTERMEDIATE_DIR.

#if defined(WIDEVINE_CDM_AVAILABLE)

using content::KeySystemInfo;
using content::SupportedCodecs;

namespace cdm {

// Return |name|'s parent key system.
static std::string GetDirectParentName(std::string name) {
  size_t last_period = name.find_last_of('.');
  DCHECK_GT(last_period, 0u);
  return name.substr(0u, last_period);
}

void AddWidevineWithCodecs(WidevineCdmType widevine_cdm_type,
                           SupportedCodecs supported_codecs,
                           std::vector<KeySystemInfo>* concrete_key_systems) {
  KeySystemInfo info(kWidevineKeySystem);

  switch (widevine_cdm_type) {
    case WIDEVINE:
      // For standard Widevine, add parent name.
      info.parent_key_system = GetDirectParentName(kWidevineKeySystem);
      break;
#if defined(OS_ANDROID)
    case WIDEVINE_HR_NON_COMPOSITING:
      info.key_system.append(".hrnoncompositing");
      break;
#endif  // defined(OS_ANDROID)
    default:
      NOTREACHED();
  }

  // TODO(xhwang): A container or an initDataType may be supported even though
  // there are no codecs supported in that container. Fix this when we support
  // initDataType.
  info.supported_codecs = supported_codecs;

#if defined(ENABLE_PEPPER_CDMS)
  info.pepper_type = kWidevineCdmPluginMimeType;
#endif  // defined(ENABLE_PEPPER_CDMS)

  concrete_key_systems->push_back(info);
}

#if defined(OS_ANDROID)
void AddAndroidWidevine(std::vector<KeySystemInfo>* concrete_key_systems) {
  SupportedKeySystemRequest request;
  SupportedKeySystemResponse response;

  request.key_system = kWidevineKeySystem;
  request.codecs = content::EME_CODEC_WEBM_ALL | content::EME_CODEC_MP4_ALL;
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_GetSupportedKeySystems(request, &response));
  DCHECK(response.compositing_codecs & content::EME_CODEC_ALL)
      << "unrecognized codec";
  DCHECK(response.non_compositing_codecs & content::EME_CODEC_ALL)
      << "unrecognized codec";
  if (response.compositing_codecs != content::EME_CODEC_NONE) {
    AddWidevineWithCodecs(
        WIDEVINE,
        static_cast<SupportedCodecs>(response.compositing_codecs),
        concrete_key_systems);
  }

  if (response.non_compositing_codecs != content::EME_CODEC_NONE) {
    AddWidevineWithCodecs(
        WIDEVINE_HR_NON_COMPOSITING,
        static_cast<SupportedCodecs>(response.non_compositing_codecs),
        concrete_key_systems);
  }
}
#endif  // OS_ANDROID

}  // namespace cdm

#endif  // defined(WIDEVINE_CDM_AVAILABLE)
