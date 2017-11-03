// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_response_handler.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/scoped_observer.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/scoped_account_consistency.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/test_signin_client.h"
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

 private:
  DISALLOW_COPY_AND_ASSIGN(DiceTestSigninClient);
};

// Checks if OnRefreshTokenAvailable() has been called for the specified
// account.
class DiceTestTokenServiceObserver : public OAuth2TokenService::Observer {
 public:
  explicit DiceTestTokenServiceObserver(const std::string& account_id)
      : account_id_(account_id) {}

  bool token_received() { return token_received_; }

 private:
  // OAuth2TokenServiceObserver:
  void OnRefreshTokenAvailable(const std::string& account_id) override {
    if (account_id == account_id_)
      token_received_ = true;
  }

  bool token_received_ = false;
  std::string account_id_;

  DISALLOW_COPY_AND_ASSIGN(DiceTestTokenServiceObserver);
};

class DiceResponseHandlerTest : public testing::Test,
                                public AccountReconcilor::Observer {
 public:
  void WillStartRefreshTokenFetch(const std::string& gaia_id,
                                  const std::string& email) {
    start_token_fetch_gaia_id_ = gaia_id;
    start_token_fetch_email_ = email;
  }

  // Called after the refresh token was fetched and added in the token service.
  void DidFinishRefreshTokenFetch(const std::string& gaia_id,
                                  const std::string& email) {
    finish_token_fetch_gaia_id_ = gaia_id;
    finish_token_fetch_email_ = email;
  }

 protected:
  DiceResponseHandlerTest()
      : loop_(base::MessageLoop::TYPE_IO),  // URLRequestContext requires IO.
        task_runner_(new base::TestMockTimeTaskRunner()),
        request_context_getter_(
            new net::TestURLRequestContextGetter(task_runner_)),
        signin_client_(&pref_service_),
        token_service_(base::MakeUnique<FakeOAuth2TokenServiceDelegate>(
            request_context_getter_.get())),
        signin_manager_(&signin_client_,
                        &token_service_,
                        &account_tracker_service_,
                        nullptr),
        reconcilor_blocked_count_(0),
        reconcilor_unblocked_count_(0) {
    loop_.SetTaskRunner(task_runner_);
    DCHECK_EQ(task_runner_, base::ThreadTaskRunnerHandle::Get());
    signin_client_.SetURLRequestContext(request_context_getter_.get());
    AccountReconcilor::RegisterProfilePrefs(pref_service_.registry());
    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    SigninManager::RegisterProfilePrefs(pref_service_.registry());
    signin::RegisterAccountConsistencyProfilePrefs(pref_service_.registry());
    account_reconcilor_ = base::MakeUnique<AccountReconcilor>(
        &token_service_, &signin_manager_, &signin_client_, nullptr, false);
    dice_response_handler_ = base::MakeUnique<DiceResponseHandler>(
        &signin_client_, &signin_manager_, &token_service_,
        &account_tracker_service_, account_reconcilor_.get());

    account_tracker_service_.Initialize(&signin_client_);
    account_reconcilor_->AddObserver(this);
  }

  ~DiceResponseHandlerTest() override {
    account_reconcilor_->RemoveObserver(this);
    task_runner_->ClearPendingTasks();
  }

  DiceResponseParams MakeDiceParams(DiceAction action) {
    DiceResponseParams dice_params;
    dice_params.user_intention = action;
    if (action == DiceAction::SIGNIN) {
      dice_params.signin_info.gaia_id = kGaiaID;
      dice_params.signin_info.email = kEmail;
      dice_params.signin_info.session_index = kSessionIndex;
      dice_params.signin_info.authorization_code = kAuthorizationCode;
    } else if (action == DiceAction::SIGNOUT) {
      dice_params.signout_info.gaia_id.push_back(kGaiaID);
      dice_params.signout_info.email.push_back(kEmail);
      dice_params.signout_info.session_index.push_back(kSessionIndex);
    }
    return dice_params;
  }

  // AccountReconcilor::Observer:
  void OnBlockReconcile() override { ++reconcilor_blocked_count_; }
  void OnUnblockReconcile() override { ++reconcilor_unblocked_count_; }

  base::MessageLoop loop_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  DiceTestSigninClient signin_client_;
  ProfileOAuth2TokenService token_service_;
  AccountTrackerService account_tracker_service_;
  FakeSigninManager signin_manager_;
  std::unique_ptr<AccountReconcilor> account_reconcilor_;
  std::unique_ptr<DiceResponseHandler> dice_response_handler_;
  int reconcilor_blocked_count_;
  int reconcilor_unblocked_count_;
  std::string start_token_fetch_gaia_id_;
  std::string start_token_fetch_email_;
  std::string finish_token_fetch_gaia_id_;
  std::string finish_token_fetch_email_;
};

