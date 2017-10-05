// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_auth_policy_client.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/test/rect_test_util.h"

using ::gfx::test::RectContains;

namespace chromeos {
namespace {

const char kPassword[] = "password";

constexpr char kAdOfflineAuthId[] = "offline-ad-auth";

constexpr char kDeviceId[] = "device_id";
constexpr char kTestActiveDirectoryUser[] = "test-user";
constexpr char kAdMachineInput[] = "machineNameInput";
constexpr char kAdUserInput[] = "userInput";
constexpr char kAdPasswordInput[] = "passwordInput";
constexpr char kAdButton[] = "button";
constexpr char kAdWelcomMessage[] = "welcomeMsg";
constexpr char kAdAutocompleteRealm[] = "userInput /deep/ #domainLabel";

class TestAuthPolicyClient : public FakeAuthPolicyClient {
 public:
  TestAuthPolicyClient() { FakeAuthPolicyClient::set_started(true); }

  void AuthenticateUser(const std::string& user_principal_name,
                        const std::string& object_guid,
                        int password_fd,
                        AuthCallback callback) override {
    authpolicy::ActiveDirectoryAccountInfo account_info;
    if (auth_error_ == authpolicy::ERROR_NONE) {
      if (object_guid.empty())
        account_info.set_account_id(base::MD5String(user_principal_name));
      else
        account_info.set_account_id(object_guid);
    }
    if (!auth_closure_.is_null()) {
      base::SequencedTaskRunnerHandle::Get()->PostNonNestableTask(
          FROM_HERE, std::move(auth_closure_));
    }
    base::SequencedTaskRunnerHandle::Get()->PostNonNestableTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), auth_error_, account_info));
  }

  void set_auth_closure(base::OnceClosure auth_closure) {
    auth_closure_ = std::move(auth_closure);
  }

 protected:
  // If set called before calling AuthCallback in AuthenticateUser.
  base::OnceClosure auth_closure_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAuthPolicyClient);
};

class ActiveDirectoryLoginTest : public LoginManagerTest {
 public:
  ActiveDirectoryLoginTest()
      : LoginManagerTest(true),
        // Using the same realm as supervised user domain. Should be treated as
        // normal realm.
        test_realm_(user_manager::kSupervisedUserDomain),
        install_attributes_(
            ScopedStubInstallAttributes::CreateActiveDirectoryManaged(
                test_realm_,
                kDeviceId)) {}

  ~ActiveDirectoryLoginTest() override = default;

  void SetUp() override {
    SetupTestAuthPolicyClient();
    LoginManagerTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
    LoginManagerTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    // Set the threshold to a max value to disable the offline message screen
    // on slow configurations like MSAN, where it otherwise triggers on every
    // run.
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->signin_screen_handler()
        ->SetOfflineTimeoutForTesting(base::TimeDelta::Max());
    fake_auth_policy_client()->DisableOperationDelayForTesting();
    LoginManagerTest::SetUpOnMainThread();
  }

  void MarkAsActiveDirectoryEnterprise() {
    StartupUtils::MarkOobeCompleted();
    base::RunLoop loop;
    fake_auth_policy_client()->RefreshDevicePolicy(
        base::BindOnce(&ActiveDirectoryLoginTest::OnRefreshedPolicy,
                       base::Unretained(this), loop.QuitClosure()));
    loop.Run();
  }

  void SetupTestAuthPolicyClient() {
    auto test_client = base::MakeUnique<TestAuthPolicyClient>();
    fake_auth_policy_client_ = test_client.get();
    DBusThreadManager::GetSetterForTesting()->SetAuthPolicyClient(
        std::move(test_client));
  }

