// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/athena_native_app_window_views.h"

namespace athena {

views::WebView* AthenaNativeAppWindowViews::GetWebView() {
  return web_view();
}

}  // namespace athena
