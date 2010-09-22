// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_ATTEMPT_STATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_ATTEMPT_STATE_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"

namespace chromeos {

// Tracks the state associated with a single attempt to log in to chromium os.
// Enforces that methods are only called on the IO thread.

class AuthAttemptState {
 public:
  AuthAttemptState(const std::string& username,
                   const std::string& password,
                   const std::string& ascii_hash,
                   const std::string& login_token,
                   const std::string& login_captcha);

  virtual ~AuthAttemptState();

  // Copy |credentials| and |outcome| into this object, so we can have
  // a copy we're sure to own, and can make available on the IO thread.
  // Must be called from the IO thread.
  void RecordOnlineLoginStatus(
      const GaiaAuthConsumer::ClientLoginResult& credentials,
      const LoginFailure& outcome);

  // Copy |offline_code| and |offline_outcome| into this object, so we can have
  // a copy we're sure to own, and can make available on the IO thread.
  // Must be called from the IO thread.
  void RecordOfflineLoginStatus(bool offline_outcome, int offline_code);

  // Blow away locally stored offline login status.
  // Must be called from the IO thread.
  void ResetOfflineLoginStatus();

  virtual bool online_complete();
  virtual const LoginFailure& online_outcome();
  virtual const GaiaAuthConsumer::ClientLoginResult& credentials();

  virtual bool offline_complete();
  virtual bool offline_outcome();
  virtual int offline_code();

  // Saved so we can retry client login, and also so we know for whom login
  // has succeeded, in the event of successful completion.
  const std::string username;

  // These fields are saved so we can retry client login.
  const std::string password;
  const std::string ascii_hash;
  const std::string login_token;
  const std::string login_captcha;

 protected:
  bool unlock_;  // True if authenticating to unlock the computer.
  bool try_again_;  // True if we're willing to retry the login attempt.

  // Status of our online login attempt.
  bool online_complete_;
  LoginFailure online_outcome_;
  GaiaAuthConsumer::ClientLoginResult credentials_;

  // Status of our offline login attempt/user homedir mount attempt.
  bool offline_complete_;
  bool offline_outcome_;
  int offline_code_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthAttemptState);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_ATTEMPT_STATE_H_
