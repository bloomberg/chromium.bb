// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_view_util.h"

#include "base/ios/ios_util.h"

namespace {

// If true, UIWebView is always used even if WKWebView is available.
bool g_force_ui_web_view = false;

}  // namespace

namespace web {

bool IsWKWebViewEnabled() {
#if defined(ENABLE_WKWEBVIEW)
  // Eventually this may be a dynamic flag, but for now it's purely a
  // compile-time option.
  return !g_force_ui_web_view && base::ios::IsRunningOnIOS8OrLater();
#else
  return false;
#endif
}

void SetForceUIWebView(bool flag) {
  g_force_ui_web_view = flag;
}

bool GetForceUIWebView() {
  return g_force_ui_web_view;
}

}  // namespace web
