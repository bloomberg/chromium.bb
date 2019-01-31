// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_signin_notifier_impl.h"

#include "base/bind.h"
#include "chrome/browser/signin/account_fetcher_service_factory.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/signin/core/browser/account_fetcher_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/identity/public/cpp/accounts_mutator.h"
#include "services/identity/public/cpp/primary_account_mutator.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace password_manager {
namespace {

class PasswordStoreSigninNotifierImplTest : public testing::Test {
 public:
  PasswordStoreSigninNotifierImplTest() {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        SigninManagerFactory::GetInstance(),
        base::BindRepeating(&BuildFakeSigninManagerForTesting));
    testing_profile_.reset(builder.Build().release());
    fake_signin_manager_ = static_cast<FakeSigninManagerForTesting*>(
        SigninManagerFactory::GetForProfile(testing_profile_.get()));
    store_ = new MockPasswordStore();
  }

  ~PasswordStoreSigninNotifierImplTest() override {
    store_->ShutdownOnUIThread();
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle;
  std::unique_ptr<TestingProfile> testing_profile_;
  FakeSigninManagerForTesting* fake_signin_manager_;  // Weak
  scoped_refptr<MockPasswordStore> store_;
};

// Checks that if a notifier is subscribed on sign-in events, then
// a password store receives sign-in notifications.
TEST_F(PasswordStoreSigninNotifierImplTest, Subscribed) {
  PasswordStoreSigninNotifierImpl notifier(testing_profile_.get());
  notifier.SubscribeToSigninEvents(store_.get());
  fake_signin_manager_->SignIn("accountid", "username", "password");
  testing::Mock::VerifyAndClearExpectations(store_.get());
  EXPECT_CALL(*store_, ClearAllGaiaPasswordHash());
  fake_signin_manager_->ForceSignOut();
  notifier.UnsubscribeFromSigninEvents();
}

// Checks that if a notifier is unsubscribed on sign-in events, then
// a password store receives no sign-in notifications.
TEST_F(PasswordStoreSigninNotifierImplTest, Unsubscribed) {
  PasswordStoreSigninNotifierImpl notifier(testing_profile_.get());
  notifier.SubscribeToSigninEvents(store_.get());
  notifier.UnsubscribeFromSigninEvents();
  EXPECT_CALL(*store_, ClearAllGaiaPasswordHash()).Times(0);
  fake_signin_manager_->SignIn("accountid", "username", "secret");
  fake_signin_manager_->ForceSignOut();
}

// Checks that if a notifier is unsubscribed on sign-in events, then
// a password store receives no sign-in notifications.
TEST_F(PasswordStoreSigninNotifierImplTest, SignOutContentArea) {
  PasswordStoreSigninNotifierImpl notifier(testing_profile_.get());
  notifier.SubscribeToSigninEvents(store_.get());

  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(testing_profile_.get());
  identity::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();
  DCHECK(primary_account_mutator);
  primary_account_mutator->LegacyStartSigninWithRefreshTokenForPrimaryAccount(
      /*refresh_token=*/"refresh_token",
      /*gaia_id=*/"primary_account_id",
      /*username=*/"username",
      /*password=*/"password", base::OnceCallback<void(const std::string&)>());
  primary_account_mutator->LegacyCompletePendingPrimaryAccountSignin();

  testing::Mock::VerifyAndClearExpectations(store_.get());

  EXPECT_CALL(*store_, ClearGaiaPasswordHash("username2"));

  AccountFetcherService* account_fetcher_service =
      AccountFetcherServiceFactory::GetForProfile(testing_profile_.get());
  // This call is necessary to ensure that the account removal is fully
  // processed in this testing context.
  account_fetcher_service->EnableNetworkFetchesForTest();
  identity_manager->GetAccountsMutator()->AddOrUpdateAccount(
      /*gaia_id=*/"secondary_account_id",
      /*email=*/"username2",
      /*refresh_token=*/"refresh_token",
      /*is_under_advanced_protection=*/false,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  identity_manager->GetAccountsMutator()->RemoveAccount(
      "secondary_account_id",
      signin_metrics::SourceForRefreshTokenOperation::kUserMenu_RemoveAccount);
  testing::Mock::VerifyAndClearExpectations(store_.get());

  EXPECT_CALL(*store_, ClearAllGaiaPasswordHash());
  primary_account_mutator->ClearPrimaryAccount(
      identity::PrimaryAccountMutator::ClearAccountsAction::kRemoveAll,
      signin_metrics::FORCE_SIGNOUT_ALWAYS_ALLOWED_FOR_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  notifier.UnsubscribeFromSigninEvents();
}

}  // namespace
}  // namespace password_manager
