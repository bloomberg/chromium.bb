// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/account_tracker.h"

#include <algorithm>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if defined(OS_CHROMEOS)
using SigninManagerForTest = FakeSigninManagerBase;
#else
using SigninManagerForTest = FakeSigninManager;
#endif  // OS_CHROMEOS

const char kPrimaryAccountEmail[] = "primary_account@example.com";
const char kPrimaryAccountGaiaId[] = "primary_account";

enum TrackingEventType { SIGN_IN, SIGN_OUT };

std::string AccountKeyToObfuscatedId(const std::string& email) {
  return "obfid-" + email;
}

class TrackingEvent {
 public:
  TrackingEvent(TrackingEventType type,
                const std::string& account_key,
                const std::string& gaia_id)
      : type_(type), account_key_(account_key), gaia_id_(gaia_id) {}

  TrackingEvent(TrackingEventType type, const std::string& account_key)
      : type_(type),
        account_key_(account_key),
        gaia_id_(AccountKeyToObfuscatedId(account_key)) {}

  bool operator==(const TrackingEvent& event) const {
    return type_ == event.type_ && account_key_ == event.account_key_ &&
           gaia_id_ == event.gaia_id_;
  }

  std::string ToString() const {
    const char* typestr = "INVALID";
    switch (type_) {
      case SIGN_IN:
        typestr = " IN";
        break;
      case SIGN_OUT:
        typestr = "OUT";
        break;
    }
    return base::StringPrintf("{ type: %s, email: %s, gaia: %s }", typestr,
                              account_key_.c_str(), gaia_id_.c_str());
  }

 private:
  friend bool CompareByUser(TrackingEvent a, TrackingEvent b);

  TrackingEventType type_;
  std::string account_key_;
  std::string gaia_id_;
};

bool CompareByUser(TrackingEvent a, TrackingEvent b) {
  return a.account_key_ < b.account_key_;
}

std::string Str(const std::vector<TrackingEvent>& events) {
  std::string str = "[";
  bool needs_comma = false;
  for (std::vector<TrackingEvent>::const_iterator it = events.begin();
       it != events.end(); ++it) {
    if (needs_comma)
      str += ",\n ";
    needs_comma = true;
    str += it->ToString();
  }
  str += "]";
  return str;
}

}  // namespace

namespace gcm {

class AccountTrackerObserver : public AccountTracker::Observer {
 public:
  AccountTrackerObserver() {}
  virtual ~AccountTrackerObserver() {}

  testing::AssertionResult CheckEvents();
  testing::AssertionResult CheckEvents(const TrackingEvent& e1);
  testing::AssertionResult CheckEvents(const TrackingEvent& e1,
                                       const TrackingEvent& e2);
  testing::AssertionResult CheckEvents(const TrackingEvent& e1,
                                       const TrackingEvent& e2,
                                       const TrackingEvent& e3);
  testing::AssertionResult CheckEvents(const TrackingEvent& e1,
                                       const TrackingEvent& e2,
                                       const TrackingEvent& e3,
                                       const TrackingEvent& e4);
  testing::AssertionResult CheckEvents(const TrackingEvent& e1,
                                       const TrackingEvent& e2,
                                       const TrackingEvent& e3,
                                       const TrackingEvent& e4,
                                       const TrackingEvent& e5);
  testing::AssertionResult CheckEvents(const TrackingEvent& e1,
                                       const TrackingEvent& e2,
                                       const TrackingEvent& e3,
                                       const TrackingEvent& e4,
                                       const TrackingEvent& e5,
                                       const TrackingEvent& e6);
  void Clear();
  void SortEventsByUser();

  // AccountTracker::Observer implementation
  void OnAccountSignInChanged(const AccountIds& ids,
                              bool is_signed_in) override;

 private:
  testing::AssertionResult CheckEvents(
      const std::vector<TrackingEvent>& events);

