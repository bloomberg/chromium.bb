// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/online_attempt.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/auth_attempt_state.h"
#include "chrome/browser/chromeos/login/auth_attempt_state_resolver.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"
#include "third_party/libjingle/source/talk/base/urlencode.h"

using content::BrowserThread;

namespace {

// The service scope of the OAuth v2 token that ChromeOS login will be
// requesting.
const char* kServiceScopeChromeOS = GaiaConstants::kSyncServiceOAuth;

}

namespace chromeos {

// static
const int OnlineAttempt::kClientLoginTimeoutMs = 10000;

OnlineAttempt::OnlineAttempt(bool using_oauth,
                             AuthAttemptState* current_attempt,
                             AuthAttemptStateResolver* callback)
    : using_oauth_(using_oauth),
      attempt_(current_attempt),
      resolver_(callback),
      weak_factory_(this),
      try_again_(true) {
}

OnlineAttempt::~OnlineAttempt() {
  // Just to be sure.
  if (client_fetcher_.get())
    client_fetcher_->CancelRequest();

  if (oauth_fetcher_.get())
    oauth_fetcher_->CancelRequest();
}

void OnlineAttempt::Initiate(Profile* auth_profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (using_oauth_) {
    oauth_fetcher_.reset(
        new GaiaOAuthFetcher(this,
                             auth_profile->GetRequestContext(),
                             auth_profile,
                             kServiceScopeChromeOS));
  } else {
    client_fetcher_.reset(
        new GaiaAuthFetcher(this, GaiaConstants::kChromeOSSource,
                            auth_profile->GetRequestContext()));
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&OnlineAttempt::TryClientLogin, this));
}

void OnlineAttempt::OnClientLoginSuccess(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VLOG(1) << "Online login successful!";

  weak_factory_.InvalidateWeakPtrs();

  if (attempt_->hosted_policy() == GaiaAuthFetcher::HostedAccountsAllowed &&
      attempt_->is_first_time_user()) {
    // First time user, and we don't know if the account is HOSTED or not.
    // Since we don't allow HOSTED accounts to log in, we need to try
    // again, without allowing HOSTED accounts.
    //
    // NOTE: we used to do this in the opposite order, so that we'd only
    // try the HOSTED pathway if GOOGLE-only failed.  This breaks CAPTCHA
    // handling, though.
    attempt_->DisableHosted();
    TryClientLogin();
    return;
  }
  TriggerResolve(credentials, LoginFailure::None());
}

void OnlineAttempt::OnClientLoginFailure(
    const GoogleServiceAuthError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  weak_factory_.InvalidateWeakPtrs();

  if (error.state() == GoogleServiceAuthError::REQUEST_CANCELED) {
    if (try_again_) {
      try_again_ = false;
      // TODO(cmasone): add UMA tracking for this to see if we can remove it.
      LOG(ERROR) << "Login attempt canceled!?!?  Trying again.";
      TryClientLogin();
      return;
    }
    LOG(ERROR) << "Login attempt canceled again?  Already retried...";
  }

  if (error.state() == GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS &&
      attempt_->is_first_time_user() &&
      attempt_->hosted_policy() != GaiaAuthFetcher::HostedAccountsAllowed) {
    // This was a first-time login, we already tried allowing HOSTED accounts
    // and succeeded.  That we've failed with INVALID_GAIA_CREDENTIALS now
    // indicates that the account is HOSTED.
    LOG(WARNING) << "Rejecting valid HOSTED account.";
    TriggerResolve(GaiaAuthConsumer::ClientLoginResult(),
                   LoginFailure::FromNetworkAuthFailure(
                       GoogleServiceAuthError(
                           GoogleServiceAuthError::HOSTED_NOT_ALLOWED)));
    return;
  }

  if (error.state() == GoogleServiceAuthError::TWO_FACTOR) {
    LOG(WARNING) << "Two factor authenticated. Sync will not work.";
    TriggerResolve(GaiaAuthConsumer::ClientLoginResult(),
                   LoginFailure::None());

    return;
  }
  VLOG(2) << "ClientLogin attempt failed with " << error.state();
  TriggerResolve(GaiaAuthConsumer::ClientLoginResult(),
                 LoginFailure::FromNetworkAuthFailure(error));
}

void OnlineAttempt::OnOAuthLoginSuccess(const std::string& sid,
                                        const std::string& lsid,
                                        const std::string& auth) {
  GaiaAuthConsumer::ClientLoginResult credentials(sid,
    lsid, auth, std::string());
  OnClientLoginSuccess(credentials);
}

void OnlineAttempt::OnOAuthLoginFailure(const GoogleServiceAuthError& error) {
  OnClientLoginFailure(error);
}

void OnlineAttempt::TryClientLogin() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserThread::PostDelayedTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&OnlineAttempt::CancelClientLogin, weak_factory_.GetWeakPtr()),
      kClientLoginTimeoutMs);

  if (using_oauth_) {
    if (!attempt_->oauth1_access_token().length() ||
        !attempt_->oauth1_access_secret().length()) {
      // Empty OAuth1 access token and secret probably means that we are
      // dealing with a legacy ChromeOS account. This should be treated as
      // invalid/expired token.
      OnClientLoginFailure(GoogleServiceAuthError(
          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
    } else {
      oauth_fetcher_->StartOAuthLogin(GaiaConstants::kChromeOSSource,
                                      GaiaConstants::kPicasaService,
                                      attempt_->oauth1_access_token(),
                                      attempt_->oauth1_access_secret());
    }
  } else {
    client_fetcher_->StartClientLogin(
        attempt_->username,
        attempt_->password,
        GaiaConstants::kPicasaService,
        attempt_->login_token,
        attempt_->login_captcha,
        attempt_->hosted_policy());
  }
}

bool OnlineAttempt::HasPendingFetch() {
  return using_oauth_ ? oauth_fetcher_->HasPendingFetch() :
      client_fetcher_->HasPendingFetch();
}

void OnlineAttempt::CancelRequest() {
  weak_factory_.InvalidateWeakPtrs();
}

void OnlineAttempt::CancelClientLogin() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (HasPendingFetch()) {
    LOG(WARNING) << "Canceling ClientLogin attempt.";
    CancelRequest();

    TriggerResolve(GaiaAuthConsumer::ClientLoginResult(),
                   LoginFailure(LoginFailure::LOGIN_TIMED_OUT));
  }
}

void OnlineAttempt::TriggerResolve(
    const GaiaAuthConsumer::ClientLoginResult& credentials,
    const LoginFailure& outcome) {
  attempt_->RecordOnlineLoginStatus(credentials, outcome);
  client_fetcher_.reset(NULL);
  oauth_fetcher_.reset(NULL);
  resolver_->Resolve();
}

}  // namespace chromeos
