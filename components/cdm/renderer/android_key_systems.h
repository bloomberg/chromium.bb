// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_RENDERER_ANDROID_KEY_SYSTEMS_H_
#define COMPONENTS_CDM_RENDERER_ANDROID_KEY_SYSTEMS_H_

#include <vector>

#include "media/base/key_system_info.h"

namespace cdm {

void AddAndroidWidevine(
    std::vector<media::KeySystemInfo>* concrete_key_systems,
    // TODO(sandersd): Non-compositing support depends on the
    // use_video_overlay_for_embedded_encrypted_video pref, which may differ per
    // WebContents, meaning this cannot be a global setting for the renderer
    // process. Handle this per WebContents instead. http://crbug.com/467779
    bool is_non_compositing_supported);

// Add platform-supported key systems which are not explicitly handled
// by Chrome.
void AddAndroidPlatformKeySystems(
    std::vector<media::KeySystemInfo>* concrete_key_systems);

}  // namespace cdm

#endif  // COMPONENTS_CDM_RENDERER_ANDROID_KEY_SYSTEMS_H_