  std::vector<TrackingEvent> events_;
};

void AccountTrackerObserver::OnAccountSignInChanged(const AccountIds& ids,
                                                    bool is_signed_in) {
  events_.push_back(
      TrackingEvent(is_signed_in ? SIGN_IN : SIGN_OUT, ids.email, ids.gaia));
}

void AccountTrackerObserver::Clear() {
  events_.clear();
}

void AccountTrackerObserver::SortEventsByUser() {
  std::stable_sort(events_.begin(), events_.end(), CompareByUser);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents() {
  std::vector<TrackingEvent> events;
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1,
    const TrackingEvent& e2) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  events.push_back(e2);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1,
    const TrackingEvent& e2,
    const TrackingEvent& e3) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  events.push_back(e2);
  events.push_back(e3);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1,
    const TrackingEvent& e2,
    const TrackingEvent& e3,
    const TrackingEvent& e4) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  events.push_back(e2);
  events.push_back(e3);
  events.push_back(e4);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1,
    const TrackingEvent& e2,
    const TrackingEvent& e3,
    const TrackingEvent& e4,
    const TrackingEvent& e5) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  events.push_back(e2);
  events.push_back(e3);
  events.push_back(e4);
  events.push_back(e5);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1,
    const TrackingEvent& e2,
    const TrackingEvent& e3,
    const TrackingEvent& e4,
    const TrackingEvent& e5,
    const TrackingEvent& e6) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  events.push_back(e2);
  events.push_back(e3);
  events.push_back(e4);
  events.push_back(e5);
  events.push_back(e6);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const std::vector<TrackingEvent>& events) {
  std::string maybe_newline = (events.size() + events_.size()) > 2 ? "\n" : "";
  testing::AssertionResult result(
      (events_ == events)
          ? testing::AssertionSuccess()
          : (testing::AssertionFailure()
             << "Expected " << maybe_newline << Str(events) << ", "
             << maybe_newline << "Got " << maybe_newline << Str(events_)));
  events_.clear();
  return result;
}

class AccountTrackerTest : public testing::Test {
 public:
  AccountTrackerTest() {}

  ~AccountTrackerTest() override {}

  void SetUp() override {
    fake_oauth2_token_service_.reset(new FakeProfileOAuth2TokenService());

    test_signin_client_.reset(new TestSigninClient(&pref_service_));
#if defined(OS_CHROMEOS)
    fake_signin_manager_.reset(new SigninManagerForTest(
        test_signin_client_.get(), &account_tracker_service_));
#else
    fake_signin_manager_.reset(new SigninManagerForTest(
        test_signin_client_.get(), fake_oauth2_token_service_.get(),
        &account_tracker_service_, nullptr));
#endif

    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    SigninManagerBase::RegisterProfilePrefs(pref_service_.registry());
    SigninManagerBase::RegisterPrefs(pref_service_.registry());
    account_tracker_service_.Initialize(test_signin_client_.get());

    account_tracker_.reset(new AccountTracker(
        fake_signin_manager_.get(), fake_oauth2_token_service_.get(),
        new net::TestURLRequestContextGetter(message_loop_.task_runner())));
    account_tracker_->AddObserver(&observer_);
  }

  void TearDown() override {
    account_tracker_->RemoveObserver(&observer_);
    account_tracker_->Shutdown();
  }

  AccountTrackerObserver* observer() { return &observer_; }

  AccountTracker* account_tracker() { return account_tracker_.get(); }

  // Helpers to pass fake events to the tracker.

  // Sets the primary account info but carries no guarantee of firing the
  // callback that signin occurred (see NotifyLogin() below if exercising a
  // true signin flow in a non-ChromeOS context). Returns the account ID of the
  // newly-set account.
  std::string SetActiveAccount(const std::string& gaia_id,
                               const std::string& email) {
#if defined(OS_CHROMEOS)
    fake_signin_manager_->SignIn(email);
#else
    fake_signin_manager_->SignIn(gaia_id, email, "" /* password */);
#endif

    return fake_signin_manager_->GetAuthenticatedAccountId();
  }

// NOTE: On ChromeOS, the login callback is never fired in production (since the
// underlying GoogleSigninSucceeded callback is never sent). Tests that
// exercise functionality dependent on that callback firing are not relevant
// on ChromeOS and should simply not run on that platform.
#if !defined(OS_CHROMEOS)
  // Sets the primary account info and fires the callback that signin occurred.
  // Returns the account ID of the newly-set account.
  std::string NotifyLogin(const std::string& gaia_id,
                          const std::string& email) {
    fake_signin_manager_->SignIn(gaia_id, email, "" /* password */);
    return fake_signin_manager_->GetAuthenticatedAccountId();
  }

