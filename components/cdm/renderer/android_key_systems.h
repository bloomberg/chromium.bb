// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_RENDERER_ANDROID_KEY_SYSTEMS_H_
#define COMPONENTS_CDM_RENDERER_ANDROID_KEY_SYSTEMS_H_

#include <vector>

#include "media/base/key_system_info.h"

namespace cdm {

void AddAndroidWidevine(
    std::vector<media::KeySystemInfo>* concrete_key_systems);

// Add platform-supported key systems which are not explicitly handled
// by Chrome.
void AddAndroidPlatformKeySystems(
    std::vector<media::KeySystemInfo>* concrete_key_systems);

}  // namespace cdm

#endif  // COMPONENTS_CDM_RENDERER_ANDROID_KEY_SYSTEMS_H_
