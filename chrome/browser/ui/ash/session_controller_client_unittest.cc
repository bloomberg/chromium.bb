// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_controller_client.h"

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/policy/policy_cert_verifier.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::FakeChromeUserManager;

namespace {

const char* kUser = "user@test.com";

// Weak ptr to PolicyCertVerifier - object is freed in test destructor once
// we've ensured the profile has been shut down.
policy::PolicyCertVerifier* g_policy_cert_verifier_for_factory = nullptr;

std::unique_ptr<KeyedService> CreateTestPolicyCertService(
    content::BrowserContext* context) {
  return policy::PolicyCertService::CreateForTesting(
      kUser, g_policy_cert_verifier_for_factory,
      user_manager::UserManager::Get());
}

}  // namespace

class SessionControllerClientTest : public testing::Test {
 protected:
  SessionControllerClientTest() {}
  ~SessionControllerClientTest() override {}

  void SetUp() override {
    // Initialize the UserManager singleton to a fresh FakeChromeUserManager
    // instance.
    user_manager_ = new FakeChromeUserManager;
    user_manager_enabler_.reset(
        new chromeos::ScopedUserManagerEnabler(user_manager_));

    testing::Test::SetUp();
  }

  void TearDown() override {
    testing::Test::TearDown();
    user_manager_enabler_.reset();
    user_manager_ = nullptr;
    // Clear our cached pointer to the PolicyCertVerifier.
    g_policy_cert_verifier_for_factory = nullptr;
    profile_manager_.reset();

    // We must ensure that the PolicyCertVerifier outlives the
    // PolicyCertService so shutdown the profile here. Additionally, we need
    // to run the message loop between freeing the PolicyCertService and
    // freeing the PolicyCertVerifier (see
    // PolicyCertService::OnTrustAnchorsChanged() which is called from
    // PolicyCertService::Shutdown()).
    base::RunLoop().RunUntilIdle();
  }

  // Add and log in a user to the session.
  void UserAddedToSession(std::string user) {
    user_manager()->AddUser(AccountId::FromUserEmail(user));
    user_manager()->LoginUser(AccountId::FromUserEmail(user));
  }

  // Get the active user.
  const std::string& GetActiveUserEmail() {
    return user_manager::UserManager::Get()
        ->GetActiveUser()
        ->GetAccountId()
        .GetUserEmail();
  }

  FakeChromeUserManager* user_manager() { return user_manager_; }

  void InitForMultiProfile() {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());

    const AccountId account_id(AccountId::FromUserEmail(kUser));
    const user_manager::User* user = user_manager()->AddUser(account_id);

    // Note that user profiles are created after user login in reality.
    user_profile_ =
        profile_manager_->CreateTestingProfile(account_id.GetUserEmail());
    user_profile_->set_profile_name(account_id.GetUserEmail());
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, user_profile_);
  }

  content::TestBrowserThreadBundle threads_;
  std::unique_ptr<policy::PolicyCertVerifier> cert_verifier_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* user_profile_;

 private:
  std::unique_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;

  // Owned by |user_manager_enabler_|.
  FakeChromeUserManager* user_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SessionControllerClientTest);
};

// Make sure that cycling one user does not cause any harm.
TEST_F(SessionControllerClientTest, CyclingOneUser) {
  UserAddedToSession("firstuser@test.com");

  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());
  SessionControllerClient::DoCycleActiveUser(ash::CycleUserDirection::NEXT);
  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());
  SessionControllerClient::DoCycleActiveUser(ash::CycleUserDirection::PREVIOUS);
  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());
}

// Cycle three users forwards and backwards to see that it works.
TEST_F(SessionControllerClientTest, CyclingThreeUsers) {
  UserAddedToSession("firstuser@test.com");
  UserAddedToSession("seconduser@test.com");
  UserAddedToSession("thirduser@test.com");
  const ash::CycleUserDirection forward = ash::CycleUserDirection::NEXT;

  // Cycle forward.
  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());
  SessionControllerClient::DoCycleActiveUser(forward);
  EXPECT_EQ("seconduser@test.com", GetActiveUserEmail());
  SessionControllerClient::DoCycleActiveUser(forward);
  EXPECT_EQ("thirduser@test.com", GetActiveUserEmail());
  SessionControllerClient::DoCycleActiveUser(forward);
  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());

  // Cycle backwards.
  const ash::CycleUserDirection backward = ash::CycleUserDirection::PREVIOUS;
  SessionControllerClient::DoCycleActiveUser(backward);
  EXPECT_EQ("thirduser@test.com", GetActiveUserEmail());
  SessionControllerClient::DoCycleActiveUser(backward);
  EXPECT_EQ("seconduser@test.com", GetActiveUserEmail());
  SessionControllerClient::DoCycleActiveUser(backward);
  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());
}

