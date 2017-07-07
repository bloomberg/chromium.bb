// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DESKTOP_CAPTURE_H_
#define CONTENT_PUBLIC_BROWSER_DESKTOP_CAPTURE_H_

#include "content/common/content_export.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"

namespace content {

// Creates a DesktopCaptureOptions with required settings.
CONTENT_EXPORT webrtc::DesktopCaptureOptions CreateDesktopCaptureOptions();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DESKTOP_CAPTURE_H_
