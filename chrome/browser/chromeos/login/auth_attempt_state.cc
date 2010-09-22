// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/auth_attempt_state.h"

#include <string>

#include "chrome/browser/chrome_thread.h"
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
      online_complete_(false),
      online_outcome_(LoginFailure::NONE),
      offline_complete_(false),
      offline_outcome_(false),
      offline_code_(kCryptohomeMountErrorNone) {
}

AuthAttemptState::~AuthAttemptState() {}

void AuthAttemptState::RecordOnlineLoginStatus(
    const GaiaAuthConsumer::ClientLoginResult& credentials,
    const LoginFailure& outcome) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  online_complete_ = true;
  online_outcome_ = outcome;
  credentials_ = credentials;
}

void AuthAttemptState::RecordOfflineLoginStatus(bool offline_outcome,
                                                int offline_code) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  offline_complete_ = true;
  offline_outcome_ = offline_outcome;
  offline_code_ = offline_code;
}

void AuthAttemptState::ResetOfflineLoginStatus() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  offline_complete_ = false;
  offline_outcome_ = false;
  offline_code_ = kCryptohomeMountErrorNone;
}

bool AuthAttemptState::online_complete() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  return online_complete_;
}

const LoginFailure& AuthAttemptState::online_outcome() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  return online_outcome_;
}

const GaiaAuthConsumer::ClientLoginResult& AuthAttemptState::credentials() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  return credentials_;
}

bool AuthAttemptState::offline_complete() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  return offline_complete_;
}

bool AuthAttemptState::offline_outcome() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  return offline_outcome_;
}

int AuthAttemptState::offline_code() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  return offline_code_;
}

}  // namespace chromeos
