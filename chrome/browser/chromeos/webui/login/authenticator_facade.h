// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_AUTHENTICATOR_FACADE_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_AUTHENTICATOR_FACADE_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"

class Profile;

namespace chromeos {

class LoginStatusConsumer;

// AuthenticatorFacade operates as an interface between the WebUI login handling
// layer and the authentication layer. This allows for using a stubbed version
// of authentication during testing if needed. Also this allows for a clear
// seperation between the WebUI login handling code and the code that deals with
// authentication.
// What code will be compiled with what DEPS flags:
// touchui == 0 || chromeos == 0
// AuthenticatorFacade is not compiled
// touchui == 1 && chromes == 1
// AuthenticatorFacade is compiled in using the functional authentication code
// TODO(rharrison): Remove AuthenticatorFacade
class AuthenticatorFacade {
 public:
  explicit AuthenticatorFacade(LoginStatusConsumer* consumer) :
      consumer_(consumer) {}
  virtual ~AuthenticatorFacade() {}
  virtual void Setup() {}
  virtual void AuthenticateToLogin(Profile* profile,
                                   const std::string& username,
                                   const std::string& password,
                                   const std::string& login_token,
                                   const std::string& login_captcha) = 0;
  void AuthenticateToUnlock(const std::string& username,
                            const std::string& password) {
    AuthenticateToLogin(NULL /* not used */,
                        username,
                        password,
                        std::string(),
                        std::string());
  }

 protected:
  LoginStatusConsumer* consumer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthenticatorFacade);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_AUTHENTICATOR_FACADE_H_
