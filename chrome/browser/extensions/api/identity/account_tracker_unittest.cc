// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/account_tracker.h"

#include <algorithm>
#include <vector>

#include "base/strings/stringprintf.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(courage): Account removal really only applies to the primary account,
// because that's the only account tracked by the SigninManager. Many of the
// tests here remove non-primary accounts. They still properly test the account
// state machine, but it may be confusing to readers. Update these tests to
// avoid causing confusion.

namespace {

const char kPrimaryAccountKey[] = "primary_account@example.com";

enum TrackingEventType {
  ADDED,
  REMOVED,
  SIGN_IN,
  SIGN_OUT
};

std::string AccountKeyToObfuscatedId(const std::string email) {
  return "obfid-" + email;
}

class TrackingEvent {
 public:
  TrackingEvent(TrackingEventType type,
                const std::string& account_key,
                const std::string& gaia_id)
      : type_(type),
        account_key_(account_key),
        gaia_id_(gaia_id) {}

  TrackingEvent(TrackingEventType type,
                const std::string& account_key)
      : type_(type),
        account_key_(account_key),
        gaia_id_(AccountKeyToObfuscatedId(account_key)) {}

  bool operator==(const TrackingEvent& event) const {
    return type_ == event.type_ && account_key_ == event.account_key_ &&
        gaia_id_ == event.gaia_id_;
  }

  std::string ToString() const {
    const char * typestr = "INVALID";
    switch (type_) {
      case ADDED:
        typestr = "ADD";
        break;
      case REMOVED:
        typestr = "REM";
        break;
      case SIGN_IN:
        typestr = " IN";
        break;
      case SIGN_OUT:
        typestr = "OUT";
        break;
    }
    return base::StringPrintf("{ type: %s, email: %s, gaia: %s }",
                              typestr,
                              account_key_.c_str(),
                              gaia_id_.c_str());
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

}  // namespace

namespace extensions {

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
  virtual void OnAccountAdded(const AccountIds& ids) OVERRIDE;
  virtual void OnAccountRemoved(const AccountIds& ids) OVERRIDE;
  virtual void OnAccountSignInChanged(const AccountIds& ids, bool is_signed_in)
      OVERRIDE;

 private:
  testing::AssertionResult CheckEvents(
      const std::vector<TrackingEvent>& events);

  std::vector<TrackingEvent> events_;
};

void AccountTrackerObserver::OnAccountAdded(const AccountIds& ids) {
  events_.push_back(TrackingEvent(ADDED, ids.email, ids.gaia));
}

void AccountTrackerObserver::OnAccountRemoved(const AccountIds& ids) {
  events_.push_back(TrackingEvent(REMOVED, ids.email, ids.gaia));
}

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

class IdentityAccountTrackerTest : public testing::Test {
 public:
  IdentityAccountTrackerTest() {}

  virtual ~IdentityAccountTrackerTest() {}

  virtual void SetUp() OVERRIDE {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                              BuildFakeProfileOAuth2TokenService);
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              FakeSigninManagerBase::Build);

    test_profile_ = builder.Build();

    fake_oauth2_token_service_ = static_cast<FakeProfileOAuth2TokenService*>(
        ProfileOAuth2TokenServiceFactory::GetForProfile(test_profile_.get()));

    fake_signin_manager_ = static_cast<FakeSigninManagerForTesting*>(
        SigninManagerFactory::GetForProfile(test_profile_.get()));
#if defined(OS_CHROMEOS)
    // We don't sign the primary user in and out on ChromeOS, so set the
    // username once in setup.
    fake_signin_manager_->SetAuthenticatedUsername(kPrimaryAccountKey);
#endif

    account_tracker_.reset(new AccountTracker(test_profile_.get()));
    account_tracker_->AddObserver(&observer_);

    // Start off signed into the primary account, because most tests need the
    // profile to be signed in. Remove the initial sign-in events that the
    // tests don't care about.
    NotifyTokenAvailable(kPrimaryAccountKey);
    ReturnOAuthUrlFetchSuccess(kPrimaryAccountKey);
    observer()->Clear();
  }

  virtual void TearDown() OVERRIDE {
    account_tracker_->RemoveObserver(&observer_);
    account_tracker_->Shutdown();
  }

  Profile* profile() {
    return test_profile_.get();
  }

  AccountTrackerObserver* observer() {
    return &observer_;
  }

  AccountTracker* account_tracker() {
    return account_tracker_.get();
  }

  // Helpers to pass fake events to the tracker.

  void NotifyRemoveAccount(const std::string& username) {
#if !defined(OS_CHROMEOS)
    if (username == kPrimaryAccountKey)
      fake_signin_manager_->SignOut(signin_metrics::SIGNOUT_TEST);
    else
      account_tracker()->GoogleSignedOut(username);
#else
    account_tracker()->GoogleSignedOut(username);
#endif
  }

