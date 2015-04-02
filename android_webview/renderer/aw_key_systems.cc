// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_key_systems.h"
#include "components/cdm/renderer/android_key_systems.h"

namespace android_webview {

void AwAddKeySystems(
    std::vector<media::KeySystemInfo>* key_systems_info) {
  // TODO(sandersd): Non-compositing support depends on the
  // use_video_overlay_for_embedded_encrypted_video pref, which may differ per
  // WebContents. For now, err on the side of registering support.
  // http://crbug.com/467779
  cdm::AddAndroidWidevine(key_systems_info, true);
  cdm::AddAndroidPlatformKeySystems(key_systems_info);
}

}  // namespace android_webview
