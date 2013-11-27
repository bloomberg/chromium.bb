// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_KEY_SYSTEMS_H_
#define ANDROID_WEBVIEW_RENDERER_AW_KEY_SYSTEMS_H_

#include "content/public/renderer/key_system_info.h"

namespace android_webview {

void AwAddKeySystems(std::vector<content::KeySystemInfo>* key_systems_info);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_KEY_SYSTEMS_H_
