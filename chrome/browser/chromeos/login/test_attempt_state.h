// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_ATTEMPT_STATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_ATTEMPT_STATE_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/login/auth_attempt_state.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"

namespace chromeos {

class TestAttemptState : public AuthAttemptState {
 public:
  TestAttemptState(const std::string& username,
                   const std::string& password,
                   const std::string& ascii_hash,
                   const std::string& login_token,
                   const std::string& login_captcha,
                   const bool user_is_new);

  TestAttemptState(const std::string& username, const std::string& ascii_hash);

  virtual ~TestAttemptState();

  // Act as though an online login attempt completed already.
  void PresetOnlineLoginStatus(
      const GaiaAuthConsumer::ClientLoginResult& credentials,
      const LoginFailure& outcome);

  // The next attempt will not allow HOSTED accounts to log in.
  void DisableHosted();

  // Act as though an cryptohome login attempt completed already.
  void PresetCryptohomeStatus(bool cryptohome_outcome, int cryptohome_code);

  // To allow state to be queried on the main thread during tests.
  virtual bool online_complete() { return online_complete_; }
  virtual const LoginFailure& online_outcome() { return online_outcome_; }
  virtual const GaiaAuthConsumer::ClientLoginResult& credentials() {
    return credentials_;
  }
  virtual bool is_first_time_user() { return is_first_time_user_; }
  virtual GaiaAuthFetcher::HostedAccountsSetting hosted_policy() {
    return hosted_policy_;
  }

  virtual bool cryptohome_complete() { return cryptohome_complete_; }
  virtual bool cryptohome_outcome() { return cryptohome_outcome_; }
  virtual int cryptohome_code() { return cryptohome_code_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAttemptState);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_ATTEMPT_STATE_H_