class TestProcessDiceHeaderObserver : public ProcessDiceHeaderObserver {
 public:
  explicit TestProcessDiceHeaderObserver(DiceResponseHandlerTest* owner)
      : owner_(owner) {}
  ~TestProcessDiceHeaderObserver() override = default;

  void WillStartRefreshTokenFetch(const std::string& gaia_id,
                                  const std::string& email) override {
    owner_->WillStartRefreshTokenFetch(gaia_id, email);
  }

  // Called after the refresh token was fetched and added in the token service.
  void DidFinishRefreshTokenFetch(const std::string& gaia_id,
                                  const std::string& email) override {
    owner_->DidFinishRefreshTokenFetch(gaia_id, email);
  }

 private:
  DiceResponseHandlerTest* owner_;
};

// Checks that a SIGNIN action triggers a token exchange request.
TEST_F(DiceResponseHandlerTest, Signin) {
  signin::ScopedAccountConsistencyDice scoped_dice;
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      dice_params.signin_info.gaia_id, dice_params.signin_info.email);
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  // Check that a GaiaAuthFetcher has been created.
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
  EXPECT_EQ(dice_params.signin_info.gaia_id, start_token_fetch_gaia_id_);
  EXPECT_EQ(dice_params.signin_info.email, start_token_fetch_email_);
  EXPECT_EQ("", finish_token_fetch_gaia_id_);
  EXPECT_EQ("", finish_token_fetch_email_);
  // Simulate GaiaAuthFetcher success.
  signin_client_.consumer_->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("refresh_token", "access_token", 10));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  // Check that the reconcilor was blocked and unblocked exactly once.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(1, reconcilor_unblocked_count_);
  EXPECT_EQ(dice_params.signin_info.gaia_id, finish_token_fetch_gaia_id_);
  EXPECT_EQ(dice_params.signin_info.email, finish_token_fetch_email_);
}

