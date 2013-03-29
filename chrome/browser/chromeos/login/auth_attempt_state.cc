// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/auth_attempt_state.h"

#include <string>

#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

using content::BrowserThread;

namespace chromeos {

AuthAttemptState::AuthAttemptState(const UserContext& user_context,
                                   const std::string& ascii_hash,
                                   const std::string& login_token,
                                   const std::string& login_captcha,
                                   const User::UserType user_type,
                                   const bool user_is_new)
    : user_context(user_context),
      ascii_hash(ascii_hash),
      login_token(login_token),
      login_captcha(login_captcha),
      user_type(user_type),
      unlock(false),
      online_complete_(false),
      online_outcome_(LoginFailure::NONE),
      hosted_policy_(GaiaAuthFetcher::HostedAccountsAllowed),
      is_first_time_user_(user_is_new),
      cryptohome_complete_(false),
      cryptohome_outcome_(false),
      cryptohome_code_(cryptohome::MOUNT_ERROR_NONE),
      username_hash_obtained_(true) {
}

AuthAttemptState::AuthAttemptState(const std::string& username,
                                   const std::string& ascii_hash)
    : user_context(username, "", ""),
      ascii_hash(ascii_hash),
      user_type(User::USER_TYPE_REGULAR),
      unlock(true),
      online_complete_(true),
      online_outcome_(LoginFailure::UNLOCK_FAILED),
      hosted_policy_(GaiaAuthFetcher::HostedAccountsAllowed),
      is_first_time_user_(false),
      cryptohome_complete_(false),
      cryptohome_outcome_(false),
      cryptohome_code_(cryptohome::MOUNT_ERROR_NONE),
      username_hash_obtained_(true) {
}

AuthAttemptState::AuthAttemptState(const UserContext& user_context,
                                   const std::string& ascii_hash,
                                   const bool user_is_new)
    : user_context(user_context),
      ascii_hash(ascii_hash),
      user_type(User::USER_TYPE_REGULAR),
      unlock(true),
      online_complete_(false),
      online_outcome_(LoginFailure::NONE),
      hosted_policy_(GaiaAuthFetcher::HostedAccountsAllowed),
      is_first_time_user_(user_is_new),
      cryptohome_complete_(false),
      cryptohome_outcome_(false),
      cryptohome_code_(cryptohome::MOUNT_ERROR_NONE),
      username_hash_obtained_(true) {
}

AuthAttemptState::~AuthAttemptState() {}

void AuthAttemptState::RecordOnlineLoginStatus(
    const LoginFailure& outcome) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  online_complete_ = true;
  online_outcome_ = outcome;
  // We're either going to not try again, or try again with HOSTED
  // accounts not allowed, so just set this here.
  DisableHosted();
}

void AuthAttemptState::DisableHosted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  hosted_policy_ = GaiaAuthFetcher::HostedAccountsNotAllowed;
}

void AuthAttemptState::RecordCryptohomeStatus(
    bool cryptohome_outcome,
    cryptohome::MountError cryptohome_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  cryptohome_complete_ = true;
  cryptohome_outcome_ = cryptohome_outcome;
  cryptohome_code_ = cryptohome_code;
}

void AuthAttemptState::RecordUsernameHash(const std::string& username_hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  user_context.username_hash = username_hash;
  username_hash_obtained_ = true;
}

void AuthAttemptState::UsernameHashRequested() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  username_hash_obtained_ = false;
}

void AuthAttemptState::ResetCryptohomeStatus() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  cryptohome_complete_ = false;
  cryptohome_outcome_ = false;
  cryptohome_code_ = cryptohome::MOUNT_ERROR_NONE;
}

bool AuthAttemptState::online_complete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return online_complete_;
}

const LoginFailure& AuthAttemptState::online_outcome() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return online_outcome_;
}

bool AuthAttemptState::is_first_time_user() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return is_first_time_user_;
}

GaiaAuthFetcher::HostedAccountsSetting AuthAttemptState::hosted_policy() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return hosted_policy_;
}

bool AuthAttemptState::cryptohome_complete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return cryptohome_complete_;
}

bool AuthAttemptState::cryptohome_outcome() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return cryptohome_outcome_;
}

cryptohome::MountError AuthAttemptState::cryptohome_code() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return cryptohome_code_;
}

bool AuthAttemptState::username_hash_obtained() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return username_hash_obtained_;
}

}  // namespace chromeos
