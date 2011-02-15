// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_LOGIN_LOGIN_UI_UNITTEST_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_LOGIN_LOGIN_UI_UNITTEST_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/dom_ui/login/mock_authenticator_facade_cros.h"
#include "chrome/browser/chromeos/dom_ui/login/mock_login_ui_helpers.h"
#include "chrome/test/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockWebUI : public WebUI {
 public:
  MockWebUI() : WebUI(NULL) {}
  MOCK_METHOD2(RegisterMessageCallback,
               void(const std::string& message,
                    MessageCallback* callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebUI);
};

class LoginUIHandlerHarness : public LoginUIHandler {
 public:
  explicit LoginUIHandlerHarness(const std::string& expected_username,
                                 const std::string& expected_password)
      : LoginUIHandler() {
    facade_.reset(new MockAuthenticatorFacadeCros(this,
                                                  expected_username,
                                                  expected_password));
    profile_operations_.reset(new MockProfileOperationsInterface());
    browser_operations_.reset(new MockBrowserOperationsInterface());
  }

  WebUI* GetWebUI() const { return web_ui_;}
  MockAuthenticatorFacadeCros* GetMockFacade() const {
    return static_cast<MockAuthenticatorFacadeCros*>
        (facade_.get());
  }
  MockProfileOperationsInterface* GetMockProfileOperations() const {
    return static_cast<MockProfileOperationsInterface*>
        (profile_operations_.get());
  }
  MockBrowserOperationsInterface* GetMockBrowserOperations() const {
    return static_cast<MockBrowserOperationsInterface*>
        (browser_operations_.get());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginUIHandlerHarness);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_LOGIN_LOGIN_UI_UNITTEST_H_
