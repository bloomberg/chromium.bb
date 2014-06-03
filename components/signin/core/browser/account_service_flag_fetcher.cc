// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_service_flag_fetcher.h"

#include "base/strings/string_split.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"

AccountServiceFlagFetcher::AccountServiceFlagFetcher(
    const std::string& account_id,
    ProfileOAuth2TokenService* token_service,
    net::URLRequestContextGetter* request_context,
    const ResultCallback& callback)
    : OAuth2TokenService::Consumer("account_service_flag_fetcher"),
      account_id_(account_id),
      token_service_(token_service),
      gaia_auth_fetcher_(this, GaiaConstants::kChromeSource, request_context),
      callback_(callback) {
  Start();
}

AccountServiceFlagFetcher::~AccountServiceFlagFetcher() {
  // Ensures PO2TS observation is cleared when AccountServiceFlagFetcher is
  // destructed before refresh token is available.
  token_service_->RemoveObserver(this);

  gaia_auth_fetcher_.CancelRequest();
}

void AccountServiceFlagFetcher::Start() {
  if (token_service_->RefreshTokenIsAvailable(account_id_)) {
    StartFetchingOAuth2AccessToken();
  } else {
    // Wait until we get a refresh token.
    token_service_->AddObserver(this);
  }
}

void AccountServiceFlagFetcher::OnRefreshTokenAvailable(
    const std::string& account_id) {
  // Wait until we get a refresh token for the requested account.
  if (account_id != account_id_)
    return;

  token_service_->RemoveObserver(this);

  StartFetchingOAuth2AccessToken();
}

void AccountServiceFlagFetcher::OnRefreshTokensLoaded() {
  token_service_->RemoveObserver(this);

  // The PO2TS has loaded all tokens, but we didn't get one for the account we
  // want. We probably won't get one any time soon, so report an error.
  DLOG(WARNING) << "AccountServiceFlagFetcher::OnRefreshTokensLoaded: "
                << "Did not get a refresh token for account " << account_id_;
  callback_.Run(TOKEN_ERROR, std::vector<std::string>());
}

void AccountServiceFlagFetcher::StartFetchingOAuth2AccessToken() {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);
  oauth2_access_token_request_ = token_service_->StartRequest(
      account_id_, scopes, this);
}

void AccountServiceFlagFetcher::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(oauth2_access_token_request_.get(), request);
  oauth2_access_token_request_.reset();

  gaia_auth_fetcher_.StartOAuthLogin(access_token, GaiaConstants::kGaiaService);
}

void AccountServiceFlagFetcher::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(oauth2_access_token_request_.get(), request);
  oauth2_access_token_request_.reset();

  DLOG(WARNING) << "AccountServiceFlagFetcher::OnGetTokenFailure: "
                << error.ToString();
  callback_.Run(TOKEN_ERROR, std::vector<std::string>());
}

void AccountServiceFlagFetcher::OnClientLoginSuccess(
    const ClientLoginResult& result) {
  gaia_auth_fetcher_.StartGetUserInfo(result.lsid);
}

void AccountServiceFlagFetcher::OnClientLoginFailure(
    const GoogleServiceAuthError& error) {
  DLOG(WARNING) << "AccountServiceFlagFetcher::OnClientLoginFailure: "
                << error.ToString();
  callback_.Run(SERVICE_ERROR, std::vector<std::string>());
}

void AccountServiceFlagFetcher::OnGetUserInfoSuccess(const UserInfoMap& data) {
  ResultCode result = SERVICE_ERROR;
  std::vector<std::string> services;
  UserInfoMap::const_iterator services_iter = data.find("allServices");
  if (services_iter != data.end()) {
    result = SUCCESS;
    base::SplitString(services_iter->second, ',', &services);
  } else {
    DLOG(WARNING) << "AccountServiceFlagFetcher::OnGetUserInfoSuccess: "
                  << "GetUserInfo response didn't include allServices field.";
  }
  callback_.Run(result, services);
}

void AccountServiceFlagFetcher::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  DLOG(WARNING) << "AccountServiceFlagFetcher::OnGetUserInfoFailure: "
                << error.ToString();
  callback_.Run(SERVICE_ERROR, std::vector<std::string>());
}