  // Checks if Active Directory login is visible.
  void TestLoginVisible() {
    OobeScreenWaiter screen_waiter(OobeScreen::SCREEN_GAIA_SIGNIN);
    screen_waiter.Wait();
    // Checks if Gaia signin is hidden.
    JSExpect("document.querySelector('#signin-frame').hidden");

    // Checks if Active Directory signin is visible.
    JSExpect("!document.querySelector('#offline-ad-auth').hidden");
    JSExpect(JSElement(kAdOfflineAuthId, kAdMachineInput) + ".hidden");
    JSExpect("!" + JSElement(kAdOfflineAuthId, kAdUserInput) + ".hidden");
    JSExpect("!" + JSElement(kAdOfflineAuthId, kAdPasswordInput) + ".hidden");

    const std::string innerText(".innerText");
    // Checks if Active Directory welcome message contains realm.
    EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_AD_DOMAIN_AUTH_WELCOME_MESSAGE,
                                        base::UTF8ToUTF16(test_realm_)),
              js_checker().GetString(
                  JSElement(kAdOfflineAuthId, kAdWelcomMessage) + innerText));

    // Checks if realm is set to autocomplete username.
    EXPECT_EQ(
        "@" + test_realm_,
        js_checker().GetString(
            JSElement(kAdOfflineAuthId, kAdAutocompleteRealm) + innerText));

    // Checks if bottom bar is visible.
    JSExpect("!Oobe.getInstance().headerHidden");
  }

  // Checks if user input is marked as invalid.
  void TestUserError() {
    TestLoginVisible();
    JSExpect(JSElement(kAdOfflineAuthId, kAdUserInput) + ".isInvalid");
  }

  // Checks if password input is marked as invalid.
  void TestPasswordError() {
    TestLoginVisible();
    JSExpect(JSElement(kAdOfflineAuthId, kAdPasswordInput) + ".isInvalid");
  }

  // Checks that machine, password and user inputs are valid.
  void TestNoError() {
    TestLoginVisible();
    JSExpect("!" + JSElement(kAdOfflineAuthId, kAdMachineInput) + ".isInvalid");
    JSExpect("!" + JSElement(kAdOfflineAuthId, kAdUserInput) + ".isInvalid");
    JSExpect("!" + JSElement(kAdOfflineAuthId, kAdPasswordInput) +
             ".isInvalid");
  }

  // Checks if autocomplete domain is visible for the user input.
  void TestDomainVisible() {
    JSExpect("!" + JSElement(kAdOfflineAuthId, kAdAutocompleteRealm) +
             ".hidden");
  }

  // Checks if autocomplete domain is hidden for the user input.
  void TestDomainHidden() {
    JSExpect(JSElement(kAdOfflineAuthId, kAdAutocompleteRealm) + ".hidden");
  }

  // Sets username and password for the Active Directory login and submits it.
  void SubmitActiveDirectoryCredentials(const std::string& username,
                                        const std::string& password) {
    js_checker().ExecuteAsync(JSElement(kAdOfflineAuthId, kAdUserInput) +
                              ".value='" + username + "'");
    js_checker().ExecuteAsync(JSElement(kAdOfflineAuthId, kAdPasswordInput) +
                              ".value='" + password + "'");
    js_checker().Evaluate(JSElement(kAdOfflineAuthId, kAdButton) +
                          ".fire('tap')");
  }

  void SetupActiveDirectoryJSNotifications() {
    js_checker().Evaluate(
        "var testInvalidateAd = login.GaiaSigninScreen.invalidateAd;"
        "login.GaiaSigninScreen.invalidateAd = function(user, errorState) {"
        "  testInvalidateAd(user, errorState);"
        "  window.domAutomationController.send('ShowAuthError');"
        "}");
  }

  void WaitForMessage(content::DOMMessageQueue* message_queue,
                      const std::string& expected_message) {
    std::string message;
    do {
      ASSERT_TRUE(message_queue->WaitForMessage(&message));
    } while (message != expected_message);
  }

 protected:
  // Returns string representing element with id=|element_id| inside Active
  // Directory login element.
  std::string JSElement(const std::string& parent_id,
                        const std::string& element_id) {
    return "document.querySelector('#" + parent_id + " /deep/ #" + element_id +
           "')";
  }
  TestAuthPolicyClient* fake_auth_policy_client() {
    return fake_auth_policy_client_;
  }

  const std::string test_realm_;

 private:
  // Used for the callback from FakeAuthPolicy::RefreshDevicePolicy.
  void OnRefreshedPolicy(const base::Closure& closure, bool status) {
    EXPECT_TRUE(status);
    closure.Run();
  }

  ScopedStubInstallAttributes install_attributes_;
  TestAuthPolicyClient* fake_auth_policy_client_;

  DISALLOW_COPY_AND_ASSIGN(ActiveDirectoryLoginTest);
};

}  // namespace

// Marks as Active Directory enterprise device and OOBE as completed.
IN_PROC_BROWSER_TEST_F(ActiveDirectoryLoginTest, PRE_LoginSuccess) {
  MarkAsActiveDirectoryEnterprise();
}

// Test successful Active Directory login.
IN_PROC_BROWSER_TEST_F(ActiveDirectoryLoginTest, LoginSuccess) {
  TestNoError();
  TestDomainVisible();

  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  SubmitActiveDirectoryCredentials(kTestActiveDirectoryUser, kPassword);
  session_start_waiter.Wait();
}

// Marks as Active Directory enterprise device and OOBE as completed.
IN_PROC_BROWSER_TEST_F(ActiveDirectoryLoginTest, PRE_LoginErrors) {
  MarkAsActiveDirectoryEnterprise();
}

// Test different UI errors for Active Directory login.
IN_PROC_BROWSER_TEST_F(ActiveDirectoryLoginTest, LoginErrors) {
  SetupActiveDirectoryJSNotifications();
  TestNoError();
  TestDomainVisible();

  content::DOMMessageQueue message_queue;

  SubmitActiveDirectoryCredentials("", "");
  TestUserError();
  TestDomainVisible();

  SubmitActiveDirectoryCredentials(kTestActiveDirectoryUser, "");
  TestPasswordError();
  TestDomainVisible();

  SubmitActiveDirectoryCredentials(std::string(kTestActiveDirectoryUser) + "@",
                                   kPassword);
  TestUserError();
  TestDomainHidden();

  fake_auth_policy_client()->set_auth_error(authpolicy::ERROR_BAD_USER_NAME);
  SubmitActiveDirectoryCredentials(
      std::string(kTestActiveDirectoryUser) + "@" + test_realm_, kPassword);
  WaitForMessage(&message_queue, "\"ShowAuthError\"");
  TestUserError();
  TestDomainVisible();

  fake_auth_policy_client()->set_auth_error(authpolicy::ERROR_BAD_PASSWORD);
  SubmitActiveDirectoryCredentials(kTestActiveDirectoryUser, kPassword);
  WaitForMessage(&message_queue, "\"ShowAuthError\"");
  TestPasswordError();
  TestDomainVisible();

  fake_auth_policy_client()->set_auth_error(authpolicy::ERROR_UNKNOWN);
  SubmitActiveDirectoryCredentials(kTestActiveDirectoryUser, kPassword);
  WaitForMessage(&message_queue, "\"ShowAuthError\"");
  // Inputs are not invalidated for the unknown error.
  TestNoError();
  TestDomainVisible();
}

}  // namespace chromeos