  void NotifyTokenAvailable(const std::string& username) {
    fake_oauth2_token_service_->IssueRefreshTokenForUser(username,
                                                         "refresh_token");
#if !defined(OS_CHROMEOS)
    if (username == kPrimaryAccountKey)
      fake_signin_manager_->OnExternalSigninCompleted(username);
#endif
  }

  void NotifyTokenRevoked(const std::string& username) {
    fake_oauth2_token_service_->IssueRefreshTokenForUser(username,
                                                         std::string());
  }

  // Helpers to fake access token and user info fetching
  void IssueAccessToken(const std::string& username) {
    fake_oauth2_token_service_->IssueAllTokensForAccount(
        username, "access_token-" + username, base::Time::Max());
  }

  std::string GetValidTokenInfoResponse(const std::string account_key) {
    return std::string("{ \"id\": \"") + AccountKeyToObfuscatedId(account_key) +
           "\" }";
  }

  void ReturnOAuthUrlFetchResults(int fetcher_id,
                                  net::HttpStatusCode response_code,
                                  const std::string& response_string);

  void ReturnOAuthUrlFetchSuccess(const std::string& account_key);
  void ReturnOAuthUrlFetchFailure(const std::string& account_key);

 private:
  scoped_ptr<TestingProfile> test_profile_;
  net::TestURLFetcherFactory test_fetcher_factory_;
  FakeProfileOAuth2TokenService* fake_oauth2_token_service_;
  FakeSigninManagerForTesting* fake_signin_manager_;
  content::TestBrowserThreadBundle thread_bundle_;

  scoped_ptr<AccountTracker> account_tracker_;
  AccountTrackerObserver observer_;
};

void IdentityAccountTrackerTest::ReturnOAuthUrlFetchResults(
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

void IdentityAccountTrackerTest::ReturnOAuthUrlFetchSuccess(
    const std::string& account_key) {
  IssueAccessToken(account_key);
  ReturnOAuthUrlFetchResults(gaia::GaiaOAuthClient::kUrlFetcherId,
                             net::HTTP_OK,
                             GetValidTokenInfoResponse(account_key));
}

void IdentityAccountTrackerTest::ReturnOAuthUrlFetchFailure(
    const std::string& account_key) {
  IssueAccessToken(account_key);
  ReturnOAuthUrlFetchResults(
      gaia::GaiaOAuthClient::kUrlFetcherId, net::HTTP_BAD_REQUEST, "");
}

TEST_F(IdentityAccountTrackerTest, Available) {
  NotifyTokenAvailable("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents());

  ReturnOAuthUrlFetchSuccess("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "user@example.com"),
      TrackingEvent(SIGN_IN, "user@example.com")));
}

TEST_F(IdentityAccountTrackerTest, Revoke) {
  account_tracker()->OnRefreshTokenRevoked("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(IdentityAccountTrackerTest, Remove) {
  NotifyRemoveAccount("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(IdentityAccountTrackerTest, AvailableRemoveFetchCancelAvailable) {
  NotifyTokenAvailable("user@example.com");
  NotifyRemoveAccount("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents());

  NotifyTokenAvailable("user@example.com");
  ReturnOAuthUrlFetchSuccess("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "user@example.com"),
      TrackingEvent(SIGN_IN, "user@example.com")));
}

TEST_F(IdentityAccountTrackerTest, AvailableRemoveAvailable) {
  NotifyTokenAvailable("user@example.com");
  ReturnOAuthUrlFetchSuccess("user@example.com");
  NotifyRemoveAccount("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "user@example.com"),
      TrackingEvent(SIGN_IN, "user@example.com"),
      TrackingEvent(SIGN_OUT, "user@example.com"),
      TrackingEvent(REMOVED, "user@example.com")));

  NotifyTokenAvailable("user@example.com");
  ReturnOAuthUrlFetchSuccess("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "user@example.com"),
      TrackingEvent(SIGN_IN, "user@example.com")));
}

TEST_F(IdentityAccountTrackerTest, AvailableRevokeAvailable) {
  NotifyTokenAvailable("user@example.com");
  ReturnOAuthUrlFetchSuccess("user@example.com");
  NotifyTokenRevoked("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "user@example.com"),
      TrackingEvent(SIGN_IN, "user@example.com"),
      TrackingEvent(SIGN_OUT, "user@example.com")));

  NotifyTokenAvailable("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(SIGN_IN, "user@example.com")));
}

TEST_F(IdentityAccountTrackerTest, AvailableRevokeAvailableWithPendingFetch) {
  NotifyTokenAvailable("user@example.com");
  NotifyTokenRevoked("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents());

  NotifyTokenAvailable("user@example.com");
  ReturnOAuthUrlFetchSuccess("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "user@example.com"),
      TrackingEvent(SIGN_IN, "user@example.com")));
}

TEST_F(IdentityAccountTrackerTest, AvailableRevokeRemove) {
  NotifyTokenAvailable("user@example.com");
  ReturnOAuthUrlFetchSuccess("user@example.com");
  NotifyTokenRevoked("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "user@example.com"),
      TrackingEvent(SIGN_IN, "user@example.com"),
      TrackingEvent(SIGN_OUT, "user@example.com")));

  NotifyRemoveAccount("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(REMOVED, "user@example.com")));
}

TEST_F(IdentityAccountTrackerTest, AvailableRevokeRevoke) {
  NotifyTokenAvailable("user@example.com");
  ReturnOAuthUrlFetchSuccess("user@example.com");
  NotifyTokenRevoked("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "user@example.com"),
      TrackingEvent(SIGN_IN, "user@example.com"),
      TrackingEvent(SIGN_OUT, "user@example.com")));

  NotifyTokenRevoked("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(IdentityAccountTrackerTest, AvailableAvailable) {
  NotifyTokenAvailable("user@example.com");
  ReturnOAuthUrlFetchSuccess("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "user@example.com"),
      TrackingEvent(SIGN_IN, "user@example.com")));

  NotifyTokenAvailable("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(IdentityAccountTrackerTest, TwoAccounts) {
  NotifyTokenAvailable("alpha@example.com");
  ReturnOAuthUrlFetchSuccess("alpha@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "alpha@example.com"),
      TrackingEvent(SIGN_IN, "alpha@example.com")));

  NotifyTokenAvailable("beta@example.com");
  ReturnOAuthUrlFetchSuccess("beta@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "beta@example.com"),
      TrackingEvent(SIGN_IN, "beta@example.com")));

  NotifyRemoveAccount("alpha@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(SIGN_OUT, "alpha@example.com"),
      TrackingEvent(REMOVED, "alpha@example.com")));

  NotifyRemoveAccount("beta@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(SIGN_OUT, "beta@example.com"),
      TrackingEvent(REMOVED, "beta@example.com")));
}

