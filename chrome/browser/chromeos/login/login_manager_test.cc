// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_manager_test.h"

#include "base/command_line.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager_test_api.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

LoginManagerTest::LoginManagerTest(bool should_launch_browser)
    : use_webview_(true),
      should_launch_browser_(should_launch_browser),
      web_contents_(NULL) {
  set_exit_when_last_browser_closes(false);
}

void LoginManagerTest::TearDownOnMainThread() {
  MixinBasedBrowserTest::TearDownOnMainThread();
  if (LoginDisplayHostImpl::default_host())
    LoginDisplayHostImpl::default_host()->Finalize();
  base::MessageLoop::current()->RunUntilIdle();
}

void LoginManagerTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitch(chromeos::switches::kLoginManager);
  command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
  MixinBasedBrowserTest::SetUpCommandLine(command_line);
}

void LoginManagerTest::SetUpInProcessBrowserTestFixture() {
  MixinBasedBrowserTest::SetUpInProcessBrowserTestFixture();
}

void LoginManagerTest::SetUpOnMainThread() {
  MixinBasedBrowserTest::SetUpOnMainThread();
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  InitializeWebContents();
  test::UserSessionManagerTestApi session_manager_test_api(
      UserSessionManager::GetInstance());
  session_manager_test_api.SetShouldLaunchBrowserInTests(
      should_launch_browser_);
  session_manager_test_api.SetShouldObtainTokenHandleInTests(false);
}

bool LoginManagerTest::SetUpUserDataDirectory() {
  base::FilePath user_data_dir;
  CHECK(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  base::FilePath local_state_path =
      user_data_dir.Append(chrome::kLocalStateFilename);

  if (!use_webview()) {
    // Set webview disabled flag only when local state file does not exist.
    // Otherwise, we break PRE tests that leave state in it.
    if (!base::PathExists(local_state_path)) {
      base::DictionaryValue local_state_dict;

      // TODO(nkostylev): Fix tests that fail with webview signin.
      local_state_dict.SetBoolean(prefs::kWebviewSigninDisabled, true);

      CHECK(JSONFileValueSerializer(local_state_path)
                .Serialize(local_state_dict));
    }
  }

  return MixinBasedBrowserTest::SetUpUserDataDirectory();
}

void LoginManagerTest::RegisterUser(const std::string& user_id) {
  ListPrefUpdate users_pref(g_browser_process->local_state(), "LoggedInUsers");
  users_pref->AppendIfNotPresent(new base::StringValue(user_id));
}

void LoginManagerTest::SetExpectedCredentials(const UserContext& user_context) {
  test::UserSessionManagerTestApi session_manager_test_api(
      UserSessionManager::GetInstance());
  session_manager_test_api.InjectStubUserContext(user_context);
}

bool LoginManagerTest::TryToLogin(const UserContext& user_context) {
  if (!AddUserToSession(user_context))
    return false;
  if (const user_manager::User* active_user =
          user_manager::UserManager::Get()->GetActiveUser())
    return active_user->email() == user_context.GetUserID();
  return false;
}

bool LoginManagerTest::AddUserToSession(const UserContext& user_context) {
  ExistingUserController* controller =
      ExistingUserController::current_controller();
  if (!controller) {
    ADD_FAILURE();
    return false;
  }
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  controller->Login(user_context, SigninSpecifics());
  observer.Wait();
  const user_manager::UserList& logged_users =
      user_manager::UserManager::Get()->GetLoggedInUsers();
  for (user_manager::UserList::const_iterator it = logged_users.begin();
       it != logged_users.end();
       ++it) {
    if ((*it)->email() == user_context.GetUserID())
      return true;
  }
  return false;
}

void LoginManagerTest::LoginUser(const std::string& user_id) {
  UserContext user_context(user_id);
  user_context.SetGaiaID(GetGaiaIDForUserID(user_id));
  user_context.SetKey(Key("password"));
  SetExpectedCredentials(user_context);
  EXPECT_TRUE(TryToLogin(user_context));
}

void LoginManagerTest::AddUser(const std::string& user_id) {
  UserContext user_context(user_id);
  user_context.SetGaiaID(GetGaiaIDForUserID(user_id));
  user_context.SetKey(Key("password"));
  SetExpectedCredentials(user_context);
  EXPECT_TRUE(AddUserToSession(user_context));
}

// static
std::string LoginManagerTest::GetGaiaIDForUserID(const std::string& user_id) {
  return "gaia-id-" + user_id;
}

void LoginManagerTest::JSExpect(const std::string& expression) {
  js_checker_.ExpectTrue(expression);
}

void LoginManagerTest::InitializeWebContents() {
  LoginDisplayHost* host = LoginDisplayHostImpl::default_host();
  EXPECT_TRUE(host != NULL);

  content::WebContents* web_contents =
      host->GetWebUILoginView()->GetWebContents();
  EXPECT_TRUE(web_contents != NULL);
  set_web_contents(web_contents);
  js_checker_.set_web_contents(web_contents);
}

}  // namespace chromeos
