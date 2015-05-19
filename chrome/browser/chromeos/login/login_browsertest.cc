// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "base/command_line.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
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
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/user_names.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_system.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace {

const char kGaiaId[] = "12345";
const char kTestUser[] = "test-user@gmail.com";
const char kPassword[] = "password";

void FilterFrameByName(std::set<content::RenderFrameHost*>* frame_set,
                       const std::string& frame_name,
                       content::RenderFrameHost* frame) {
  if (frame->GetFrameName() == frame_name)
    frame_set->insert(frame);
}

class LoginUserTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        chromeos::switches::kLoginUser, "TestUser@gmail.com");
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                    "hash");
  }
};

class LoginGuestTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                    "hash");
    command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                    chromeos::login::kGuestUserName);
  }
};

class LoginCursorTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
  }
};

class LoginSigninTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(tracing::BeginTracingWithWatch(
        "ui", "ui", "ShowLoginWebUI", 1));
  }
};

class LoginTest : public chromeos::LoginManagerTest {
 public:
  LoginTest() : LoginManagerTest(true) {}
  ~LoginTest() override {}

  content::RenderFrameHost* GetNamedFrame(const std::string& frame_name) {
    std::set<content::RenderFrameHost*> frame_set;
    web_contents()->ForEachFrame(
        base::Bind(&FilterFrameByName, &frame_set, frame_name));
    return frame_set.empty() ? NULL : *frame_set.begin();
  }

  void ExecuteJsInGaiaAuthFrame(const std::string& js) {
    content::RenderFrameHost* frame = GetNamedFrame("signin-frame");
    ASSERT_TRUE(frame);
    ASSERT_TRUE(content::ExecuteScript(frame, js));
  }

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

  void SubmitOldGaiaAuthOfflineForm(const std::string& user_email,
                                    const std::string& password) {
    // Note the input elements must match gaia_auth/offline.html.
    JSExpect("document.querySelector('#offline-gaia').hidden");
    JSExpect("!document.querySelector('#signin-frame').hidden");
    std::string js =
        "(function(){"
          "document.getElementsByName('email')[0].value = '$Email';"
          "document.getElementsByName('password')[0].value = '$Password';"
          "document.getElementById('submit-button').click();"
        "})();";
    ReplaceSubstringsAfterOffset(&js, 0, "$Email", user_email);
    ReplaceSubstringsAfterOffset(&js, 0, "$Password", password);
    ExecuteJsInGaiaAuthFrame(js);
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
        "/deep/ .blue-button')";
    const std::string password_input =
        "document.querySelector('#offline-gaia /deep/ "
        "#passwordInput')";
    const std::string password_next_button =
        "document.querySelector('#offline-gaia /deep/ #passwordSection"
        " /deep/ .blue-button')";

    content::DOMMessageQueue message_queue;
    JSExpect("!document.querySelector('#offline-gaia').hidden");
    JSExpect("document.querySelector('#signin-frame').hidden");
    const std::string js =
        animated_pages +
        ".addEventListener('core-animated-pages-transition-end',"
        "function() {"
        "window.domAutomationController.setAutomationId(0);"
        "window.domAutomationController.send('switchToPassword');"
        "})";
    ASSERT_TRUE(content::ExecuteScript(web_contents(), js));
    std::string set_email = email_input + ".value = '$Email'";
    ReplaceSubstringsAfterOffset(&set_email, 0, "$Email", user_email);
    ASSERT_TRUE(content::ExecuteScript(web_contents(), set_email));
    ASSERT_TRUE(content::ExecuteScript(web_contents(),
                                       email_next_button + ".fire('tap')"));
    std::string message;
    do {
      ASSERT_TRUE(message_queue.WaitForMessage(&message));
      LOG(ERROR) << message;
    } while (message != "\"switchToPassword\"");

    std::string set_password = password_input + ".value = '$Password'";
    ReplaceSubstringsAfterOffset(&set_password, 0, "$Password", password);
    ASSERT_TRUE(content::ExecuteScript(web_contents(), set_password));
    ASSERT_TRUE(content::ExecuteScript(web_contents(),
                                       password_next_button + ".fire('tap')"));
  }

  void PrepareOfflineLogin() {
    bool show_user;
    ASSERT_TRUE(chromeos::CrosSettings::Get()->GetBoolean(
        chromeos::kAccountsPrefShowUserNamesOnSignIn, &show_user));
    ASSERT_FALSE(show_user);

    StartGaiaAuthOffline();

    chromeos::UserContext user_context(kTestUser);
    user_context.SetGaiaID(kGaiaId);
    user_context.SetKey(chromeos::Key(kPassword));
    SetExpectedCredentials(user_context);
  }
};

class LoginOfflineTest : public LoginTest {
 public:
  LoginOfflineTest() {}
  ~LoginOfflineTest() override {}

