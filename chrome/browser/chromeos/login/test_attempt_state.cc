// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test_attempt_state.h"

#include <string>

#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "cros/chromeos_cryptohome.h"

namespace chromeos {

TestAttemptState::TestAttemptState(const std::string& username,
                                   const std::string& password,
                                   const std::string& ascii_hash,
                                   const std::string& login_token,
                                   const std::string& login_captcha)
    : AuthAttemptState(username,
                       password,
                       ascii_hash,
                       login_token,
                       login_captcha) {
}

TestAttemptState::~TestAttemptState() {}

void TestAttemptState::PresetOnlineLoginStatus(
    const GaiaAuthConsumer::ClientLoginResult& credentials,
    const LoginFailure& outcome) {
  online_complete_ = true;
  online_outcome_ = outcome;
  credentials_ = credentials;
}

void TestAttemptState::PresetOfflineLoginStatus(bool offline_outcome,
                                                int offline_code) {
  offline_complete_ = true;
  offline_outcome_ = offline_outcome;
  offline_code_ = offline_code;
}

}  // namespace chromeos
