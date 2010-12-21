// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_LOGIN_MOCK_AUTHENTICATOR_FACADE_STUB_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_LOGIN_MOCK_AUTHENTICATOR_FACADE_STUB_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/dom_ui/login/authenticator_facade_stub.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockAuthenticatorFacadeStub : public AuthenticatorFacadeStub {
 public:
  explicit MockAuthenticatorFacadeStub(LoginStatusConsumer* consumer,
                                       const std::string& expected_username,
                                       const std::string& expected_password)
      : chromeos::AuthenticatorFacadeStub(consumer,
                                          expected_username,
                                          expected_password) {}
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

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAuthenticatorFacadeStub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_LOGIN_MOCK_AUTHENTICATOR_FACADE_STUB_H_
