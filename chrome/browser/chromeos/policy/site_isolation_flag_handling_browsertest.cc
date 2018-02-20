// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager_test_api.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"

namespace chromeos {
namespace {

struct Params {
  Params(bool login_screen_site_per_process,
         std::string login_screen_isolate_origins,
         bool user_policy_site_per_process,
         std::string user_policy_isolate_origins,
         bool expected_request_restart,
         std::vector<std::string> expected_flags_for_user)
      : login_screen_site_per_process(login_screen_site_per_process),
        login_screen_isolate_origins(login_screen_isolate_origins),
        user_policy_site_per_process(user_policy_site_per_process),
        user_policy_isolate_origins(user_policy_isolate_origins),
        expected_request_restart(expected_request_restart),
        expected_flags_for_user(expected_flags_for_user) {}

  friend std::ostream& operator<<(std::ostream& os, const Params& p) {
    os << "{" << std::endl
       << "  login_screen_site_per_process: " << p.login_screen_site_per_process
       << std::endl
       << "  login_screen_isolate_origins: " << p.login_screen_isolate_origins
       << std::endl
       << "  user_policy_site_per_process: " << p.user_policy_site_per_process
       << std::endl
       << "  user_policy_isolate_origins: " << p.user_policy_isolate_origins
       << std::endl
       << "  expected_request_restart: " << p.expected_request_restart
       << std::endl
       << "  expected_flags_for_user: "
       << base::JoinString(p.expected_flags_for_user, ", ") << std::endl
       << "}";
    return os;
  }

  // If true, --site-per-process will be passed to the login manager chrome
  // instance between policy flag sentinels.
  // Note: On Chrome OS, login_manager evaluates device policy and does this.
  bool login_screen_site_per_process;
  // If non-empty, --isolate-origins=|login_screen_isolate_origins| will be
  // passed to the login manager chrome instance between policy flag sentinels.
  // Note: On Chrome OS, login_manager evaluates device policy and does this.
  std::string login_screen_isolate_origins;

  // If true, the SitePerProcess user policy will be simulated to be set to
  // true.
  bool user_policy_site_per_process;
  // If non-empty, the IsolateOrigins user policy will be simulated to be set
  // |user_policy_isolate_origins|.
  std::string user_policy_isolate_origins;

