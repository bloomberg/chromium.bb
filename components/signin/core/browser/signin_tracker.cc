// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_tracker.h"

#include "google_apis/gaia/gaia_constants.h"

SigninTracker::SigninTracker(identity::IdentityManager* identity_manager,
                             Observer* observer)
    : identity_manager_(identity_manager), observer_(observer) {
  Initialize();
}

SigninTracker::~SigninTracker() {
  identity_manager_->RemoveObserver(this);
}

void SigninTracker::Initialize() {
  DCHECK(observer_);
  identity_manager_->AddObserver(this);
}

void SigninTracker::OnPrimaryAccountSet(const AccountInfo& account_info) {
  if (identity_manager_->HasAccountWithRefreshToken(account_info.account_id))
    observer_->SigninSuccess();
}

void SigninTracker::OnPrimaryAccountSigninFailed(
    const GoogleServiceAuthError& error) {
  observer_->SigninFailed(error);
}

void SigninTracker::OnRefreshTokenUpdatedForAccount(
    const AccountInfo& account_info) {
  if (account_info.account_id != identity_manager_->GetPrimaryAccountId())
    return;

  observer_->SigninSuccess();
}

void SigninTracker::OnAddAccountToCookieCompleted(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  observer_->AccountAddedToCookie(error);
}
