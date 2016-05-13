// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/policy/affiliation_test_helper.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char kAffiliatedUser[] = "affiliated-user@example.com";
const char kAffiliationID[] = "some-affiliation-id";
const char kAnotherAffiliationID[] = "another-affiliation-id";
struct Params {
  explicit Params(bool affiliated) : affiliated_(affiliated) {}
  bool affiliated_;
};

}  // namespace

class UserAffiliationBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<Params> {
 public:
  UserAffiliationBrowserTest() { set_exit_when_last_browser_closes(false); }

 protected:
  // InProcessBrowserTest
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    affiliation_test_helper::AppendCommandLineSwitchesForLoginManager(
        command_line);
  }

  // InProcessBrowserTest
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    chromeos::FakeSessionManagerClient* fake_session_manager_client =
        new chromeos::FakeSessionManagerClient;
    chromeos::DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        base::WrapUnique<chromeos::SessionManagerClient>(
            fake_session_manager_client));

    UserPolicyBuilder user_policy;
    DevicePolicyCrosTestHelper test_helper;

    std::set<std::string> device_affiliation_ids;
    device_affiliation_ids.insert(kAffiliationID);
    affiliation_test_helper::SetDeviceAffiliationID(
        &test_helper, fake_session_manager_client, device_affiliation_ids);

    std::set<std::string> user_affiliation_ids;
    if (GetParam().affiliated_) {
      user_affiliation_ids.insert(kAffiliationID);
    } else {
      user_affiliation_ids.insert(kAnotherAffiliationID);
    }
    affiliation_test_helper::SetUserAffiliationIDs(
        &user_policy, fake_session_manager_client, kAffiliatedUser,
        user_affiliation_ids);

    // Set retry delay to prevent timeouts.
    policy::DeviceManagementService::SetRetryDelayForTesting(0);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UserAffiliationBrowserTest);
};

IN_PROC_BROWSER_TEST_P(UserAffiliationBrowserTest, PRE_Affiliated) {
  affiliation_test_helper::PreLoginUser(kAffiliatedUser);
}

IN_PROC_BROWSER_TEST_P(UserAffiliationBrowserTest, Affiliated) {
  affiliation_test_helper::LoginUser(kAffiliatedUser);
  EXPECT_EQ(GetParam().affiliated_,
            user_manager::UserManager::Get()
                ->FindUser(AccountId::FromUserEmail(kAffiliatedUser))
                ->IsAffiliated());
}

INSTANTIATE_TEST_CASE_P(AffiliationCheck,
                        UserAffiliationBrowserTest,
                        ::testing::Values(Params(true), Params(false)));

}  // namespace policy
