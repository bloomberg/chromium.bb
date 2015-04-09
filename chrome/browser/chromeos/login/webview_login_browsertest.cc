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
namespace {
const char kFakeUserEmail[] = "fake-email@gmail.com";
const char kFakeUserPassword[] = "fake-password";
const char kFakeSIDCookie[] = "fake-SID-cookie";
const char kFakeLSIDCookie[] = "fake-LSID-cookie";
}

class WebviewLoginTest : public OobeBaseTest {
 public:
  WebviewLoginTest() { use_webview_ = true; }
  ~WebviewLoginTest() override {}

  void SetUpOnMainThread() override {
    fake_gaia_->SetFakeMergeSessionParams(kFakeUserEmail, kFakeSIDCookie,
                                          kFakeLSIDCookie);

    OobeBaseTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
    OobeBaseTest::SetUpCommandLine(command_line);
  }

  void WaitForGaiaPageLoaded() {
    WaitForSigninScreen();

    ASSERT_TRUE(content::ExecuteScript(
        GetLoginUI()->GetWebContents(),
        "$('gaia-signin').gaiaAuthHost_.addEventListener('ready',"
          "function() {"
            "window.domAutomationController.setAutomationId(0);"
            "window.domAutomationController.send('GaiaReady');"
          "});"));

    content::DOMMessageQueue message_queue;
    std::string message;
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"GaiaReady\"", message);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewLoginTest);
};

IN_PROC_BROWSER_TEST_F(WebviewLoginTest, Basic) {
  WaitForGaiaPageLoaded();

  JsExpect("$('close-button-item').hidden");

  SetSignFormField("identifier", kFakeUserEmail);
  ExecuteJsInSigninFrame("document.getElementById('nextButton').click();");

  JsExpect("$('close-button-item').hidden");

  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());

  SetSignFormField("password", kFakeUserPassword);
  ExecuteJsInSigninFrame("document.getElementById('nextButton').click();");

  session_start_waiter.Wait();
}

}  // namespace chromeos