  // If true, the test case will expect that AttemptRestart has been called by
  // UserSessionManager.
  bool expected_request_restart;
  // When a restart was requested, the test case verifies that the flags passed
  // to |SessionManagerClient::SetFlagsForUser| match
  // |expected_flags_for_user|.
  std::vector<std::string> expected_flags_for_user;
};

// Defines the test cases that will be executed.
const Params kTestCases[] = {
    // No site isolation in device or user policy - no restart expected.
    Params(false /* login_screen_site_per_process */,
           std::string() /* login_screen_isolate_origins */,
           false /* user_policy_site_per_process */,
           std::string() /* user_policy_isolate_origins */,
           false /* expected_request_restart */,
           {} /* expected_flags_for_user */),
    // SitePerProcess in user policy only - restart expected with
    // additional --site-per-process flag.
    Params(false /* login_screen_site_per_process */,
           std::string() /* login_screen_isolate_origins */,
           true /* user_policy_site_per_process */,
           std::string() /* user_policy_isolate_origins */,
           true /* expected_request_restart */,
           {"--policy-switches-begin", "--site-per-process",
            "--policy-switches-end"} /* expected_flags_for_user */),
    // SitePerProcess in device and user policy - no restart expected.
    Params(true /* login_screen_site_per_process */,
           std::string() /* login_screen_isolate_origins */,
           true /* user_policy_site_per_process */,
           std::string() /* user_policy_isolate_origins */,
           false /* expected_request_restart */,
           {} /* expected_flags_for_user */),
    // SitePerProcess only in device policy - restart expected.
    Params(true /* login_screen_site_per_process */,
           std::string() /* login_screen_isolate_origins */,
           false /* user_policy_site_per_process */,
           std::string() /* user_policy_isolate_origins */,
           true /* expected_request_restart */,
           {} /* expected_flags_for_user */),
    // IsolateOrigins in user policy only - restart expected with
    // additional --isolate-origins flag.
    Params(false /* login_screen_site_per_process */,
           std::string() /* login_screen_isolate_origins */,
           false /* user_policy_site_per_process */,
           "https://example.com" /* user_policy_isolate_origins */,
           true /* expected_request_restart */,
           {"--policy-switches-begin", "--isolate-origins=https://example.com",
            "--policy-switches-end"} /* expected_flags_for_user */),
    // Equal IsolateOrigins in device and user policy - no restart expected.
    Params(false /* login_screen_site_per_process */,
           "https://example.com" /* login_screen_isolate_origins */,
           false /* user_policy_site_per_process */,
           "https://example.com" /* user_policy_isolate_origins */,
           false /* expected_request_restart */,
           {} /* expected_flags_for_user */),
    // Different IsolateOrigins in device and user policy - restart expected.
    Params(false /* login_screen_site_per_process */,
           "https://example.com" /* login_screen_isolate_origins */,
           false /* user_policy_site_per_process */,
           "https://example2.com" /* user_policy_isolate_origins */,
           true /* expected_request_restart */,
           {"--policy-switches-begin", "--isolate-origins=https://example2.com",
            "--policy-switches-end"} /* expected_flags_for_user */),
    // IsolateOrigins only in device policy - restart expected.
    Params(true /* login_screen_site_per_process */,
           "https://example.com" /* login_screen_isolate_origins */,
           false /* user_policy_site_per_process */,
           std::string() /* user_policy_isolate_origins */,
           true /* expected_request_restart */,
           {} /* expected_flags_for_user */),
    // SitePerProcess in device policy, IsolateOrigins in user policy - restart
    // expected.
    Params(true /* login_screen_site_per_process */,
           std::string() /* login_screen_isolate_origins */,
           false /* user_policy_site_per_process */,
           "https://example.com" /* user_policy_isolate_origins */,
           true /* expected_request_restart */,
           {"--policy-switches-begin", "--isolate-origins=https://example.com",
            "--policy-switches-end"} /* expected_flags_for_user */)};

constexpr char kTestUser[] = "test-user@gmail.com";
constexpr char kTestUserGaiaId[] = "1111111111";

class SiteIsolationFlagHandlingTest
    : public LoginManagerTest,
      public ::testing::WithParamInterface<Params> {
 protected:
  SiteIsolationFlagHandlingTest()
      : LoginManagerTest(false /* should_launch_browser */) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);

    // Simulate login_manager behavior: pass --site-per-process or
    // --isolate-origins between policy flag sentinels according to test case
    // parameters.
    bool use_policy_switches_sentinels =
        GetParam().login_screen_site_per_process ||
        !GetParam().login_screen_isolate_origins.empty();
    if (use_policy_switches_sentinels)
      command_line->AppendSwitch(switches::kPolicySwitchesBegin);

    if (GetParam().login_screen_site_per_process)
      command_line->AppendSwitch(::switches::kSitePerProcess);
    if (!GetParam().login_screen_isolate_origins.empty()) {
      command_line->AppendSwitchASCII(::switches::kIsolateOrigins,
                                      GetParam().login_screen_isolate_origins);
    }

    if (use_policy_switches_sentinels)
      command_line->AppendSwitch(switches::kPolicySwitchesEnd);
  }

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();

