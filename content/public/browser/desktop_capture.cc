// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/desktop_capture.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace content {

webrtc::DesktopCaptureOptions CreateDesktopCaptureOptions() {
  auto options = webrtc::DesktopCaptureOptions::CreateDefault();
  // Leave desktop effects enabled during WebRTC captures.
  options.set_disable_effects(false);
#if defined(OS_WIN)
  static constexpr base::Feature kDirectXCapturer{
      "DirectXCapturer",
      base::FEATURE_ENABLED_BY_DEFAULT};
  if (base::FeatureList::IsEnabled(kDirectXCapturer)) {
    options.set_allow_directx_capturer(true);
    options.set_allow_use_magnification_api(false);
  } else {
    options.set_allow_use_magnification_api(true);
  }
#endif  // defined(OS_WIN)
  return options;
}

}  // namespace content
