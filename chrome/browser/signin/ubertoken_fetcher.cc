// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/signin/ubertoken_fetcher.h"
#include "chrome/common/chrome_notification_types.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"

UbertokenFetcher::UbertokenFetcher(Profile* profile,
                                   UbertokenConsumer* consumer)
    : profile_(profile), consumer_(consumer) {
  DCHECK(profile);
  DCHECK(consumer);
}

UbertokenFetcher::~UbertokenFetcher() {
}

void UbertokenFetcher::StartFetchingToken() {
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  if (token_service->HasOAuthLoginToken()) {
    StartFetchingUbertoken();
  } else {
    registrar_.Add(this,
                   chrome::NOTIFICATION_TOKEN_AVAILABLE,
                   content::Source<TokenService>(token_service));
    registrar_.Add(this,
                   chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
                   content::Source<TokenService>(token_service));
    token_service->StartFetchingTokens();
  }
}

void UbertokenFetcher::StartFetchingUbertoken() {
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  DCHECK(token_service->HasOAuthLoginToken());
  gaia::OAuthClientInfo client_info;
  GaiaUrls* urls = GaiaUrls::GetInstance();
  client_info.client_id = urls->oauth2_chrome_client_id();
  client_info.client_secret = urls->oauth2_chrome_client_secret();
  gaia_oauth_client_.reset(new gaia::GaiaOAuthClient(
      urls->oauth2_token_url(), profile_->GetRequestContext()));
  gaia_oauth_client_->RefreshToken(
      client_info, token_service->GetOAuth2LoginRefreshToken(), 1, this);
}

void UbertokenFetcher::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TOKEN_AVAILABLE ||
         type == chrome::NOTIFICATION_TOKEN_REQUEST_FAILED);

  if (type == chrome::NOTIFICATION_TOKEN_AVAILABLE) {
    TokenService::TokenAvailableDetails* token_details =
        content::Details<TokenService::TokenAvailableDetails>(details).ptr();
    if (token_details->service() !=
        GaiaConstants::kGaiaOAuth2LoginRefreshToken) {
      return;
    }
    registrar_.RemoveAll();
    StartFetchingUbertoken();
  } else {
    TokenService::TokenRequestFailedDetails* token_details =
        content::Details<TokenService::TokenRequestFailedDetails>(details).
            ptr();
    if (token_details->service() == GaiaConstants::kLSOService ||
        token_details->service() ==
            GaiaConstants::kGaiaOAuth2LoginRefreshToken) {
      consumer_->OnUbertokenFailure(token_details->error());
    }
  }
}

void UbertokenFetcher::OnGetTokensResponse(const std::string& refresh_token,
                                           const std::string& access_token,
                                           int expires_in_seconds) {
  NOTREACHED();
}

void UbertokenFetcher::OnRefreshTokenResponse(const std::string& access_token,
                                              int expires_in_seconds) {
  gaia_auth_fetcher_.reset(new GaiaAuthFetcher(this,
                                               GaiaConstants::kChromeSource,
                                               profile_->GetRequestContext()));
  gaia_auth_fetcher_->StartTokenFetchForUberAuthExchange(access_token);
}

void UbertokenFetcher::OnOAuthError() {
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  consumer_->OnUbertokenFailure(error);
}

void UbertokenFetcher::OnNetworkError(int response_code) {
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromConnectionError(response_code);
  consumer_->OnUbertokenFailure(error);
}

void UbertokenFetcher::OnUberAuthTokenSuccess(const std::string& token) {
  consumer_->OnUbertokenSuccess(token);
}

void UbertokenFetcher::OnUberAuthTokenFailure(
    const GoogleServiceAuthError& error) {
  consumer_->OnUbertokenFailure(error);
}
