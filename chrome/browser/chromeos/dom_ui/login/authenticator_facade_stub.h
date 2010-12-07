// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_LOGIN_AUTHENTICATOR_FACADE_STUB_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_LOGIN_AUTHENTICATOR_FACADE_STUB_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/dom_ui/login/authenticator_facade.h"

namespace chromeos {

// This class is a stubbed out version of the authentication that is used for
// functional testing of the LoginUI code. It takes in an expected user/pass
// pair during construction, which is used for authentication. Any pair that
// isn't the expected pair will fail authentication and the pair will pass.
// TODO(rharrison): Once the functional authentication code is completed make
// sure that this code isn't used in chromeos builds.
class AuthenticatorFacadeStub : public AuthenticatorFacade {
 public:
  AuthenticatorFacadeStub(LoginStatusConsumer* consumer,
                          const std::string& expected_username,
                          const std::string& expected_password) :
      AuthenticatorFacade(consumer),
      expected_username_(expected_username),
      expected_password_(expected_password) {}
  virtual ~AuthenticatorFacadeStub() {}

  virtual void AuthenticateToLogin(Profile* profile,
                                   const std::string& username,
                                   const std::string& password,
                                   const std::string& login_token,
                                   const std::string& login_captcha);

 protected:
  const std::string expected_username_;
  const std::string expected_password_;
  GaiaAuthConsumer::ClientLoginResult credentials_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthenticatorFacadeStub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_LOGIN_AUTHENTICATOR_FACADE_STUB_H_
