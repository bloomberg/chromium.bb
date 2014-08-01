// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/ui/webui_login_display.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

#if defined(ENABLE_RLZ)
void GetAccessPointRlzInBackgroundThread(rlz_lib::AccessPoint point,
                                         base::string16* rlz) {
  ASSERT_FALSE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ASSERT_TRUE(RLZTracker::GetAccessPointRlz(point, rlz));
}
#endif

}  // namespace

class LoginUtilsTest : public InProcessBrowserTest {
 public:
  LoginUtilsTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Initialize the test server early, so that we can use its base url for
    // the command line flags.
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

    // Use the login manager screens and the gaia auth extension.
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
    command_line->AppendSwitchASCII(::switches::kAuthExtensionPath,
                                    "gaia_auth");

    // Redirect requests to gaia and the policy server to the test server.
    command_line->AppendSwitchASCII(::switches::kGaiaUrl,
                                    embedded_test_server()->base_url().spec());
    command_line->AppendSwitchASCII(::switches::kLsoUrl,
                                    embedded_test_server()->base_url().spec());
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    fake_gaia_.Initialize();
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    RunUntilIdle();
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  void RunUntilIdle() {
    base::RunLoop().RunUntilIdle();
  }

  PrefService* local_state() {
    return g_browser_process->local_state();
  }

  void Login(const std::string& username) {
    content::WindowedNotificationObserver session_started_observer(
        chrome::NOTIFICATION_SESSION_STARTED,
        content::NotificationService::AllSources());

    ExistingUserController* controller =
        ExistingUserController::current_controller();
    ASSERT_TRUE(controller);
    WebUILoginDisplay* login_display =
        static_cast<WebUILoginDisplay*>(controller->login_display());
    ASSERT_TRUE(login_display);

    login_display->ShowSigninScreenForCreds(username, "password");

    // Wait for the session to start after submitting the credentials. This
    // will wait until all the background requests are done.
    session_started_observer.Wait();
  }

 private:
  FakeGaia fake_gaia_;

  DISALLOW_COPY_AND_ASSIGN(LoginUtilsTest);
};

#if defined(ENABLE_RLZ)
IN_PROC_BROWSER_TEST_F(LoginUtilsTest, RlzInitialized) {
  // Skip to the signin screen.
  WizardController::SkipPostLoginScreensForTesting();
  WizardController* wizard_controller = WizardController::default_controller();
  ASSERT_TRUE(wizard_controller);
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());

  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  RunUntilIdle();

  // No RLZ brand code set initially.
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kRLZBrand));

  // Wait for blocking RLZ tasks to complete.
  {
    base::RunLoop loop;
    PrefChangeRegistrar registrar;
    registrar.Init(local_state());
    registrar.Add(prefs::kRLZBrand, loop.QuitClosure());
    Login("username");
    loop.Run();
  }

  // RLZ brand code has been set to empty string.
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kRLZBrand));
  EXPECT_EQ(std::string(), local_state()->GetString(prefs::kRLZBrand));

  // RLZ value for homepage access point should have been initialized.
  // This value must be obtained in a background thread.
  {
    base::RunLoop loop;
    base::string16 rlz_string;
    content::BrowserThread::PostBlockingPoolTaskAndReply(
        FROM_HERE,
        base::Bind(&GetAccessPointRlzInBackgroundThread,
                   RLZTracker::ChromeHomePage(),
                   &rlz_string),
        loop.QuitClosure());
    loop.Run();
    EXPECT_EQ(base::string16(), rlz_string);
  }
}
#endif

}  // namespace chromeos
