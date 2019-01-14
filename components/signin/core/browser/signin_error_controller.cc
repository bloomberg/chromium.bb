// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_error_controller.h"

#include "components/signin/core/browser/signin_metrics.h"

SigninErrorController::SigninErrorController(
    AccountMode mode,
    identity::IdentityManager* identity_manager)
    : account_mode_(mode),
      identity_manager_(identity_manager),
      scoped_identity_manager_observer_(this),
      auth_error_(GoogleServiceAuthError::AuthErrorNone()) {
  DCHECK(identity_manager_);
  scoped_identity_manager_observer_.Add(identity_manager_);

  Update();
}

SigninErrorController::~SigninErrorController() = default;

void SigninErrorController::Shutdown() {
  scoped_identity_manager_observer_.RemoveAll();
}

void SigninErrorController::Update() {
  GoogleServiceAuthError::State prev_state = auth_error_.state();
  std::string prev_account_id = error_account_id_;
  bool error_changed = false;

  // Find an error among the status providers. If |auth_error_| has an
  // actionable error state and some provider exposes a similar error and
  // account id, use that error. Otherwise, just take the first actionable
  // error we find.
  for (const AccountInfo& account_info :
       identity_manager_->GetAccountsWithRefreshTokens()) {
    std::string account_id = account_info.account_id;

    // In PRIMARY_ACCOUNT mode, ignore all secondary accounts.
    if (account_mode_ == AccountMode::PRIMARY_ACCOUNT &&
        (account_id != identity_manager_->GetPrimaryAccountId())) {
      continue;
    }

    if (!identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
            account_id)) {
      continue;
    }

    GoogleServiceAuthError error =
        identity_manager_->GetErrorStateOfRefreshTokenForAccount(account_id);
    // IdentityManager only reports persistent errors.
    DCHECK(error.IsPersistentError());

    // Prioritize this error if it matches the previous |auth_error_|.
    if (error.state() == prev_state && account_id == prev_account_id) {
      // The previous error for the previous account still exists. This error is
      // preferred to avoid UI churn, so |auth_error_| and |error_account_id_|
      // must be updated to match the previous state. This is needed in case
      // |auth_error_| and |error_account_id_| were updated to other values in
      // a previous iteration via the if statement below.
      auth_error_ = error;
      error_account_id_ = account_id;
      error_changed = true;
      break;
    }

    // Use this error if we haven't found one already, but keep looking for the
    // previous |auth_error_| in case there's a match elsewhere in the set.
    if (!error_changed) {
      auth_error_ = error;
      error_account_id_ = account_id;
      error_changed = true;
    }
  }

  if (!error_changed && prev_state != GoogleServiceAuthError::NONE) {
    // No provider reported an error, so clear the error we have now.
    auth_error_ = GoogleServiceAuthError::AuthErrorNone();
    error_account_id_.clear();
    error_changed = true;
  }

  if (!error_changed)
    return;

  if (auth_error_.state() == prev_state &&
      error_account_id_ == prev_account_id) {
    // Only fire notification if the auth error state or account were updated.
    return;
  }

  signin_metrics::LogAuthError(auth_error_);
  for (auto& observer : observer_list_)
    observer.OnErrorChanged();
}

bool SigninErrorController::HasError() const {
  DCHECK(!auth_error_.IsTransientError());
  return auth_error_.state() != GoogleServiceAuthError::NONE;
}

void SigninErrorController::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SigninErrorController::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SigninErrorController::OnEndBatchOfRefreshTokenStateChanges() {
  Update();
}

void SigninErrorController::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const AccountInfo& account_info,
    const GoogleServiceAuthError& error) {
  Update();
}

void SigninErrorController::OnPrimaryAccountSet(
    const AccountInfo& primary_account_info) {
  // Ignore updates to the primary account if not in PRIMARY_ACCOUNT mode.
  if (account_mode_ != AccountMode::PRIMARY_ACCOUNT)
    return;

  Update();
}

void SigninErrorController::OnPrimaryAccountCleared(
    const AccountInfo& previous_primary_account_info) {
  // Ignore updates to the primary account if not in PRIMARY_ACCOUNT mode.
  if (account_mode_ != AccountMode::PRIMARY_ACCOUNT)
    return;

  Update();
}
