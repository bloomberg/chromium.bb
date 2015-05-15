// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/stringprintf.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::string kTokenInfoResponseFormat =
    "{                        \
      \"id\": \"%s\",         \
      \"email\": \"%s\",      \
      \"hd\": \"\",           \
      \"name\": \"%s\",       \
      \"given_name\": \"%s\", \
      \"locale\": \"%s\"      \
    }";

const std::string kTokenInfoIncompleteResponseFormat =
    "{                        \
      \"id\": \"%s\",         \
      \"email\": \"%s\",      \
      \"hd\": \"\",           \
    }";

const std::string kLSIDResponse = "{ lsid: \"Foo\" }";

const std::string kServiceFlags = "allServices=Service1,Service2";

enum TrackingEventType {
  UPDATED,
  REMOVED,
};

std::string AccountIdToEmail(const std::string account_id) {
  return account_id + "@gmail.com";
}

std::string AccountIdToGaiaId(const std::string account_id) {
  return "gaia-" + account_id;
}

std::string AccountIdToFullName(const std::string account_id) {
  return "full-name-" + account_id;
}

std::string AccountIdToGivenName(const std::string account_id) {
  return "given-name-" + account_id;
}

std::string AccountIdToLocale(const std::string account_id) {
  return "locale-" + account_id;
}

void CheckAccountDetails(const std::string account_id,
                         const AccountTrackerService::AccountInfo& info) {
  EXPECT_EQ(account_id, info.account_id);
  EXPECT_EQ(AccountIdToGaiaId(account_id), info.gaia);
  EXPECT_EQ(AccountIdToEmail(account_id), info.email);
  EXPECT_EQ(AccountTrackerService::kNoHostedDomainFound,
            info.hosted_domain);
  EXPECT_EQ(AccountIdToFullName(account_id), info.full_name);
  EXPECT_EQ(AccountIdToGivenName(account_id), info.given_name);
  EXPECT_EQ(AccountIdToLocale(account_id), info.locale);
  EXPECT_EQ(2U, info.service_flags.size());
  EXPECT_EQ("Service1", info.service_flags[0]);
  EXPECT_EQ("Service2", info.service_flags[1]);
}

class TrackingEvent {
 public:
  TrackingEvent(TrackingEventType type,
                const std::string& account_id,
                const std::string& gaia_id)
      : type_(type),
        account_id_(account_id),
        gaia_id_(gaia_id) {}

  TrackingEvent(TrackingEventType type,
                const std::string& account_id)
      : type_(type),
        account_id_(account_id),
        gaia_id_(AccountIdToGaiaId(account_id)) {}

  bool operator==(const TrackingEvent& event) const {
    return type_ == event.type_ && account_id_ == event.account_id_ &&
        gaia_id_ == event.gaia_id_;
  }

  std::string ToString() const {
    const char * typestr = "INVALID";
    switch (type_) {
      case UPDATED:
        typestr = "UPD";
        break;
      case REMOVED:
        typestr = "REM";
        break;
    }
    return base::StringPrintf("{ type: %s, account_id: %s, gaia: %s }",
                              typestr,
                              account_id_.c_str(),
                              gaia_id_.c_str());
  }

 private:
  friend bool CompareByUser(TrackingEvent a, TrackingEvent b);

  TrackingEventType type_;
  std::string account_id_;
  std::string gaia_id_;
};

bool CompareByUser(TrackingEvent a, TrackingEvent b) {
  return a.account_id_ < b.account_id_;
}

std::string Str(const std::vector<TrackingEvent>& events) {
  std::string str = "[";
  bool needs_comma = false;
  for (std::vector<TrackingEvent>::const_iterator it =
       events.begin(); it != events.end(); ++it) {
    if (needs_comma)
      str += ",\n ";
    needs_comma = true;
    str += it->ToString();
  }
  str += "]";
  return str;
}

class AccountTrackerObserver : public AccountTrackerService::Observer {
 public:
  AccountTrackerObserver() {}
  ~AccountTrackerObserver() override {}

  void Clear();
  void SortEventsByUser();

  testing::AssertionResult CheckEvents();
  testing::AssertionResult CheckEvents(const TrackingEvent& e1);
  testing::AssertionResult CheckEvents(const TrackingEvent& e1,
                                       const TrackingEvent& e2);
  testing::AssertionResult CheckEvents(const TrackingEvent& e1,
                                       const TrackingEvent& e2,
                                       const TrackingEvent& e3);

