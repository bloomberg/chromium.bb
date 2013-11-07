// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/signin/account_reconcilor.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if defined(OS_CHROMEOS)
typedef FakeSigninManagerBase FakeSigninManagerForTesting;
#else
typedef FakeSigninManager FakeSigninManagerForTesting;
#endif

const char kTestEmail[] = "user@gmail.com";

class AccountReconcilorTest : public testing::Test {
 public:
  AccountReconcilorTest();
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  TestingProfile* profile() { return profile_.get(); }
  FakeSigninManagerForTesting* signin_manager() { return signin_manager_; }
  FakeProfileOAuth2TokenService* token_service() { return token_service_; }

private:
  content::TestBrowserThreadBundle bundle_;
  scoped_ptr<TestingProfile> profile_;
  FakeSigninManagerForTesting* signin_manager_;
  FakeProfileOAuth2TokenService* token_service_;
};

AccountReconcilorTest::AccountReconcilorTest()
    : signin_manager_(NULL), token_service_(NULL) {}

void AccountReconcilorTest::SetUp() {
  TestingProfile::Builder builder;
  builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                            FakeProfileOAuth2TokenService::Build);
  builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                            FakeSigninManagerForTesting::Build);
  profile_ = builder.Build();

  signin_manager_ =
      static_cast<FakeSigninManagerForTesting*>(
          SigninManagerFactory::GetForProfile(profile()));
  signin_manager_->Initialize(profile(), NULL);

  token_service_ =
      static_cast<FakeProfileOAuth2TokenService*>(
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile()));
}

void AccountReconcilorTest::TearDown() {
  // Destroy the profile before all threads are torn down.
  profile_.reset();
}

}  // namespace

TEST_F(AccountReconcilorTest, Basic) {
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(NULL != reconcilor);
  ASSERT_EQ(profile(), reconcilor->profile());
}

#if !defined(OS_CHROMEOS)
TEST_F(AccountReconcilorTest, SigninManagerRegistration) {
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(NULL != reconcilor);
  ASSERT_FALSE(reconcilor->IsPeriodicReconciliationRunning());

  signin_manager()->OnExternalSigninCompleted(kTestEmail);
  ASSERT_TRUE(reconcilor->IsPeriodicReconciliationRunning());

  signin_manager()->SignOut();
  ASSERT_FALSE(reconcilor->IsPeriodicReconciliationRunning());
}
#endif

TEST_F(AccountReconcilorTest, ProfileAlreadyConnected) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(NULL != reconcilor);
  ASSERT_TRUE(reconcilor->IsPeriodicReconciliationRunning());
}

TEST_F(AccountReconcilorTest, ParseListAccountsData) {
  std::vector<std::string> accounts;
  accounts = AccountReconcilor::ParseListAccountsData("");
  ASSERT_EQ(0u, accounts.size());

  accounts = AccountReconcilor::ParseListAccountsData("1");
  ASSERT_EQ(0u, accounts.size());

  accounts = AccountReconcilor::ParseListAccountsData("[]");
  ASSERT_EQ(0u, accounts.size());

  accounts = AccountReconcilor::ParseListAccountsData("[\"foo\", \"bar\"]");
  ASSERT_EQ(0u, accounts.size());

  accounts = AccountReconcilor::ParseListAccountsData("[\"foo\", []]");
  ASSERT_EQ(0u, accounts.size());

  accounts = AccountReconcilor::ParseListAccountsData(
      "[\"foo\", [[\"bar\", 0, \"name\", 0, \"photo\", 0, 0, 0]]]");
  ASSERT_EQ(0u, accounts.size());

  accounts = AccountReconcilor::ParseListAccountsData(
      "[\"foo\", [[\"bar\", 0, \"name\", \"email\", \"photo\", 0, 0, 0]]]");
  ASSERT_EQ(1u, accounts.size());
  ASSERT_EQ("email", accounts[0]);

  accounts = AccountReconcilor::ParseListAccountsData(
      "[\"foo\", [[\"bar1\", 0, \"name1\", \"email1\", \"photo1\", 0, 0, 0], "
                 "[\"bar2\", 0, \"name2\", \"email2\", \"photo2\", 0, 0, 0]]]");
  ASSERT_EQ(2u, accounts.size());
  ASSERT_EQ("email1", accounts[0]);
  ASSERT_EQ("email2", accounts[1]);
}
