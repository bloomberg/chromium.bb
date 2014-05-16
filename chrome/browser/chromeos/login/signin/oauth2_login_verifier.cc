// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/oauth2_login_verifier.h"

#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/net/delay_network_call.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"

using content::BrowserThread;

namespace {

// OAuth token request max retry count.
const int kMaxRequestAttemptCount = 5;

// OAuth token request retry delay in milliseconds.
const int kRequestRestartDelay = 3000;

// Post merge session verification delay.
#ifndef NDEBUG
const int kPostResoreVerificationDelay = 1000;
#else
const int kPostResoreVerificationDelay = 1000*60*3;
#endif

bool IsConnectionOrServiceError(const GoogleServiceAuthError& error) {
  return error.state() == GoogleServiceAuthError::CONNECTION_FAILED ||
         error.state() == GoogleServiceAuthError::SERVICE_UNAVAILABLE ||
         error.state() == GoogleServiceAuthError::REQUEST_CANCELED;
}

}  // namespace

namespace chromeos {

OAuth2LoginVerifier::OAuth2LoginVerifier(
    OAuth2LoginVerifier::Delegate* delegate,
    net::URLRequestContextGetter* system_request_context,
    net::URLRequestContextGetter* user_request_context,
    const std::string& oauthlogin_access_token)
    : OAuth2TokenService::Consumer("cros_login_verifier"),
      delegate_(delegate),
      system_request_context_(system_request_context),
      user_request_context_(user_request_context),
      access_token_(oauthlogin_access_token),
      retry_count_(0) {
  DCHECK(delegate);
}

OAuth2LoginVerifier::~OAuth2LoginVerifier() {
}

void OAuth2LoginVerifier::VerifyUserCookies(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Delay the verification if the network is not connected or on a captive
  // portal.
  DelayNetworkCall(
      base::Bind(&OAuth2LoginVerifier::StartAuthCookiesVerification,
                 AsWeakPtr()),
      base::TimeDelta::FromMilliseconds(kRequestRestartDelay));
}

void OAuth2LoginVerifier::VerifyProfileTokens(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Delay the verification if the network is not connected or on a captive
  // portal.
  DelayNetworkCall(
      base::Bind(
          &OAuth2LoginVerifier::VerifyProfileTokensImpl, AsWeakPtr(), profile),
      base::TimeDelta::FromMilliseconds(kRequestRestartDelay));
}

void OAuth2LoginVerifier::VerifyProfileTokensImpl(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  gaia_token_.clear();
  if (access_token_.empty()) {
    // Fetch /OAuthLogin scoped access token.
    StartFetchingOAuthLoginAccessToken(profile);
  } else {
    // If OAuthLogin-scoped access token already exists (if it's generated
    // together with freshly minted refresh token), then fetch GAIA uber token.
    StartOAuthLoginForUberToken();
  }
}

void OAuth2LoginVerifier::StartFetchingOAuthLoginAccessToken(Profile* profile) {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  login_token_request_ = token_service->StartRequestWithContext(
      signin_manager->GetAuthenticatedAccountId(),
      system_request_context_.get(),
      scopes,
      this);
}

void OAuth2LoginVerifier::StartOAuthLoginForUberToken() {
  // No service will fetch us uber auth token.
  gaia_fetcher_.reset(
      new GaiaAuthFetcher(this,
                          std::string(GaiaConstants::kChromeOSSource),
                          user_request_context_.get()));
  gaia_fetcher_->StartTokenFetchForUberAuthExchange(access_token_);
}


void OAuth2LoginVerifier::OnUberAuthTokenSuccess(
    const std::string& uber_token) {
  VLOG(1) << "OAuthLogin(uber_token) successful!";
  retry_count_ = 0;
  gaia_token_ = uber_token;
  StartMergeSession();
}

void OAuth2LoginVerifier::OnUberAuthTokenFailure(
    const GoogleServiceAuthError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LOG(WARNING) << "OAuthLogin(uber_token) failed,"
               << " error: " << error.state();
  RetryOnError("OAuthLoginUberToken", error,
               base::Bind(&OAuth2LoginVerifier::StartOAuthLoginForUberToken,
                          AsWeakPtr()),
               base::Bind(&Delegate::OnSessionMergeFailure,
                          base::Unretained(delegate_)));
}

void OAuth2LoginVerifier::StartMergeSession() {
  DCHECK(!gaia_token_.empty());
  gaia_fetcher_.reset(
      new GaiaAuthFetcher(this,
                          std::string(GaiaConstants::kChromeOSSource),
                          user_request_context_.get()));
  gaia_fetcher_->StartMergeSession(gaia_token_);
}

void OAuth2LoginVerifier::OnMergeSessionSuccess(const std::string& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "MergeSession successful.";
  delegate_->OnSessionMergeSuccess();
  // Schedule post-merge verification to analyze how many LSID/SID overruns
  // were created by the session restore.
  SchedulePostMergeVerification();
}

void OAuth2LoginVerifier::SchedulePostMergeVerification() {
  BrowserThread::PostDelayedTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &OAuth2LoginVerifier::StartAuthCookiesVerification, AsWeakPtr()),
      base::TimeDelta::FromMilliseconds(kPostResoreVerificationDelay));
}

