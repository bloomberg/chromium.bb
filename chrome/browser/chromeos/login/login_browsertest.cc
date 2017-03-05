// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/wm_window.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_system.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/test/rect_test_util.h"

using ::gfx::test::RectContains;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace chromeos {
namespace {

const char kGaiaId[] = "12345";
const char kTestUser[] = "test-user@gmail.com";
const char kPassword[] = "password";

class LoginUserTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kLoginUser, "TestUser@gmail.com");
    command_line->AppendSwitchASCII(switches::kLoginProfile, "hash");
  }
};

class LoginGuestTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "hash");
    command_line->AppendSwitchASCII(
        switches::kLoginUser, user_manager::GuestAccountId().GetUserEmail());
  }
};

class LoginCursorTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLoginManager);
  }
};

class LoginSigninTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
  }

  void TearDownOnMainThread() override {
    // Close the login manager, which otherwise holds a KeepAlive that is not
    // cleared in time by the end of the test.
    LoginDisplayHost::default_host()->Finalize();
  }

  void SetUpOnMainThread() override {
    LoginDisplayHostImpl::DisableRestrictiveProxyCheckForTest();
  }
};

class LoginTest : public LoginManagerTest {
 public:
  LoginTest() : LoginManagerTest(true) {}
  ~LoginTest() override {}

  void StartGaiaAuthOffline() {
    content::DOMMessageQueue message_queue;
    const std::string js = "(function() {"
      "var authenticator = $('gaia-signin').gaiaAuthHost_;"
      "authenticator.addEventListener('ready',"
        "function f() {"
          "authenticator.removeEventListener('ready', f);"
          "window.domAutomationController.setAutomationId(0);"
          "window.domAutomationController.send('offlineLoaded');"
        "});"
      "$('error-offline-login-link').onclick();"
    "})();";
    ASSERT_TRUE(content::ExecuteScript(web_contents(), js));

    std::string message;
    do {
      ASSERT_TRUE(message_queue.WaitForMessage(&message));
    } while (message != "\"offlineLoaded\"");
  }

  void SubmitGaiaAuthOfflineForm(const std::string& user_email,
                                 const std::string& password) {
    const std::string animated_pages =
        "document.querySelector('#offline-gaia /deep/ "
        "#animatedPages')";
    const std::string email_input =
        "document.querySelector('#offline-gaia /deep/ #emailInput')";
    const std::string email_next_button =
        "document.querySelector('#offline-gaia /deep/ #emailSection "
        "/deep/ #button')";
    const std::string password_input =
        "document.querySelector('#offline-gaia /deep/ "
        "#passwordInput')";
    const std::string password_next_button =
        "document.querySelector('#offline-gaia /deep/ #passwordSection"
        " /deep/ #button')";

    content::DOMMessageQueue message_queue;
    JSExpect("!document.querySelector('#offline-gaia').hidden");
    JSExpect("document.querySelector('#signin-frame').hidden");
    const std::string js =
        animated_pages +
        ".addEventListener('neon-animation-finish',"
        "function() {"
        "window.domAutomationController.setAutomationId(0);"
        "window.domAutomationController.send('switchToPassword');"
        "})";
    ASSERT_TRUE(content::ExecuteScript(web_contents(), js));
    std::string set_email = email_input + ".value = '$Email'";
    base::ReplaceSubstringsAfterOffset(&set_email, 0, "$Email", user_email);
    ASSERT_TRUE(content::ExecuteScript(web_contents(), set_email));
    ASSERT_TRUE(content::ExecuteScript(web_contents(),
                                       email_next_button + ".fire('tap')"));
    std::string message;
    do {
      ASSERT_TRUE(message_queue.WaitForMessage(&message));
    } while (message != "\"switchToPassword\"");

    std::string set_password = password_input + ".value = '$Password'";
    base::ReplaceSubstringsAfterOffset(&set_password, 0, "$Password", password);
    ASSERT_TRUE(content::ExecuteScript(web_contents(), set_password));
    ASSERT_TRUE(content::ExecuteScript(web_contents(),
                                       password_next_button + ".fire('tap')"));
  }

  void PrepareOfflineLogin() {
    bool show_user;
    ASSERT_TRUE(CrosSettings::Get()->GetBoolean(
        kAccountsPrefShowUserNamesOnSignIn, &show_user));
    ASSERT_FALSE(show_user);

    StartGaiaAuthOffline();

    UserContext user_context(
        AccountId::FromUserEmailGaiaId(kTestUser, kGaiaId));
    user_context.SetKey(Key(kPassword));
    SetExpectedCredentials(user_context);
  }
};

