// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/auth_attempt_state.h"

#include <string>

#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher.h"
#include "content/browser/browser_thread.h"
#include "third_party/cros/chromeos_cryptohome.h"

namespace chromeos {

AuthAttemptState::AuthAttemptState(const std::string& username,
                                   const std::string& password,
                                   const std::string& ascii_hash,
                                   const std::string& login_token,
                                   const std::string& login_captcha,
                                   const bool user_is_new)
    : username(username),
      password(password),
      ascii_hash(ascii_hash),
      login_token(login_token),
      login_captcha(login_captcha),
      unlock(false),
      online_complete_(false),
      online_outcome_(LoginFailure::NONE),
      hosted_policy_(GaiaAuthFetcher::HostedAccountsAllowed),
      is_first_time_user_(user_is_new),
      cryptohome_complete_(false),
      cryptohome_outcome_(false),
      cryptohome_code_(kCryptohomeMountErrorNone) {
}

AuthAttemptState::AuthAttemptState(const std::string& username,
                                   const std::string& ascii_hash)
    : username(username),
      ascii_hash(ascii_hash),
      unlock(true),
      online_complete_(true),
      online_outcome_(LoginFailure::UNLOCK_FAILED),
      credentials_(GaiaAuthConsumer::ClientLoginResult()),
      hosted_policy_(GaiaAuthFetcher::HostedAccountsAllowed),
      is_first_time_user_(false),
      cryptohome_complete_(false),
      cryptohome_outcome_(false),
      cryptohome_code_(kCryptohomeMountErrorNone) {
}

AuthAttemptState::~AuthAttemptState() {}

void AuthAttemptState::RecordOnlineLoginStatus(
    const GaiaAuthConsumer::ClientLoginResult& credentials,
    const LoginFailure& outcome) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  online_complete_ = true;
  online_outcome_ = outcome;
  credentials_ = credentials;
  // We're either going to not try again, or try again with HOSTED
  // accounts not allowed, so just set this here.
  DisableHosted();
}

void AuthAttemptState::DisableHosted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  hosted_policy_ = GaiaAuthFetcher::HostedAccountsNotAllowed;
}

void AuthAttemptState::RecordCryptohomeStatus(bool cryptohome_outcome,
                                              int cryptohome_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  cryptohome_complete_ = true;
  cryptohome_outcome_ = cryptohome_outcome;
  cryptohome_code_ = cryptohome_code;
}

void AuthAttemptState::ResetCryptohomeStatus() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  cryptohome_complete_ = false;
  cryptohome_outcome_ = false;
  cryptohome_code_ = kCryptohomeMountErrorNone;
}

bool AuthAttemptState::online_complete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return online_complete_;
}

const LoginFailure& AuthAttemptState::online_outcome() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return online_outcome_;
}

const GaiaAuthConsumer::ClientLoginResult& AuthAttemptState::credentials() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return credentials_;
}

bool AuthAttemptState::is_first_time_user() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return is_first_time_user_;
}

GaiaAuthFetcher::HostedAccountsSetting AuthAttemptState::hosted_policy() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return hosted_policy_;
}

bool AuthAttemptState::cryptohome_complete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return cryptohome_complete_;
}

bool AuthAttemptState::cryptohome_outcome() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return cryptohome_outcome_;
}

int AuthAttemptState::cryptohome_code() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return cryptohome_code_;
}

}  // namespace chromeos
