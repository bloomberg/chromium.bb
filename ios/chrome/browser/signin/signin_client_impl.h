// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_SIGNIN_CLIENT_IMPL_H_
#define IOS_CHROME_BROWSER_SIGNIN_SIGNIN_CLIENT_IMPL_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/network_change_notifier.h"

namespace ios {
class ChromeBrowserState;
}

class SigninClientImpl
    : public SigninClient,
      public net::NetworkChangeNotifier::NetworkChangeObserver,
      public SigninErrorController::Observer,
      public gaia::GaiaOAuthClient::Delegate,
      public OAuth2TokenService::Consumer {
 public:
  SigninClientImpl(ios::ChromeBrowserState* browser_state,
                   SigninErrorController* signin_error_controller);
  ~SigninClientImpl() override;

  // Utility method.
  static bool AllowsSigninCookies(ios::ChromeBrowserState* browser_state);

  // If |for_ephemeral| is true, special kind of device ID for ephemeral users
  // is generated.
  static std::string GenerateSigninScopedDeviceID(bool for_ephemeral);

  // SigninClient implementation.
  void DoFinalInit() override;
  PrefService* GetPrefs() override;
  scoped_refptr<TokenWebData> GetDatabase() override;
  bool CanRevokeCredentials() override;
  std::string GetSigninScopedDeviceId() override;
  void OnSignedOut() override;
  net::URLRequestContextGetter* GetURLRequestContext() override;
  bool ShouldMergeSigninCredentialsIntoCookieJar() override;
  bool IsFirstRun() const override;
  base::Time GetInstallDate() override;
  bool AreSigninCookiesAllowed() override;
  void AddContentSettingsObserver(
      content_settings::Observer* observer) override;
  void RemoveContentSettingsObserver(
      content_settings::Observer* observer) override;
  void DelayNetworkCall(const base::Closure& callback) override;
  GaiaAuthFetcher* CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      const std::string& source,
      net::URLRequestContextGetter* getter) override;

  // KeyedService implementation.
  void Shutdown() override;

  // Returns a string describing the chrome version environment. Version format:
  // <Build Info> <OS> <Version number> (<Last change>)<channel or "-devel">
  // If version information is unavailable, returns "invalid."
  std::string GetProductVersion() override;
  std::unique_ptr<CookieChangedSubscription> AddCookieChangedCallback(
      const GURL& url,
      const std::string& name,
      const net::CookieStore::CookieChangedCallback& callback) override;
  void OnSignedIn(const std::string& account_id,
                  const std::string& gaia_id,
                  const std::string& username,
                  const std::string& password) override;

  // SigninErrorController::Observer implementation.
  void OnErrorChanged() override;

  // gaia::GaiaOAuthClient::Delegate implementation.
  void OnGetTokenInfoResponse(
      std::unique_ptr<base::DictionaryValue> token_info) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

  // OAuth2TokenService::Consumer implementation
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // net::NetworkChangeController::NetworkChangeObserver implementation.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

 private:
  ios::ChromeBrowserState* browser_state_;

  SigninErrorController* signin_error_controller_;
  std::list<base::Closure> delayed_callbacks_;

  std::unique_ptr<gaia::GaiaOAuthClient> oauth_client_;
  std::unique_ptr<OAuth2TokenService::Request> oauth_request_;

  DISALLOW_COPY_AND_ASSIGN(SigninClientImpl);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_SIGNIN_CLIENT_IMPL_H_
