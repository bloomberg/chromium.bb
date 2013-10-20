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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class AccountReconcilorTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  TestingProfile* profile() { return profile_.get(); }

private:
  scoped_ptr<TestingProfile> profile_;
};

void AccountReconcilorTest::SetUp() {
  TestingProfile::Builder builder;
  builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                            FakeProfileOAuth2TokenService::Build);
  builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                            FakeSigninManagerBase::Build);
  profile_ = builder.Build();
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
