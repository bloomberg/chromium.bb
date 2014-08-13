// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_state_delegate_chromeos.h"

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/policy/policy_cert_verifier.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/cert/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char* kUser = "user@test.com";

// Weak ptr to PolicyCertVerifier - object is freed in test destructor once
// we've ensured the profile has been shut down.
policy::PolicyCertVerifier* g_policy_cert_verifier_for_factory = NULL;

KeyedService* CreateTestPolicyCertService(content::BrowserContext* context) {
  return policy::PolicyCertService::CreateForTesting(
             kUser,
             g_policy_cert_verifier_for_factory,
             chromeos::UserManager::Get()).release();
}

}  // namespace

class SessionStateDelegateChromeOSTest : public testing::Test {
 protected:
  SessionStateDelegateChromeOSTest() : user_manager_(NULL) {
  }

  virtual ~SessionStateDelegateChromeOSTest() {
  }

  virtual void SetUp() OVERRIDE {
    // Initialize the UserManager singleton to a fresh FakeUserManager instance.
    user_manager_ = new chromeos::FakeUserManager;
    user_manager_enabler_.reset(
        new chromeos::ScopedUserManagerEnabler(user_manager_));

    // Create our SessionStateDelegate to experiment with.
    session_state_delegate_.reset(new SessionStateDelegateChromeos());
    testing::Test::SetUp();
  }

  virtual void TearDown() OVERRIDE {
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
    user_manager()->AddUser(user);
    user_manager()->LoginUser(user);
  }

  // Get the active user.
  const std::string& GetActiveUser() {
    return chromeos::UserManager::Get()->GetActiveUser()->email();
  }

  chromeos::FakeUserManager* user_manager() { return user_manager_; }
  SessionStateDelegateChromeos* session_state_delegate() {
    return session_state_delegate_.get();
  }

  void InitForMultiProfile() {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());

    const std::string user_email(kUser);
    const user_manager::User* user = user_manager()->AddUser(user_email);

    // Note that user profiles are created after user login in reality.
    user_profile_ = profile_manager_->CreateTestingProfile(user_email);
    user_profile_->set_profile_name(user_email);
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, user_profile_);
  }

  content::TestBrowserThreadBundle threads_;
  scoped_ptr<policy::PolicyCertVerifier> cert_verifier_;
  scoped_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* user_profile_;

 private:
  scoped_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;
  scoped_ptr<SessionStateDelegateChromeos> session_state_delegate_;

  // Not owned.
  chromeos::FakeUserManager* user_manager_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegateChromeOSTest);
};

// Make sure that cycling one user does not cause any harm.
TEST_F(SessionStateDelegateChromeOSTest, CyclingOneUser) {
  UserAddedToSession("firstuser@test.com");

  EXPECT_EQ("firstuser@test.com", GetActiveUser());
  session_state_delegate()->CycleActiveUser(
      ash::SessionStateDelegate::CYCLE_TO_NEXT_USER);
  EXPECT_EQ("firstuser@test.com", GetActiveUser());
  session_state_delegate()->CycleActiveUser(
      ash::SessionStateDelegate::CYCLE_TO_PREVIOUS_USER);
  EXPECT_EQ("firstuser@test.com", GetActiveUser());
}

// Cycle three users forwards and backwards to see that it works.
TEST_F(SessionStateDelegateChromeOSTest, CyclingThreeUsers) {
  UserAddedToSession("firstuser@test.com");
  UserAddedToSession("seconduser@test.com");
  UserAddedToSession("thirduser@test.com");
  const ash::SessionStateDelegate::CycleUser forward =
      ash::SessionStateDelegate::CYCLE_TO_NEXT_USER;

  // Cycle forward.
  EXPECT_EQ("firstuser@test.com", GetActiveUser());
  session_state_delegate()->CycleActiveUser(forward);
  EXPECT_EQ("seconduser@test.com", GetActiveUser());
  session_state_delegate()->CycleActiveUser(forward);
  EXPECT_EQ("thirduser@test.com", GetActiveUser());
  session_state_delegate()->CycleActiveUser(forward);
  EXPECT_EQ("firstuser@test.com", GetActiveUser());

  // Cycle backwards.
  const ash::SessionStateDelegate::CycleUser backward =
      ash::SessionStateDelegate::CYCLE_TO_PREVIOUS_USER;
  session_state_delegate()->CycleActiveUser(backward);
  EXPECT_EQ("thirduser@test.com", GetActiveUser());
  session_state_delegate()->CycleActiveUser(backward);
  EXPECT_EQ("seconduser@test.com", GetActiveUser());
  session_state_delegate()->CycleActiveUser(backward);
  EXPECT_EQ("firstuser@test.com", GetActiveUser());
}

// Make sure MultiProfile disabled by primary user policy.
TEST_F(SessionStateDelegateChromeOSTest, MultiProfileDisallowedByUserPolicy) {
  InitForMultiProfile();
  EXPECT_TRUE(
      session_state_delegate()->IsMultiProfileAllowedByPrimaryUserPolicy());
  const std::string user_email(kUser);
  user_manager()->LoginUser(user_email);
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
  const std::string user_email(kUser);
  user_manager()->LoginUser(user_email);
  EXPECT_TRUE(
      session_state_delegate()->IsMultiProfileAllowedByPrimaryUserPolicy());
  policy::PolicyCertServiceFactory::SetUsedPolicyCertificates(user_email);
  EXPECT_FALSE(
      session_state_delegate()->IsMultiProfileAllowedByPrimaryUserPolicy());

  // Flush tasks posted to IO.
  base::RunLoop().RunUntilIdle();
}

// Make sure MultiProfile disabled by primary user certificates in memory.
TEST_F(SessionStateDelegateChromeOSTest,
       MultiProfileDisallowedByPrimaryUserCertificatesInMemory) {
  InitForMultiProfile();
  const std::string user_email(kUser);
  user_manager()->LoginUser(user_email);
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
  certificates.push_back(new net::X509Certificate(
      "subject", "issuer", base::Time(), base::Time()));
  service->OnTrustAnchorsChanged(certificates);
  EXPECT_TRUE(service->has_policy_certificates());
  EXPECT_FALSE(
      session_state_delegate()->IsMultiProfileAllowedByPrimaryUserPolicy());

  // Flush tasks posted to IO.
  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
