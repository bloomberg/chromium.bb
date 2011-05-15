// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test_attempt_state.h"

#include <string>

#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "third_party/cros/chromeos_cryptohome.h"

namespace chromeos {

TestAttemptState::TestAttemptState(const std::string& username,
                                   const std::string& password,
                                   const std::string& ascii_hash,
                                   const std::string& login_token,
                                   const std::string& login_captcha,
                                   const bool user_is_new)
    : AuthAttemptState(username,
                       password,
                       ascii_hash,
                       login_token,
                       login_captcha,
                       user_is_new) {
}

TestAttemptState::TestAttemptState(const std::string& username,
                                   const std::string& ascii_hash)
    : AuthAttemptState(username, ascii_hash) {
}

TestAttemptState::~TestAttemptState() {}

void TestAttemptState::PresetOnlineLoginStatus(
    const GaiaAuthConsumer::ClientLoginResult& credentials,
    const LoginFailure& outcome) {
  online_complete_ = true;
  online_outcome_ = outcome;
  credentials_ = credentials;
}

void TestAttemptState::DisableHosted() {
  hosted_policy_ = GaiaAuthFetcher::HostedAccountsNotAllowed;
}

void TestAttemptState::PresetCryptohomeStatus(bool cryptohome_outcome,
                                                int cryptohome_code) {
  cryptohome_complete_ = true;
  cryptohome_outcome_ = cryptohome_outcome;
  cryptohome_code_ = cryptohome_code;
}

bool TestAttemptState::online_complete() {
  return online_complete_;
}

const LoginFailure& TestAttemptState::online_outcome() {
  return online_outcome_;
}

const GaiaAuthConsumer::ClientLoginResult& TestAttemptState::credentials() {
  return credentials_;
}

bool TestAttemptState::is_first_time_user() {
  return is_first_time_user_;
}

GaiaAuthFetcher::HostedAccountsSetting TestAttemptState::hosted_policy() {
  return hosted_policy_;
}

bool TestAttemptState::cryptohome_complete() {
  return cryptohome_complete_;
}

bool TestAttemptState::cryptohome_outcome() {
  return cryptohome_outcome_;
}

int TestAttemptState::cryptohome_code() {
  return cryptohome_code_;
}

}  // namespace chromeos
