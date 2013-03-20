// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_LOGIN_DISPLAY_HOST_H_

#include "base/basictypes.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockLoginDisplayHost : public LoginDisplayHost {
 public:
  MockLoginDisplayHost();
  virtual ~MockLoginDisplayHost();

  MOCK_METHOD1(CreateLoginDisplay, LoginDisplay*(LoginDisplay::Delegate*));
  MOCK_CONST_METHOD0(GetNativeWindow, gfx::NativeWindow(void));
  MOCK_CONST_METHOD0(GetWidget, views::Widget*(void));
  MOCK_METHOD0(BeforeSessionStart, void(void));
  MOCK_METHOD0(OnSessionStart, void(void));
  MOCK_METHOD0(OnCompleteLogin, void(void));
  MOCK_METHOD0(OpenProxySettings, void(void));
  MOCK_METHOD1(SetOobeProgressBarVisible, void(bool));
  MOCK_METHOD1(SetShutdownButtonEnabled, void(bool));
  MOCK_METHOD1(SetStatusAreaVisible, void(bool));
  MOCK_METHOD0(ShowBackground, void(void));
  MOCK_METHOD0(CheckForAutoEnrollment, void(void));
  MOCK_METHOD2(StartWizard, void(const std::string&, DictionaryValue*));
  MOCK_METHOD0(StartSignInScreen, void(void));
  MOCK_METHOD0(ResumeSignInScreen, void(void));
  MOCK_METHOD0(OnPreferencesChanged, void(void));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLoginDisplayHost);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_LOGIN_DISPLAY_HOST_H_
