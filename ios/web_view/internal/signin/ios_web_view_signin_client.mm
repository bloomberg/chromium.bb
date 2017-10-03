// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/ios_web_view_signin_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSWebViewSigninClient::IOSWebViewSigninClient(
    PrefService* pref_service,
    net::URLRequestContextGetter* url_request_context,
    SigninErrorController* signin_error_controller,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    scoped_refptr<HostContentSettingsMap> host_content_settings_map,
    scoped_refptr<TokenWebData> token_web_data)
    : IOSSigninClient(pref_service,
                      url_request_context,
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
