// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_MOCK_AUTHENTICATOR_FACADE_CROS_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_MOCK_AUTHENTICATOR_FACADE_CROS_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/webui/login/authenticator_facade_cros.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockAuthenticatorFacadeCros : public AuthenticatorFacadeCros {
 public:
  explicit MockAuthenticatorFacadeCros(LoginStatusConsumer* consumer,
                                       const std::string& expected_username,
                                       const std::string& expected_password)
      : chromeos::AuthenticatorFacadeCros(consumer),
        expected_username_(expected_username),
        expected_password_(expected_password) {}

  MOCK_METHOD0(Setup,
               void());
  MOCK_METHOD5(AuthenticateToLogin,
               void(Profile* profile,
                    const std::string& username,
                    const std::string& password,
                    const std::string& login_token,
                    const std::string& login_captcha));
  MOCK_METHOD2(AuthenticateToUnlock,
               void(const std::string& username,
                    const std::string& password));
  const std::string& GetUsername() { return expected_username_; }
  const std::string& GetPassword() { return expected_password_; }

 protected:
  std::string expected_username_;
  std::string expected_password_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAuthenticatorFacadeCros);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_MOCK_AUTHENTICATOR_FACADE_CROS_H_