void OAuth2LoginVerifier::StartAuthCookiesVerification() {
  gaia_fetcher_.reset(
      new GaiaAuthFetcher(this,
                          std::string(GaiaConstants::kChromeOSSource),
                          user_request_context_.get()));
  gaia_fetcher_->StartListAccounts();
}

void OAuth2LoginVerifier::OnMergeSessionFailure(
    const GoogleServiceAuthError& error) {
  LOG(WARNING) << "Failed MergeSession request," << " error: " << error.state();
  // If MergeSession from GAIA service token fails, retry the session restore
  // from OAuth2 refresh token. If that failed too, signal the delegate.
  RetryOnError(
      "MergeSession",
      error,
      base::Bind(&OAuth2LoginVerifier::StartMergeSession,
                 AsWeakPtr()),
      base::Bind(&Delegate::OnSessionMergeFailure,
                 base::Unretained(delegate_)));
}

void OAuth2LoginVerifier::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(login_token_request_.get(), request);
  login_token_request_.reset();

  VLOG(1) << "Got OAuth2 access token!";
  retry_count_ = 0;
  access_token_ = access_token;
  StartOAuthLoginForUberToken();
}

void OAuth2LoginVerifier::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(login_token_request_.get(), request);
  login_token_request_.reset();

  LOG(WARNING) << "Failed to get OAuth2 access token, "
               << " error: " << error.state();
  UMA_HISTOGRAM_ENUMERATION(
      base::StringPrintf("OAuth2Login.%sFailure", "GetOAuth2AccessToken"),
      error.state(),
      GoogleServiceAuthError::NUM_STATES);
  delegate_->OnSessionMergeFailure(IsConnectionOrServiceError(error));
}

void OAuth2LoginVerifier::OnListAccountsSuccess(
    const std::string& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "ListAccounts successful.";
  delegate_->OnListAccountsSuccess(data);
}

void OAuth2LoginVerifier::OnListAccountsFailure(
    const GoogleServiceAuthError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LOG(WARNING) << "Failed to get list of session accounts, "
               << " error: " << error.state();
  RetryOnError(
      "ListAccounts",
      error,
      base::Bind(&OAuth2LoginVerifier::StartAuthCookiesVerification,
                 AsWeakPtr()),
      base::Bind(&Delegate::OnListAccountsFailure,
                 base::Unretained(delegate_)));
}

void OAuth2LoginVerifier::RetryOnError(const char* operation_id,
                                       const GoogleServiceAuthError& error,
                                       const base::Closure& task_to_retry,
                                       const ErrorHandler& error_handler) {
  if (IsConnectionOrServiceError(error) &&
      retry_count_ < kMaxRequestAttemptCount) {
    retry_count_++;
    UMA_HISTOGRAM_ENUMERATION(
        base::StringPrintf("OAuth2Login.%sRetry", operation_id),
        error.state(),
        GoogleServiceAuthError::NUM_STATES);
    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE, task_to_retry,
        base::TimeDelta::FromMilliseconds(kRequestRestartDelay));
    return;
  }

  LOG(WARNING) << "Unrecoverable error or retry count max reached for "
               << operation_id;
  UMA_HISTOGRAM_ENUMERATION(
      base::StringPrintf("OAuth2Login.%sFailure", operation_id),
      error.state(),
      GoogleServiceAuthError::NUM_STATES);

  error_handler.Run(IsConnectionOrServiceError(error));
}

}  // namespace chromeos