// Used to make sure that the system tray is visible and within the screen
// bounds after login.
void TestSystemTrayIsVisible() {
  ash::SystemTray* tray = ash::Shell::GetInstance()->GetPrimarySystemTray();
  aura::Window* primary_win = ash::Shell::GetPrimaryRootWindow();
  ash::WmWindow* wm_primary_win = ash::WmWindow::Get(primary_win);
  ash::WmShelf* wm_shelf = ash::WmShelf::ForWindow(wm_primary_win);
  SCOPED_TRACE(testing::Message()
               << "ShelfVisibilityState=" << wm_shelf->GetVisibilityState()
               << " ShelfAutoHideBehavior=" << wm_shelf->auto_hide_behavior());
  EXPECT_TRUE(tray->visible());
  EXPECT_TRUE(RectContains(primary_win->bounds(), tray->GetBoundsInScreen()));
}

}  // namespace

// After a chrome crash, the session manager will restart chrome with
// the -login-user flag indicating that the user is already logged in.
// This profile should NOT be an OTR profile.
IN_PROC_BROWSER_TEST_F(LoginUserTest, UserPassed) {
  Profile* profile = browser()->profile();
  std::string profile_base_path("hash");
  profile_base_path.insert(0, chrome::kProfileDirPrefix);
  EXPECT_EQ(profile_base_path, profile->GetPath().BaseName().value());
  EXPECT_FALSE(profile->IsOffTheRecord());

  TestSystemTrayIsVisible();
}

// Verifies the cursor is not hidden at startup when user is logged in.
IN_PROC_BROWSER_TEST_F(LoginUserTest, CursorShown) {
  EXPECT_TRUE(ash::Shell::GetInstance()->cursor_manager()->IsCursorVisible());

  TestSystemTrayIsVisible();
}

// After a guest login, we should get the OTR default profile.
// Test is flaky https://crbug.com/693106
IN_PROC_BROWSER_TEST_F(LoginGuestTest, DISABLED_GuestIsOTR) {
  Profile* profile = browser()->profile();
  EXPECT_TRUE(profile->IsOffTheRecord());
  // Ensure there's extension service for this profile.
  EXPECT_TRUE(extensions::ExtensionSystem::Get(profile)->extension_service());

  TestSystemTrayIsVisible();
}

// Verifies the cursor is not hidden at startup when running guest session.
// Test is flaky https://crbug.com/693106
IN_PROC_BROWSER_TEST_F(LoginGuestTest, DISABLED_CursorShown) {
  EXPECT_TRUE(ash::Shell::GetInstance()->cursor_manager()->IsCursorVisible());

  TestSystemTrayIsVisible();
}

// Verifies the cursor is hidden at startup on login screen.
IN_PROC_BROWSER_TEST_F(LoginCursorTest, CursorHidden) {
  // Login screen needs to be shown explicitly when running test.
  ShowLoginWizard(OobeScreen::SCREEN_SPECIAL_LOGIN);

  // Cursor should be hidden at startup
  EXPECT_FALSE(ash::Shell::GetInstance()->cursor_manager()->IsCursorVisible());

  // Cursor should be shown after cursor is moved.
  EXPECT_TRUE(ui_test_utils::SendMouseMoveSync(gfx::Point()));
  EXPECT_TRUE(ash::Shell::GetInstance()->cursor_manager()->IsCursorVisible());

  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
      FROM_HERE, LoginDisplayHost::default_host());

  TestSystemTrayIsVisible();
}

// Verifies that the webui for login comes up successfully.
IN_PROC_BROWSER_TEST_F(LoginSigninTest, WebUIVisible) {
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources())
      .Wait();
}


IN_PROC_BROWSER_TEST_F(LoginTest, PRE_GaiaAuthOffline) {
  RegisterUser(kTestUser);
  StartupUtils::MarkOobeCompleted();
  CrosSettings::Get()->SetBoolean(kAccountsPrefShowUserNamesOnSignIn, false);
}

// Flaky, see http://crbug/692364.
IN_PROC_BROWSER_TEST_F(LoginTest, DISABLED_GaiaAuthOffline) {
  PrepareOfflineLogin();
  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  SubmitGaiaAuthOfflineForm(kTestUser, kPassword);
  session_start_waiter.Wait();

  TestSystemTrayIsVisible();
}

}  // namespace chromeos
