// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_INFO_FETCHER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_INFO_FETCHER_H_

#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace net {
class URLRequestContextGetter;
}

class AccountFetcherService;
class GaiaAuthFetcher;

// An account information fetcher that gets an OAuth token of appropriate
// scope and uses it to fetch account infromation. This does not handle
// refreshing the information and is meant to be used in a one shot fashion.
class AccountInfoFetcher : public OAuth2TokenService::Consumer,
                           public gaia::GaiaOAuthClient::Delegate,
                           public GaiaAuthConsumer {
 public:
  AccountInfoFetcher(OAuth2TokenService* token_service,
                     net::URLRequestContextGetter* request_context_getter,
                     AccountFetcherService* service,
                     const std::string& account_id);
  ~AccountInfoFetcher() override;

  const std::string& account_id() { return account_id_; }

  void Start();
  void SendSuccessIfDoneFetching();

  // OAuth2TokenService::Consumer implementation.
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // gaia::GaiaOAuthClient::Delegate implementation.
  void OnGetUserInfoResponse(
      scoped_ptr<base::DictionaryValue> user_info) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

  // Overridden from GaiaAuthConsumer:
  void OnClientLoginSuccess(const ClientLoginResult& result) override;
  void OnClientLoginFailure(const GoogleServiceAuthError& error) override;
  void OnGetUserInfoSuccess(const UserInfoMap& data) override;
  void OnGetUserInfoFailure(const GoogleServiceAuthError& error) override;

 private:
  OAuth2TokenService* token_service_;
  net::URLRequestContextGetter* request_context_getter_;
  AccountFetcherService* service_;
  const std::string account_id_;

  scoped_ptr<OAuth2TokenService::Request> login_token_request_;
  scoped_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;
  scoped_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;

  scoped_ptr<base::DictionaryValue> fetched_user_info_;
  scoped_ptr<std::vector<std::string>> fetched_service_flags_;

  DISALLOW_COPY_AND_ASSIGN(AccountInfoFetcher);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_INFO_FETCHER_H_
