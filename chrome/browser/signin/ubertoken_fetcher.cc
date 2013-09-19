// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/ubertoken_fetcher.h"

#include <vector>

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"

UbertokenFetcher::UbertokenFetcher(Profile* profile,
                                   UbertokenConsumer* consumer)
    : profile_(profile), consumer_(consumer) {
  DCHECK(profile);
  DCHECK(consumer);
}

UbertokenFetcher::~UbertokenFetcher() {
}

void UbertokenFetcher::StartFetchingToken() {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaUrls::GetInstance()->oauth1_login_scope());
  access_token_request_ =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)->
          StartRequest(scopes, this);
}

void UbertokenFetcher::OnUberAuthTokenSuccess(const std::string& token) {
  consumer_->OnUbertokenSuccess(token);
}

void UbertokenFetcher::OnUberAuthTokenFailure(
    const GoogleServiceAuthError& error) {
  consumer_->OnUbertokenFailure(error);
}

void UbertokenFetcher::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  access_token_request_.reset();
  gaia_auth_fetcher_.reset(new GaiaAuthFetcher(this,
                                               GaiaConstants::kChromeSource,
                                               profile_->GetRequestContext()));
  gaia_auth_fetcher_->StartTokenFetchForUberAuthExchange(access_token);
}

void UbertokenFetcher::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  access_token_request_.reset();
  consumer_->OnUbertokenFailure(error);
}
