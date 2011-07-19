// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/online_attempt.h"

#include <string>

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
#include "content/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"
#include "third_party/libjingle/source/talk/base/urlencode.h"

namespace chromeos {

// static
const int OnlineAttempt::kClientLoginTimeoutMs = 10000;

OnlineAttempt::OnlineAttempt(AuthAttemptState* current_attempt,
                             AuthAttemptStateResolver* callback)
    : attempt_(current_attempt),
      resolver_(callback),
      fetch_canceler_(NULL),
      try_again_(true) {
  CHECK(chromeos::CrosLibrary::Get()->EnsureLoaded());
}

OnlineAttempt::~OnlineAttempt() {
  // Just to be sure.
  if (gaia_authenticator_.get())
    gaia_authenticator_->CancelRequest();
}

void OnlineAttempt::Initiate(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  gaia_authenticator_.reset(new GaiaAuthFetcher(this,
                                                GaiaConstants::kChromeOSSource,
                                                profile->GetRequestContext()));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &OnlineAttempt::TryClientLogin));
}

void OnlineAttempt::OnClientLoginSuccess(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VLOG(1) << "Online login successful!";
  if (fetch_canceler_) {
    fetch_canceler_->Cancel();
    fetch_canceler_ = NULL;
  }

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
  if (fetch_canceler_) {
    fetch_canceler_->Cancel();
    fetch_canceler_ = NULL;
  }
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

void OnlineAttempt::TryClientLogin() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  fetch_canceler_ = NewRunnableMethod(this, &OnlineAttempt::CancelClientLogin);
  BrowserThread::PostDelayedTask(BrowserThread::IO, FROM_HERE,
                                 fetch_canceler_,
                                 kClientLoginTimeoutMs);
  gaia_authenticator_->StartClientLogin(
      attempt_->username,
      attempt_->password,
      GaiaConstants::kContactsService,
      attempt_->login_token,
      attempt_->login_captcha,
      attempt_->hosted_policy());
}

void OnlineAttempt::CancelClientLogin() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (gaia_authenticator_->HasPendingFetch()) {
    LOG(WARNING) << "Canceling ClientLogin attempt.";
    gaia_authenticator_->CancelRequest();
    fetch_canceler_ = NULL;

    TriggerResolve(GaiaAuthConsumer::ClientLoginResult(),
                   LoginFailure(LoginFailure::LOGIN_TIMED_OUT));
  }
}

void OnlineAttempt::TriggerResolve(
    const GaiaAuthConsumer::ClientLoginResult& credentials,
    const LoginFailure& outcome) {
  attempt_->RecordOnlineLoginStatus(credentials, outcome);
  gaia_authenticator_.reset(NULL);
  resolver_->Resolve();
}

}  // namespace chromeos
