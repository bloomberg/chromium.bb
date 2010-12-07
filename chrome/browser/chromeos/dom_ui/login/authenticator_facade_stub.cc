// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/login/authenticator_facade_stub.h"

namespace chromeos {

void AuthenticatorFacadeStub::AuthenticateToLogin(
    Profile* profile,
    const std::string& username,
    const std::string& password,
    const std::string& login_token,
    const std::string& login_captcha) {
  if (!expected_username_.compare(username) &&
      !expected_password_.compare(password)) {
    consumer_->OnLoginSuccess(username,
                              password,
                              credentials_,
                              false);
  } else {
    GoogleServiceAuthError error(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
    consumer_->OnLoginFailure(LoginFailure::FromNetworkAuthFailure(error));
  }
}

}  // namespace chromeos
