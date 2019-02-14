// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"

#include "base/bind.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/list_accounts_test_utils.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<KeyedService> BuildTestSigninClient(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<TestSigninClient>(profile->GetPrefs());
}

class ChildAccountServiceTest : public ::testing::Test {
 public:
  ChildAccountServiceTest() = default;

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ChromeSigninClientFactory::GetInstance(),
                              base::BindRepeating(&BuildTestSigninClient));
    profile_ = builder.Build();
    gaia_cookie_manager_service_ =
        GaiaCookieManagerServiceFactory::GetForProfile(profile_.get());
  }

 protected:
  network::TestURLLoaderFactory* url_loader_factory() {
    auto* signin_client =
        ChromeSigninClientFactory::GetForProfile(profile_.get());
    return static_cast<TestSigninClient*>(signin_client)
        ->test_url_loader_factory();
  }

  content::TestBrowserThreadBundle thread_bundle_;

  std::unique_ptr<TestingProfile> profile_;
  GaiaCookieManagerService* gaia_cookie_manager_service_;
};

TEST_F(ChildAccountServiceTest, GetGoogleAuthState) {
  network::TestURLLoaderFactory* test_url_loader_factory = url_loader_factory();
  signin::SetListAccountsResponseNoAccounts(test_url_loader_factory);

  ChildAccountService* child_account_service =
      ChildAccountServiceFactory::GetForProfile(profile_.get());

  // Initial state should be PENDING.
  EXPECT_EQ(ChildAccountService::AuthState::PENDING,
            child_account_service->GetGoogleAuthState());

  // Wait until the response to the ListAccount request triggered by the call
  // above comes back.
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(ChildAccountService::AuthState::NOT_AUTHENTICATED,
            child_account_service->GetGoogleAuthState());

  // A valid, signed-in account means authenticated.
  signin::SetListAccountsResponseOneAccountWithParams(
      {"me@example.com", "abcdef",
       /* valid = */ true,
       /* is_signed_out = */ false,
       /* verified = */ true},
      test_url_loader_factory);
  gaia_cookie_manager_service_->TriggerListAccounts();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(ChildAccountService::AuthState::AUTHENTICATED,
            child_account_service->GetGoogleAuthState());

  // An invalid (but signed-in) account means not authenticated.
  signin::SetListAccountsResponseOneAccountWithParams(
      {"me@example.com", "abcdef",
       /* valid = */ false,
       /* is_signed_out = */ false,
       /* verified = */ true},
      test_url_loader_factory);
  gaia_cookie_manager_service_->TriggerListAccounts();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(ChildAccountService::AuthState::NOT_AUTHENTICATED,
            child_account_service->GetGoogleAuthState());

  // A valid but not signed-in account means not authenticated.
  signin::SetListAccountsResponseOneAccountWithParams(
      {"me@example.com", "abcdef",
       /* valid = */ true,
       /* is_signed_out = */ true,
       /* verified = */ true},
      test_url_loader_factory);
  gaia_cookie_manager_service_->TriggerListAccounts();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(ChildAccountService::AuthState::NOT_AUTHENTICATED,
            child_account_service->GetGoogleAuthState());
}

}  // namespace
