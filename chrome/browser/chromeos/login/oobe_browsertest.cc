// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/webui_login_display.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/widget/widget.h"

using namespace net::test_server;

namespace chromeos {

class OobeTest : public InProcessBrowserTest {
 public:
  OobeTest() {}
  virtual ~OobeTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
    command_line->AppendSwitchASCII(
        ::switches::kAuthExtensionPath, "gaia_auth");
    fake_gaia_.Initialize();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    CHECK(embedded_test_server()->InitializeAndWaitUntilReady());
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));
    LOG(INFO) << "Set up http server at " << embedded_test_server()->base_url();

    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ::switches::kGaiaUrl, embedded_test_server()->base_url().spec());
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHostImpl::default_host()) {
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             base::Bind(&chrome::AttemptExit));
      content::RunMessageLoop();
    }
  }

  chromeos::WebUILoginDisplay* GetLoginDisplay() {
    chromeos::ExistingUserController* controller =
        chromeos::ExistingUserController::current_controller();
    CHECK(controller);
    return static_cast<chromeos::WebUILoginDisplay*>(
        controller->login_display());
  }

  views::Widget* GetLoginWindowWidget() {
    return static_cast<chromeos::LoginDisplayHostImpl*>(
        chromeos::LoginDisplayHostImpl::default_host())
        ->login_window_for_test();
  }

 private:
  FakeGaia fake_gaia_;
  DISALLOW_COPY_AND_ASSIGN(OobeTest);
};

IN_PROC_BROWSER_TEST_F(OobeTest, NewUser) {
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());

  content::WindowedNotificationObserver(
    chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
    content::NotificationService::AllSources()).Wait();

  // TODO(glotov): mock GAIA server (test_server()) should support
  // username/password configuration.
  GetLoginDisplay()->ShowSigninScreenForCreds("username", "password");

  content::WindowedNotificationObserver(
    chrome::NOTIFICATION_SESSION_STARTED,
    content::NotificationService::AllSources()).Wait();
}

IN_PROC_BROWSER_TEST_F(OobeTest, Accelerator) {
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());

  content::WindowedNotificationObserver(
    chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
    content::NotificationService::AllSources()).Wait();

  gfx::NativeWindow login_window = GetLoginWindowWidget()->GetNativeWindow();

  ui_controls::SendKeyPress(login_window,
                            ui::VKEY_E,
                            true,    // control
                            false,   // shift
                            true,    // alt
                            false);  // command
  OobeScreenWaiter(OobeDisplay::SCREEN_OOBE_ENROLLMENT).Wait();
}

}  // namespace chromeos
