// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_base.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/signin/core/browser/account_tracker_service.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/user.h"
#endif

namespace ash {
namespace test {

namespace {

const char kTestGaiaId[] = "gaia_id";
const char kTestAccountId[] = "test@test.com";

class MultiUserTestingProfile : public TestingProfile {
 public:
  explicit MultiUserTestingProfile(TestingProfile* profile)
      : profile_(profile) {}
  ~MultiUserTestingProfile() override {}

  Profile* GetOriginalProfile() override { return this; }

  std::string GetProfileUserName() const override {
    const SigninManagerBase* signin_manager =
        SigninManagerFactory::GetForProfileIfExists(profile_.get());
    if (signin_manager)
      return signin_manager->GetAuthenticatedAccountInfo().email;

    return std::string();
  }

  TestingProfile* profile() { return profile_.get(); }

 private:
  std::unique_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(MultiUserTestingProfile);
};

}  // namespace

class MultiUserUtilTest : public AshTestBase {
 public:
  MultiUserUtilTest() {}
  ~MultiUserUtilTest() override{};

  void SetUp() override {
    AshTestBase::SetUp();

#if defined(OS_CHROMEOS)
    fake_user_manager_ = new user_manager::FakeUserManager;
    user_manager_enabler_.reset(
        new chromeos::ScopedUserManagerEnabler(fake_user_manager_));
#endif

    TestingProfile::Builder builder;
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              BuildFakeSigninManagerBase);
    TestingProfile* profile = builder.Build().release();
    profile_.reset(new MultiUserTestingProfile(profile));
  }

  void TearDown() override {
    profile_.reset();
    AshTestBase::TearDown();
  }

  // Add a user to account tracker service with given gaia_id and email.
  std::string AddUserToAccountTracker(const std::string& gaia_id,
                                      const std::string& email) {
    AccountTrackerService* account_tracker_service =
        AccountTrackerServiceFactory::GetForProfile(profile_->profile());
    FakeSigninManagerBase* signin_manager = static_cast<FakeSigninManagerBase*>(
        SigninManagerFactory::GetForProfile(profile_->profile()));
    account_tracker_service->SeedAccountInfo(gaia_id, email);
    const std::string id =
        account_tracker_service->PickAccountIdForAccount(gaia_id, email);
    signin_manager->SignIn(id);

#if defined(OS_CHROMEOS)
    fake_user_manager_->AddUser(multi_user_util::GetAccountIdFromEmail(id));
    fake_user_manager_->UserLoggedIn(
        multi_user_util::GetAccountIdFromEmail(id),
        chromeos::ProfileHelper::GetUserIdHashByUserIdForTesting(id),
        false /* browser_restart */);
#endif

    return id;
  }

  void SimulateTokenRevoked(const std::string& account_id) {
    AccountTrackerService* account_tracker_service =
        AccountTrackerServiceFactory::GetForProfile(profile_->profile());
    account_tracker_service->RemoveAccount(account_id);
  }

  MultiUserTestingProfile* profile() { return profile_.get(); }

 private:
  std::unique_ptr<MultiUserTestingProfile> profile_;
#if defined(OS_CHROMEOS)
  user_manager::FakeUserManager* fake_user_manager_;  // Not owned.
  std::unique_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;
#endif

  DISALLOW_COPY_AND_ASSIGN(MultiUserUtilTest);
};

#if defined(OS_CHROMEOS)
// Test that during the session it will always return a valid account id if a
// valid profile is provided, even if this profile's refresh token has been
// revoked. (On Chrome OS we don't force to end the session in this case.)
TEST_F(MultiUserUtilTest, ReturnValidAccountIdIfTokenRevoked) {
  std::string id = AddUserToAccountTracker(kTestGaiaId, kTestAccountId);
  EXPECT_EQ(kTestAccountId, profile()->GetProfileUserName());
  EXPECT_EQ(kTestAccountId,
            multi_user_util::GetAccountIdFromProfile(profile()).GetUserEmail());

  SimulateTokenRevoked(id);
  EXPECT_EQ(std::string(), profile()->GetProfileUserName());
  EXPECT_EQ(kTestAccountId,
            multi_user_util::GetAccountIdFromProfile(profile()).GetUserEmail());
}
#endif

}  // namespace test
}  // namespace ash
