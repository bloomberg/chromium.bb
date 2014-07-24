// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/auth_attempt_state.h"

#include <string>

#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

namespace chromeos {

AuthAttemptState::AuthAttemptState(const UserContext& user_context,
                                   user_manager::UserType user_type,
                                   bool unlock,
                                   bool online_complete,
                                   bool user_is_new)
    : user_context(user_context),
      user_type(user_type),
      unlock(unlock),
      online_complete_(online_complete),
      online_outcome_(online_complete ? AuthFailure::UNLOCK_FAILED
                                      : AuthFailure::NONE),
      hosted_policy_(GaiaAuthFetcher::HostedAccountsAllowed),
      is_first_time_user_(user_is_new),
      cryptohome_complete_(false),
      cryptohome_outcome_(false),
      cryptohome_code_(cryptohome::MOUNT_ERROR_NONE),
      username_hash_obtained_(true),
      username_hash_valid_(true) {
}

AuthAttemptState::~AuthAttemptState() {
}

void AuthAttemptState::RecordOnlineLoginStatus(const AuthFailure& outcome) {
  online_complete_ = true;
  online_outcome_ = outcome;
  // We're either going to not try again, or try again with HOSTED
  // accounts not allowed, so just set this here.
  DisableHosted();
}

void AuthAttemptState::DisableHosted() {
  hosted_policy_ = GaiaAuthFetcher::HostedAccountsNotAllowed;
}

void AuthAttemptState::RecordCryptohomeStatus(
    bool cryptohome_outcome,
    cryptohome::MountError cryptohome_code) {
  cryptohome_complete_ = true;
  cryptohome_outcome_ = cryptohome_outcome;
  cryptohome_code_ = cryptohome_code;
}

void AuthAttemptState::RecordUsernameHash(const std::string& username_hash) {
  user_context.SetUserIDHash(username_hash);
  username_hash_obtained_ = true;
  username_hash_valid_ = true;
}

void AuthAttemptState::RecordUsernameHashFailed() {
  username_hash_obtained_ = true;
  username_hash_valid_ = false;
}

void AuthAttemptState::UsernameHashRequested() {
  username_hash_obtained_ = false;
}

void AuthAttemptState::ResetCryptohomeStatus() {
  cryptohome_complete_ = false;
  cryptohome_outcome_ = false;
  cryptohome_code_ = cryptohome::MOUNT_ERROR_NONE;
}

bool AuthAttemptState::online_complete() {
  return online_complete_;
}

const AuthFailure& AuthAttemptState::online_outcome() {
  return online_outcome_;
}

bool AuthAttemptState::is_first_time_user() {
  return is_first_time_user_;
}

GaiaAuthFetcher::HostedAccountsSetting AuthAttemptState::hosted_policy() {
  return hosted_policy_;
}

bool AuthAttemptState::cryptohome_complete() {
  return cryptohome_complete_;
}

bool AuthAttemptState::cryptohome_outcome() {
  return cryptohome_outcome_;
}

cryptohome::MountError AuthAttemptState::cryptohome_code() {
  return cryptohome_code_;
}

bool AuthAttemptState::username_hash_obtained() {
  return username_hash_obtained_;
}

bool AuthAttemptState::username_hash_valid() {
  return username_hash_obtained_;
}

}  // namespace chromeos
