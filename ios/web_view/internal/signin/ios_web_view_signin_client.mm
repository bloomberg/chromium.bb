// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/ios_web_view_signin_client.h"

#include "base/memory/ptr_util.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_cookie_changed_subscription.h"
#include "ios/web_view/internal/content_settings/web_view_cookie_settings_factory.h"
#include "ios/web_view/internal/content_settings/web_view_host_content_settings_map_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSWebViewSigninClient::IOSWebViewSigninClient(
    ios_web_view::WebViewBrowserState* browser_state,
    SigninErrorController* signin_error_controller,
    content_settings::CookieSettings* cookie_settings,
    HostContentSettingsMap* host_content_settings_map,
    scoped_refptr<TokenWebData> token_web_data)
    : IOSSigninClient(browser_state->GetPrefs(),
                      browser_state->GetRequestContext(),
                      signin_error_controller,
                      cookie_settings,
                      host_content_settings_map,
                      token_web_data) {}

void IOSWebViewSigninClient::OnSignedOut() {}

std::string IOSWebViewSigninClient::GetProductVersion() {
  // TODO(crbug.com/768689): Implement this method with appropriate values.
  return "";
}

base::Time IOSWebViewSigninClient::GetInstallDate() {
  // TODO(crbug.com/768689): Implement this method with appropriate values.
  return base::Time::FromTimeT(0);
}

void IOSWebViewSigninClient::OnErrorChanged() {}
