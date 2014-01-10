// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/common/pref_names.h"

namespace chromeos {

namespace {

const char kTestUser1[] = "test-user1@gmail.com";
const char kTestUser2[] = "test-user2@gmail.com";

}  // anonymous namespace

class LoginUITest : public chromeos::LoginManagerTest {
 public:
  LoginUITest() : LoginManagerTest(false) {}
  virtual ~LoginUITest() {}
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
