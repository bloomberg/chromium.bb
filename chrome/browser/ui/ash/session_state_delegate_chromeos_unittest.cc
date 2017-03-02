// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_state_delegate_chromeos.h"

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

namespace chromeos {

namespace {

const char* kUser = "user@test.com";

// Weak ptr to PolicyCertVerifier - object is freed in test destructor once
// we've ensured the profile has been shut down.
policy::PolicyCertVerifier* g_policy_cert_verifier_for_factory = NULL;

std::unique_ptr<KeyedService> CreateTestPolicyCertService(
    content::BrowserContext* context) {
  return policy::PolicyCertService::CreateForTesting(
      kUser, g_policy_cert_verifier_for_factory,
      user_manager::UserManager::Get());
}

}  // namespace

class SessionStateDelegateChromeOSTest : public testing::Test {
 protected:
  SessionStateDelegateChromeOSTest() : user_manager_(NULL) {
  }

  ~SessionStateDelegateChromeOSTest() override {}

  void SetUp() override {
    // Initialize the UserManager singleton to a fresh FakeChromeUserManager
    // instance.
    user_manager_ = new FakeChromeUserManager;
    user_manager_enabler_.reset(
        new chromeos::ScopedUserManagerEnabler(user_manager_));

    // Create our SessionStateDelegate to experiment with.
    session_state_delegate_.reset(new SessionStateDelegateChromeos());
    testing::Test::SetUp();
  }

  void TearDown() override {
    testing::Test::TearDown();
    session_state_delegate_.reset();
    user_manager_enabler_.reset();
    user_manager_ = NULL;
    // Clear our cached pointer to the PolicyCertVerifier.
    g_policy_cert_verifier_for_factory = NULL;
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
  SessionStateDelegateChromeos* session_state_delegate() {
    return session_state_delegate_.get();
  }

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
  std::unique_ptr<SessionStateDelegateChromeos> session_state_delegate_;

  // Not owned.
  FakeChromeUserManager* user_manager_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegateChromeOSTest);
};

// Make sure that cycling one user does not cause any harm.
TEST_F(SessionStateDelegateChromeOSTest, CyclingOneUser) {
  UserAddedToSession("firstuser@test.com");

  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());
  session_state_delegate()->CycleActiveUser(ash::CycleUserDirection::NEXT);
  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());
  session_state_delegate()->CycleActiveUser(ash::CycleUserDirection::PREVIOUS);
  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());
}

// Cycle three users forwards and backwards to see that it works.
TEST_F(SessionStateDelegateChromeOSTest, CyclingThreeUsers) {
  UserAddedToSession("firstuser@test.com");
  UserAddedToSession("seconduser@test.com");
  UserAddedToSession("thirduser@test.com");
  const ash::CycleUserDirection forward = ash::CycleUserDirection::NEXT;

  // Cycle forward.
  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());
  session_state_delegate()->CycleActiveUser(forward);
  EXPECT_EQ("seconduser@test.com", GetActiveUserEmail());
  session_state_delegate()->CycleActiveUser(forward);
  EXPECT_EQ("thirduser@test.com", GetActiveUserEmail());
  session_state_delegate()->CycleActiveUser(forward);
  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());

  // Cycle backwards.
  const ash::CycleUserDirection backward = ash::CycleUserDirection::PREVIOUS;
  session_state_delegate()->CycleActiveUser(backward);
  EXPECT_EQ("thirduser@test.com", GetActiveUserEmail());
  session_state_delegate()->CycleActiveUser(backward);
  EXPECT_EQ("seconduser@test.com", GetActiveUserEmail());
  session_state_delegate()->CycleActiveUser(backward);
  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());
}

