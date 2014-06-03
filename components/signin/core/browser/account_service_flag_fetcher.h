// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_SERVICE_FLAG_FETCHER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_SERVICE_FLAG_FETCHER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/oauth2_token_service.h"

class ProfileOAuth2TokenService;

namespace net {
class URLRequestContextGetter;
}

// Downloads an account's list of Gaia service flags.
// On construction, the download starts immediately and calls the given callback
// when either the download is successful or an error is detected. It is valid
// to destruct the object before the callback is called; this will cancel the
// pending request.
class AccountServiceFlagFetcher : public GaiaAuthConsumer,
                                  public OAuth2TokenService::Observer,
                                  public OAuth2TokenService::Consumer {
 public:
  enum ResultCode {
    SUCCESS,
    TOKEN_ERROR,    // Failed to get OAuth2 token.
    SERVICE_ERROR,  // Service returned an error or malformed reply.
  };

  // If the flag download is successful, this will return the list of service
  // flags that are set for the given account.
  typedef base::Callback<void(ResultCode /* result */,
                              const std::vector<std::string>& /* flags */)>
      ResultCallback;

  // Immediately starts fetching the flags.
  AccountServiceFlagFetcher(const std::string& account_id,
                            ProfileOAuth2TokenService* token_service,
                            net::URLRequestContextGetter* request_context,
                            const ResultCallback& callback);

  // Destructing the object before the callback is called cancels the request.
  virtual ~AccountServiceFlagFetcher();

 private:
  void Start();
  void StartFetchingOAuth2AccessToken();

  // Overridden from OAuth2TokenService::Observer:
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;
  virtual void OnRefreshTokensLoaded() OVERRIDE;

  // Overridden from OAuth2TokenService::Consumer:
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

  // Overridden from GaiaAuthConsumer:
  virtual void OnClientLoginSuccess(const ClientLoginResult& result) OVERRIDE;
  virtual void OnClientLoginFailure(const GoogleServiceAuthError& error)
      OVERRIDE;
  virtual void OnGetUserInfoSuccess(const UserInfoMap& data) OVERRIDE;
  virtual void OnGetUserInfoFailure(const GoogleServiceAuthError& error)
      OVERRIDE;

  const std::string account_id_;
  ProfileOAuth2TokenService* token_service_;
  GaiaAuthFetcher gaia_auth_fetcher_;

  ResultCallback callback_;

  scoped_ptr<OAuth2TokenService::Request> oauth2_access_token_request_;

  DISALLOW_COPY_AND_ASSIGN(AccountServiceFlagFetcher);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_SERVICE_FLAG_FETCHER_H_
