// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_response_handler.h"

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/fake_oauth2_token_service_delegate.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using signin::DiceAction;
using signin::DiceResponseParams;

namespace {

const char kAuthorizationCode[] = "authorization_code";
const char kEmail[] = "email";
const char kGaiaID[] = "gaia_id";
const int kSessionIndex = 42;

// TestSigninClient implementation that intercepts the GaiaAuthConsumer and
// replaces it by a dummy one.
class DiceTestSigninClient : public TestSigninClient, public GaiaAuthConsumer {
 public:
  explicit DiceTestSigninClient(PrefService* pref_service)
      : TestSigninClient(pref_service), consumer_(nullptr) {}

  ~DiceTestSigninClient() override {}

  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      const std::string& source,
      net::URLRequestContextGetter* getter) override {
    DCHECK(!consumer_ || (consumer_ == consumer));
    consumer_ = consumer;

    // Pass |this| as a dummy consumer to CreateGaiaAuthFetcher().
    // Since DiceTestSigninClient does not overrides any consumer method,
    // everything will be dropped on the floor.
    return TestSigninClient::CreateGaiaAuthFetcher(this, source, getter);
  }

  GaiaAuthConsumer* consumer_;
};

class DiceResponseHandlerTest : public testing::Test {
 protected:
  DiceResponseHandlerTest()
      : loop_(base::MessageLoop::TYPE_IO),  // URLRequestContext requires IO.
        task_runner_(new base::TestMockTimeTaskRunner()),
        request_context_getter_(
            new net::TestURLRequestContextGetter(task_runner_)),
        signin_client_(&pref_service_),
        token_service_(base::MakeUnique<FakeOAuth2TokenServiceDelegate>(
            request_context_getter_.get())),
        dice_response_handler_(&signin_client_,
                               &token_service_,
                               &account_tracker_service_) {
    loop_.SetTaskRunner(task_runner_);
    DCHECK_EQ(task_runner_, base::ThreadTaskRunnerHandle::Get());
    switches::EnableAccountConsistencyDiceForTesting(
        base::CommandLine::ForCurrentProcess());
    signin_client_.SetURLRequestContext(request_context_getter_.get());
    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    account_tracker_service_.Initialize(&signin_client_);
  }

  ~DiceResponseHandlerTest() override { task_runner_->ClearPendingTasks(); }

  DiceResponseParams MakeDiceParams(DiceAction action) {
    DiceResponseParams dice_params;
    dice_params.user_intention = action;
    dice_params.gaia_id = kGaiaID;
    dice_params.email = kEmail;
    dice_params.session_index = kSessionIndex;
    dice_params.authorization_code = kAuthorizationCode;
    return dice_params;
  }

  base::MessageLoop loop_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  DiceTestSigninClient signin_client_;
  ProfileOAuth2TokenService token_service_;
  AccountTrackerService account_tracker_service_;
  DiceResponseHandler dice_response_handler_;
};

// Checks that a SIGNIN action triggers a token exchange request.
TEST_F(DiceResponseHandlerTest, Signin) {
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(dice_params.gaia_id));
  dice_response_handler_.ProcessDiceHeader(dice_params);
  // Check that a GaiaAuthFetcher has been created.
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  // Simulate GaiaAuthFetcher success.
  signin_client_.consumer_->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("refresh_token", "access_token", 10));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(dice_params.gaia_id));
}

// Checks that a GaiaAuthFetcher failure is handled correctly.
TEST_F(DiceResponseHandlerTest, SigninFailure) {
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(dice_params.gaia_id));
  dice_response_handler_.ProcessDiceHeader(dice_params);
  // Check that a GaiaAuthFetcher has been created.
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  EXPECT_EQ(
      1u, dice_response_handler_.GetPendingDiceTokenFetchersCountForTesting());
  // Simulate GaiaAuthFetcher failure.
  signin_client_.consumer_->OnClientOAuthFailure(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  EXPECT_EQ(
      0u, dice_response_handler_.GetPendingDiceTokenFetchersCountForTesting());
  // Check that the token has not been inserted in the token service.
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(dice_params.gaia_id));
}

// Checks that a second token for the same account is not requested when a
// request is already in flight.
TEST_F(DiceResponseHandlerTest, SigninRepeatedWithSameAccount) {
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(dice_params.gaia_id));
  dice_response_handler_.ProcessDiceHeader(dice_params);
  // Check that a GaiaAuthFetcher has been created.
  GaiaAuthConsumer* consumer = signin_client_.consumer_;
  ASSERT_THAT(consumer, testing::NotNull());
  // Start a second request for the same account.
  signin_client_.consumer_ = nullptr;
  dice_response_handler_.ProcessDiceHeader(dice_params);
  // Check that there is no new request.
  ASSERT_THAT(signin_client_.consumer_, testing::IsNull());
  // Simulate GaiaAuthFetcher success for the first request.
  consumer->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("refresh_token", "access_token", 10));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(dice_params.gaia_id));
}

// Checks that two SIGNIN requests can happen concurrently.
TEST_F(DiceResponseHandlerTest, SigninWithTwoAccounts) {
  DiceResponseParams dice_params_1 = MakeDiceParams(DiceAction::SIGNIN);
  DiceResponseParams dice_params_2 = MakeDiceParams(DiceAction::SIGNIN);
  dice_params_2.email = "other_email";
  dice_params_2.gaia_id = "other_gaia_id";
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(dice_params_1.gaia_id));
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(dice_params_2.gaia_id));
  // Start first request.
  dice_response_handler_.ProcessDiceHeader(dice_params_1);
  // Check that a GaiaAuthFetcher has been created.
  GaiaAuthConsumer* consumer_1 = signin_client_.consumer_;
  ASSERT_THAT(consumer_1, testing::NotNull());
  // Start second request.
  signin_client_.consumer_ = nullptr;
  dice_response_handler_.ProcessDiceHeader(dice_params_2);
  GaiaAuthConsumer* consumer_2 = signin_client_.consumer_;
  ASSERT_THAT(consumer_2, testing::NotNull());
  // Simulate GaiaAuthFetcher success for the first request.
  consumer_1->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("refresh_token", "access_token", 10));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(dice_params_1.gaia_id));
  // Simulate GaiaAuthFetcher success for the second request.
  consumer_2->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("refresh_token", "access_token", 10));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(dice_params_2.gaia_id));
}

TEST_F(DiceResponseHandlerTest, Timeout) {
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(dice_params.gaia_id));
  dice_response_handler_.ProcessDiceHeader(dice_params);
  // Check that a GaiaAuthFetcher has been created.
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  EXPECT_EQ(
      1u, dice_response_handler_.GetPendingDiceTokenFetchersCountForTesting());
  // Force a timeout.
  task_runner_->FastForwardBy(
      base::TimeDelta::FromSeconds(kDiceTokenFetchTimeoutSeconds + 1));
  EXPECT_EQ(
      0u, dice_response_handler_.GetPendingDiceTokenFetchersCountForTesting());
  // Check that the token has not been inserted in the token service.
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(dice_params.gaia_id));
}

// Tests that the DiceResponseHandler is created for a normal profile but not
// for an incognito profile.
TEST(DiceResponseHandlerFactoryTest, NotInIncognito) {
  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile;
  EXPECT_THAT(DiceResponseHandler::GetForProfile(&profile), testing::NotNull());
  EXPECT_THAT(
      DiceResponseHandler::GetForProfile(profile.GetOffTheRecordProfile()),
      testing::IsNull());
}

}  // namespace