TEST_F(IdentityAccountTrackerTest, GlobalErrors) {
  NotifyTokenAvailable("alpha@example.com");
  ReturnOAuthUrlFetchSuccess("alpha@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "alpha@example.com"),
      TrackingEvent(SIGN_IN, "alpha@example.com")));
  NotifyTokenAvailable("beta@example.com");
  ReturnOAuthUrlFetchSuccess("beta@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "beta@example.com"),
      TrackingEvent(SIGN_IN, "beta@example.com")));

  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            account_tracker()->GetAuthStatus());

  account_tracker()->ReportAuthError(
      "beta@example.com",
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(SIGN_OUT, "beta@example.com")));
  EXPECT_EQ(GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED),
            account_tracker()->GetAuthStatus());

  account_tracker()->ReportAuthError(
      "alpha@example.com",
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(SIGN_OUT, "alpha@example.com")));
  EXPECT_EQ(GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED),
            account_tracker()->GetAuthStatus());

  NotifyRemoveAccount("alpha@example.com");
  EXPECT_EQ(GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED),
            account_tracker()->GetAuthStatus());

  NotifyTokenAvailable("beta@example.com");
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            account_tracker()->GetAuthStatus());
}

TEST_F(IdentityAccountTrackerTest, AvailableTokenFetchFailAvailable) {
  NotifyTokenAvailable("alpha@example.com");
  ReturnOAuthUrlFetchFailure("alpha@example.com");
  EXPECT_TRUE(observer()->CheckEvents());

  NotifyTokenAvailable("user@example.com");
  ReturnOAuthUrlFetchSuccess("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "user@example.com"),
      TrackingEvent(SIGN_IN, "user@example.com")));
}

// The Chrome OS fake sign-in manager doesn't do sign-in or sign-out.
#if !defined(OS_CHROMEOS)

TEST_F(IdentityAccountTrackerTest, PrimarySignOutSignIn) {
  NotifyRemoveAccount(kPrimaryAccountKey);
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(SIGN_OUT, kPrimaryAccountKey),
      TrackingEvent(REMOVED, kPrimaryAccountKey)));

  NotifyTokenAvailable(kPrimaryAccountKey);
  ReturnOAuthUrlFetchSuccess(kPrimaryAccountKey);
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, kPrimaryAccountKey),
      TrackingEvent(SIGN_IN, kPrimaryAccountKey)));
}

