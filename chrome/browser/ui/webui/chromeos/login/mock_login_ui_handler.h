// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MOCK_LOGIN_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MOCK_LOGIN_UI_HANDLER_H_
#pragma once

#include <string>

#include "chrome/browser/ui/webui/chromeos/login/login_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/login_ui_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockLoginUIHandler : public chromeos::LoginUIHandler {
 public:
  MOCK_METHOD1(Attach,
      WebUIMessageHandler*(WebUI* web_ui));
  MOCK_METHOD0(RegisterMessages,
      void());
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MOCK_LOGIN_UI_HANDLER_H_