 private:
  // AccountTrackerService::Observer implementation
  void OnAccountUpdated(const AccountTrackerService::AccountInfo& ids) override;
  void OnAccountRemoved(const AccountTrackerService::AccountInfo& ids) override;

  testing::AssertionResult CheckEvents(
      const std::vector<TrackingEvent>& events);

  std::vector<TrackingEvent> events_;
};

void AccountTrackerObserver::OnAccountUpdated(
    const AccountTrackerService::AccountInfo& ids) {
  events_.push_back(TrackingEvent(UPDATED, ids.account_id, ids.gaia));
}

void AccountTrackerObserver::OnAccountRemoved(
    const AccountTrackerService::AccountInfo& ids) {
  events_.push_back(TrackingEvent(REMOVED, ids.account_id, ids.gaia));
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

}  // namespace

class AccountTrackerServiceTest : public testing::Test {
 public:
  AccountTrackerServiceTest() {}

  ~AccountTrackerServiceTest() override {}

  void SetUp() override {
    fake_oauth2_token_service_.reset(new FakeOAuth2TokenService());

    pref_service_.registry()->RegisterListPref(
        AccountTrackerService::kAccountInfoPref);
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kAccountIdMigrationState,
        AccountTrackerService::MIGRATION_NOT_STARTED);
    pref_service_.registry()->RegisterInt64Pref(
        AccountTrackerService::kAccountTrackerServiceLastUpdate, 0);
    signin_client_.reset(new TestSigninClient(&pref_service_));
    signin_client_.get()->SetURLRequestContext(
        new net::TestURLRequestContextGetter(
            message_loop_.message_loop_proxy()));

    account_tracker_.reset(new AccountTrackerService());
    account_tracker_->Initialize(fake_oauth2_token_service_.get(),
                                 signin_client_.get());
    account_tracker_->EnableNetworkFetches();
    account_tracker_->AddObserver(&observer_);
  }

  void TearDown() override {
    account_tracker_->RemoveObserver(&observer_);
    account_tracker_->Shutdown();
  }

  void SimulateTokenAvailable(const std::string& account_id) {
    fake_oauth2_token_service_->AddAccount(account_id);
  }

  void SimulateTokenRevoked(const std::string& account_id) {
    fake_oauth2_token_service_->RemoveAccount(account_id);
  }

  // Helpers to fake access token and user info fetching
  void IssueAccessToken(const std::string& account_id) {
    fake_oauth2_token_service_->IssueAllTokensForAccount(
        account_id, "access_token-" + account_id, base::Time::Max());
  }

  std::string GenerateValidTokenInfoResponse(const std::string& account_id) {
    return base::StringPrintf(
        kTokenInfoResponseFormat.c_str(),
        AccountIdToGaiaId(account_id).c_str(),
        AccountIdToEmail(account_id).c_str(),
        AccountIdToFullName(account_id).c_str(),
        AccountIdToGivenName(account_id).c_str(),
        AccountIdToLocale(account_id).c_str());
  }

  std::string GenerateIncompleteTokenInfoResponse(
      const std::string& account_id) {
    return base::StringPrintf(
        kTokenInfoIncompleteResponseFormat.c_str(),
        AccountIdToGaiaId(account_id).c_str(),
        AccountIdToEmail(account_id).c_str());
  }
  void ReturnOAuthUrlFetchSuccess(const std::string& account_id);
  void ReturnOAuthUrlFetchSuccessIncomplete(const std::string& account_id);
  void ReturnOAuthUrlFetchFailure(const std::string& account_id);

  net::TestURLFetcherFactory* test_fetcher_factory() {
    return &test_fetcher_factory_;
  }
  AccountTrackerService* account_tracker() { return account_tracker_.get(); }
  AccountTrackerObserver* observer() { return &observer_; }
  OAuth2TokenService* token_service() {
    return fake_oauth2_token_service_.get();
  }
  SigninClient* signin_client() { return signin_client_.get(); }

 private:
  void ReturnOAuthUrlFetchResults(int fetcher_id,
                                  net::HttpStatusCode response_code,
                                  const std::string& response_string);

  base::MessageLoopForIO message_loop_;
  net::TestURLFetcherFactory test_fetcher_factory_;
  scoped_ptr<FakeOAuth2TokenService> fake_oauth2_token_service_;
  TestingPrefServiceSimple pref_service_;
  scoped_ptr<AccountTrackerService> account_tracker_;
  AccountTrackerObserver observer_;
  scoped_ptr<TestSigninClient> signin_client_;
};

