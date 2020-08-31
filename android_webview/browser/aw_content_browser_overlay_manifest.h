// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_CONTENT_BROWSER_OVERLAY_MANIFEST_H_
#define ANDROID_WEBVIEW_BROWSER_AW_CONTENT_BROWSER_OVERLAY_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace android_webview {

// Returns the manifest Android WebView amends to Content's content_browser
// service manifest. This allows WebView to extend the capabilities exposed
// and/or required by content_browser service instances, as well as declaring
// any additional in- and out-of-process per-profile packaged services.
const service_manager::Manifest& GetAWContentBrowserOverlayManifest();

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_CONTENT_BROWSER_OVERLAY_MANIFEST_H_
