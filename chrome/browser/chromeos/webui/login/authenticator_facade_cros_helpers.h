// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_AUTHENTICATOR_FACADE_CROS_HELPERS_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_AUTHENTICATOR_FACADE_CROS_HELPERS_H_
#pragma once

#include <string>

class Profile;

namespace chromeos {

class Authenticator;
class LoginStatusConsumer;

class AuthenticatorFacadeCrosHelpers {
 public:
  AuthenticatorFacadeCrosHelpers();
  virtual ~AuthenticatorFacadeCrosHelpers() {}

  virtual Authenticator* CreateAuthenticator(LoginStatusConsumer* consumer);
  virtual void PostAuthenticateToLogin(Authenticator* authenticator,
                                       Profile* profile,
                                       const std::string& username,
                                       const std::string& password);

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthenticatorFacadeCrosHelpers);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_AUTHENTICATOR_FACADE_CROS_HELPERS_H_
