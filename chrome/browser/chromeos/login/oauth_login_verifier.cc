// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/oauth_login_verifier.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// OAuth token verification max retry count.
const int kMaxOAuthTokenVerificationAttemptCount = 5;
// OAuth token verification retry delay in milliseconds.
const int kOAuthVerificationRestartDelay = 10000;

// The service scope of the OAuth v2 token that ChromeOS login will be
// requesting.
const char kServiceScopeChromeOS[] =
    "https://www.googleapis.com/auth/chromesync";

}  // namespace

OAuthLoginVerifier::OAuthLoginVerifier(OAuthLoginVerifier::Delegate* delegate,
                                       Profile* user_profile,
                                       const std::string& oauth1_token,
                                       const std::string& oauth1_secret,
                                       const std::string& username)
    : delegate_(delegate),
      oauth_fetcher_(this,
                     g_browser_process->system_request_context(),
                     kServiceScopeChromeOS),
      gaia_fetcher_(this,
                    std::string(GaiaConstants::kChromeOSSource),
                    user_profile->GetRequestContext()),
      oauth1_token_(oauth1_token),
      oauth1_secret_(oauth1_secret),
      username_(username),
      user_profile_(user_profile),
      verification_count_(0),
      step_(VERIFICATION_STEP_UNVERIFIED) {
}

OAuthLoginVerifier::~OAuthLoginVerifier() {
}

void OAuthLoginVerifier::StartOAuthVerification() {
  if (oauth1_token_.empty() || oauth1_secret_.empty()) {
    // Empty OAuth1 access token or secret probably means that we are
    // dealing with a legacy ChromeOS account. This should be treated as
    // invalid/expired token.
    OnOAuthLoginFailure(GoogleServiceAuthError(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  } else {
    oauth_fetcher_.StartOAuthLogin(GaiaConstants::kChromeOSSource,
                                   GaiaConstants::kPicasaService,
                                   oauth1_token_,
                                   oauth1_secret_);
  }
}

void OAuthLoginVerifier::ContinueVerification() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Check if we have finished with this one already.
  if (is_done())
    return;

  if (user_profile_ != ProfileManager::GetDefaultProfile())
    return;

  // Check if we currently trying to fetch something.
  if (oauth_fetcher_.HasPendingFetch() || gaia_fetcher_.HasPendingFetch())
    return;

  if (CrosLibrary::Get()->libcros_loaded()) {
    // Delay the verification if the network is not connected or on a captive
    // portal.
    const Network* network =
        CrosLibrary::Get()->GetNetworkLibrary()->active_network();
    if (!network || !network->connected() || network->restricted_pool()) {
      BrowserThread::PostDelayedTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&OAuthLoginVerifier::ContinueVerification, AsWeakPtr()),
          base::TimeDelta::FromMilliseconds(kOAuthVerificationRestartDelay));
      return;
    }
  }

  verification_count_++;
  if (step_ == VERIFICATION_STEP_UNVERIFIED) {
    DVLOG(1) << "Retrying to verify OAuth1 access tokens.";
    StartOAuthVerification();
  } else {
    DVLOG(1) << "Retrying to fetch user cookies.";
    StartCookiesRetrieval();
  }
}

void OAuthLoginVerifier::StartCookiesRetrieval() {
  DCHECK(!sid_.empty());
  DCHECK(!lsid_.empty());
  gaia_fetcher_.StartIssueAuthToken(sid_, lsid_, GaiaConstants::kGaiaService);
}

bool OAuthLoginVerifier::RetryOnError(const GoogleServiceAuthError& error) {
  if (error.state() == GoogleServiceAuthError::CONNECTION_FAILED ||
      error.state() == GoogleServiceAuthError::SERVICE_UNAVAILABLE ||
      error.state() == GoogleServiceAuthError::REQUEST_CANCELED) {
    if (verification_count_ < kMaxOAuthTokenVerificationAttemptCount) {
      BrowserThread::PostDelayedTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&OAuthLoginVerifier::ContinueVerification, AsWeakPtr()),
          base::TimeDelta::FromMilliseconds(kOAuthVerificationRestartDelay));
      return true;
    }
  }
  step_ = VERIFICATION_STEP_FAILED;
  return false;
}

void OAuthLoginVerifier::OnOAuthLoginSuccess(const std::string& sid,
                                             const std::string& lsid,
                                             const std::string& auth) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  step_ = VERIFICATION_STEP_OAUTH_VERIFIED;
  verification_count_ = 0;
  sid_ = sid;
  lsid_ = lsid;
  delegate_->OnOAuthVerificationSucceeded(username_, sid, lsid, auth);
  StartCookiesRetrieval();
}

void OAuthLoginVerifier::OnOAuthLoginFailure(
    const GoogleServiceAuthError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LOG(WARNING) << "Failed to verify OAuth1 access tokens,"
               << " error.state=" << error.state();
  if (!RetryOnError(error))
    delegate_->OnOAuthVerificationFailed(username_);
}

void OAuthLoginVerifier::OnCookieFetchFailed(
    const GoogleServiceAuthError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!RetryOnError(error))
    delegate_->OnUserCookiesFetchFailed(username_);
}

void OAuthLoginVerifier::OnIssueAuthTokenSuccess(
    const std::string& service,
    const std::string& auth_token) {
  gaia_fetcher_.StartMergeSession(auth_token);
}

void OAuthLoginVerifier::OnIssueAuthTokenFailure(
    const std::string& service,
      const GoogleServiceAuthError& error) {
  DVLOG(1) << "Failed IssueAuthToken request,"
           << " error.state=" << error.state();
  OnCookieFetchFailed(error);
}

void OAuthLoginVerifier::OnMergeSessionSuccess(const std::string& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "MergeSession successful.";
  step_ = VERIFICATION_STEP_COOKIES_FETCHED;
  delegate_->OnUserCookiesFetchSucceeded(username_);
}

void OAuthLoginVerifier::OnMergeSessionFailure(
    const GoogleServiceAuthError& error) {
  DVLOG(1) << "Failed MergeSession request,"
           << " error.state=" << error.state();
  OnCookieFetchFailed(error);
}

}  // namespace chromeos
