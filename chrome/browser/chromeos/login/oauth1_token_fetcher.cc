// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/oauth1_token_fetcher.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/google_service_auth_error.h"

using content::BrowserThread;

namespace {

// OAuth token request max retry count.
const int kMaxOAuth1TokenRequestAttemptCount = 5;
// OAuth token request retry delay in milliseconds.
const int kOAuth1TokenRequestRestartDelay = 3000;

// The service scope of the OAuth v2 token that ChromeOS login will be
// requesting.
const char kServiceScopeChromeOS[] =
    "https://www.googleapis.com/auth/chromesync";

}  // namespace

namespace chromeos {

OAuth1TokenFetcher::OAuth1TokenFetcher(
    OAuth1TokenFetcher::Delegate* delegate,
    net::URLRequestContextGetter* auth_context_getter)
    : delegate_(delegate),
      oauth_fetcher_(this, auth_context_getter, kServiceScopeChromeOS),
      retry_count_(0) {
  DCHECK(delegate);
}

OAuth1TokenFetcher::~OAuth1TokenFetcher() {
}

void OAuth1TokenFetcher::Start() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (CrosLibrary::Get()->libcros_loaded()) {
    // Delay the verification if the network is not connected or on a captive
    // portal.
    const Network* network =
        CrosLibrary::Get()->GetNetworkLibrary()->active_network();
    if (!network || !network->connected() || network->restricted_pool()) {
      // If network is offline, defer the token fetching until online.
      VLOG(1) << "Network is offline.  Deferring OAuth1 token fetch.";
      BrowserThread::PostDelayedTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&OAuth1TokenFetcher::Start, AsWeakPtr()),
          base::TimeDelta::FromMilliseconds(kOAuth1TokenRequestRestartDelay));
      return;
    }
  }
  oauth_fetcher_.SetAutoFetchLimit(GaiaOAuthFetcher::OAUTH1_ALL_ACCESS_TOKEN);
  oauth_fetcher_.StartGetOAuthTokenRequest();
}

bool OAuth1TokenFetcher::RetryOnError(const GoogleServiceAuthError& error) {
  if ((error.state() == GoogleServiceAuthError::CONNECTION_FAILED ||
       error.state() == GoogleServiceAuthError::SERVICE_UNAVAILABLE ||
       error.state() == GoogleServiceAuthError::REQUEST_CANCELED) &&
      retry_count_++ < kMaxOAuth1TokenRequestAttemptCount) {
    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&OAuth1TokenFetcher::Start, AsWeakPtr()),
        base::TimeDelta::FromMilliseconds(kOAuth1TokenRequestRestartDelay));
    return true;
  }
  LOG(ERROR) << "Unrecoverable error or retry count max reached.";
  return false;
}

void OAuth1TokenFetcher::OnGetOAuthTokenSuccess(
    const std::string& oauth_token) {
  LOG(INFO) << "Got OAuth request token!";
}

void OAuth1TokenFetcher::OnGetOAuthTokenFailure(
    const GoogleServiceAuthError& error) {
  LOG(ERROR) << "Failed to get OAuth1 request token, error: "
             << error.state();
  if (!RetryOnError(error))
    delegate_->OnOAuth1AccessTokenFetchFailed();
}

void OAuth1TokenFetcher::OnOAuthGetAccessTokenSuccess(
    const std::string& token,
    const std::string& secret) {
  LOG(INFO) << "Got OAuth v1 token!";
  retry_count_ = 0;
  delegate_->OnOAuth1AccessTokenAvailable(token, secret);
}

void OAuth1TokenFetcher::OnOAuthGetAccessTokenFailure(
    const GoogleServiceAuthError& error) {
  LOG(ERROR) << "Failed fetching OAuth1 access token, error: "
             << error.state();
  if (!RetryOnError(error))
    delegate_->OnOAuth1AccessTokenFetchFailed();
}

}  // namespace chromeos
