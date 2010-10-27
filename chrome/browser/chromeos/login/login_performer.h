// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_PERFORMER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_PERFORMER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/signed_settings_helper.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"

namespace chromeos {

// This class encapsulates sign in operations.
// Sign in is performed in a way that offline login is executed first.
// Once it's successful user homedir is mounted, UI is launched.
// If concurrent online login operation would fail that means:
// - User password has changed. Ask user for the new password.
// - User password has changed & CAPTCHA input is required.
// If |delegate_| is not NULL it will handle
// password changed and CAPTCHA dialogs.
// If |delegate_| is NULL that does mean that LoginPerformer instance
// is waiting for online login operation.
// In case of failure use ScreenLock and ask for a new password.
class LoginPerformer : public LoginStatusConsumer,
                       public SignedSettingsHelper::Callback {
 public:
  // Delegate class to get notifications from the LoginPerformer.
  class Delegate : public LoginStatusConsumer {
   public:
    virtual ~Delegate() {}
    virtual void WhiteListCheckFailed(const std::string& email) = 0;
  };

  explicit LoginPerformer(Delegate* delegate);

  // LoginStatusConsumer implementation:
  virtual void OnLoginFailure(const LoginFailure& error);
  virtual void OnLoginSuccess(
      const std::string& username,
      const std::string& password,
      const GaiaAuthConsumer::ClientLoginResult& credentials,
      bool pending_requests);
  virtual void OnOffTheRecordLoginSuccess();
  virtual void OnPasswordChangeDetected(
      const GaiaAuthConsumer::ClientLoginResult& credentials);

  // SignedSettingsHelper::Callback
  virtual void OnCheckWhiteListCompleted(bool success,
                                         const std::string& email);

  // Performs login with the |username| and |password| specified.
  void Login(const std::string& username, const std::string& password);

  // Performs actions to prepare Guest mode login.
  void LoginOffTheRecord();

  // Migrates cryptohome using |old_password| specified.
  void RecoverEncryptedData(const std::string& old_password);

  // Reinitializes cryptohome with the new password.
  void ResyncEncryptedData();

  // Returns latest auth error.
  const GoogleServiceAuthError& error() const {
    return last_login_failure_.error();
  }

  // True if last login operation has timed out.
  bool login_timed_out() {
    return last_login_failure_.reason() == LoginFailure::LOGIN_TIMED_OUT;
  }

  void set_captcha(const std::string& captcha) { captcha_ = captcha; }
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

 private:
  // Starts authentication.
  void StartAuthentication();

  // Used for logging in.
  scoped_refptr<Authenticator> authenticator_;

  // Represents last login failure that was encountered when communicating to
  // sign-in server. LoginFailure.None() by default.
  LoginFailure last_login_failure_;

  // String entered by the user as an answer to a CAPTCHA challenge.
  std::string captcha_;

  // Token representing the specific CAPTCHA challenge.
  std::string captcha_token_;

  // Cached credentials data when password change is detected.
  GaiaAuthConsumer::ClientLoginResult cached_credentials_;

  // Username and password for the current login attempt.
  std::string username_;
  std::string password_;

  // Notifications receiver.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(LoginPerformer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_PERFORMER_H_
