// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_IOS_CHROME_SIGNIN_CLIENT_H_
#define IOS_CHROME_BROWSER_SIGNIN_IOS_CHROME_SIGNIN_CLIENT_H_

#include "components/signin/ios/browser/ios_signin_client.h"

namespace ios {
class ChromeBrowserState;
}

// Concrete implementation of IOSSigninClient for //ios/chrome.
class IOSChromeSigninClient : public IOSSigninClient {
 public:
  IOSChromeSigninClient(
      ios::ChromeBrowserState* browser_state,
      SigninErrorController* signin_error_controller,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      scoped_refptr<HostContentSettingsMap> host_content_settings_map,
      scoped_refptr<TokenWebData> token_web_data);
  ~IOSChromeSigninClient() override = default;

  // SigninClient implementation.
  base::Time GetInstallDate() override;
  std::string GetProductVersion() override;
  void OnSignedIn(const std::string& account_id,
                  const std::string& gaia_id,
                  const std::string& username,
                  const std::string& password) override;
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      const std::string& source,
      net::URLRequestContextGetter* getter) override;
  void PreGaiaLogout(base::OnceClosure callback) override;

  // SigninErrorController::Observer implementation.
  void OnErrorChanged() override;

 private:
  // SigninClient private implementation.
  void OnSignedOut() override;

  ios::ChromeBrowserState* browser_state_;
  SigninErrorController* signin_error_controller_;
  DISALLOW_COPY_AND_ASSIGN(IOSChromeSigninClient);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_IOS_CHROME_SIGNIN_CLIENT_H_
