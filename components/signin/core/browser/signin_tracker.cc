// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_tracker.h"

#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "google_apis/gaia/gaia_constants.h"

SigninTracker::SigninTracker(ProfileOAuth2TokenService* token_service,
                             SigninManagerBase* signin_manager,
                             AccountReconcilor* account_reconcilor,
                             SigninClient* client,
                             Observer* observer)
    : token_service_(token_service),
      signin_manager_(signin_manager),
      account_reconcilor_(account_reconcilor),
      client_(client),
      observer_(observer) {
  Initialize();
}

SigninTracker::~SigninTracker() {
  signin_manager_->RemoveObserver(this);
  token_service_->RemoveObserver(this);

  if (account_reconcilor_) {
    account_reconcilor_->RemoveMergeSessionObserver(this);
#if !defined(OS_CHROMEOS)
  } else if (client_->ShouldMergeSigninCredentialsIntoCookieJar()) {
    SigninManager* manager = static_cast<SigninManager*>(signin_manager_);
    manager->RemoveMergeSessionObserver(this);
#endif
  }
}

void SigninTracker::Initialize() {
  DCHECK(observer_);
  signin_manager_->AddObserver(this);
  token_service_->AddObserver(this);
}

void SigninTracker::GoogleSigninFailed(const GoogleServiceAuthError& error) {
  observer_->SigninFailed(error);
}

void SigninTracker::OnRefreshTokenAvailable(const std::string& account_id) {
  if (account_id != signin_manager_->GetAuthenticatedAccountId())
    return;

  if (account_reconcilor_) {
    account_reconcilor_->AddMergeSessionObserver(this);
#if !defined(OS_CHROMEOS)
  } else if (client_->ShouldMergeSigninCredentialsIntoCookieJar()) {
    SigninManager* manager = static_cast<SigninManager*>(signin_manager_);
    manager->AddMergeSessionObserver(this);
#endif
  }

  observer_->SigninSuccess();
}

void SigninTracker::OnRefreshTokenRevoked(const std::string& account_id) {
  NOTREACHED();
}

void SigninTracker::MergeSessionCompleted(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  observer_->MergeSessionComplete(error);
}
