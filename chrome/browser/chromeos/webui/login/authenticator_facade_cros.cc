// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/webui/login/authenticator_facade_cros.h"
#include "chrome/browser/chromeos/webui/login/authenticator_facade_cros_helpers.h"

#include <string>

namespace chromeos {

AuthenticatorFacadeCros::AuthenticatorFacadeCros(
    LoginStatusConsumer* consumer)
    : AuthenticatorFacade(consumer),
      authenticator_(NULL),
      helpers_(new AuthenticatorFacadeCrosHelpers()) {
}

void AuthenticatorFacadeCros::Setup() {
  authenticator_ = helpers_->CreateAuthenticator(consumer_);
}

void AuthenticatorFacadeCros::AuthenticateToLogin(
    Profile* profile,
    const std::string& username,
    const std::string& password,
    const std::string& login_token,
    const std::string& login_captcha) {
  helpers_->PostAuthenticateToLogin(authenticator_.get(),
                                    profile,
                                    username,
                                    password);
}

}  // namespace chromeos