// Checks that a GaiaAuthFetcher failure is handled correctly.
TEST_F(DiceResponseHandlerTest, SigninFailure) {
  signin::ScopedAccountConsistencyDice scoped_dice;
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      dice_params.signin_info.gaia_id, dice_params.signin_info.email);
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  EXPECT_EQ(dice_params.signin_info.gaia_id, start_token_fetch_gaia_id_);
  EXPECT_EQ(dice_params.signin_info.email, start_token_fetch_email_);
  EXPECT_EQ("", finish_token_fetch_gaia_id_);
  EXPECT_EQ("", finish_token_fetch_email_);
  // Check that a GaiaAuthFetcher has been created.
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  EXPECT_EQ(
      1u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Simulate GaiaAuthFetcher failure.
  signin_client_.consumer_->OnClientOAuthFailure(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  EXPECT_EQ(
      0u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Check that the token has not been inserted in the token service.
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_EQ("", finish_token_fetch_gaia_id_);
  EXPECT_EQ("", finish_token_fetch_email_);
}

// Checks that a second token for the same account is not requested when a
// request is already in flight.
TEST_F(DiceResponseHandlerTest, SigninRepeatedWithSameAccount) {
  signin::ScopedAccountConsistencyDice scoped_dice;
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      dice_params.signin_info.gaia_id, dice_params.signin_info.email);
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  EXPECT_EQ(dice_params.signin_info.gaia_id, start_token_fetch_gaia_id_);
  EXPECT_EQ(dice_params.signin_info.email, start_token_fetch_email_);
  // Check that a GaiaAuthFetcher has been created.
  GaiaAuthConsumer* consumer = signin_client_.consumer_;
  ASSERT_THAT(consumer, testing::NotNull());
  // Start a second request for the same account.
  signin_client_.consumer_ = nullptr;
  dice_response_handler_->ProcessDiceHeader(
      dice_params, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  // Check that there is no new request.
  ASSERT_THAT(signin_client_.consumer_, testing::IsNull());
  // Simulate GaiaAuthFetcher success for the first request.
  consumer->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("refresh_token", "access_token", 10));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_EQ(dice_params.signin_info.gaia_id, finish_token_fetch_gaia_id_);
  EXPECT_EQ(dice_params.signin_info.email, finish_token_fetch_email_);
}

// Checks that two SIGNIN requests can happen concurrently.
TEST_F(DiceResponseHandlerTest, SigninWithTwoAccounts) {
  signin::ScopedAccountConsistencyDice scoped_dice;
  DiceResponseParams dice_params_1 = MakeDiceParams(DiceAction::SIGNIN);
  DiceResponseParams dice_params_2 = MakeDiceParams(DiceAction::SIGNIN);
  dice_params_2.signin_info.email = "other_email";
  dice_params_2.signin_info.gaia_id = "other_gaia_id";
  std::string account_id_1 = account_tracker_service_.PickAccountIdForAccount(
      dice_params_1.signin_info.gaia_id, dice_params_1.signin_info.email);
  std::string account_id_2 = account_tracker_service_.PickAccountIdForAccount(
      dice_params_2.signin_info.gaia_id, dice_params_2.signin_info.email);
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id_1));
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id_2));
  // Start first request.
  dice_response_handler_->ProcessDiceHeader(
      dice_params_1, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  // Check that a GaiaAuthFetcher has been created.
  GaiaAuthConsumer* consumer_1 = signin_client_.consumer_;
  ASSERT_THAT(consumer_1, testing::NotNull());
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
  // Start second request.
  signin_client_.consumer_ = nullptr;
  dice_response_handler_->ProcessDiceHeader(
      dice_params_2, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  GaiaAuthConsumer* consumer_2 = signin_client_.consumer_;
  ASSERT_THAT(consumer_2, testing::NotNull());
  // Simulate GaiaAuthFetcher success for the first request.
  consumer_1->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("refresh_token", "access_token", 10));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id_1));
  // Simulate GaiaAuthFetcher success for the second request.
  consumer_2->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("refresh_token", "access_token", 10));
  // Check that the token has been inserted in the token service.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id_2));
  // Check that the reconcilor was blocked and unblocked exactly once.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(1, reconcilor_unblocked_count_);
}

TEST_F(DiceResponseHandlerTest, Timeout) {
  signin::ScopedAccountConsistencyDice scoped_dice;
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      dice_params.signin_info.gaia_id, dice_params.signin_info.email);
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  // Check that a GaiaAuthFetcher has been created.
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  EXPECT_EQ(
      1u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Force a timeout.
  task_runner_->FastForwardBy(
      base::TimeDelta::FromSeconds(kDiceTokenFetchTimeoutSeconds + 1));
  EXPECT_EQ(
      0u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Check that the token has not been inserted in the token service.
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  // Check that the reconcilor was blocked and unblocked exactly once.
  EXPECT_EQ(1, reconcilor_blocked_count_);
  EXPECT_EQ(1, reconcilor_unblocked_count_);
}

TEST_F(DiceResponseHandlerTest, SignoutMainAccount) {
  signin::ScopedAccountConsistencyDice scoped_dice;
  const char kSecondaryGaiaID[] = "secondary_account";
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      dice_params.signout_info.gaia_id[0], dice_params.signout_info.email[0]);
  // User is signed in to Chrome, and has some refresh token for a secondary
  // account.
  signin_manager_.SignIn(dice_params.signout_info.gaia_id[0],
                         dice_params.signout_info.email[0], "password");
  token_service_.UpdateCredentials(account_id, "token1");
  token_service_.UpdateCredentials(kSecondaryGaiaID, "token2");
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(kSecondaryGaiaID));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());
  // Receive signout response for the main account.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  EXPECT_EQ("", start_token_fetch_gaia_id_);
  EXPECT_EQ("", start_token_fetch_email_);

  // User is signed out and all tokens are cleared.
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(kSecondaryGaiaID));
  EXPECT_FALSE(signin_manager_.IsAuthenticated());
  EXPECT_EQ("", finish_token_fetch_gaia_id_);
  EXPECT_EQ("", finish_token_fetch_email_);
  // Check that the reconcilor was not blocked.
  EXPECT_EQ(0, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
}

