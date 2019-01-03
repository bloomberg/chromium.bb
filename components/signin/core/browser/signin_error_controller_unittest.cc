// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_error_controller.h"

#include <stddef.h>

#include <functional>
#include <memory>

#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/test/scoped_task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static const char kTestAccountId[] = "account_id";
static const char kOtherTestAccountId[] = "other_id";

class MockSigninErrorControllerObserver
    : public SigninErrorController::Observer {
 public:
  MOCK_METHOD0(OnErrorChanged, void());
};

}  // namespace

TEST(SigninErrorControllerTest, SingleAccount) {
  MockSigninErrorControllerObserver observer;
  EXPECT_CALL(observer, OnErrorChanged()).Times(0);

  TestingPrefServiceSimple pref_service;
  FakeProfileOAuth2TokenService token_service(&pref_service);
  SigninErrorController error_controller(
      SigninErrorController::AccountMode::ANY_ACCOUNT, &token_service, nullptr);
  ScopedObserver<SigninErrorController, SigninErrorController::Observer>
      scoped_observer(&observer);
  scoped_observer.Add(&error_controller);
  ASSERT_FALSE(error_controller.HasError());
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  // The fake token service does not call OnEndBatchChanges() as part of
  // UpdateCredentials(), and thus the signin error controller is not updated.
  EXPECT_CALL(observer, OnErrorChanged()).Times(0);

  token_service.UpdateCredentials(kTestAccountId, "token");
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  GoogleServiceAuthError error1 =
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_CALL(observer, OnErrorChanged()).Times(1);
  token_service.UpdateAuthErrorForTesting(kTestAccountId, error1);
  EXPECT_TRUE(error_controller.HasError());
  EXPECT_EQ(error1, error_controller.auth_error());
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  GoogleServiceAuthError error2 =
      GoogleServiceAuthError(GoogleServiceAuthError::ACCOUNT_DISABLED);
  EXPECT_CALL(observer, OnErrorChanged()).Times(1);
  token_service.UpdateAuthErrorForTesting(kTestAccountId, error2);
  EXPECT_TRUE(error_controller.HasError());
  EXPECT_EQ(error2, error_controller.auth_error());
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnErrorChanged()).Times(1);
  token_service.UpdateAuthErrorForTesting(
      kTestAccountId, GoogleServiceAuthError::AuthErrorNone());
  EXPECT_FALSE(error_controller.HasError());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            error_controller.auth_error());
  ::testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST(SigninErrorControllerTest, AccountTransitionAnyAccount) {
  TestingPrefServiceSimple pref_service;
  FakeProfileOAuth2TokenService token_service(&pref_service);
  token_service.UpdateCredentials(kTestAccountId, "token");
  token_service.UpdateCredentials(kOtherTestAccountId, "token");
  SigninErrorController error_controller(
      SigninErrorController::AccountMode::ANY_ACCOUNT, &token_service, nullptr);
  ASSERT_FALSE(error_controller.HasError());

  token_service.UpdateAuthErrorForTesting(
      kTestAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  token_service.UpdateAuthErrorForTesting(
      kOtherTestAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  ASSERT_TRUE(error_controller.HasError());
  ASSERT_STREQ(kTestAccountId, error_controller.error_account_id().c_str());

  // Now resolve the auth errors - the menu item should go away.
  token_service.UpdateAuthErrorForTesting(
      kTestAccountId, GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(error_controller.HasError());
}

// This test exercises behavior on signin/signout, which is not relevant on
// ChromeOS.
#if !defined(OS_CHROMEOS)
TEST(SigninErrorControllerTest, AccountTransitionPrimaryAccount) {
  base::test::ScopedTaskEnvironment task_environment;

  TestingPrefServiceSimple pref_service;
  FakeProfileOAuth2TokenService token_service(&pref_service);
  ProfileOAuth2TokenService::RegisterProfilePrefs(pref_service.registry());
  TestSigninClient signin_client(&pref_service);
  AccountTrackerService account_tracker;
  AccountTrackerService::RegisterPrefs(pref_service.registry());
  account_tracker.Initialize(&pref_service, base::FilePath());
  FakeGaiaCookieManagerService cookie_manager_service(&token_service,
                                                      &signin_client);
  FakeSigninManager signin_manager(&signin_client, &token_service,
                                   &account_tracker, &cookie_manager_service);
  SigninManagerBase::RegisterProfilePrefs(pref_service.registry());
  SigninManagerBase::RegisterPrefs(pref_service.registry());
  signin_manager.Initialize(nullptr);

  token_service.UpdateCredentials(kTestAccountId, "token");
  token_service.UpdateCredentials(kOtherTestAccountId, "token");
  SigninErrorController error_controller(
      SigninErrorController::AccountMode::PRIMARY_ACCOUNT, &token_service,
      &signin_manager);
  ASSERT_FALSE(error_controller.HasError());

  token_service.UpdateAuthErrorForTesting(
      kTestAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  token_service.UpdateAuthErrorForTesting(
      kOtherTestAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  ASSERT_FALSE(error_controller.HasError());  // No primary account.

  // Set the primary account.
  signin_manager.SignIn(kOtherTestAccountId, kOtherTestAccountId, "");

  ASSERT_FALSE(error_controller.HasError());  // Error is on secondary.

  // Change the primary account to the account with an error and check that the
  // error controller updates its error status accordingly.
  signin_manager.SignOutAndKeepAllAccounts(
      signin_metrics::FORCE_SIGNOUT_ALWAYS_ALLOWED_FOR_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  signin_manager.SignIn(kTestAccountId, kTestAccountId, "");
  ASSERT_TRUE(error_controller.HasError());
  ASSERT_STREQ(kTestAccountId, error_controller.error_account_id().c_str());

  token_service.UpdateAuthErrorForTesting(
      kOtherTestAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  ASSERT_TRUE(error_controller.HasError());
  ASSERT_STREQ(kTestAccountId, error_controller.error_account_id().c_str());

  // Change the primary account again and check that the error controller
  // updates its error status accordingly.
  signin_manager.SignOutAndKeepAllAccounts(
      signin_metrics::FORCE_SIGNOUT_ALWAYS_ALLOWED_FOR_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  signin_manager.SignIn(kOtherTestAccountId, kOtherTestAccountId, "");
  ASSERT_TRUE(error_controller.HasError());
  ASSERT_STREQ(kOtherTestAccountId,
               error_controller.error_account_id().c_str());

  // Sign out and check that that the error controller updates its error status
  // accordingly.
  signin_manager.SignOutAndKeepAllAccounts(
      signin_metrics::FORCE_SIGNOUT_ALWAYS_ALLOWED_FOR_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  ASSERT_FALSE(error_controller.HasError());
}
#endif

// Verify that SigninErrorController handles errors properly.
TEST(SigninErrorControllerTest, AuthStatusEnumerateAllErrors) {
  TestingPrefServiceSimple pref_service;
  FakeProfileOAuth2TokenService token_service(&pref_service);
  token_service.UpdateCredentials(kTestAccountId, "token");
  SigninErrorController error_controller(
      SigninErrorController::AccountMode::ANY_ACCOUNT, &token_service, nullptr);

  GoogleServiceAuthError::State table[] = {
      GoogleServiceAuthError::NONE,
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
      GoogleServiceAuthError::USER_NOT_SIGNED_UP,
      GoogleServiceAuthError::CONNECTION_FAILED,
      GoogleServiceAuthError::CAPTCHA_REQUIRED,
      GoogleServiceAuthError::ACCOUNT_DELETED,
      GoogleServiceAuthError::ACCOUNT_DISABLED,
      GoogleServiceAuthError::SERVICE_UNAVAILABLE,
      GoogleServiceAuthError::TWO_FACTOR,
      GoogleServiceAuthError::REQUEST_CANCELED,
      GoogleServiceAuthError::HOSTED_NOT_ALLOWED_DEPRECATED,
      GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE,
      GoogleServiceAuthError::SERVICE_ERROR,
      GoogleServiceAuthError::WEB_LOGIN_REQUIRED};
  static_assert(base::size(table) == GoogleServiceAuthError::NUM_STATES,
                "table array does not match the number of auth error types");

  for (GoogleServiceAuthError::State state : table) {
    if (GoogleServiceAuthError::IsDeprecated(state))
      continue;

    GoogleServiceAuthError error(state);

    if (error.IsTransientError())
      continue;  // Only persistent errors or non-errors are reported.

    token_service.UpdateAuthErrorForTesting(kTestAccountId, error);

    EXPECT_EQ(error_controller.HasError(), error.IsPersistentError());

    if (error.IsPersistentError()) {
      EXPECT_EQ(state, error_controller.auth_error().state());
      EXPECT_STREQ(kTestAccountId, error_controller.error_account_id().c_str());
    } else {
      EXPECT_EQ(GoogleServiceAuthError::NONE,
                error_controller.auth_error().state());
      EXPECT_STREQ("", error_controller.error_account_id().c_str());
    }
  }
}

// Verify that existing error is not replaced by new error.
TEST(SigninErrorControllerTest, AuthStatusChange) {
  TestingPrefServiceSimple pref_service;
  FakeProfileOAuth2TokenService token_service(&pref_service);
  token_service.UpdateCredentials(kTestAccountId, "token");
  token_service.UpdateCredentials(kOtherTestAccountId, "token");
  SigninErrorController error_controller(
      SigninErrorController::AccountMode::ANY_ACCOUNT, &token_service, nullptr);
  ASSERT_FALSE(error_controller.HasError());

  // Set an error for kOtherTestAccountId.
  token_service.UpdateAuthErrorForTesting(
      kTestAccountId, GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  token_service.UpdateAuthErrorForTesting(
      kOtherTestAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            error_controller.auth_error().state());
  ASSERT_STREQ(kOtherTestAccountId,
               error_controller.error_account_id().c_str());

  // Change the error for kOtherTestAccountId.
  token_service.UpdateAuthErrorForTesting(
      kOtherTestAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR));
  ASSERT_EQ(GoogleServiceAuthError::SERVICE_ERROR,
            error_controller.auth_error().state());
  ASSERT_STREQ(kOtherTestAccountId,
               error_controller.error_account_id().c_str());

  // Set the error for kTestAccountId -- nothing should change.
  token_service.UpdateAuthErrorForTesting(
      kTestAccountId, GoogleServiceAuthError(
                          GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE));
  ASSERT_EQ(GoogleServiceAuthError::SERVICE_ERROR,
            error_controller.auth_error().state());
  ASSERT_STREQ(kOtherTestAccountId,
               error_controller.error_account_id().c_str());

  // Clear the error for kOtherTestAccountId, so the kTestAccountId's error is
  // used.
  token_service.UpdateAuthErrorForTesting(
      kOtherTestAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  ASSERT_EQ(GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE,
            error_controller.auth_error().state());
  ASSERT_STREQ(kTestAccountId, error_controller.error_account_id().c_str());

  // Clear the remaining error.
  token_service.UpdateAuthErrorForTesting(
      kTestAccountId, GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  ASSERT_FALSE(error_controller.HasError());
}
