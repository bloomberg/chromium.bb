// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/oauth2_policy_fetcher.h"

#include <vector>

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Max retry count for token fetching requests.
const int kMaxRequestAttemptCount = 5;

// OAuth token request retry delay in milliseconds.
const int kRequestRestartDelay = 3000;

}  // namespace

OAuth2PolicyFetcher::OAuth2PolicyFetcher(
    net::URLRequestContextGetter* auth_context_getter,
    net::URLRequestContextGetter* system_context_getter)
    : auth_context_getter_(auth_context_getter),
      system_context_getter_(system_context_getter),
      retry_count_(0),
      failed_(false) {
}

OAuth2PolicyFetcher::OAuth2PolicyFetcher(
    net::URLRequestContextGetter* system_context_getter,
    const std::string& oauth2_refresh_token)
    : system_context_getter_(system_context_getter),
      oauth2_refresh_token_(oauth2_refresh_token),
      retry_count_(0),
      failed_(false) {
}

OAuth2PolicyFetcher::~OAuth2PolicyFetcher() {
}

void OAuth2PolicyFetcher::Start() {
  retry_count_ = 0;
  if (oauth2_refresh_token_.empty()) {
    StartFetchingRefreshToken();
  } else {
    StartFetchingAccessToken();
  }
}

void OAuth2PolicyFetcher::StartFetchingRefreshToken() {
  DCHECK(!refresh_token_fetcher_.get());
  refresh_token_fetcher_.reset(
            new GaiaAuthFetcher(this,
                                GaiaConstants::kChromeSource,
                                auth_context_getter_));
  refresh_token_fetcher_->StartCookieForOAuthLoginTokenExchange(EmptyString());
}

void OAuth2PolicyFetcher::StartFetchingAccessToken() {
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

void OAuth2PolicyFetcher::OnClientOAuthSuccess(
    const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens) {
  LOG(INFO) << "OAuth2 tokens for policy fetching succeeded.";
  oauth2_tokens_ = oauth2_tokens;
  oauth2_refresh_token_ = oauth2_tokens.refresh_token;
  retry_count_ = 0;
  StartFetchingAccessToken();
}

void OAuth2PolicyFetcher::OnClientOAuthFailure(
    const GoogleServiceAuthError& error) {
  LOG(ERROR) << "OAuth2 tokens fetch for policy fetch failed!";
  RetryOnError(error,
               base::Bind(&OAuth2PolicyFetcher::StartFetchingRefreshToken,
                          AsWeakPtr()));
}

void OAuth2PolicyFetcher::OnGetTokenSuccess(
    const std::string& access_token,
    const base::Time& expiration_time) {
  LOG(INFO) << "OAuth2 access token (device management) fetching succeeded.";
  SetPolicyToken(access_token);
}

void OAuth2PolicyFetcher::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  LOG(ERROR) << "OAuth2 access token (device management) fetching failed!";
  RetryOnError(error,
               base::Bind(&OAuth2PolicyFetcher::StartFetchingAccessToken,
                          AsWeakPtr()));
}

void OAuth2PolicyFetcher::RetryOnError(const GoogleServiceAuthError& error,
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
  SetPolicyToken(EmptyString());
  failed_ = true;
}

void OAuth2PolicyFetcher::SetPolicyToken(const std::string& token) {
  policy_token_ = token;

  policy::BrowserPolicyConnector* browser_policy_connector =
      g_browser_process->browser_policy_connector();
  policy::UserCloudPolicyManagerChromeOS* cloud_policy_manager =
      browser_policy_connector->GetUserCloudPolicyManager();
  if (cloud_policy_manager) {
    if (token.empty())
      cloud_policy_manager->CancelWaitForPolicyFetch();
    else
      cloud_policy_manager->RegisterClient(token);
  }
}

}  // namespace chromeos