  void NotifyLogoutOfPrimaryAccountOnly() {
    fake_signin_manager_->SignOutAndKeepAllAccounts(
        signin_metrics::SIGNOUT_TEST,
        signin_metrics::SignoutDelete::IGNORE_METRIC);
  }

  void NotifyLogoutOfAllAccounts() {
    fake_signin_manager_->SignOutAndRemoveAllAccounts(
        signin_metrics::SIGNOUT_TEST,
        signin_metrics::SignoutDelete::IGNORE_METRIC);
  }
#endif

  std::string AddAccountWithToken(const std::string& gaia,
                                  const std::string& email) {
    std::string account_id =
        account_tracker_service_.SeedAccountInfo(gaia, email);
    fake_oauth2_token_service_->UpdateCredentials(account_id,
                                                  "fake_refresh_token");
    return account_id;
  }

  void NotifyTokenAvailable(const std::string& account_id) {
    fake_oauth2_token_service_->UpdateCredentials(account_id,
                                                  "fake_refresh_token");
  }

  void NotifyTokenRevoked(const std::string& account_id) {
    fake_oauth2_token_service_->RevokeCredentials(account_id);
  }

  // Helpers to fake access token and user info fetching
  void IssueAccessToken(const std::string& account_id) {
    fake_oauth2_token_service_->IssueAllTokensForAccount(
        account_id, "access_token-" + account_id, base::Time::Max());
  }

  std::string GetValidTokenInfoResponse(const std::string& account_id) {
    return std::string("{ \"id\": \"") + AccountKeyToObfuscatedId(account_id) +
           "\" }";
  }

  void ReturnOAuthUrlFetchResults(int fetcher_id,
                                  net::HttpStatusCode response_code,
                                  const std::string& response_string);

  void ReturnOAuthUrlFetchSuccess(const std::string& account_key);
  void ReturnOAuthUrlFetchFailure(const std::string& account_key);

  std::string SetupPrimaryLogin() {
    // Initial setup for tests that start with a signed in profile.
    std::string primary_account_id =
        SetActiveAccount(kPrimaryAccountGaiaId, kPrimaryAccountEmail);
    NotifyTokenAvailable(primary_account_id);
    ReturnOAuthUrlFetchSuccess(primary_account_id);
    observer()->Clear();

    return primary_account_id;
  }

 private:
  base::MessageLoopForIO message_loop_;  // net:: stuff needs IO message loop.
  net::TestURLFetcherFactory test_fetcher_factory_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  AccountTrackerService account_tracker_service_;
  std::unique_ptr<TestSigninClient> test_signin_client_;
  std::unique_ptr<SigninManagerForTest> fake_signin_manager_;
  std::unique_ptr<FakeProfileOAuth2TokenService> fake_oauth2_token_service_;

  std::unique_ptr<AccountTracker> account_tracker_;
  AccountTrackerObserver observer_;
};

void AccountTrackerTest::ReturnOAuthUrlFetchResults(
    int fetcher_id,
    net::HttpStatusCode response_code,
    const std::string& response_string) {
  net::TestURLFetcher* fetcher =
      test_fetcher_factory_.GetFetcherByID(fetcher_id);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(response_code);
  fetcher->SetResponseString(response_string);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

void AccountTrackerTest::ReturnOAuthUrlFetchSuccess(
    const std::string& account_id) {
  IssueAccessToken(account_id);
  ReturnOAuthUrlFetchResults(gaia::GaiaOAuthClient::kUrlFetcherId, net::HTTP_OK,
                             GetValidTokenInfoResponse(account_id));
}

void AccountTrackerTest::ReturnOAuthUrlFetchFailure(
    const std::string& account_id) {
  IssueAccessToken(account_id);
  ReturnOAuthUrlFetchResults(gaia::GaiaOAuthClient::kUrlFetcherId,
                             net::HTTP_BAD_REQUEST, "");
}

// Primary tests just involve the Active account

TEST_F(AccountTrackerTest, PrimaryNoEventsBeforeLogin) {
  NotifyTokenAvailable("dummy_account_id");
  NotifyTokenRevoked("dummy_account_id");

// Logout is not possible on ChromeOS.
#if !defined(OS_CHROMEOS)
  NotifyLogoutOfAllAccounts();
#endif

  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, PrimaryLoginThenTokenAvailable) {
  std::string primary_account_id =
      SetActiveAccount(kPrimaryAccountGaiaId, kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  EXPECT_TRUE(observer()->CheckEvents());

  ReturnOAuthUrlFetchSuccess(primary_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_IN, primary_account_id)));
}

