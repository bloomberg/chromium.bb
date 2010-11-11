// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_ATTEMPT_STATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_ATTEMPT_STATE_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher.h"

namespace chromeos {

// Tracks the state associated with a single attempt to log in to chromium os.
// Enforces that methods are only called on the IO thread.

class AuthAttemptState {
 public:
  // Used to initalize for a login attempt.
  AuthAttemptState(const std::string& username,
                   const std::string& password,
                   const std::string& ascii_hash,
                   const std::string& login_token,
                   const std::string& login_captcha,
                   const bool user_is_new);

  // Used to initalize for a screen unlock attempt.
  AuthAttemptState(const std::string& username, const std::string& ascii_hash);

  virtual ~AuthAttemptState();

  // Copy |credentials| and |outcome| into this object, so we can have
  // a copy we're sure to own, and can make available on the IO thread.
  // Must be called from the IO thread.
  void RecordOnlineLoginStatus(
      const GaiaAuthConsumer::ClientLoginResult& credentials,
      const LoginFailure& outcome);

  // The next attempt will not allow HOSTED accounts to log in.
  void DisableHosted();

  // Copy |cryptohome_code| and |cryptohome_outcome| into this object,
  // so we can have a copy we're sure to own, and can make available
  // on the IO thread.  Must be called from the IO thread.
  void RecordCryptohomeStatus(bool cryptohome_outcome, int cryptohome_code);

  // Blow away locally stored cryptohome login status.
  // Must be called from the IO thread.
  void ResetCryptohomeStatus();

  virtual bool online_complete();
  virtual const LoginFailure& online_outcome();
  virtual const GaiaAuthConsumer::ClientLoginResult& credentials();
  virtual bool is_first_time_user();
  virtual GaiaAuthFetcher::HostedAccountsSetting hosted_policy();

  virtual bool cryptohome_complete();
  virtual bool cryptohome_outcome();
  virtual int cryptohome_code();

  // Saved so we can retry client login, and also so we know for whom login
  // has succeeded, in the event of successful completion.
  const std::string username;

  // These fields are saved so we can retry client login.
  const std::string password;
  const std::string ascii_hash;
  const std::string login_token;
  const std::string login_captcha;

  const bool unlock;  // True if authenticating to unlock the computer.

 protected:
  bool try_again_;  // True if we're willing to retry the login attempt.

  // Status of our online login attempt.
  bool online_complete_;
  LoginFailure online_outcome_;
  GaiaAuthConsumer::ClientLoginResult credentials_;

  // Whether or not we're accepting HOSTED accounts during the current
  // online auth attempt.
  GaiaAuthFetcher::HostedAccountsSetting hosted_policy_;
  bool is_first_time_user_;

  // Status of our cryptohome op attempt. Can only have one in flight at a time.
  bool cryptohome_complete_;
  bool cryptohome_outcome_;
  int cryptohome_code_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthAttemptState);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_ATTEMPT_STATE_H_