TEST_F(DiceResponseHandlerTest, MigrationSignout) {
  signin::ScopedAccountConsistencyDiceMigration scoped_dice_migration;
  const char kSecondaryGaiaID[] = "secondary_account";
  const char kSecondaryEmail[] = "other@gmail.com";
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  dice_params.signout_info.gaia_id.push_back(kSecondaryGaiaID);
  dice_params.signout_info.email.push_back(kSecondaryEmail);
  dice_params.signout_info.session_index.push_back(1);
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      dice_params.signout_info.gaia_id[0], dice_params.signout_info.email[0]);
  std::string secondary_account_id =
      account_tracker_service_.PickAccountIdForAccount(kSecondaryGaiaID,
                                                       kSecondaryEmail);

  // User is signed in to Chrome, and has some refresh token for a secondary
  // account.
  signin_manager_.SignIn(dice_params.signout_info.gaia_id[0],
                         dice_params.signout_info.email[0], "password");
  token_service_.UpdateCredentials(account_id, "token1");
  token_service_.UpdateCredentials(secondary_account_id, "token2");
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(secondary_account_id));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());

  // Receive signout response for all accounts.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  EXPECT_EQ("", start_token_fetch_gaia_id_);
  EXPECT_EQ("", start_token_fetch_email_);

  // User is not signed out from Chrome, only the secondary token is deleted.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(secondary_account_id));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());
  EXPECT_EQ("", finish_token_fetch_gaia_id_);
  EXPECT_EQ("", finish_token_fetch_email_);
  // Check that the reconcilor was not blocked.
  EXPECT_EQ(0, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
}

TEST_F(DiceResponseHandlerTest, SignoutSecondaryAccount) {
  signin::ScopedAccountConsistencyDice scoped_dice;
  const char kMainGaiaID[] = "main_account";
  const char kMainEmail[] = "main@gmail.com";
  std::string main_account_id =
      account_tracker_service_.PickAccountIdForAccount(kMainGaiaID, kMainEmail);
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      dice_params.signout_info.gaia_id[0], dice_params.signout_info.email[0]);
  // User is signed in to Chrome, and has some refresh token for a secondary
  // account.
  signin_manager_.SignIn(kMainGaiaID, "user", "password");
  token_service_.UpdateCredentials(main_account_id, "token2");
  token_service_.UpdateCredentials(account_id, "token1");
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(main_account_id));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());
  // Receive signout response for the secondary account.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  EXPECT_EQ("", start_token_fetch_gaia_id_);
  EXPECT_EQ("", start_token_fetch_email_);

  // Only the token corresponding the the Dice parameter has been removed, and
  // the user is still signed in.
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(main_account_id));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());
  EXPECT_EQ("", finish_token_fetch_gaia_id_);
  EXPECT_EQ("", finish_token_fetch_email_);
}

TEST_F(DiceResponseHandlerTest, SignoutWebOnly) {
  signin::ScopedAccountConsistencyDice scoped_dice;
  const char kSecondaryAccountID[] = "secondary_account";
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      dice_params.signout_info.gaia_id[0], dice_params.signout_info.email[0]);
  // User is NOT signed in to Chrome, and has some refresh tokens for two
  // accounts.
  token_service_.UpdateCredentials(account_id, "refresh_token");
  token_service_.UpdateCredentials(kSecondaryAccountID, "refresh_token");
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(kSecondaryAccountID));
  EXPECT_FALSE(signin_manager_.IsAuthenticated());
  // Receive signout response.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  // Only the token corresponding the the Dice parameter has been removed.
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(kSecondaryAccountID));
  EXPECT_FALSE(signin_manager_.IsAuthenticated());
}