#if !defined(OS_CHROMEOS)
TEST_F(AccountTrackerTest, PrimaryTokenAvailableThenLogin) {
  AddAccountWithToken(kPrimaryAccountGaiaId, kPrimaryAccountEmail);
  EXPECT_TRUE(observer()->CheckEvents());

  std::string primary_account_id =
      NotifyLogin(kPrimaryAccountGaiaId, kPrimaryAccountEmail);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_IN, primary_account_id)));
}

TEST_F(AccountTrackerTest, PrimaryTokenAvailableAndRevokedThenLogin) {
  std::string primary_account_id =
      AddAccountWithToken(kPrimaryAccountGaiaId, kPrimaryAccountEmail);
  EXPECT_TRUE(observer()->CheckEvents());

  NotifyTokenRevoked(primary_account_id);
  EXPECT_TRUE(observer()->CheckEvents());

  NotifyLogin(kPrimaryAccountGaiaId, kPrimaryAccountEmail);
  EXPECT_TRUE(observer()->CheckEvents());
}
#endif

TEST_F(AccountTrackerTest, PrimaryRevoke) {
  std::string primary_account_id =
      SetActiveAccount(kPrimaryAccountGaiaId, kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  observer()->Clear();

  NotifyTokenRevoked(primary_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, primary_account_id)));
}

TEST_F(AccountTrackerTest, PrimaryRevokeThenLogin) {
  std::string primary_account_id =
      SetActiveAccount(kPrimaryAccountGaiaId, kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  NotifyTokenRevoked(primary_account_id);
  observer()->Clear();

  SetActiveAccount(kPrimaryAccountGaiaId, kPrimaryAccountEmail);
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, PrimaryRevokeThenTokenAvailable) {
  std::string primary_account_id =
      SetActiveAccount(kPrimaryAccountGaiaId, kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  NotifyTokenRevoked(primary_account_id);
  observer()->Clear();

  NotifyTokenAvailable(primary_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_IN, primary_account_id)));
}

// These tests exercise true login/logout, which are not possible on ChromeOS.
#if !defined(OS_CHROMEOS)
TEST_F(AccountTrackerTest, PrimaryLogoutThenRevoke) {
  std::string primary_account_id =
      NotifyLogin(kPrimaryAccountGaiaId, kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  observer()->Clear();

  NotifyLogoutOfAllAccounts();
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, primary_account_id)));

  NotifyTokenRevoked(primary_account_id);
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, PrimaryLogoutFetchCancelAvailable) {
  std::string primary_account_id =
      NotifyLogin(kPrimaryAccountGaiaId, kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  // TokenAvailable kicks off a fetch. Logout without satisfying it.
  NotifyLogoutOfAllAccounts();
  EXPECT_TRUE(observer()->CheckEvents());

  SetActiveAccount(kPrimaryAccountGaiaId, kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_IN, primary_account_id)));
}
#endif

// Non-primary accounts

TEST_F(AccountTrackerTest, Available) {
  SetupPrimaryLogin();

  std::string account_id = AddAccountWithToken("user", "user@example.com");
  EXPECT_TRUE(observer()->CheckEvents());

  ReturnOAuthUrlFetchSuccess(account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account_id)));
}

TEST_F(AccountTrackerTest, AvailableRevokeAvailable) {
  SetupPrimaryLogin();

  std::string account_id = AddAccountWithToken("user", "user@example.com");
  ReturnOAuthUrlFetchSuccess(account_id);
  NotifyTokenRevoked(account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account_id),
                                      TrackingEvent(SIGN_OUT, account_id)));

  NotifyTokenAvailable(account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account_id)));
}

