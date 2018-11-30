// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_error_controller.h"

#include "components/signin/core/browser/signin_metrics.h"

SigninErrorController::SigninErrorController(AccountMode mode,
                                             OAuth2TokenService* token_service)
    : account_mode_(mode),
      token_service_(token_service),
      scoped_token_service_observer_(this),
      auth_error_(GoogleServiceAuthError::AuthErrorNone()) {
  DCHECK(token_service_);
  scoped_token_service_observer_.Add(token_service_);
  Update();
}

SigninErrorController::~SigninErrorController() = default;

void SigninErrorController::Shutdown() {
  scoped_token_service_observer_.RemoveAll();
}

void SigninErrorController::Update() {
  GoogleServiceAuthError::State prev_state = auth_error_.state();
  std::string prev_account_id = error_account_id_;
  bool error_changed = false;

  // Find an error among the status providers. If |auth_error_| has an
  // actionable error state and some provider exposes a similar error and
  // account id, use that error. Otherwise, just take the first actionable
  // error we find.
  for (const std::string& account_id : token_service_->GetAccounts()) {
    // In PRIMARY_ACCOUNT mode, ignore all secondary accounts.
    if (account_mode_ == AccountMode::PRIMARY_ACCOUNT &&
        (account_id != primary_account_id_)) {
      continue;
    }

    if (!token_service_->RefreshTokenHasError(account_id))
      continue;

    GoogleServiceAuthError error = token_service_->GetAuthError(account_id);
    // TokenService only reports persistent errors.
    DCHECK(error.IsPersistentError());

    // Prioritize this error if it matches the previous |auth_error_|.
    if (error.state() == prev_state && account_id == prev_account_id) {
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

  if (error_changed) {
    signin_metrics::LogAuthError(auth_error_);
    for (auto& observer : observer_list_)
      observer.OnErrorChanged();
  }
}

bool SigninErrorController::HasError() const {
  DCHECK(!auth_error_.IsTransientError());
  return auth_error_.state() != GoogleServiceAuthError::NONE;
}

void SigninErrorController::SetPrimaryAccountID(const std::string& account_id) {
  primary_account_id_ = account_id;
  if (account_mode_ == AccountMode::PRIMARY_ACCOUNT)
    Update();  // Recompute the error state.
}

void SigninErrorController::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SigninErrorController::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SigninErrorController::OnEndBatchChanges() {
  Update();
}

void SigninErrorController::OnAuthErrorChanged(
    const std::string& account_id,
    const GoogleServiceAuthError& auth_error) {
  Update();
}
