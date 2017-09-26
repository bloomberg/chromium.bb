// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SIGNIN_IOS_WEB_VIEW_SIGNIN_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_SIGNIN_IOS_WEB_VIEW_SIGNIN_CLIENT_H_

#include "components/signin/ios/browser/ios_signin_client.h"

namespace ios_web_view {
class WebViewBrowserState;
}

// iOS WebView specific signin client.
class IOSWebViewSigninClient : public IOSSigninClient {
 public:
  IOSWebViewSigninClient(ios_web_view::WebViewBrowserState* browser_state,
                         SigninErrorController* signin_error_controller,
                         content_settings::CookieSettings* cookie_settings,
                         HostContentSettingsMap* host_content_settings_map,
                         scoped_refptr<TokenWebData> token_web_data);

  // SigninClient implementation.
  void OnSignedOut() override;
  std::string GetProductVersion() override;
  base::Time GetInstallDate() override;

  // SigninErrorController::Observer implementation.
  void OnErrorChanged() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(IOSWebViewSigninClient);
};

#endif  // IOS_WEB_VIEW_INTERNAL_SIGNIN_IOS_WEB_VIEW_SIGNIN_CLIENT_H_
