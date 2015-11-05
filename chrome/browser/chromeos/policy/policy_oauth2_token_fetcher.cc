// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/policy_oauth2_token_fetcher.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace policy {

namespace {

// Max retry count for token fetching requests.
const int kMaxRequestAttemptCount = 5;

// OAuth token request retry delay in milliseconds.
const int kRequestRestartDelay = 3000;

}  // namespace

PolicyOAuth2TokenFetcher::PolicyOAuth2TokenFetcher() {
}

PolicyOAuth2TokenFetcher::~PolicyOAuth2TokenFetcher() {
}

void PolicyOAuth2TokenFetcher::StartWithSigninContext(
    net::URLRequestContextGetter* auth_context_getter,
    net::URLRequestContextGetter* system_context_getter,
    const TokenCallback& callback) {
  DCHECK(!refresh_token_fetcher_ && !access_token_fetcher_);

  auth_context_getter_ = auth_context_getter;
  system_context_getter_ = system_context_getter;
  callback_ = callback;
  StartFetchingRefreshToken();
}

void PolicyOAuth2TokenFetcher::StartWithAuthCode(
    const std::string& auth_code,
    net::URLRequestContextGetter* system_context_getter,
    const TokenCallback& callback) {
  DCHECK(!refresh_token_fetcher_ && !access_token_fetcher_);

  auth_code_ = auth_code;
  system_context_getter_ = system_context_getter;
  callback_ = callback;
  StartFetchingRefreshToken();
}

void PolicyOAuth2TokenFetcher::StartWithRefreshToken(
    const std::string& oauth2_refresh_token,
    net::URLRequestContextGetter* system_context_getter,
    const TokenCallback& callback) {
  DCHECK(!refresh_token_fetcher_ && !access_token_fetcher_);

  oauth2_refresh_token_ = oauth2_refresh_token;
  system_context_getter_ = system_context_getter;
  callback_ = callback;
  StartFetchingAccessToken();
}

void PolicyOAuth2TokenFetcher::StartFetchingRefreshToken() {
  if (auth_code_.empty()) {
    refresh_token_fetcher_.reset(new GaiaAuthFetcher(
        this, GaiaConstants::kChromeSource, auth_context_getter_.get()));
    refresh_token_fetcher_->StartCookieForOAuthLoginTokenExchange(
        std::string());
  } else {
    refresh_token_fetcher_.reset(new GaiaAuthFetcher(
        this, GaiaConstants::kChromeSource, system_context_getter_.get()));
    refresh_token_fetcher_->StartAuthCodeForOAuth2TokenExchange(auth_code_);
  }
}

void PolicyOAuth2TokenFetcher::StartFetchingAccessToken() {
  std::vector<std::string> scopes;
  scopes.push_back(GaiaConstants::kDeviceManagementServiceOAuth);
  scopes.push_back(GaiaConstants::kOAuthWrapBridgeUserInfoScope);
  access_token_fetcher_.reset(
      new OAuth2AccessTokenFetcherImpl(this,
                                       system_context_getter_.get(),
                                       oauth2_refresh_token_));
  access_token_fetcher_->Start(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
      scopes);
}

void PolicyOAuth2TokenFetcher::OnClientOAuthSuccess(
    const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens) {
  VLOG(1) << "OAuth2 tokens for policy fetching succeeded.";
  oauth2_refresh_token_ = oauth2_tokens.refresh_token;
  retry_count_ = 0;
  StartFetchingAccessToken();
}

void PolicyOAuth2TokenFetcher::OnClientOAuthFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "OAuth2 tokens fetch for policy fetch failed! (error = "
          << error.state() << ")";
  RetryOnError(error,
               base::Bind(&PolicyOAuth2TokenFetcher::StartFetchingRefreshToken,
                          AsWeakPtr()));
}

void PolicyOAuth2TokenFetcher::OnGetTokenSuccess(
    const std::string& access_token,
    const base::Time& expiration_time) {
  VLOG(1) << "OAuth2 access token (device management) fetching succeeded.";
  oauth2_access_token_ = access_token;
  ForwardPolicyToken(access_token,
                     GoogleServiceAuthError(GoogleServiceAuthError::NONE));
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (error.IsTransientError() && retry_count_ < kMaxRequestAttemptCount) {
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
  ForwardPolicyToken(std::string(), error);
}

void PolicyOAuth2TokenFetcher::ForwardPolicyToken(
    const std::string& token,
    const GoogleServiceAuthError& error) {
  if (!callback_.is_null())
    callback_.Run(token, error);
}

}  // namespace policy
