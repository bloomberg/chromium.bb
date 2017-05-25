// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_MOCK_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_MOCK_LOGIN_DISPLAY_HOST_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockLoginDisplayHost : public LoginDisplayHost {
 public:
  MockLoginDisplayHost();
  virtual ~MockLoginDisplayHost();

  MOCK_METHOD1(CreateLoginDisplay, LoginDisplay*(LoginDisplay::Delegate*));
  MOCK_CONST_METHOD0(GetNativeWindow, gfx::NativeWindow(void));
  MOCK_CONST_METHOD0(GetOobeUI, OobeUI*(void));
  MOCK_CONST_METHOD0(GetWebUILoginView, WebUILoginView*(void));
  MOCK_METHOD0(BeforeSessionStart, void(void));

  // Workaround for move-only args in GMock.
  MOCK_METHOD1(MockFinalize, void(base::OnceClosure*));
  void Finalize(base::OnceClosure completion_callback) override {
    MockFinalize(&completion_callback);
  }

  MOCK_METHOD0(OpenProxySettings, void(void));
  MOCK_METHOD1(SetStatusAreaVisible, void(bool));
  MOCK_METHOD0(ShowBackground, void(void));
  MOCK_METHOD1(StartWizard, void(OobeScreen));
  MOCK_METHOD0(GetWizardController, WizardController*(void));
  MOCK_METHOD0(GetAppLaunchController, AppLaunchController*(void));

  // Workaround for move-only args in GMock.
  MOCK_METHOD1(MockStartUserAdding, void(base::OnceClosure*));
  void StartUserAdding(base::OnceClosure completion_callback) {
    MockStartUserAdding(&completion_callback);
  }

  MOCK_METHOD0(CancelUserAdding, void(void));
  MOCK_METHOD1(StartSignInScreen, void(const LoginScreenContext&));
  MOCK_METHOD0(ResumeSignInScreen, void(void));
  MOCK_METHOD0(OnPreferencesChanged, void(void));
  MOCK_METHOD0(PrewarmAuthentication, void(void));
  MOCK_METHOD3(StartAppLaunch, void(const std::string&, bool, bool));
  MOCK_METHOD0(StartDemoAppLaunch, void(void));
  MOCK_METHOD1(StartArcKiosk, void(const AccountId&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLoginDisplayHost);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_MOCK_LOGIN_DISPLAY_HOST_H_