// Make sure MultiProfile disabled by primary user policy.
TEST_F(SessionControllerClientTest, MultiProfileDisallowedByUserPolicy) {
  InitForMultiProfile();
  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClient::GetAddUserSessionPolicy());
  const AccountId account_id(AccountId::FromUserEmail(kUser));
  user_manager()->LoginUser(account_id);
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS,
            SessionControllerClient::GetAddUserSessionPolicy());

  user_manager()->AddUser(AccountId::FromUserEmail("bb@b.b"));
  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClient::GetAddUserSessionPolicy());

  user_profile_->GetPrefs()->SetString(
      prefs::kMultiProfileUserBehavior,
      chromeos::MultiProfileUserController::kBehaviorNotAllowed);
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER,
            SessionControllerClient::GetAddUserSessionPolicy());
}

// Make sure MultiProfile disabled by primary user policy certificates.
TEST_F(SessionControllerClientTest,
       MultiProfileDisallowedByPolicyCertificates) {
  InitForMultiProfile();
  user_manager()->AddUser(AccountId::FromUserEmail("bb@b.b"));

  const AccountId account_id(AccountId::FromUserEmail(kUser));
  user_manager()->LoginUser(account_id);
  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClient::GetAddUserSessionPolicy());
  policy::PolicyCertServiceFactory::SetUsedPolicyCertificates(
      account_id.GetUserEmail());
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER,
            SessionControllerClient::GetAddUserSessionPolicy());

  // Flush tasks posted to IO.
  base::RunLoop().RunUntilIdle();
}

// Make sure MultiProfile disabled by primary user certificates in memory.
TEST_F(SessionControllerClientTest,
       MultiProfileDisallowedByPrimaryUserCertificatesInMemory) {
  InitForMultiProfile();
  user_manager()->AddUser(AccountId::FromUserEmail("bb@b.b"));

  const AccountId account_id(AccountId::FromUserEmail(kUser));
  user_manager()->LoginUser(account_id);
  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClient::GetAddUserSessionPolicy());
  cert_verifier_.reset(new policy::PolicyCertVerifier(base::Closure()));
  g_policy_cert_verifier_for_factory = cert_verifier_.get();
  ASSERT_TRUE(
      policy::PolicyCertServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          user_profile_, CreateTestPolicyCertService));
  policy::PolicyCertService* service =
      policy::PolicyCertServiceFactory::GetForProfile(user_profile_);
  ASSERT_TRUE(service);

  EXPECT_FALSE(service->has_policy_certificates());
  net::CertificateList certificates;
  certificates.push_back(
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem"));
  service->OnTrustAnchorsChanged(certificates);
  EXPECT_TRUE(service->has_policy_certificates());
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER,
            SessionControllerClient::GetAddUserSessionPolicy());

  // Flush tasks posted to IO.
  base::RunLoop().RunUntilIdle();
}

// Make sure adding users to multiprofiles disabled by reaching maximum
// number of users in sessions.
TEST_F(SessionControllerClientTest,
       AddUserToMultiprofileDisallowedByMaximumUsers) {
  InitForMultiProfile();

  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClient::GetAddUserSessionPolicy());
  const AccountId account_id(AccountId::FromUserEmail(kUser));
  user_manager()->LoginUser(account_id);
  while (user_manager()->GetLoggedInUsers().size() <
         session_manager::kMaxmiumNumberOfUserSessions) {
    UserAddedToSession("bb@b.b");
  }
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED,
            SessionControllerClient::GetAddUserSessionPolicy());
}

// Make sure adding users to multiprofiles disabled by logging in all possible
// users.
TEST_F(SessionControllerClientTest,
       AddUserToMultiprofileDisallowedByAllUsersLogged) {
  InitForMultiProfile();

  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClient::GetAddUserSessionPolicy());
  const AccountId account_id(AccountId::FromUserEmail(kUser));
  user_manager()->LoginUser(account_id);
  UserAddedToSession("bb@b.b");
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS,
            SessionControllerClient::GetAddUserSessionPolicy());
}

// Make sure adding users to multiprofiles disabled by primary user policy.
TEST_F(SessionControllerClientTest,
       AddUserToMultiprofileDisallowedByPrimaryUserPolicy) {
  InitForMultiProfile();

  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClient::GetAddUserSessionPolicy());
  const AccountId account_id(AccountId::FromUserEmail(kUser));
  user_manager()->LoginUser(account_id);
  user_profile_->GetPrefs()->SetString(
      prefs::kMultiProfileUserBehavior,
      chromeos::MultiProfileUserController::kBehaviorNotAllowed);
  user_manager()->AddUser(AccountId::FromUserEmail("bb@b.b"));
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER,
            SessionControllerClient::GetAddUserSessionPolicy());
}
