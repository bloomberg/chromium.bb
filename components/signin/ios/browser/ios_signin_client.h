// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_IOS_SIGNIN_CLIENT_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_IOS_SIGNIN_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request_context_getter.h"

// iOS specific signin client.
class IOSSigninClient
    : public SigninClient,
      public net::NetworkChangeNotifier::NetworkChangeObserver,
      public SigninErrorController::Observer,
      public gaia::GaiaOAuthClient::Delegate,
      public OAuth2TokenService::Consumer {
 public:
  IOSSigninClient(PrefService* pref_service,
                  net::URLRequestContextGetter* url_request_context,
                  SigninErrorController* signin_error_controller,
                  content_settings::CookieSettings* cookie_settings,
                  HostContentSettingsMap* host_content_settings_map,
                  scoped_refptr<TokenWebData> token_web_data);
  ~IOSSigninClient() override;

  // SigninClient implementation.
  scoped_refptr<TokenWebData> GetDatabase() override;
  PrefService* GetPrefs() override;
  net::URLRequestContextGetter* GetURLRequestContext() override;
  void DoFinalInit() override;
  bool CanRevokeCredentials() override;
  std::string GetSigninScopedDeviceId() override;
  bool ShouldMergeSigninCredentialsIntoCookieJar() override;
  bool IsFirstRun() const override;
  bool AreSigninCookiesAllowed() override;
  void AddContentSettingsObserver(
      content_settings::Observer* observer) override;
  void RemoveContentSettingsObserver(
      content_settings::Observer* observer) override;
  std::unique_ptr<CookieChangedSubscription> AddCookieChangedCallback(
      const GURL& url,
      const std::string& name,
      const net::CookieStore::CookieChangedCallback& callback) override;
  void DelayNetworkCall(const base::Closure& callback) override;
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      const std::string& source,
      net::URLRequestContextGetter* getter) override;

  // KeyedService implementation.
  void Shutdown() override;

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
  PrefService* pref_service_;
  net::URLRequestContextGetter* url_request_context_;
  SigninErrorController* signin_error_controller_;
  content_settings::CookieSettings* cookie_settings_;
  HostContentSettingsMap* host_content_settings_map_;
  scoped_refptr<TokenWebData> token_web_data_;
  std::list<base::Closure> delayed_callbacks_;

  std::unique_ptr<gaia::GaiaOAuthClient> oauth_client_;
  std::unique_ptr<OAuth2TokenService::Request> oauth_request_;

  DISALLOW_COPY_AND_ASSIGN(IOSSigninClient);
};

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_IOS_SIGNIN_CLIENT_H_
