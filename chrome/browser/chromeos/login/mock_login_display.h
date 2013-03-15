// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_LOGIN_DISPLAY_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_LOGIN_DISPLAY_H_

#include "base/basictypes.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockLoginDisplay : public LoginDisplay {
 public:
  MockLoginDisplay();
  ~MockLoginDisplay();

  MOCK_METHOD4(Init, void(const UserList&, bool, bool, bool));
  MOCK_METHOD0(OnPreferencesChanged, void(void));
  MOCK_METHOD1(OnUserImageChanged, void(const User&));
  MOCK_METHOD0(OnFadeOut, void(void));
  MOCK_METHOD1(OnLoginSuccess, void(const std::string&));
  MOCK_METHOD1(SetUIEnabled, void(bool));
  MOCK_METHOD1(SelectPod, void(int));
  MOCK_METHOD3(ShowError, void(int, int, HelpAppLauncher::HelpTopic));
  MOCK_METHOD1(ShowErrorScreen, void(LoginDisplay::SigninError));
  MOCK_METHOD1(ShowGaiaPasswordChanged, void(const std::string&));
  MOCK_METHOD1(ShowPasswordChangedDialog, void(bool));
  MOCK_METHOD1(ShowSigninUI, void(const std::string&));
  MOCK_METHOD1(OnBeforeUserRemoved, void(const std::string&));
  MOCK_METHOD1(OnUserRemoved, void(const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLoginDisplay);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_LOGIN_DISPLAY_H_
