// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/oauth2_token_fetcher.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"

using content::BrowserThread;

namespace {

// OAuth token request max retry count.
const int kMaxRequestAttemptCount = 5;
// OAuth token request retry delay in milliseconds.
const int kRequestRestartDelay = 3000;

// The service scope of the OAuth v2 token that ChromeOS login will be
// requesting.
const char kServiceScopeChromeOS[] =
    "https://www.googleapis.com/auth/chromesync";

}  // namespace

namespace chromeos {

OAuth2TokenFetcher::OAuth2TokenFetcher(
    OAuth2TokenFetcher::Delegate* delegate,
    net::URLRequestContextGetter* context_getter)
    : delegate_(delegate),
      auth_fetcher_(this, GaiaConstants::kChromeSource, context_getter),
      retry_count_(0) {
  DCHECK(delegate);
}

OAuth2TokenFetcher::~OAuth2TokenFetcher() {
}

void OAuth2TokenFetcher::Start() {
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
          base::Bind(&OAuth2TokenFetcher::Start, AsWeakPtr()),
          base::TimeDelta::FromMilliseconds(kRequestRestartDelay));
      return;
    }
  }
  auth_fetcher_.StartCookieForOAuthLoginTokenExchange(EmptyString());
}

void OAuth2TokenFetcher::OnClientOAuthSuccess(
    const GaiaAuthConsumer::ClientOAuthResult& oauth_tokens) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LOG(INFO) << "Got OAuth2 tokens!";
  retry_count_ = 0;
  oauth_tokens_ = oauth_tokens;
  delegate_->OnOAuth2TokensAvailable(oauth_tokens_);
}

void OAuth2TokenFetcher::OnClientOAuthFailure(
    const GoogleServiceAuthError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RetryOnError(error,
               base::Bind(&OAuth2TokenFetcher::Start, AsWeakPtr()),
               base::Bind(&Delegate::OnOAuth2TokensFetchFailed,
                          base::Unretained(delegate_)));
}

void OAuth2TokenFetcher::RetryOnError(const GoogleServiceAuthError& error,
                                      const base::Closure& task,
                                      const base::Closure& error_handler) {
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
  LOG(INFO) << "Unrecoverable error or retry count max reached.";
  error_handler.Run();
}

}  // namespace chromeos
