// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_LOGIN_MOCK_LOGIN_UI_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_LOGIN_MOCK_LOGIN_UI_HANDLER_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/dom_ui/login/login_ui.h"
#include "chrome/browser/chromeos/dom_ui/login/login_ui_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockLoginUIHandler : public chromeos::LoginUIHandler {
 public:
  MOCK_METHOD1(Attach,
      WebUIMessageHandler*(DOMUI* dom_ui));
  MOCK_METHOD0(RegisterMessages,
      void());
  MOCK_METHOD1(OnLoginFailure,
      void(const chromeos::LoginFailure& failure));
  MOCK_METHOD4(OnLoginSuccess,
      void(const std::string& username,
           const std::string& password,
           const GaiaAuthConsumer::ClientLoginResult& credentials,
           bool pending_requests));
  MOCK_METHOD0(OnOffTheRecordLoginSuccess,
      void());
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_LOGIN_MOCK_LOGIN_UI_HANDLER_H_