TEST_F(IdentityAccountTrackerTest, PrimarySignOutSignInTwoAccounts) {
  NotifyTokenAvailable("alpha@example.com");
  ReturnOAuthUrlFetchSuccess("alpha@example.com");
  NotifyTokenAvailable("beta@example.com");
  ReturnOAuthUrlFetchSuccess("beta@example.com");

  observer()->SortEventsByUser();
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "alpha@example.com"),
      TrackingEvent(SIGN_IN, "alpha@example.com"),
      TrackingEvent(ADDED, "beta@example.com"),
      TrackingEvent(SIGN_IN, "beta@example.com")));

  NotifyRemoveAccount(kPrimaryAccountKey);
  observer()->SortEventsByUser();
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(SIGN_OUT, "alpha@example.com"),
      TrackingEvent(REMOVED, "alpha@example.com"),
      TrackingEvent(SIGN_OUT, "beta@example.com"),
      TrackingEvent(REMOVED, "beta@example.com"),
      TrackingEvent(SIGN_OUT, kPrimaryAccountKey),
      TrackingEvent(REMOVED, kPrimaryAccountKey)));

  // No events fire at all while profile is signed out.
  NotifyTokenRevoked("alpha@example.com");
  NotifyTokenAvailable("gamma@example.com");
  EXPECT_TRUE(observer()->CheckEvents());

  // Signing the profile in again will resume tracking all accounts.
  NotifyTokenAvailable(kPrimaryAccountKey);
  ReturnOAuthUrlFetchSuccess("beta@example.com");
  ReturnOAuthUrlFetchSuccess("gamma@example.com");
  ReturnOAuthUrlFetchSuccess(kPrimaryAccountKey);
  observer()->SortEventsByUser();
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(ADDED, "beta@example.com"),
      TrackingEvent(SIGN_IN, "beta@example.com"),
      TrackingEvent(ADDED, "gamma@example.com"),
      TrackingEvent(SIGN_IN, "gamma@example.com"),
      TrackingEvent(ADDED, kPrimaryAccountKey),
      TrackingEvent(SIGN_IN, kPrimaryAccountKey)));

  // Revoking the primary token does not affect other accounts.
  NotifyTokenRevoked(kPrimaryAccountKey);
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(SIGN_OUT, kPrimaryAccountKey)));

  NotifyTokenAvailable(kPrimaryAccountKey);
  EXPECT_TRUE(observer()->CheckEvents(
      TrackingEvent(SIGN_IN, kPrimaryAccountKey)));
}

#endif  // !defined(OS_CHROMEOS)

TEST_F(IdentityAccountTrackerTest, GetAccountsPrimary) {
  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(1ul, ids.size());
  EXPECT_EQ(kPrimaryAccountKey, ids[0].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(kPrimaryAccountKey), ids[0].gaia);
}

TEST_F(IdentityAccountTrackerTest, GetAccountsSignedOut) {
  NotifyTokenRevoked(kPrimaryAccountKey);

  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(0ul, ids.size());
}

TEST_F(IdentityAccountTrackerTest, GetAccountsOnlyReturnAccountsWithTokens) {
  NotifyTokenAvailable("alpha@example.com");
  NotifyTokenAvailable("beta@example.com");
  ReturnOAuthUrlFetchSuccess("beta@example.com");

  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(2ul, ids.size());
  EXPECT_EQ(kPrimaryAccountKey, ids[0].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(kPrimaryAccountKey), ids[0].gaia);
  EXPECT_EQ("beta@example.com", ids[1].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId("beta@example.com"), ids[1].gaia);
}

TEST_F(IdentityAccountTrackerTest, GetAccountsSortOrder) {
  NotifyTokenAvailable("zeta@example.com");
  ReturnOAuthUrlFetchSuccess("zeta@example.com");
  NotifyTokenAvailable("alpha@example.com");
  ReturnOAuthUrlFetchSuccess("alpha@example.com");

  // The primary account will be first in the vector. Remaining accounts
  // will be sorted by gaia ID.
  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(3ul, ids.size());
  EXPECT_EQ(kPrimaryAccountKey, ids[0].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(kPrimaryAccountKey), ids[0].gaia);
  EXPECT_EQ("alpha@example.com", ids[1].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId("alpha@example.com"), ids[1].gaia);
  EXPECT_EQ("zeta@example.com", ids[2].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId("zeta@example.com"), ids[2].gaia);
}

TEST_F(IdentityAccountTrackerTest,
       GetAccountsReturnNothingWhenPrimarySignedOut) {
  NotifyTokenAvailable("zeta@example.com");
  ReturnOAuthUrlFetchSuccess("zeta@example.com");
  NotifyTokenAvailable("alpha@example.com");
  ReturnOAuthUrlFetchSuccess("alpha@example.com");

  NotifyTokenRevoked(kPrimaryAccountKey);

  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(0ul, ids.size());
}

}  // namespace extensions
