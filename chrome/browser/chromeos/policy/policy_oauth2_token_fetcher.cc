// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/policy_oauth2_token_fetcher.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace policy {

namespace {

// Max retry count for token fetching requests.
const int kMaxRequestAttemptCount = 5;

// OAuth token request retry delay in milliseconds.
const int kRequestRestartDelay = 3000;

}  // namespace

PolicyOAuth2TokenFetcher::PolicyOAuth2TokenFetcher(
    net::URLRequestContextGetter* auth_context_getter,
    net::URLRequestContextGetter* system_context_getter,
    const TokenCallback& callback)
    : auth_context_getter_(auth_context_getter),
      system_context_getter_(system_context_getter),
      retry_count_(0),
      failed_(false),
      callback_(callback) {}

PolicyOAuth2TokenFetcher::PolicyOAuth2TokenFetcher(
    net::URLRequestContextGetter* system_context_getter,
    const std::string& oauth2_refresh_token,
    const TokenCallback& callback)
    : system_context_getter_(system_context_getter),
      oauth2_refresh_token_(oauth2_refresh_token),
      retry_count_(0),
      failed_(false),
      callback_(callback) {}

PolicyOAuth2TokenFetcher::~PolicyOAuth2TokenFetcher() {}

void PolicyOAuth2TokenFetcher::Start() {
  retry_count_ = 0;
  if (oauth2_refresh_token_.empty()) {
    StartFetchingRefreshToken();
  } else {
    StartFetchingAccessToken();
  }
}

void PolicyOAuth2TokenFetcher::StartFetchingRefreshToken() {
  DCHECK(!refresh_token_fetcher_.get());
  refresh_token_fetcher_.reset(
      new GaiaAuthFetcher(this,
                          GaiaConstants::kChromeSource,
                          auth_context_getter_));
  refresh_token_fetcher_->StartCookieForOAuthLoginTokenExchange(EmptyString());
}

void PolicyOAuth2TokenFetcher::StartFetchingAccessToken() {
  std::vector<std::string> scopes;
  scopes.push_back(GaiaConstants::kDeviceManagementServiceOAuth);
  access_token_fetcher_.reset(
      new OAuth2AccessTokenFetcher(this, system_context_getter_));
  access_token_fetcher_->Start(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
      oauth2_refresh_token_,
      scopes);
}

void PolicyOAuth2TokenFetcher::OnClientOAuthSuccess(
    const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens) {
  LOG(INFO) << "OAuth2 tokens for policy fetching succeeded.";
  oauth2_tokens_ = oauth2_tokens;
  oauth2_refresh_token_ = oauth2_tokens.refresh_token;
  retry_count_ = 0;
  StartFetchingAccessToken();
}

void PolicyOAuth2TokenFetcher::OnClientOAuthFailure(
    const GoogleServiceAuthError& error) {
  LOG(ERROR) << "OAuth2 tokens fetch for policy fetch failed!";
  RetryOnError(error,
               base::Bind(&PolicyOAuth2TokenFetcher::StartFetchingRefreshToken,
                          AsWeakPtr()));
}

void PolicyOAuth2TokenFetcher::OnGetTokenSuccess(
    const std::string& access_token,
    const base::Time& expiration_time) {
  LOG(INFO) << "OAuth2 access token (device management) fetching succeeded.";
  ForwardPolicyToken(access_token);
}

void PolicyOAuth2TokenFetcher::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  LOG(ERROR) << "OAuth2 access token (device management) fetching failed!";
  RetryOnError(error,
               base::Bind(&PolicyOAuth2TokenFetcher::StartFetchingAccessToken,
                          AsWeakPtr()));
}

void PolicyOAuth2TokenFetcher::RetryOnError(const GoogleServiceAuthError& error,
                                            const base::Closure& task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if ((error.state() == GoogleServiceAuthError::CONNECTION_FAILED ||
       error.state() == GoogleServiceAuthError::SERVICE_UNAVAILABLE ||
       error.state() == GoogleServiceAuthError::REQUEST_CANCELED) &&
      retry_count_ < kMaxRequestAttemptCount) {
    retry_count_++;
    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE, task,
        base::TimeDelta::FromMilliseconds(kRequestRestartDelay));
    return;
  }
  LOG(ERROR) << "Unrecoverable error or retry count max reached.";
  failed_ = true;
  // Invoking the |callback_| signals to the owner of this object that it has
  // completed, and the owner may delete this object on the callback method.
  // So don't rely on |this| still being valid after ForwardPolicyToken()
  // returns i.e. don't write to |failed_| or other fields.
  ForwardPolicyToken(EmptyString());
}

void PolicyOAuth2TokenFetcher::ForwardPolicyToken(const std::string& token) {
  callback_.Run(token);
}

}  // namespace policy
