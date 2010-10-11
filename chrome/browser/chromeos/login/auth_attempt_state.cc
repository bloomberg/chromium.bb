// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/auth_attempt_state.h"

#include <string>

#include "chrome/browser/browser_thread.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "cros/chromeos_cryptohome.h"

namespace chromeos {

AuthAttemptState::AuthAttemptState(const std::string& username,
                                   const std::string& password,
                                   const std::string& ascii_hash,
                                   const std::string& login_token,
                                   const std::string& login_captcha)
    : username(username),
      password(password),
      ascii_hash(ascii_hash),
      login_token(login_token),
      login_captcha(login_captcha),
      unlock(false),
      online_complete_(false),
      online_outcome_(LoginFailure::NONE),
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