  bool SetUpUserDataDirectory() override {
    base::FilePath user_data_dir;
    CHECK(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
    base::FilePath local_state_path =
        user_data_dir.Append(chrome::kLocalStateFilename);
    base::DictionaryValue local_state_dict;

    // Set webview disabled flag only when local state file does not exist.
    // Otherwise, we break PRE tests that leave state in it.
    if (!base::PathExists(local_state_path)) {
      // TODO(rsorokin): Fix offline test for webview signin.
      // http://crbug.com/475569
      local_state_dict.SetBoolean(prefs::kWebviewSigninDisabled, true);

      CHECK(JSONFileValueSerializer(local_state_path)
                .Serialize(local_state_dict));
    }

    return LoginTest::SetUpUserDataDirectory();
  }
};

// Used to make sure that the system tray is visible and within the screen
// bounds after login.
void TestSystemTrayIsVisible() {
  ash::SystemTray* tray = ash::Shell::GetInstance()->GetPrimarySystemTray();
  aura::Window* primary_win = ash::Shell::GetPrimaryRootWindow();
  EXPECT_TRUE(tray->visible());
  EXPECT_TRUE(primary_win->bounds().Contains(tray->GetBoundsInScreen()));
}

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
IN_PROC_BROWSER_TEST_F(LoginGuestTest, GuestIsOTR) {
  Profile* profile = browser()->profile();
  EXPECT_TRUE(profile->IsOffTheRecord());
  // Ensure there's extension service for this profile.
  EXPECT_TRUE(extensions::ExtensionSystem::Get(profile)->extension_service());

  TestSystemTrayIsVisible();
}

// Verifies the cursor is not hidden at startup when running guest session.
IN_PROC_BROWSER_TEST_F(LoginGuestTest, CursorShown) {
  EXPECT_TRUE(ash::Shell::GetInstance()->cursor_manager()->IsCursorVisible());

  TestSystemTrayIsVisible();
}

// Verifies the cursor is hidden at startup on login screen.
IN_PROC_BROWSER_TEST_F(LoginCursorTest, CursorHidden) {
  // Login screen needs to be shown explicitly when running test.
  chromeos::ShowLoginWizard(chromeos::WizardController::kLoginScreenName);

  // Cursor should be hidden at startup
  EXPECT_FALSE(ash::Shell::GetInstance()->cursor_manager()->IsCursorVisible());

  // Cursor should be shown after cursor is moved.
  EXPECT_TRUE(ui_test_utils::SendMouseMoveSync(gfx::Point()));
  EXPECT_TRUE(ash::Shell::GetInstance()->cursor_manager()->IsCursorVisible());

  base::MessageLoop::current()->DeleteSoon(
      FROM_HERE, chromeos::LoginDisplayHostImpl::default_host());

  TestSystemTrayIsVisible();
}

// Verifies that the webui for login comes up successfully.
IN_PROC_BROWSER_TEST_F(LoginSigninTest, WebUIVisible) {
  base::TimeDelta no_timeout;
  EXPECT_TRUE(tracing::WaitForWatchEvent(no_timeout));
  std::string json_events;
  ASSERT_TRUE(tracing::EndTracing(&json_events));
}

IN_PROC_BROWSER_TEST_F(LoginOfflineTest, PRE_OldGaiaAuthOffline) {
  RegisterUser(kTestUser);
  chromeos::StartupUtils::MarkOobeCompleted();
  chromeos::CrosSettings::Get()->SetBoolean(
      chromeos::kAccountsPrefShowUserNamesOnSignIn, false);
}

IN_PROC_BROWSER_TEST_F(LoginOfflineTest, OldGaiaAuthOffline) {
  PrepareOfflineLogin();
  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  SubmitOldGaiaAuthOfflineForm(kTestUser, kPassword);
  session_start_waiter.Wait();

  TestSystemTrayIsVisible();
}

IN_PROC_BROWSER_TEST_F(LoginTest, PRE_GaiaAuthOffline) {
  RegisterUser(kTestUser);
  chromeos::StartupUtils::MarkOobeCompleted();
  chromeos::CrosSettings::Get()->SetBoolean(
      chromeos::kAccountsPrefShowUserNamesOnSignIn, false);
}

#if defined(MEMORY_SANITIZER)
#define MAYBE_GaiaAuthOffline DISABLED_GaiaAuthOffline
#else
#define MAYBE_GaiaAuthOffline GaiaAuthOffline
#endif
IN_PROC_BROWSER_TEST_F(LoginTest, MAYBE_GaiaAuthOffline) {
  PrepareOfflineLogin();
  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  SubmitGaiaAuthOfflineForm(kTestUser, kPassword);
  session_start_waiter.Wait();

  TestSystemTrayIsVisible();
}

}  // namespace
