// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/web_view_type_util.h"

#include "ios/chrome/browser/experimental_flags.h"

namespace web_view_type_util {

web::WebViewType GetWebViewType() {
  return experimental_flags::IsWKWebViewEnabled() ? web::WK_WEB_VIEW_TYPE
                                                  : web::UI_WEB_VIEW_TYPE;
}

}  // namespace web_view_type_util
