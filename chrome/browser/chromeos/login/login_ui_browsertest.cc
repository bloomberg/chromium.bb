// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/screenshot_testing_mixin.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "ui/compositor/compositor_switches.h"

namespace chromeos {

namespace {

const char kTestUser1[] = "test-user1@gmail.com";
const char kTestUser2[] = "test-user2@gmail.com";

}

class LoginUITest : public chromeos::LoginManagerTest {
 public:
  bool enable_test_screenshots_;
  LoginUITest() : LoginManagerTest(false) {
    screenshot_testing_ = new ScreenshotTestingMixin;
    AddMixin(screenshot_testing_);
  }
  virtual ~LoginUITest() {}

 protected:
  ScreenshotTestingMixin* screenshot_testing_;
};

IN_PROC_BROWSER_TEST_F(LoginUITest, PRE_LoginUIVisible) {
  RegisterUser(kTestUser1);
  RegisterUser(kTestUser2);
  StartupUtils::MarkOobeCompleted();
}

// Verifies basic login UI properties.
IN_PROC_BROWSER_TEST_F(LoginUITest, LoginUIVisible) {
  JSExpect("!!document.querySelector('#account-picker')");
  JSExpect("!!document.querySelector('#pod-row')");
  JSExpect(
      "document.querySelectorAll('.pod:not(#user-pod-template)').length == 2");

  JSExpect("document.querySelectorAll('.pod:not(#user-pod-template)')[0]"
           ".user.emailAddress == '" + std::string(kTestUser1) + "'");
  JSExpect("document.querySelectorAll('.pod:not(#user-pod-template)')[1]"
           ".user.emailAddress == '" + std::string(kTestUser2) + "'");
  screenshot_testing_->RunScreenshotTesting("LoginUITest-LoginUIVisible");
}

IN_PROC_BROWSER_TEST_F(LoginUITest, PRE_InterruptedAutoStartEnrollment) {
  StartupUtils::MarkOobeCompleted();
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kDeviceEnrollmentAutoStart, true);
}

// Tests that the default first screen is the network screen after OOBE
// when auto enrollment is enabled and device is not yet enrolled.
IN_PROC_BROWSER_TEST_F(LoginUITest, InterruptedAutoStartEnrollment) {
  OobeScreenWaiter(OobeDisplay::SCREEN_OOBE_NETWORK).Wait();
}

}  // namespace chromeos