    // Set up fake_session_manager_client_ so we can verify the flags for the
    // user session.
    auto fake_session_manager_client =
        std::make_unique<FakeSessionManagerClient>();
    fake_session_manager_client_ = fake_session_manager_client.get();
    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::move(fake_session_manager_client));

    // Mark that chrome restart can be requested.
    // Note that AttemptRestart() is mocked out in UserSessionManager through
    // |SetAttemptRestartClosureInTests| (set up in SetUpOnMainThread).
    fake_session_manager_client_->set_supports_restart_to_apply_user_flags(
        true);

    // Set up user policy with values given from test case parameters.
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));

    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    policy::PolicyMap values;
    if (GetParam().user_policy_site_per_process) {
      values.Set(policy::key::kSitePerProcess, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(true), nullptr);
    }
    if (!GetParam().user_policy_isolate_origins.empty()) {
      values.Set(
          policy::key::kIsolateOrigins, policy::POLICY_LEVEL_MANDATORY,
          policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(GetParam().user_policy_isolate_origins),
          nullptr);
    }
    provider_.UpdateChromePolicy(values);
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();

    login_wait_loop_ = std::make_unique<base::RunLoop>();

    // Mock out chrome restart.
    test::UserSessionManagerTestApi session_manager_test_api(
        UserSessionManager::GetInstance());
    session_manager_test_api.SetAttemptRestartClosureInTests(
        base::BindRepeating(
            &SiteIsolationFlagHandlingTest::AttemptRestartCalled,
            base::Unretained(this)));

    // Observe for user session start.
    user_session_started_observer_ =
        std::make_unique<content::WindowedNotificationObserver>(
            chrome::NOTIFICATION_SESSION_STARTED,
            base::BindRepeating(
                &SiteIsolationFlagHandlingTest::UserSessionStarted,
                base::Unretained(this)));
  }

  bool HasAttemptRestartBeenCalled() const { return attempt_restart_called_; }

  FakeSessionManagerClient* fake_session_manager_client() {
    return fake_session_manager_client_;
  }

  UserContext CreateUserContext(const AccountId& account_id) {
    UserContext user_context(account_id);
    user_context.SetKey(Key("password"));
    return user_context;
  }

  // Called when log-in was successful.
  bool UserSessionStarted(const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
    login_wait_loop_->Quit();
    return true;
  }

  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId(kTestUser, kTestUserGaiaId);

  // Used to wait until either login succeeds by starting a user session, or
  // chrome requests a restart.
  std::unique_ptr<base::RunLoop> login_wait_loop_;

 private:
  // Called when chrome requests a restarted.
  void AttemptRestartCalled() {
    login_wait_loop_->Quit();
    attempt_restart_called_ = true;
  }

  // This will be set to |true| when chrome has requested a restart.
  bool attempt_restart_called_ = false;

  // Observes for user session start.
  std::unique_ptr<content::WindowedNotificationObserver>
      user_session_started_observer_;
  // Unowned pointer - owned by DBusThreadManager.
  FakeSessionManagerClient* fake_session_manager_client_;
  policy::MockConfigurationPolicyProvider provider_;
  DISALLOW_COPY_AND_ASSIGN(SiteIsolationFlagHandlingTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_P(SiteIsolationFlagHandlingTest, PRE_FlagHandlingTest) {
  RegisterUser(test_account_id_);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_P(SiteIsolationFlagHandlingTest, FlagHandlingTest) {
  // Start user sign-in. We can't use |LoginManagerTest::LoginUser|, because
  // it waits for a user session start unconditionally, which will not happen if
  // chrome requests a restart to set user-session flags.
  ExistingUserController* controller =
      ExistingUserController::current_controller();
  ASSERT_TRUE(controller);
  controller->Login(CreateUserContext(test_account_id_), SigninSpecifics());

  // Wait for either the user session to start, or for restart to be requested
  // (whichever happens first).
  login_wait_loop_->Run();

  EXPECT_EQ(GetParam().expected_request_restart, HasAttemptRestartBeenCalled());

  if (!HasAttemptRestartBeenCalled())
    return;

  // Also verify flags if chrome was restart.
  std::vector<std::string> flags_for_user;
  bool has_flags_for_user = fake_session_manager_client()->GetFlagsForUser(
      cryptohome::Identification(test_account_id_), &flags_for_user);
  EXPECT_TRUE(has_flags_for_user);

  // Remove flag sentinels. Keep whatever is between those sentinels, to
  // verify that we don't pass additional parameters in there.
  base::EraseIf(flags_for_user, [](const std::string& flag) {
    return flag == "--flag-switches-begin" || flag == "--flag-switches-end";
  });
  EXPECT_EQ(GetParam().expected_flags_for_user, flags_for_user);
}

INSTANTIATE_TEST_CASE_P(,
                        SiteIsolationFlagHandlingTest,
                        ::testing::ValuesIn(kTestCases));

}  // namespace chromeos