TEST_F(AccountTrackerTest, AvailableRevokeAvailableWithPendingFetch) {
  SetupPrimaryLogin();

  std::string account_id = AddAccountWithToken("user", "user@example.com");
  NotifyTokenRevoked(account_id);
  EXPECT_TRUE(observer()->CheckEvents());

  NotifyTokenAvailable(account_id);
  ReturnOAuthUrlFetchSuccess(account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account_id)));
}

TEST_F(AccountTrackerTest, AvailableRevokeRevoke) {
  SetupPrimaryLogin();

  std::string account_id = AddAccountWithToken("user", "user@example.com");
  ReturnOAuthUrlFetchSuccess(account_id);
  NotifyTokenRevoked(account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account_id),
                                      TrackingEvent(SIGN_OUT, account_id)));

  NotifyTokenRevoked(account_id);
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, AvailableAvailable) {
  SetupPrimaryLogin();

  std::string account_id = AddAccountWithToken("user", "user@example.com");
  ReturnOAuthUrlFetchSuccess(account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account_id)));

  NotifyTokenAvailable(account_id);
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, TwoAccounts) {
  SetupPrimaryLogin();

  std::string alpha_account_id =
      AddAccountWithToken("alpha", "alpha@example.com");
  ReturnOAuthUrlFetchSuccess(alpha_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_IN, alpha_account_id)));

  std::string beta_account_id = AddAccountWithToken("beta", "beta@example.com");
  ReturnOAuthUrlFetchSuccess(beta_account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, beta_account_id)));

  NotifyTokenRevoked(alpha_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, alpha_account_id)));

  NotifyTokenRevoked(beta_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, beta_account_id)));
}

TEST_F(AccountTrackerTest, AvailableTokenFetchFailAvailable) {
  SetupPrimaryLogin();

  std::string account_id = AddAccountWithToken("user", "user@example.com");
  ReturnOAuthUrlFetchFailure(account_id);
  EXPECT_TRUE(observer()->CheckEvents());

  NotifyTokenAvailable(account_id);
  ReturnOAuthUrlFetchSuccess(account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account_id)));
}

// These tests exercise true login/logout, which are not possible on ChromeOS.
#if !defined(OS_CHROMEOS)
TEST_F(AccountTrackerTest, MultiSignOutSignIn) {
  std::string primary_account_id = SetupPrimaryLogin();

  std::string alpha_account_id =
      AddAccountWithToken("alpha", "alpha@example.com");
  ReturnOAuthUrlFetchSuccess(alpha_account_id);
  std::string beta_account_id = AddAccountWithToken("beta", "beta@example.com");
  ReturnOAuthUrlFetchSuccess(beta_account_id);

  observer()->SortEventsByUser();
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, alpha_account_id),
                                      TrackingEvent(SIGN_IN, beta_account_id)));

  // Log out of the primary account only (allows for testing that the account
  // tracker preserves knowledge of "beta@example.com").
  NotifyLogoutOfPrimaryAccountOnly();
  observer()->SortEventsByUser();
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, alpha_account_id),
                              TrackingEvent(SIGN_OUT, beta_account_id),
                              TrackingEvent(SIGN_OUT, primary_account_id)));

  // No events fire at all while profile is signed out.
  NotifyTokenRevoked(alpha_account_id);
  std::string gamma_account_id =
      AddAccountWithToken("gamma", "gamma@example.com");
  EXPECT_TRUE(observer()->CheckEvents());

  // Signing the profile in again will resume tracking all accounts.
  NotifyLogin(kPrimaryAccountGaiaId, kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  ReturnOAuthUrlFetchSuccess(beta_account_id);
  ReturnOAuthUrlFetchSuccess(gamma_account_id);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  observer()->SortEventsByUser();
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_IN, beta_account_id),
                              TrackingEvent(SIGN_IN, gamma_account_id),
                              TrackingEvent(SIGN_IN, primary_account_id)));

  // Revoking the primary token does not affect other accounts.
  NotifyTokenRevoked(primary_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, primary_account_id)));

  NotifyTokenAvailable(primary_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_IN, primary_account_id)));
}
#endif

// Primary/non-primary interactions

