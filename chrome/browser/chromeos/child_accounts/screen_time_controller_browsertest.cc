// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/login_policy_test_base.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/child_accounts/child_account_test_utils.h"
#include "chrome/browser/chromeos/child_accounts/screen_time_controller.h"
#include "chrome/browser/chromeos/child_accounts/screen_time_controller_factory.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_test_utils.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/lock/screen_locker_tester.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {
namespace utils = time_limit_test_utils;

class ScreenTimeControllerTest : public policy::LoginPolicyTestBase {
 public:
  ScreenTimeControllerTest() {
    base::Time start_time = utils::TimeFromString("1 Jan 2018 10:00:00 GMT");
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        start_time, base::TimeTicks::UnixEpoch());
  }

  ~ScreenTimeControllerTest() override = default;

  // policy::LoginPolicyTestBase:
  void SetUp() override {
    // Recognize example.com (used by LoginPolicyTestBase) as non-enterprise
    // account.
    policy::BrowserPolicyConnector::SetNonEnterpriseDomainForTesting(
        "example.com");

    policy::LoginPolicyTestBase::SetUp();
  }

  void GetMandatoryPoliciesValue(base::DictionaryValue* policy) const override {
    // A basic starting policy.
    std::unique_ptr<base::DictionaryValue> policy_content =
        utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
    policy->SetKey("UsageTimeLimit",
                   base::Value(utils::PolicyToString(policy_content.get())));
  }

  std::string GetIdToken() const override {
    return test::GetChildAccountOAuthIdToken();
  }

 protected:
  void MockClockForActiveUser() {
    const user_manager::UserManager* const user_manager =
        user_manager::UserManager::Get();
    EXPECT_EQ(user_manager->GetActiveUser()->GetType(),
              user_manager::USER_TYPE_CHILD);
    child_profile_ =
        ProfileHelper::Get()->GetProfileByUser(user_manager->GetActiveUser());

    // Mock time for ScreenTimeController.
    ScreenTimeControllerFactory::GetForBrowserContext(child_profile_)
        ->SetClocksForTesting(task_runner_->GetMockClock(),
                              task_runner_->GetMockTickClock(), task_runner_);
  }

  bool IsAuthEnabled() {
    return ScreenLocker::default_screen_locker()->IsAuthEnabledForUser(
        AccountId::FromUserEmail(kAccountId));
  }

  void MockChildScreenTime(base::TimeDelta used_time) {
    Profile::FromBrowserContext(child_profile_)
        ->GetPrefs()
        ->SetInteger(prefs::kChildScreenTimeMilliseconds,
                     used_time.InMilliseconds());
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  Profile* child_profile_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenTimeControllerTest);
};

// Tests a simple lock override.
IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest, LockOverride) {
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, test::kChildAccountServiceFlags);
  MockClockForActiveUser();
  std::unique_ptr<chromeos::ScreenLockerTester> tester =
      chromeos::ScreenLockerTester::Create();
  tester->Lock();

  // Verify user is able to log in.
  EXPECT_TRUE(IsAuthEnabled());

  // Wait one hour.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
  EXPECT_TRUE(IsAuthEnabled());

  // Set new policy.
  std::unique_ptr<base::DictionaryValue> policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddOverride(policy_content.get(), utils::kLock, task_runner_->Now());

  auto policy = std::make_unique<base::DictionaryValue>();
  policy->SetKey("UsageTimeLimit",
                 base::Value(utils::PolicyToString(policy_content.get())));

  user_policy_helper()->UpdatePolicy(*policy, base::DictionaryValue(),
                                     child_profile_);

  EXPECT_FALSE(IsAuthEnabled());
}

// Tests the default time window limit.
IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest, DefaultBedtime) {
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, test::kChildAccountServiceFlags);
  MockClockForActiveUser();
  std::unique_ptr<chromeos::ScreenLockerTester> tester =
      chromeos::ScreenLockerTester::Create();
  tester->Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("GMT"));

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 GMT");
  std::unique_ptr<base::DictionaryValue> policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(policy_content.get(), utils::kMonday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(policy_content.get(), utils::kTuesday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(policy_content.get(), utils::kWednesday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(policy_content.get(), utils::kThursday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(policy_content.get(), utils::kFriday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(policy_content.get(), utils::kSaturday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(policy_content.get(), utils::kSunday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);

  auto policy = std::make_unique<base::DictionaryValue>();
  policy->SetKey("UsageTimeLimit",
                 base::Value(utils::PolicyToString(policy_content.get())));

  user_policy_helper()->UpdatePolicy(*policy, base::DictionaryValue(),
                                     child_profile_);

  // Iterate over a week checking that the device is locked properly everyday.
  for (int i = 0; i < 7; i++) {
    // Verify that auth is enabled at 10 AM.
    EXPECT_TRUE(IsAuthEnabled());

    // Verify that auth is enabled at 8 PM.
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(10));
    EXPECT_TRUE(IsAuthEnabled());

    // Verify that the auth was disabled at 9 PM (start of bedtime).
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
    EXPECT_FALSE(IsAuthEnabled());

    // Forward to 7 AM and check that auth was re-enabled (end of bedtime).
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(10));
    EXPECT_TRUE(IsAuthEnabled());

    // Forward to 10 AM.
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(3));
  }
}

// Tests the default time window limit.
IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest, DefaultDailyLimit) {
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, test::kChildAccountServiceFlags);
  MockClockForActiveUser();
  std::unique_ptr<chromeos::ScreenLockerTester> tester =
      chromeos::ScreenLockerTester::Create();
  tester->Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("GMT"));

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 GMT");
  std::unique_ptr<base::DictionaryValue> policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(policy_content.get(), utils::kMonday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(policy_content.get(), utils::kTuesday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(policy_content.get(), utils::kWednesday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(policy_content.get(), utils::kThursday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(policy_content.get(), utils::kFriday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(policy_content.get(), utils::kSaturday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(policy_content.get(), utils::kSunday,
                           base::TimeDelta::FromHours(3), last_updated);

  auto policy = std::make_unique<base::DictionaryValue>();
  policy->SetKey("UsageTimeLimit",
                 base::Value(utils::PolicyToString(policy_content.get())));

  user_policy_helper()->UpdatePolicy(*policy, base::DictionaryValue(),
                                     child_profile_);

  // Iterate over a week checking that the device is locked properly
  // every day.
  for (int i = 0; i < 7; i++) {
    // Check that auth is enabled at 10 AM with 0 usage time.
    EXPECT_TRUE(IsAuthEnabled());

    // Check that auth is enabled after forwarding to 1 PM and using the device
    // for 2 hours.
    MockChildScreenTime(base::TimeDelta::FromHours(2));
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(3));
    EXPECT_TRUE(IsAuthEnabled());

    // Check that auth is enabled after forwarding to 2 PM with no extra usage.
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
    EXPECT_TRUE(IsAuthEnabled());

    // Check that auth is disabled after forwarding to 3 PM and using the device
    // for 3 hours.
    MockChildScreenTime(base::TimeDelta::FromHours(3));
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
    EXPECT_FALSE(IsAuthEnabled());

    // Forward to 6 AM, reset the usage time and check that auth was re-enabled.
    MockChildScreenTime(base::TimeDelta::FromHours(0));
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(15));
    EXPECT_TRUE(IsAuthEnabled());

    // Forward to 10 AM.
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(4));
  }
}

}  // namespace chromeos
