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

 protected:
  void ClickNext() {
    ExecuteJsInSigninFrame("document.getElementById('nextButton').click();");
  }

  void ExpectIdentifierPage() {
    // First page: no back button, no close button, refresh button, #identifier
    // input field.
    JsExpect("!$('gaia-navigation').backVisible");
    JsExpect("!$('gaia-navigation').closeVisible");
    JsExpect("$('gaia-navigation').refreshVisible");
    JsExpect("$('signin-frame').src.indexOf('#identifier') != -1");
  }

  void ExpectPasswordPage() {
    // Second page: back button, close button, no refresh button,
    // #challengepassword input field.
    JsExpect("$('gaia-navigation').backVisible");
    JsExpect("$('gaia-navigation').closeVisible");
    JsExpect("!$('gaia-navigation').refreshVisible");
    JsExpect("$('signin-frame').src.indexOf('#challengepassword') != -1");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewLoginTest);
};

// Basic signin with username and password.
IN_PROC_BROWSER_TEST_F(WebviewLoginTest, Basic) {
  WaitForGaiaPageLoad();

  ExpectIdentifierPage();

  SetSignFormField("identifier", OobeBaseTest::kFakeUserEmail);
  ClickNext();
  ExpectPasswordPage();

  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());

  SetSignFormField("password", OobeBaseTest::kFakeUserPassword);
  ClickNext();

  session_start_waiter.Wait();
}

// Fails: http://crbug.com/512648.
IN_PROC_BROWSER_TEST_F(WebviewLoginTest, DISABLED_BackButton) {
  WaitForGaiaPageLoad();

  // Start with identifer page.
  ExpectIdentifierPage();

  // Move to password page.
  SetSignFormField("identifier", OobeBaseTest::kFakeUserEmail);
  ClickNext();
  ExpectPasswordPage();

  // Click back to identifier page.
  JS().Evaluate("$('gaia-navigation').$.backButton.click();");
  ExpectIdentifierPage();

  // Click next to password page, user id is remembered.
  ClickNext();
  ExpectPasswordPage();

  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());

  // Finish sign-up.
  SetSignFormField("password", OobeBaseTest::kFakeUserPassword);
  ClickNext();

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

IN_PROC_BROWSER_TEST_F(WebviewLoginTest, EmailPrefill) {
  WaitForGaiaPageLoad();
  JS().ExecuteAsync("Oobe.showSigninUI('user@example.com')");
  WaitForGaiaPageReload();
  EXPECT_EQ(fake_gaia_->prefilled_email(), "user@example.com");
}

}  // namespace chromeos