// Checks that signin in progress is canceled by a signout for the main account.
TEST_F(DiceResponseHandlerTest, SigninSignoutMainAccount) {
  signin::ScopedAccountConsistencyDice scoped_dice;
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      dice_params.signout_info.gaia_id[0], dice_params.signout_info.email[0]);
  // User is signed in to Chrome with a main account.
  signin_manager_.SignIn(dice_params.signout_info.gaia_id[0],
                         dice_params.signout_info.email[0], "password");
  token_service_.UpdateCredentials(account_id, "token");
  ASSERT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  // Start Dice signin with a secondary account.
  DiceResponseParams dice_params_2 = MakeDiceParams(DiceAction::SIGNIN);
  dice_params_2.signin_info.email = "other_email";
  dice_params_2.signin_info.gaia_id = "other_gaia_id";
  std::string account_id_2 = account_tracker_service_.PickAccountIdForAccount(
      dice_params_2.signin_info.gaia_id, dice_params_2.signin_info.email);
  dice_response_handler_->ProcessDiceHeader(
      dice_params_2, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  // Check that a GaiaAuthFetcher has been created and is pending.
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  EXPECT_EQ(
      1u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id_2));
  // Signout from main account while signin for the other account is in flight.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  // Check that the token fetcher has been canceled and all tokens erased.
  EXPECT_EQ(
      0u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(account_id_2));
}

// Checks that signin in progress is canceled by a signout for a secondary
// account.
TEST_F(DiceResponseHandlerTest, SigninSignoutSecondaryAccount) {
  signin::ScopedAccountConsistencyDice scoped_dice;
  // User starts signin in the web with two accounts, and is not signed into
  // Chrome.
  DiceResponseParams signout_params_1 = MakeDiceParams(DiceAction::SIGNOUT);
  DiceResponseParams signin_params_1 = MakeDiceParams(DiceAction::SIGNIN);
  DiceResponseParams signin_params_2 = MakeDiceParams(DiceAction::SIGNIN);
  signin_params_2.signin_info.email = "other_email";
  signin_params_2.signin_info.gaia_id = "other_gaia_id";
  std::string account_id_1 = account_tracker_service_.PickAccountIdForAccount(
      signin_params_1.signin_info.gaia_id, signin_params_1.signin_info.email);
  std::string account_id_2 = account_tracker_service_.PickAccountIdForAccount(
      signin_params_2.signin_info.gaia_id, signin_params_2.signin_info.email);
  dice_response_handler_->ProcessDiceHeader(
      signin_params_1, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  signin_client_.consumer_ = nullptr;
  dice_response_handler_->ProcessDiceHeader(
      signin_params_2, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  EXPECT_EQ(
      2u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id_1));
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(account_id_2));
  // Signout from one of the accounts while signin is in flight.
  dice_response_handler_->ProcessDiceHeader(
      signout_params_1, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  // Check that one of the fetchers is cancelled.
  EXPECT_EQ(
      1u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Allow the remaining fetcher to complete.
  signin_client_.consumer_->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("refresh_token", "access_token", 10));
  EXPECT_EQ(
      0u, dice_response_handler_->GetPendingDiceTokenFetchersCountForTesting());
  // Check that the right token is available.
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(account_id_1));
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id_2));
}

