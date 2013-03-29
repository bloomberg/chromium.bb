// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_

#include <string>

#include "chrome/browser/chromeos/login/authenticator.h"
#include "testing/gtest/include/gtest/gtest.h"

class Profile;

namespace chromeos {

class LoginStatusConsumer;

class MockAuthenticator : public Authenticator {
 public:
  MockAuthenticator(LoginStatusConsumer* consumer,
                    const std::string& expected_username,
                    const std::string& expected_password)
      : Authenticator(consumer),
        expected_username_(expected_username),
        expected_password_(expected_password) {
  }

  virtual void CompleteLogin(Profile* profile,
                             const UserContext& user_context) OVERRIDE;

  virtual void AuthenticateToLogin(Profile* profile,
                                   const UserContext& user_context,
                                   const std::string& login_token,
                                   const std::string& login_captcha) OVERRIDE;

  virtual void AuthenticateToUnlock(const UserContext& user_context) OVERRIDE;

  virtual void LoginAsLocallyManagedUser(
      const UserContext& user_context) OVERRIDE;
  virtual void LoginRetailMode() OVERRIDE;
  virtual void LoginAsPublicAccount(const std::string& username) OVERRIDE;
  virtual void LoginOffTheRecord() OVERRIDE;

  virtual void OnRetailModeLoginSuccess() OVERRIDE;

  virtual void OnLoginSuccess(bool request_pending) OVERRIDE;

  virtual void OnLoginFailure(const LoginFailure& failure) OVERRIDE;

  virtual void RecoverEncryptedData(
      const std::string& old_password) OVERRIDE {}

  virtual void ResyncEncryptedData() OVERRIDE {}

  virtual void RetryAuth(Profile* profile,
                         const UserContext& user_context,
                         const std::string& login_token,
                         const std::string& login_captcha) OVERRIDE {}

 protected:
  virtual ~MockAuthenticator() {}

 private:
  std::string expected_username_;
  std::string expected_password_;

  DISALLOW_COPY_AND_ASSIGN(MockAuthenticator);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_