// Make sure MultiProfile disabled by primary user policy.
TEST_F(SessionStateDelegateChromeOSTest, MultiProfileDisallowedByUserPolicy) {
  InitForMultiProfile();
  EXPECT_TRUE(
      session_state_delegate()->IsMultiProfileAllowedByPrimaryUserPolicy());
  const AccountId account_id(AccountId::FromUserEmail(kUser));
  user_manager()->LoginUser(account_id);
  EXPECT_TRUE(
      session_state_delegate()->IsMultiProfileAllowedByPrimaryUserPolicy());

  user_profile_->GetPrefs()->SetString(
      prefs::kMultiProfileUserBehavior,
      chromeos::MultiProfileUserController::kBehaviorNotAllowed);
  EXPECT_FALSE(
      session_state_delegate()->IsMultiProfileAllowedByPrimaryUserPolicy());
}

// Make sure MultiProfile disabled by primary user policy certificates.
TEST_F(SessionStateDelegateChromeOSTest,
       MultiProfileDisallowedByPolicyCertificates) {
  InitForMultiProfile();
  const AccountId account_id(AccountId::FromUserEmail(kUser));
  user_manager()->LoginUser(account_id);
  EXPECT_TRUE(
      session_state_delegate()->IsMultiProfileAllowedByPrimaryUserPolicy());
  policy::PolicyCertServiceFactory::SetUsedPolicyCertificates(
      account_id.GetUserEmail());
  EXPECT_FALSE(
      session_state_delegate()->IsMultiProfileAllowedByPrimaryUserPolicy());

  // Flush tasks posted to IO.
  base::RunLoop().RunUntilIdle();
}

// Make sure MultiProfile disabled by primary user certificates in memory.
TEST_F(SessionStateDelegateChromeOSTest,
       MultiProfileDisallowedByPrimaryUserCertificatesInMemory) {
  InitForMultiProfile();
  const AccountId account_id(AccountId::FromUserEmail(kUser));
  user_manager()->LoginUser(account_id);
  EXPECT_TRUE(
      session_state_delegate()->IsMultiProfileAllowedByPrimaryUserPolicy());
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
  EXPECT_FALSE(
      session_state_delegate()->IsMultiProfileAllowedByPrimaryUserPolicy());

  // Flush tasks posted to IO.
  base::RunLoop().RunUntilIdle();
}

// Make sure adding users to multiprofiles disabled by reaching maximum
// number of users in sessions.
TEST_F(SessionStateDelegateChromeOSTest,
       AddUserToMultiprofileDisallowedByMaximumUsers) {
  InitForMultiProfile();

  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            session_state_delegate()->GetAddUserSessionPolicy());
  const AccountId account_id(AccountId::FromUserEmail(kUser));
  user_manager()->LoginUser(account_id);
  while (session_state_delegate()->NumberOfLoggedInUsers() <
         session_state_delegate()->GetMaximumNumberOfLoggedInUsers()) {
    UserAddedToSession("bb@b.b");
  }
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED,
            session_state_delegate()->GetAddUserSessionPolicy());
}

// Make sure adding users to multiprofiles disabled by logging in all possible
// users.
TEST_F(SessionStateDelegateChromeOSTest,
       AddUserToMultiprofileDisallowedByAllUsersLogged) {
  InitForMultiProfile();

  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            session_state_delegate()->GetAddUserSessionPolicy());
  const AccountId account_id(AccountId::FromUserEmail(kUser));
  user_manager()->LoginUser(account_id);
  UserAddedToSession("bb@b.b");
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS,
            session_state_delegate()->GetAddUserSessionPolicy());
}

// Make sure adding users to multiprofiles disabled by primary user policy.
TEST_F(SessionStateDelegateChromeOSTest,
       AddUserToMultiprofileDisallowedByPrimaryUserPolicy) {
  InitForMultiProfile();

  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            session_state_delegate()->GetAddUserSessionPolicy());
  const AccountId account_id(AccountId::FromUserEmail(kUser));
  user_manager()->LoginUser(account_id);
  user_profile_->GetPrefs()->SetString(
      prefs::kMultiProfileUserBehavior,
      chromeos::MultiProfileUserController::kBehaviorNotAllowed);
  user_manager()->AddUser(AccountId::FromUserEmail("bb@b.b"));
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER,
            session_state_delegate()->GetAddUserSessionPolicy());
}

}  // namespace chromeos
