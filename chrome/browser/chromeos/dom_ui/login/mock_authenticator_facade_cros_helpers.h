// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_LOGIN_MOCK_AUTHENTICATOR_FACADE_CROS_HELPERS_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_LOGIN_MOCK_AUTHENTICATOR_FACADE_CROS_HELPERS_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/dom_ui/login/authenticator_facade_cros_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockAuthenticatorFacadeCrosHelpers
    : public AuthenticatorFacadeCrosHelpers {
 public:
  MockAuthenticatorFacadeCrosHelpers()
      : AuthenticatorFacadeCrosHelpers() {}

  MOCK_METHOD1(CreateAuthenticator,
               Authenticator*(LoginStatusConsumer* consumer));
  MOCK_METHOD4(PostAuthenticateToLogin,
               void(Authenticator* authenticator,
                    Profile* profile,
                    const std::string& username,
                    const std::string& password));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAuthenticatorFacadeCrosHelpers);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_LOGIN_MOCK_AUTHENTICATOR_FACADE_CROS_HELPERS_H_
