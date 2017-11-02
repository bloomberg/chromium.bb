// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SIGNIN_IOS_WEB_VIEW_SIGNIN_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_SIGNIN_IOS_WEB_VIEW_SIGNIN_CLIENT_H_

#include "components/signin/ios/browser/ios_signin_client.h"

#include "base/ios/weak_nsobject.h"

@class CWVAuthenticationController;

// iOS WebView specific signin client.
class IOSWebViewSigninClient : public IOSSigninClient {
 public:
  IOSWebViewSigninClient(
      PrefService* pref_service,
      net::URLRequestContextGetter* url_request_context,
      SigninErrorController* signin_error_controller,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      scoped_refptr<HostContentSettingsMap> host_content_settings_map,
      scoped_refptr<TokenWebData> token_web_data);

  ~IOSWebViewSigninClient() override;

  // SigninClient implementation.
  void OnSignedOut() override;
  std::string GetProductVersion() override;
  base::Time GetInstallDate() override;

  // SigninErrorController::Observer implementation.
  void OnErrorChanged() override;

  // Setter and getter for |authentication_controller_|.
  void SetAuthenticationController(
      CWVAuthenticationController* authentication_controller);
  CWVAuthenticationController* GetAuthenticationController();

 private:
  base::WeakNSObject<CWVAuthenticationController> authentication_controller_;

  DISALLOW_COPY_AND_ASSIGN(IOSWebViewSigninClient);
};

#endif  // IOS_WEB_VIEW_INTERNAL_SIGNIN_IOS_WEB_VIEW_SIGNIN_CLIENT_H_
