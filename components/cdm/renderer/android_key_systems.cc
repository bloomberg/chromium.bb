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

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

using content::KeySystemInfo;
using content::SupportedCodecs;

namespace cdm {

static SupportedKeySystemResponse QueryKeySystemSupport(
    const std::string& key_system) {
  SupportedKeySystemRequest request;
  SupportedKeySystemResponse response;

  request.key_system = key_system;
  request.codecs = content::EME_CODEC_ALL;
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_QueryKeySystemSupport(request, &response));
  DCHECK(!(response.compositing_codecs & ~content::EME_CODEC_ALL))
      << "unrecognized codec";
  DCHECK(!(response.non_compositing_codecs & ~content::EME_CODEC_ALL))
      << "unrecognized codec";
  return response;
}

void AddAndroidWidevine(std::vector<KeySystemInfo>* concrete_key_systems) {
  SupportedKeySystemResponse response = QueryKeySystemSupport(
      kWidevineKeySystem);
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

void AddAndroidPlatformKeySystems(
    std::vector<KeySystemInfo>* concrete_key_systems) {
  std::vector<std::string> key_system_names;
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_GetPlatformKeySystemNames(&key_system_names));

  for (std::vector<std::string>::const_iterator it = key_system_names.begin();
       it != key_system_names.end(); ++it) {
    SupportedKeySystemResponse response = QueryKeySystemSupport(*it);
    if (response.compositing_codecs != content::EME_CODEC_NONE) {
      KeySystemInfo info(*it);
      info.supported_codecs = response.compositing_codecs;
      concrete_key_systems->push_back(info);
    }
  }
}

}  // namespace cdm
