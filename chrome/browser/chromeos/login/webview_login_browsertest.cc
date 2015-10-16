// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/ui/webui_login_display.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

namespace chromeos {

class WebviewLoginTest : public OobeBaseTest {
 public:
  WebviewLoginTest() {}
  ~WebviewLoginTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
    OobeBaseTest::SetUpCommandLine(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewLoginTest);
};

IN_PROC_BROWSER_TEST_F(WebviewLoginTest, Basic) {
  WaitForGaiaPageLoad();

  JsExpect("$('close-button-item').hidden");

  SetSignFormField("identifier", OobeBaseTest::kFakeUserEmail);
  ExecuteJsInSigninFrame("document.getElementById('nextButton').click();");

  JsExpect("$('close-button-item').hidden");

  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());

  SetSignFormField("password", OobeBaseTest::kFakeUserPassword);
  ExecuteJsInSigninFrame("document.getElementById('nextButton').click();");

  session_start_waiter.Wait();
}

// Flaky: http://crbug.com/512648.
IN_PROC_BROWSER_TEST_F(WebviewLoginTest, DISABLED_BackButton) {
  WaitForGaiaPageLoad();

  // Start: no back button, first page.
  JsExpect("$('back-button-item').hidden");
  JsExpect("$('signin-frame').src.indexOf('#identifier') != -1");

  // Next step: back button active, second page.
  SetSignFormField("identifier", OobeBaseTest::kFakeUserEmail);
  ExecuteJsInSigninFrame("document.getElementById('nextButton').click();");
  JsExpect("!$('back-button-item').hidden");
  JsExpect("$('signin-frame').src.indexOf('#challengepassword') != -1");

  // One step back: no back button, first page.
  ASSERT_TRUE(content::ExecuteScript(GetLoginUI()->GetWebContents(),
                                     "$('back-button-item').click();"));
  JsExpect("$('back-button-item').hidden");
  JsExpect("$('signin-frame').src.indexOf('#identifier') != -1");

  // Next step (again): back button active, second page, user id remembered.
  ExecuteJsInSigninFrame("document.getElementById('nextButton').click();");
  JsExpect("!$('back-button-item').hidden");
  JsExpect("$('signin-frame').src.indexOf('#challengepassword') != -1");

  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());

  SetSignFormField("password", OobeBaseTest::kFakeUserPassword);
  ExecuteJsInSigninFrame("document.getElementById('nextButton').click();");

  session_start_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(WebviewLoginTest, AllowGuest) {
  WaitForGaiaPageLoad();
  JsExpect("!$('guest-user-header-bar-item').hidden");
  CrosSettings::Get()->SetBoolean(kAccountsPrefAllowGuest, false);
  JsExpect("$('guest-user-header-bar-item').hidden");
}

// Create new account option should be available only if the settings allow it.
IN_PROC_BROWSER_TEST_F(WebviewLoginTest, AllowNewUser) {
  WaitForGaiaPageLoad();

  std::string frame_url = "$('gaia-signin').gaiaAuthHost_.reloadUrl_";
  // New users are allowed.
  JsExpect(frame_url + ".search('flow=nosignup') == -1");

  // Disallow new users - we also need to set a whitelist due to weird logic.
  CrosSettings::Get()->Set(kAccountsPrefUsers, base::ListValue());
  CrosSettings::Get()->SetBoolean(kAccountsPrefAllowNewUser, false);
  WaitForGaiaPageReload();

  // flow=nosignup indicates that user creation is not allowed.
  JsExpect(frame_url + ".search('flow=nosignup') != -1");
}

}  // namespace chromeos
