// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_AUTHENTICATOR_FACADE_CROS_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_AUTHENTICATOR_FACADE_CROS_H_
#pragma once

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/webui/login/authenticator_facade.h"
#include "chrome/browser/chromeos/webui/login/authenticator_facade_cros_helpers.h"

namespace chromeos {

class AuthenticatorFacadeCrosHelpers;

// This class provides authentication services to the DOM login screen through
// libcros. It is only compiled and used when TOUCH_UI and OS_CHROMEOS are
// defined.
class AuthenticatorFacadeCros : public AuthenticatorFacade {
 public:
  explicit AuthenticatorFacadeCros(LoginStatusConsumer* consumer);
  virtual ~AuthenticatorFacadeCros() {}

  void Setup();
  virtual void AuthenticateToLogin(Profile* profile,
                                   const std::string& username,
                                   const std::string& password,
                                   const std::string& login_token,
                                   const std::string& login_captcha);

 protected:
  scoped_refptr<Authenticator> authenticator_;
  scoped_ptr<AuthenticatorFacadeCrosHelpers> helpers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthenticatorFacadeCros);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_AUTHENTICATOR_FACADE_CROS_H_