// Checks that no auth error fix happens if the user is signed out.
TEST_F(DiceResponseHandlerTest, FixAuthErrorSignedOut) {
  signin::ScopedAccountConsistencyDiceFixAuthErrors scoped_dice_fix_auth_errors;
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  ASSERT_FALSE(token_service_.RefreshTokenIsAvailable(
      account_tracker_service_.PickAccountIdForAccount(
          dice_params.signin_info.gaia_id, dice_params.signin_info.email)));
  dice_response_handler_->ProcessDiceHeader(
      dice_params, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  // Check that a GaiaAuthFetcher has not been created.
  ASSERT_THAT(signin_client_.consumer_, testing::IsNull());
}

// Checks that the token is not stored if the user signs out during the token
// request.
TEST_F(DiceResponseHandlerTest, FixAuthErrorSignOutDuringRequest) {
  signin::ScopedAccountConsistencyDiceFixAuthErrors scoped_dice_fix_auth_errors;
  // User is signed in to Chrome.
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      dice_params.signin_info.gaia_id, dice_params.signin_info.email);
  signin_manager_.SignIn(dice_params.signin_info.gaia_id,
                         dice_params.signin_info.email, "password");
  token_service_.UpdateCredentials(account_id, "token1");
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());
  // Start re-authentication on the web.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  // Check that a GaiaAuthFetcher has been created.
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  // Sign out.
  signin_manager_.ForceSignOut();
  // Simulate GaiaAuthFetcher success.
  signin_client_.consumer_->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("refresh_token", "access_token", 10));
  // Check that the token has not been inserted in the token service.
  EXPECT_FALSE(token_service_.RefreshTokenIsAvailable(account_id));
}

// Checks that the token is fixed if the Chrome account matches the web account.
TEST_F(DiceResponseHandlerTest, FixAuthError) {
  signin::ScopedAccountConsistencyDiceFixAuthErrors scoped_dice_fix_auth_errors;
  // User is signed in to Chrome.
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNIN);
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      dice_params.signin_info.gaia_id, dice_params.signin_info.email);
  signin_manager_.SignIn(dice_params.signin_info.gaia_id,
                         dice_params.signin_info.email, "password");
  token_service_.UpdateCredentials(account_id, "token1");
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());
  // Start re-authentication on the web.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  // Check that a GaiaAuthFetcher has been created.
  ASSERT_THAT(signin_client_.consumer_, testing::NotNull());
  // We need to listen for new token notifications, since there is no way to
  // check the actual value of the token in the token service.
  std::unique_ptr<DiceTestTokenServiceObserver> token_service_observer =
      base::MakeUnique<DiceTestTokenServiceObserver>(
          dice_params.signin_info.gaia_id);
  ScopedObserver<ProfileOAuth2TokenService, DiceTestTokenServiceObserver>
      scoped_token_service_observer(token_service_observer.get());
  scoped_token_service_observer.Add(&token_service_);
  // Simulate GaiaAuthFetcher success.
  signin_client_.consumer_->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("refresh_token", "access_token", 10));
  // Check that the token has not been inserted in the token service.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(token_service_observer->token_received());
  // Check that the reconcilor was not blocked or unblocked when fixing auth
  // errors.
  EXPECT_EQ(0, reconcilor_blocked_count_);
  EXPECT_EQ(0, reconcilor_unblocked_count_);
}

// Tests that the Dice Signout response is ignored when kDiceFixAuthErrors is
// used.
TEST_F(DiceResponseHandlerTest, FixAuthErroDoesNotSignout) {
  signin::ScopedAccountConsistencyDiceFixAuthErrors scoped_dice_fix_auth_errors;
  DiceResponseParams dice_params = MakeDiceParams(DiceAction::SIGNOUT);
  std::string account_id = account_tracker_service_.PickAccountIdForAccount(
      dice_params.signout_info.gaia_id[0], dice_params.signout_info.email[0]);
  // User is signed in to Chrome, and has some refresh token for a secondary
  // account.
  signin_manager_.SignIn(dice_params.signout_info.gaia_id[0],
                         dice_params.signout_info.email[0], "password");
  token_service_.UpdateCredentials(account_id, "token1");
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());
  // Receive signout response for the main account.
  dice_response_handler_->ProcessDiceHeader(
      dice_params, base::MakeUnique<TestProcessDiceHeaderObserver>(this));
  // User is not signed out from Chrome.
  EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(signin_manager_.IsAuthenticated());
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