TEST_F(AccountTrackerTest, MultiNoEventsBeforeLogin) {
  NotifyTokenAvailable("dummy_account_id");
  std::string account_id = AddAccountWithToken("user", "user@example.com");
  NotifyTokenRevoked(account_id);
  NotifyTokenRevoked("dummy_account_id");

// Logout is not possible on ChromeOS.
#if !defined(OS_CHROMEOS)
  NotifyLogoutOfAllAccounts();
#endif

  EXPECT_TRUE(observer()->CheckEvents());
}

// This test exercises true login/logout, which are not possible on ChromeOS.
#if !defined(OS_CHROMEOS)
TEST_F(AccountTrackerTest, MultiLogoutRemovesAllAccounts) {
  std::string primary_account_id =
      NotifyLogin(kPrimaryAccountGaiaId, kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  std::string account_id = AddAccountWithToken("user", "user@example.com");
  ReturnOAuthUrlFetchSuccess(account_id);
  observer()->Clear();

  NotifyLogoutOfAllAccounts();
  observer()->SortEventsByUser();
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, primary_account_id),
                              TrackingEvent(SIGN_OUT, account_id)));
}
#endif

TEST_F(AccountTrackerTest, MultiRevokePrimaryDoesNotRemoveAllAccounts) {
  std::string primary_account_id =
      SetActiveAccount(kPrimaryAccountGaiaId, kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  std::string account_id = AddAccountWithToken("user", "user@example.com");
  ReturnOAuthUrlFetchSuccess(account_id);
  observer()->Clear();

  NotifyTokenRevoked(primary_account_id);
  observer()->SortEventsByUser();
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, primary_account_id)));
}

TEST_F(AccountTrackerTest, GetAccountsPrimary) {
  std::string primary_account_id = SetupPrimaryLogin();

  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(1ul, ids.size());
  EXPECT_EQ(primary_account_id, ids[0].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(primary_account_id), ids[0].gaia);
}

TEST_F(AccountTrackerTest, GetAccountsSignedOut) {
  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(0ul, ids.size());
}

TEST_F(AccountTrackerTest, GetAccountsOnlyReturnAccountsWithTokens) {
  std::string primary_account_id = SetupPrimaryLogin();

  std::string alpha_account_id =
      AddAccountWithToken("alpha", "alpha@example.com");
  std::string beta_account_id = AddAccountWithToken("beta", "beta@example.com");
  ReturnOAuthUrlFetchSuccess(beta_account_id);

  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(2ul, ids.size());
  EXPECT_EQ(primary_account_id, ids[0].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(primary_account_id), ids[0].gaia);
  EXPECT_EQ(beta_account_id, ids[1].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(beta_account_id), ids[1].gaia);
}

TEST_F(AccountTrackerTest, GetAccountsSortOrder) {
  std::string primary_account_id = SetupPrimaryLogin();

  std::string zeta_account_id = AddAccountWithToken("zeta", "zeta@example.com");
  ReturnOAuthUrlFetchSuccess(zeta_account_id);
  std::string alpha_account_id =
      AddAccountWithToken("alpha", "alpha@example.com");
  ReturnOAuthUrlFetchSuccess(alpha_account_id);

  // The primary account will be first in the vector. Remaining accounts
  // will be sorted by gaia ID.
  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(3ul, ids.size());
  EXPECT_EQ(primary_account_id, ids[0].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(primary_account_id), ids[0].gaia);
  EXPECT_EQ(alpha_account_id, ids[1].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(alpha_account_id), ids[1].gaia);
  EXPECT_EQ(zeta_account_id, ids[2].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(zeta_account_id), ids[2].gaia);
}

TEST_F(AccountTrackerTest, GetAccountsReturnNothingWhenPrimarySignedOut) {
  std::string primary_account_id = SetupPrimaryLogin();

  std::string zeta_account_id = AddAccountWithToken("zeta", "zeta@example.com");
  ReturnOAuthUrlFetchSuccess(zeta_account_id);
  std::string alpha_account_id =
      AddAccountWithToken("alpha", "alpha@example.com");
  ReturnOAuthUrlFetchSuccess(alpha_account_id);

  NotifyTokenRevoked(primary_account_id);

  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(0ul, ids.size());
}

}  // namespace gcm
