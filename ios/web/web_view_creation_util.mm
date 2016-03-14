// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/web_view_creation_util.h"

#include "base/logging.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/web_view_internal_creation_util.h"

namespace web {

WKWebView* CreateWKWebView(CGRect frame, BrowserState* browser_state) {
  DCHECK(browser_state);

  WKWebViewConfigurationProvider& config_provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  return CreateWKWebView(frame, config_provider.GetWebViewConfiguration(),
                         browser_state);
}

}  // namespace web