void AccountTrackerServiceTest::ReturnOAuthUrlFetchResults(
    int fetcher_id,
    net::HttpStatusCode response_code,
    const std::string&  response_string) {
  net::TestURLFetcher* fetcher =
      test_fetcher_factory_.GetFetcherByID(fetcher_id);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(response_code);
  fetcher->SetResponseString(response_string);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

void AccountTrackerServiceTest::ReturnOAuthUrlFetchSuccess(
    const std::string& account_id) {
  IssueAccessToken(account_id);
  ReturnOAuthUrlFetchResults(gaia::GaiaOAuthClient::kUrlFetcherId,
                             net::HTTP_OK,
                             GenerateValidTokenInfoResponse(account_id));
  ReturnOAuthUrlFetchResults(0, net::HTTP_OK, kLSIDResponse);
  ReturnOAuthUrlFetchResults(0, net::HTTP_OK, kServiceFlags);
}

void AccountTrackerServiceTest::ReturnOAuthUrlFetchSuccessIncomplete(
    const std::string& account_id) {
  IssueAccessToken(account_id);
  ReturnOAuthUrlFetchResults(gaia::GaiaOAuthClient::kUrlFetcherId,
                             net::HTTP_OK,
                             GenerateIncompleteTokenInfoResponse(account_id));
}

void AccountTrackerServiceTest::ReturnOAuthUrlFetchFailure(
    const std::string& account_id) {
  IssueAccessToken(account_id);
  ReturnOAuthUrlFetchResults(
      gaia::GaiaOAuthClient::kUrlFetcherId, net::HTTP_BAD_REQUEST, "");
}

TEST_F(AccountTrackerServiceTest, Basic) {
}

TEST_F(AccountTrackerServiceTest, TokenAvailable) {
  SimulateTokenAvailable("alpha");
  ASSERT_FALSE(account_tracker()->IsAllUserInfoFetched());
  ASSERT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerServiceTest, TokenAvailable_Revoked) {
  SimulateTokenAvailable("alpha");
  SimulateTokenRevoked("alpha");
  ASSERT_TRUE(account_tracker()->IsAllUserInfoFetched());
  ASSERT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerServiceTest, TokenAvailable_UserInfo) {
  SimulateTokenAvailable("alpha");
  ReturnOAuthUrlFetchSuccess("alpha");
  ASSERT_TRUE(account_tracker()->IsAllUserInfoFetched());
  ASSERT_TRUE(observer()->CheckEvents(TrackingEvent(UPDATED, "alpha")));
}

TEST_F(AccountTrackerServiceTest, TokenAvailable_UserInfo_Revoked) {
  SimulateTokenAvailable("alpha");
  ReturnOAuthUrlFetchSuccess("alpha");
  ASSERT_TRUE(account_tracker()->IsAllUserInfoFetched());
  ASSERT_TRUE(observer()->CheckEvents(TrackingEvent(UPDATED, "alpha")));
  SimulateTokenRevoked("alpha");
  ASSERT_TRUE(observer()->CheckEvents(TrackingEvent(REMOVED, "alpha")));
}

TEST_F(AccountTrackerServiceTest, TokenAvailable_UserInfoFailed) {
  SimulateTokenAvailable("alpha");
  ReturnOAuthUrlFetchFailure("alpha");
  ASSERT_TRUE(account_tracker()->IsAllUserInfoFetched());
  ASSERT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerServiceTest, TokenAvailableTwice_UserInfoOnce) {
  SimulateTokenAvailable("alpha");
  ReturnOAuthUrlFetchSuccess("alpha");
  ASSERT_TRUE(account_tracker()->IsAllUserInfoFetched());
  ASSERT_TRUE(observer()->CheckEvents(TrackingEvent(UPDATED, "alpha")));

  SimulateTokenAvailable("alpha");
  ASSERT_TRUE(account_tracker()->IsAllUserInfoFetched());
  ASSERT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerServiceTest, TokenAlreadyExists) {
  SimulateTokenAvailable("alpha");
  AccountTrackerService tracker;
  AccountTrackerObserver observer;
  tracker.AddObserver(&observer);
  tracker.Initialize(token_service(), signin_client());
  tracker.EnableNetworkFetches();
  ASSERT_FALSE(tracker.IsAllUserInfoFetched());
  ASSERT_TRUE(observer.CheckEvents());
  tracker.RemoveObserver(&observer);
  tracker.Shutdown();
}

TEST_F(AccountTrackerServiceTest, TwoTokenAvailable_TwoUserInfo) {
  SimulateTokenAvailable("alpha");
  SimulateTokenAvailable("beta");
  ReturnOAuthUrlFetchSuccess("alpha");
  ReturnOAuthUrlFetchSuccess("beta");
  ASSERT_TRUE(account_tracker()->IsAllUserInfoFetched());
  ASSERT_TRUE(observer()->CheckEvents(TrackingEvent(UPDATED, "alpha"),
                                      TrackingEvent(UPDATED, "beta")));
}

TEST_F(AccountTrackerServiceTest, TwoTokenAvailable_OneUserInfo) {
  SimulateTokenAvailable("alpha");
  SimulateTokenAvailable("beta");
  ReturnOAuthUrlFetchSuccess("beta");
  ASSERT_FALSE(account_tracker()->IsAllUserInfoFetched());
  ASSERT_TRUE(observer()->CheckEvents(TrackingEvent(UPDATED, "beta")));
  ReturnOAuthUrlFetchSuccess("alpha");
  ASSERT_TRUE(account_tracker()->IsAllUserInfoFetched());
  ASSERT_TRUE(observer()->CheckEvents(TrackingEvent(UPDATED, "alpha")));
}

TEST_F(AccountTrackerServiceTest, GetAccounts) {
  SimulateTokenAvailable("alpha");
  SimulateTokenAvailable("beta");
  SimulateTokenAvailable("gamma");
  ReturnOAuthUrlFetchSuccess("alpha");
  ReturnOAuthUrlFetchSuccess("beta");
  ReturnOAuthUrlFetchSuccess("gamma");

  std::vector<AccountTrackerService::AccountInfo> infos =
      account_tracker()->GetAccounts();

  EXPECT_EQ(3u, infos.size());
  CheckAccountDetails("alpha", infos[0]);
  CheckAccountDetails("beta", infos[1]);
  CheckAccountDetails("gamma", infos[2]);
}

TEST_F(AccountTrackerServiceTest, GetAccountInfo_Empty) {
  AccountTrackerService::AccountInfo info =
      account_tracker()->GetAccountInfo("alpha");
  ASSERT_EQ("", info.account_id);
}

TEST_F(AccountTrackerServiceTest, GetAccountInfo_TokenAvailable) {
  SimulateTokenAvailable("alpha");
  AccountTrackerService::AccountInfo info =
      account_tracker()->GetAccountInfo("alpha");
  ASSERT_EQ("alpha", info.account_id);
  ASSERT_EQ("", info.gaia);
  ASSERT_EQ("", info.email);
}

TEST_F(AccountTrackerServiceTest, GetAccountInfo_TokenAvailable_UserInfo) {
  SimulateTokenAvailable("alpha");
  ReturnOAuthUrlFetchSuccess("alpha");
  AccountTrackerService::AccountInfo info =
      account_tracker()->GetAccountInfo("alpha");
  CheckAccountDetails("alpha", info);
}

TEST_F(AccountTrackerServiceTest, GetAccountInfo_TokenAvailable_EnableNetwork) {
  // Shutdown the network-enabled tracker built into the test case.
  TearDown();

  // Create an account tracker but do not enable network fetches.
  AccountTrackerService tracker;
  tracker.AddObserver(observer());
  tracker.Initialize(token_service(), signin_client());

  SimulateTokenAvailable("alpha");
  IssueAccessToken("alpha");
  // No fetcher has been created yet.
  net::TestURLFetcher* fetcher = test_fetcher_factory()->GetFetcherByID(
      gaia::GaiaOAuthClient::kUrlFetcherId);
  ASSERT_FALSE(fetcher);

  // Enable the network to create the fetcher then issue the access token.
  tracker.EnableNetworkFetches();

  // Fetcher was created and executes properly.
  ReturnOAuthUrlFetchSuccess("alpha");

  AccountTrackerService::AccountInfo info =
      tracker.GetAccountInfo("alpha");
  CheckAccountDetails("alpha", info);
  tracker.Shutdown();
}

TEST_F(AccountTrackerServiceTest, FindAccountInfoByGaiaId) {
  SimulateTokenAvailable("alpha");
  ReturnOAuthUrlFetchSuccess("alpha");

  std::string gaia_id = AccountIdToGaiaId("alpha");
  AccountTrackerService::AccountInfo info =
      account_tracker()->FindAccountInfoByGaiaId(gaia_id);
  ASSERT_EQ("alpha", info.account_id);
  ASSERT_EQ(gaia_id, info.gaia);

  gaia_id = AccountIdToGaiaId("beta");
  info = account_tracker()->FindAccountInfoByGaiaId(gaia_id);
  ASSERT_EQ("", info.account_id);
}

TEST_F(AccountTrackerServiceTest, FindAccountInfoByEmail) {
  SimulateTokenAvailable("alpha");
  ReturnOAuthUrlFetchSuccess("alpha");

  std::string email = AccountIdToEmail("alpha");
  AccountTrackerService::AccountInfo info =
      account_tracker()->FindAccountInfoByEmail(email);
  ASSERT_EQ("alpha", info.account_id);
  ASSERT_EQ(email, info.email);

  // Should also work with "canonically-equal" email addresses.
  info = account_tracker()->FindAccountInfoByEmail("Alpha@Gmail.COM");
  ASSERT_EQ("alpha", info.account_id);
  ASSERT_EQ(email, info.email);
  info = account_tracker()->FindAccountInfoByEmail("al.pha@gmail.com");
  ASSERT_EQ("alpha", info.account_id);
  ASSERT_EQ(email, info.email);

  email = AccountIdToEmail("beta");
  info = account_tracker()->FindAccountInfoByEmail(email);
  ASSERT_EQ("", info.account_id);
}

TEST_F(AccountTrackerServiceTest, Persistence) {
  // Create a tracker and add two accounts.  This should cause the accounts
  // to be saved to persistence.
  {
    AccountTrackerService tracker;
    tracker.Initialize(token_service(), signin_client());
    tracker.EnableNetworkFetches();
    SimulateTokenAvailable("alpha");
    ReturnOAuthUrlFetchSuccess("alpha");
    SimulateTokenAvailable("beta");
    ReturnOAuthUrlFetchSuccess("beta");
    tracker.Shutdown();
  }

  // Create a new tracker and make sure it loads the accounts correctly from
  // persistence.
  {
    AccountTrackerService tracker;
    tracker.AddObserver(observer());
    tracker.Initialize(token_service(), signin_client());
    tracker.EnableNetworkFetches();
    ASSERT_TRUE(observer()->CheckEvents(TrackingEvent(UPDATED, "alpha"),
                                        TrackingEvent(UPDATED, "beta")));

    std::vector<AccountTrackerService::AccountInfo> infos =
        tracker.GetAccounts();
    ASSERT_EQ(2u, infos.size());
    CheckAccountDetails("alpha", infos[0]);
    CheckAccountDetails("beta", infos[1]);

    // Remove account.
    SimulateTokenRevoked("alpha");
    tracker.RemoveObserver(observer());
    tracker.Shutdown();
 }

  // Create a new tracker and make sure it loads the single account from
  // persistence.
  {
    AccountTrackerService tracker;
    tracker.Initialize(token_service(), signin_client());
    tracker.EnableNetworkFetches();

    std::vector<AccountTrackerService::AccountInfo> infos =
        tracker.GetAccounts();
    ASSERT_EQ(1u, infos.size());
    CheckAccountDetails("beta", infos[0]);
    tracker.Shutdown();
  }
}

TEST_F(AccountTrackerServiceTest, SeedAccountInfo) {
  std::vector<AccountTrackerService::AccountInfo> infos =
      account_tracker()->GetAccounts();
  EXPECT_EQ(0u, infos.size());

  const std::string gaia_id = AccountIdToGaiaId("alpha");
  const std::string email = AccountIdToEmail("alpha");
  const std::string account_id =
      account_tracker()->PickAccountIdForAccount(gaia_id, email);
  account_tracker()->SeedAccountInfo(gaia_id, email);

  infos = account_tracker()->GetAccounts();
  EXPECT_EQ(1u, infos.size());
  EXPECT_EQ(account_id, infos[0].account_id);
  EXPECT_EQ(gaia_id, infos[0].gaia);
  EXPECT_EQ(email, infos[0].email);
}

TEST_F(AccountTrackerServiceTest, UpgradeToFullAccountInfo) {
  // Start by simulating an incomplete account info and let it be saved to
  // prefs.
  {
    AccountTrackerService tracker;
    tracker.Initialize(token_service(), signin_client());
    tracker.EnableNetworkFetches();
    SimulateTokenAvailable("incomplete");
    ReturnOAuthUrlFetchSuccessIncomplete("incomplete");
    tracker.Shutdown();
  }

  {
    AccountTrackerService tracker;
    tracker.Initialize(token_service(), signin_client());

    // Validate that the loaded AccountInfo from prefs is considered invalid.
    std::vector<AccountTrackerService::AccountInfo> infos =
        tracker.GetAccounts();
    ASSERT_EQ(1u, infos.size());
    ASSERT_FALSE(infos[0].IsValid());

    // Enable network fetches and simulate the same account getting a refresh
    // token containing all the info.
    tracker.EnableNetworkFetches();
    SimulateTokenAvailable("incomplete");
    ReturnOAuthUrlFetchSuccess("incomplete");

    // Validate that the account is now considered valid.
    infos = tracker.GetAccounts();
    ASSERT_EQ(1u, infos.size());
    ASSERT_TRUE(infos[0].IsValid());

    tracker.Shutdown();
  }

  // Reinstantiate a tracker to validate that the AccountInfo saved to prefs is
  // now the upgraded one, considered valid.
  {
    AccountTrackerService tracker;
    tracker.AddObserver(observer());
    tracker.Initialize(token_service(), signin_client());
    ASSERT_TRUE(observer()->CheckEvents(TrackingEvent(UPDATED, "incomplete")));
    // Make sure there are no events in the observer
    observer()->Clear();
    // Enabling network fetches shouldn't cause any actual fetch since the
    // AccountInfos loaded from prefs should be valid.
    tracker.EnableNetworkFetches();

    std::vector<AccountTrackerService::AccountInfo> infos =
        tracker.GetAccounts();
    ASSERT_EQ(1u, infos.size());
    ASSERT_TRUE(infos[0].IsValid());
    // Check that no network fetches were made.
    ASSERT_TRUE(observer()->CheckEvents());

    tracker.Shutdown();
  }
}

TEST_F(AccountTrackerServiceTest, TimerRefresh) {
  // Start by creating a tracker and adding a couple accounts to be persisted to
  // prefs.
  {
    AccountTrackerService tracker;
    tracker.Initialize(token_service(), signin_client());
    tracker.EnableNetworkFetches();
    SimulateTokenAvailable("alpha");
    ReturnOAuthUrlFetchSuccess("alpha");
    SimulateTokenAvailable("beta");
    ReturnOAuthUrlFetchSuccess("beta");
    tracker.Shutdown();
  }

  // Rewind the time by half a day, which shouldn't be enough to trigger a
  // network refresh.
  base::Time fake_update = base::Time::Now() - base::TimeDelta::FromHours(12);
  signin_client()->GetPrefs()->SetInt64(
      AccountTrackerService::kAccountTrackerServiceLastUpdate,
      fake_update.ToInternalValue());

  // Instantiate a new ATS, making sure the persisted accounts are still there
  // and that no network fetches happen.
  {
    AccountTrackerService tracker;
    tracker.Initialize(token_service(), signin_client());

    ASSERT_TRUE(tracker.IsAllUserInfoFetched());
    std::vector<AccountTrackerService::AccountInfo> infos =
        tracker.GetAccounts();
    ASSERT_EQ(2u, infos.size());
    ASSERT_TRUE(infos[0].IsValid());
    ASSERT_TRUE(infos[1].IsValid());

    tracker.EnableNetworkFetches();
    ASSERT_TRUE(tracker.IsAllUserInfoFetched());
    tracker.Shutdown();
  }

  // Rewind the last updated time enough to trigger a network refresh.
  fake_update = base::Time::Now() - base::TimeDelta::FromHours(25);
  signin_client()->GetPrefs()->SetInt64(
      AccountTrackerService::kAccountTrackerServiceLastUpdate,
      fake_update.ToInternalValue());

  // Instantiate a new tracker and validate that even though the AccountInfos
  // are still valid, the network fetches are started.
  {
    AccountTrackerService tracker;
    tracker.Initialize(token_service(), signin_client());

    ASSERT_TRUE(tracker.IsAllUserInfoFetched());
    std::vector<AccountTrackerService::AccountInfo> infos =
        tracker.GetAccounts();
    ASSERT_EQ(2u, infos.size());
    ASSERT_TRUE(infos[0].IsValid());
    ASSERT_TRUE(infos[1].IsValid());

    tracker.EnableNetworkFetches();
    ASSERT_FALSE(tracker.IsAllUserInfoFetched());
    tracker.Shutdown();
  }
}
