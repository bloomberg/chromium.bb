// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_view_internal_creation_util.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

namespace web {

namespace {

// Verifies the preconditions for creating a WKWebView. Must be called before
// a WKWebView is allocated. Not verifying the preconditions before creating
// a WKWebView will lead to undefined behavior.
void VerifyWKWebViewCreationPreConditions(
    BrowserState* browser_state,
    WKWebViewConfiguration* configuration) {
  DCHECK(browser_state);
  DCHECK(configuration);
  WKWebViewConfigurationProvider& config_provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  DCHECK_EQ([config_provider.GetWebViewConfiguration() processPool],
            [configuration processPool]);
}

}  // namespace

WKWebView* CreateWKWebView(CGRect frame,
                           WKWebViewConfiguration* configuration,
                           BrowserState* browser_state,
                           BOOL use_desktop_user_agent) {
  WKWebView* web_view = CreateWKWebView(frame, configuration, browser_state);
  web_view.customUserAgent = base::SysUTF8ToNSString(
      web::GetWebClient()->GetUserAgent(use_desktop_user_agent));
  return web_view;
}

WKWebView* CreateWKWebView(CGRect frame,
                           WKWebViewConfiguration* configuration,
                           BrowserState* browser_state) {
  VerifyWKWebViewCreationPreConditions(browser_state, configuration);

  GetWebClient()->PreWebViewCreation();
  WKWebView* result =
      [[WKWebView alloc] initWithFrame:frame configuration:configuration];

  // By default the web view uses a very sluggish scroll speed. Set it to a more
  // reasonable value.
  result.scrollView.decelerationRate = UIScrollViewDecelerationRateNormal;

  return result;
}

}  // namespace web
