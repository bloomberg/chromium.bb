// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/online_attempt.h"

#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/auth_attempt_state.h"
#include "chrome/browser/chromeos/login/auth_attempt_state_resolver.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/net/gaia/gaia_authenticator2.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
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

OnlineAttempt::~OnlineAttempt() {}

void OnlineAttempt::Initiate(Profile* profile) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  gaia_authenticator_.reset(
      new GaiaAuthenticator2(this,
                             GaiaConstants::kChromeOSSource,
                             profile->GetRequestContext()));
  TryClientLogin();
}

void OnlineAttempt::OnClientLoginSuccess(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  LOG(INFO) << "Online login successful!";
  if (fetch_canceler_) {
    fetch_canceler_->Cancel();
    fetch_canceler_ = NULL;
  }

  TriggerResolve(credentials, LoginFailure::None());
}

void OnlineAttempt::OnClientLoginFailure(
    const GoogleServiceAuthError& error) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (fetch_canceler_) {
    fetch_canceler_->Cancel();
    fetch_canceler_ = NULL;
  }
  if (error.state() == GoogleServiceAuthError::REQUEST_CANCELED) {
    if (try_again_) {
      try_again_ = false;
      LOG(ERROR) << "Login attempt canceled!?!?  Trying again.";
      TryClientLogin();
      return;
    }
    LOG(ERROR) << "Login attempt canceled again?  Already retried...";
  }

  if (error.state() == GoogleServiceAuthError::TWO_FACTOR) {
    LOG(WARNING) << "Two factor authenticated. Sync will not work.";
    TriggerResolve(GaiaAuthConsumer::ClientLoginResult(),
                   LoginFailure::None());

    return;
  }

  TriggerResolve(GaiaAuthConsumer::ClientLoginResult(),
                 LoginFailure::FromNetworkAuthFailure(error));
}

void OnlineAttempt::TryClientLogin() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  fetch_canceler_ = NewRunnableMethod(this, &OnlineAttempt::CancelClientLogin);
  ChromeThread::PostDelayedTask(ChromeThread::IO, FROM_HERE,
                                fetch_canceler_,
                                kClientLoginTimeoutMs);
  gaia_authenticator_->StartClientLogin(attempt_->username,
                                        attempt_->password,
                                        GaiaConstants::kContactsService,
                                        attempt_->login_token,
                                        attempt_->login_captcha);
}

void OnlineAttempt::CancelClientLogin() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
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
