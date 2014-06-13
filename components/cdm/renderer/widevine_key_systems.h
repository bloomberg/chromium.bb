// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_RENDERER_WIDEVINE_KEY_SYSTEMS_H_
#define COMPONENTS_CDM_RENDERER_WIDEVINE_KEY_SYSTEMS_H_

#include <vector>

#include "content/public/renderer/key_system_info.h"

namespace cdm {

enum WidevineCdmType {
  WIDEVINE,
#if defined(OS_ANDROID)
  WIDEVINE_HR_NON_COMPOSITING,
#endif  // defined(OS_ANDROID)
};

void AddWidevineWithCodecs(
    WidevineCdmType widevine_cdm_type,
    content::SupportedCodecs supported_codecs,
    std::vector<content::KeySystemInfo>* concrete_key_systems);

}  // namespace cdm

#endif  // COMPONENTS_CDM_RENDERER_WIDEVINE_KEY_SYSTEMS_H_
