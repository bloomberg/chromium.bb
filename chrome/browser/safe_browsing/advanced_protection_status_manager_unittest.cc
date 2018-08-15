// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"

#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

class AdvancedProtectionStatusManagerTest : public testing::Test {
 public:
  AdvancedProtectionStatusManagerTest() {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              BuildFakeSigninManagerBase);
    testing_profile_.reset(builder.Build().release());
    fake_signin_manager_ = static_cast<FakeSigninManagerForTesting*>(
        SigninManagerFactory::GetForProfile(testing_profile_.get()));
    account_tracker_service_ =
        AccountTrackerServiceFactory::GetForProfile(testing_profile_.get());
  }

  ~AdvancedProtectionStatusManagerTest() override {}

#if !defined(OS_CHROMEOS)
  std::string SignIn(const std::string& gaia_id,
                     const std::string& email,
                     bool is_under_advanced_protection) {
    AccountInfo account_info;
    account_info.gaia = gaia_id;
    account_info.email = email;
    account_info.is_under_advanced_protection = is_under_advanced_protection;
    std::string account_id =
        account_tracker_service_->SeedAccountInfo(account_info);
    fake_signin_manager_->SignIn(gaia_id, email, "password");
    return account_id;
  }
#endif

 protected:
  content::TestBrowserThreadBundle thread_bundle;
  std::unique_ptr<TestingProfile> testing_profile_;
  FakeSigninManagerForTesting* fake_signin_manager_;
  AccountTrackerService* account_tracker_service_;
};

}  // namespace

#if !defined(OS_CHROMEOS)
TEST_F(AdvancedProtectionStatusManagerTest, SignInAndSignOutEvent) {
  AdvancedProtectionStatusManager aps_manager(testing_profile_.get());
  ASSERT_FALSE(aps_manager.is_under_advanced_protection());

  SignIn("gaia_id", "email", /* is_under_advanced_protection = */ true);
  EXPECT_TRUE(aps_manager.is_under_advanced_protection());

  fake_signin_manager_->ForceSignOut();
  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest, AccountRemoval) {
  AdvancedProtectionStatusManager aps_manager(testing_profile_.get());
  ASSERT_FALSE(aps_manager.is_under_advanced_protection());

  std::string account_id =
      SignIn("gaia_id", "email", /* is_under_advanced_protection = */ false);
  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  account_tracker_service_->SetIsAdvancedProtectionAccount(
      account_id, /* is_under_advanced_protection= */ true);
  EXPECT_TRUE(aps_manager.is_under_advanced_protection());

  account_tracker_service_->RemoveAccount(account_id);
  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  aps_manager.UnsubscribeFromSigninEvents();
}
#endif
}  // namespace safe_browsing
