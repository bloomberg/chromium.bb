// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/gaia_cookie_manager_service.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockObserver : public GaiaCookieManagerService::Observer {
 public:
  explicit MockObserver(GaiaCookieManagerService* helper) : helper_(helper) {
    helper_->AddObserver(this);
  }

  ~MockObserver() override { helper_->RemoveObserver(this); }

  MOCK_METHOD2(OnAddAccountToCookieCompleted,
               void(const std::string&, const GoogleServiceAuthError&));
  MOCK_METHOD3(OnGaiaAccountsInCookieUpdated,
               void(const std::vector<gaia::ListedAccount>&,
                    const std::vector<gaia::ListedAccount>&,
                    const GoogleServiceAuthError&));
 private:
  GaiaCookieManagerService* helper_;

  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

// Counts number of InstrumentedGaiaCookieManagerService created.
// We can EXPECT_* to be zero at the end of our unit tests
// to make sure everything is properly deleted.

int total = 0;

bool AreAccountListsEqual(const std::vector<gaia::ListedAccount>& left,
                          const std::vector<gaia::ListedAccount>& right) {
  if (left.size() != right.size())
    return false;

  for (size_t i = 0u; i < left.size(); ++i) {
    const gaia::ListedAccount& left_account = left[i];
    const gaia::ListedAccount& actual_account = right[i];
    // If both accounts have an ID, use it for the comparison.
    if (!left_account.id.empty() && !actual_account.id.empty()) {
      if (left_account.id != actual_account.id)
        return false;
    } else if (left_account.email != actual_account.email ||
               left_account.gaia_id != actual_account.gaia_id ||
               left_account.raw_email != actual_account.raw_email ||
               left_account.valid != actual_account.valid ||
               left_account.signed_out != actual_account.signed_out ||
               left_account.verified != actual_account.verified) {
      return false;
    }
  }
  return true;
}

// Custom matcher for ListedAccounts.
MATCHER_P(ListedAccountEquals, expected, "") {
  return AreAccountListsEqual(expected, arg);
}

class InstrumentedGaiaCookieManagerService : public GaiaCookieManagerService {
 public:
  InstrumentedGaiaCookieManagerService(
      OAuth2TokenService* token_service,
      SigninClient* signin_client)
      : GaiaCookieManagerService(token_service,
                                 GaiaConstants::kChromeSource,
                                 signin_client) {
    total++;
  }

  ~InstrumentedGaiaCookieManagerService() override { total--; }

  MOCK_METHOD0(StartFetchingUbertoken, void());
  MOCK_METHOD0(StartFetchingListAccounts, void());
  MOCK_METHOD0(StartFetchingLogOut, void());
  MOCK_METHOD0(StartFetchingMergeSession, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(InstrumentedGaiaCookieManagerService);
};

class GaiaCookieManagerServiceTest : public testing::Test {
 public:
  GaiaCookieManagerServiceTest()
      : no_error_(GoogleServiceAuthError::NONE),
        error_(GoogleServiceAuthError::SERVICE_ERROR),
        canceled_(GoogleServiceAuthError::REQUEST_CANCELED) {}

  void SetUp() override {
    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    signin_client_.reset(new TestSigninClient(&pref_service_));
  }

  OAuth2TokenService* token_service() { return &token_service_; }
  TestSigninClient* signin_client() { return signin_client_.get(); }

  void SimulateUbertokenSuccess(UbertokenConsumer* consumer,
                                const std::string& uber_token) {
    consumer->OnUbertokenSuccess(uber_token);
  }

  void SimulateUbertokenFailure(UbertokenConsumer* consumer,
                                const GoogleServiceAuthError& error) {
    consumer->OnUbertokenFailure(error);
  }

  void SimulateMergeSessionSuccess(GaiaAuthConsumer* consumer,
                                   const std::string& data) {
    consumer->OnMergeSessionSuccess(data);
  }

  void SimulateMergeSessionFailure(GaiaAuthConsumer* consumer,
                                   const GoogleServiceAuthError& error) {
    consumer->OnMergeSessionFailure(error);
  }

  void SimulateListAccountsSuccess(GaiaAuthConsumer* consumer,
                                   const std::string& data) {
    consumer->OnListAccountsSuccess(data);
  }

  void SimulateLogOutSuccess(GaiaAuthConsumer* consumer) {
    consumer->OnLogOutSuccess();
  }

  void SimulateLogOutFailure(GaiaAuthConsumer* consumer,
                             const GoogleServiceAuthError& error) {
    consumer->OnLogOutFailure(error);
  }

  void SimulateGetCheckConnectionInfoSuccess(const std::string& data) {
    signin_client_->test_url_loader_factory()->AddResponse(
        GaiaUrls::GetInstance()
            ->GetCheckConnectionInfoURLWithSource(GaiaConstants::kChromeSource)
            .spec(),
        data);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateGetCheckConnectionInfoResult(const std::string& url,
                                            const std::string& result) {
    signin_client_->test_url_loader_factory()->AddResponse(url, result);
    base::RunLoop().RunUntilIdle();
  }

  bool IsLoadPending(const std::string& url) {
    return signin_client_->test_url_loader_factory()->IsPending(
        GURL(url).spec());
  }

  bool IsLoadPending() {
    return signin_client_->test_url_loader_factory()->NumPending() > 0;
  }

  const GoogleServiceAuthError& no_error() { return no_error_; }
  const GoogleServiceAuthError& error() { return error_; }
  const GoogleServiceAuthError& canceled() { return canceled_; }

  scoped_refptr<network::SharedURLLoaderFactory> factory() const {
    return signin_client_->GetURLLoaderFactory();
  }

 private:
  base::MessageLoop message_loop_;
  FakeOAuth2TokenService token_service_;
  GoogleServiceAuthError no_error_;
  GoogleServiceAuthError error_;
  GoogleServiceAuthError canceled_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TestSigninClient> signin_client_;
};

}  // namespace

using ::testing::_;

TEST_F(GaiaCookieManagerServiceTest, Success) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      no_error()));

  helper.AddAccountToCookie("acc1@gmail.com", GaiaConstants::kChromeSource);
  SimulateMergeSessionSuccess(&helper, "token");
}

TEST_F(GaiaCookieManagerServiceTest, FailedMergeSession) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);
  base::HistogramTester histograms;

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      error()));

  helper.AddAccountToCookie("acc1@gmail.com", GaiaConstants::kChromeSource);
  SimulateMergeSessionFailure(&helper, error());
  // Persistent error incurs no further retries.
  DCHECK(!helper.is_running());
  histograms.ExpectUniqueSample("OAuth2Login.MergeSessionFailure",
      GoogleServiceAuthError::SERVICE_ERROR, 1);
}

TEST_F(GaiaCookieManagerServiceTest, AddAccountCookiesDisabled) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);
  signin_client()->set_are_signin_cookies_allowed(false);

  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      canceled()));

  helper.AddAccountToCookie("acc1@gmail.com", GaiaConstants::kChromeSource);
}

TEST_F(GaiaCookieManagerServiceTest, MergeSessionRetried) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(helper, StartFetchingMergeSession());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      no_error()));

  helper.AddAccountToCookie("acc1@gmail.com", GaiaConstants::kChromeSource);
  SimulateMergeSessionFailure(&helper, canceled());
  DCHECK(helper.is_running());
  base::RunLoop run_loop;
  // Transient error incurs a retry after 1 second.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(1100));
  run_loop.Run();
  SimulateMergeSessionSuccess(&helper, "token");
  DCHECK(!helper.is_running());
}

TEST_F(GaiaCookieManagerServiceTest, MergeSessionRetriedTwice) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);
  base::HistogramTester histograms;

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(helper, StartFetchingMergeSession()).Times(2);
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      no_error()));

  helper.AddAccountToCookie("acc1@gmail.com", GaiaConstants::kChromeSource);
  SimulateMergeSessionFailure(&helper, canceled());
  DCHECK(helper.is_running());
  // Transient error incurs a retry after 1 second.
  EXPECT_LT(helper.GetBackoffEntry()->GetTimeUntilRelease(),
      base::TimeDelta::FromMilliseconds(1100));
  {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(1100));
    run_loop.Run();
  }
  SimulateMergeSessionFailure(&helper, canceled());
  DCHECK(helper.is_running());
  // Next transient error incurs a retry after 3 seconds.
  EXPECT_LT(helper.GetBackoffEntry()->GetTimeUntilRelease(),
      base::TimeDelta::FromMilliseconds(3100));
  {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(3100));
    run_loop.Run();
  }
  SimulateMergeSessionSuccess(&helper, "token");
  DCHECK(!helper.is_running());
  histograms.ExpectUniqueSample("OAuth2Login.MergeSessionRetry",
      GoogleServiceAuthError::REQUEST_CANCELED, 2);
}

TEST_F(GaiaCookieManagerServiceTest, FailedUbertoken) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      error()));

  helper.AddAccountToCookie("acc1@gmail.com", GaiaConstants::kChromeSource);
  SimulateUbertokenFailure(&helper, error());
}

TEST_F(GaiaCookieManagerServiceTest, ContinueAfterSuccess) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken()).Times(2);
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      no_error()));
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc2@gmail.com",
                                                      no_error()));

  helper.AddAccountToCookie("acc1@gmail.com", GaiaConstants::kChromeSource);
  helper.AddAccountToCookie("acc2@gmail.com", GaiaConstants::kChromeSource);
  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(GaiaCookieManagerServiceTest, ContinueAfterFailure1) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken()).Times(2);
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      error()));
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc2@gmail.com",
                                                      no_error()));

  helper.AddAccountToCookie("acc1@gmail.com", GaiaConstants::kChromeSource);
  helper.AddAccountToCookie("acc2@gmail.com", GaiaConstants::kChromeSource);
  SimulateMergeSessionFailure(&helper, error());
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(GaiaCookieManagerServiceTest, ContinueAfterFailure2) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken()).Times(2);
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      error()));
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc2@gmail.com",
                                                      no_error()));

  helper.AddAccountToCookie("acc1@gmail.com", GaiaConstants::kChromeSource);
  helper.AddAccountToCookie("acc2@gmail.com", GaiaConstants::kChromeSource);
  SimulateUbertokenFailure(&helper, error());
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(GaiaCookieManagerServiceTest, AllRequestsInMultipleGoes) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken()).Times(4);
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted(_, no_error())).Times(4);

  helper.AddAccountToCookie("acc1@gmail.com", GaiaConstants::kChromeSource);
  helper.AddAccountToCookie("acc2@gmail.com", GaiaConstants::kChromeSource);

  SimulateMergeSessionSuccess(&helper, "token1");

  helper.AddAccountToCookie("acc3@gmail.com", GaiaConstants::kChromeSource);

  SimulateMergeSessionSuccess(&helper, "token2");
  SimulateMergeSessionSuccess(&helper, "token3");

  helper.AddAccountToCookie("acc4@gmail.com", GaiaConstants::kChromeSource);

  SimulateMergeSessionSuccess(&helper, "token4");
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsNoQueue) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc2@gmail.com",
                                                      no_error()));
  EXPECT_CALL(helper, StartFetchingLogOut());

  helper.AddAccountToCookie("acc2@gmail.com", GaiaConstants::kChromeSource);
  SimulateMergeSessionSuccess(&helper, "token1");

  helper.LogOutAllAccounts(GaiaConstants::kChromeSource);
  SimulateLogOutSuccess(&helper);
  ASSERT_FALSE(helper.is_running());
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsFails) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc2@gmail.com",
                                                      no_error()));
  EXPECT_CALL(helper, StartFetchingLogOut());

  helper.AddAccountToCookie("acc2@gmail.com", GaiaConstants::kChromeSource);
  SimulateMergeSessionSuccess(&helper, "token1");

  helper.LogOutAllAccounts(GaiaConstants::kChromeSource);
  SimulateLogOutFailure(&helper, error());
  // CookieManagerService is still running; it is retrying the failed logout.
  ASSERT_TRUE(helper.is_running());
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsAfterOneAddInQueue) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc2@gmail.com",
                                                      no_error()));
  EXPECT_CALL(helper, StartFetchingLogOut());

  helper.AddAccountToCookie("acc2@gmail.com", GaiaConstants::kChromeSource);
  helper.LogOutAllAccounts(GaiaConstants::kChromeSource);

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogOutSuccess(&helper);
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsAfterTwoAddsInQueue) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      no_error()));
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc2@gmail.com",
                                                      canceled()));
  EXPECT_CALL(helper, StartFetchingLogOut());

  helper.AddAccountToCookie("acc1@gmail.com", GaiaConstants::kChromeSource);
  // The Log Out should prevent this AddAccount from being fetched.
  helper.AddAccountToCookie("acc2@gmail.com", GaiaConstants::kChromeSource);
  helper.LogOutAllAccounts(GaiaConstants::kChromeSource);

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogOutSuccess(&helper);
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsTwice) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc2@gmail.com",
                                                      no_error()));
  EXPECT_CALL(helper, StartFetchingLogOut());

  helper.AddAccountToCookie("acc2@gmail.com", GaiaConstants::kChromeSource);
  SimulateMergeSessionSuccess(&helper, "token1");

  helper.LogOutAllAccounts(GaiaConstants::kChromeSource);
  // Only one LogOut will be fetched.
  helper.LogOutAllAccounts(GaiaConstants::kChromeSource);
  SimulateLogOutSuccess(&helper);
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsBeforeAdd) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken()).Times(2);
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc2@gmail.com",
                                                      no_error()));
  EXPECT_CALL(helper, StartFetchingLogOut());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc3@gmail.com",
                                                      no_error()));
  helper.AddAccountToCookie("acc2@gmail.com", GaiaConstants::kChromeSource);
  SimulateMergeSessionSuccess(&helper, "token1");

  helper.LogOutAllAccounts(GaiaConstants::kChromeSource);
  helper.AddAccountToCookie("acc3@gmail.com", GaiaConstants::kChromeSource);

  SimulateLogOutSuccess(&helper);
  // After LogOut the MergeSession should be fetched.
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsBeforeLogoutAndAdd) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);


  EXPECT_CALL(helper, StartFetchingUbertoken()).Times(2);
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc2@gmail.com",
                                                      no_error()));
  EXPECT_CALL(helper, StartFetchingLogOut());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc3@gmail.com",
                                                      no_error()));

  helper.AddAccountToCookie("acc2@gmail.com", GaiaConstants::kChromeSource);
  SimulateMergeSessionSuccess(&helper, "token1");

  helper.LogOutAllAccounts(GaiaConstants::kChromeSource);
  // Second LogOut will never be fetched.
  helper.LogOutAllAccounts(GaiaConstants::kChromeSource);
  helper.AddAccountToCookie("acc3@gmail.com", GaiaConstants::kChromeSource);

  SimulateLogOutSuccess(&helper);
  // After LogOut the MergeSession should be fetched.
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(GaiaCookieManagerServiceTest, PendingSigninThenSignout) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  // From the first Signin.
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      no_error()));

  // From the sign out and then re-sign in.
  EXPECT_CALL(helper, StartFetchingLogOut());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc3@gmail.com",
                                                      no_error()));

  // Total sign in 2 times, not enforcing ordered sequences.
  EXPECT_CALL(helper, StartFetchingUbertoken()).Times(2);

  helper.AddAccountToCookie("acc1@gmail.com", GaiaConstants::kChromeSource);
  helper.LogOutAllAccounts(GaiaConstants::kChromeSource);

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogOutSuccess(&helper);

  helper.AddAccountToCookie("acc3@gmail.com", GaiaConstants::kChromeSource);
  SimulateMergeSessionSuccess(&helper, "token3");
}

TEST_F(GaiaCookieManagerServiceTest, CancelSignIn) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc2@gmail.com",
                                                      canceled()));
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      no_error()));
  EXPECT_CALL(helper, StartFetchingLogOut());

  helper.AddAccountToCookie("acc1@gmail.com", GaiaConstants::kChromeSource);
  helper.AddAccountToCookie("acc2@gmail.com", GaiaConstants::kChromeSource);
  helper.LogOutAllAccounts(GaiaConstants::kChromeSource);

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogOutSuccess(&helper);
}

TEST_F(GaiaCookieManagerServiceTest, ListAccountsFirstReturnsEmpty) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  std::vector<gaia::ListedAccount> list_accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;

  EXPECT_CALL(helper, StartFetchingListAccounts());

  ASSERT_FALSE(helper.ListAccounts(&list_accounts, &signed_out_accounts,
                                   GaiaConstants::kChromeSource));
  ASSERT_TRUE(list_accounts.empty());
  ASSERT_TRUE(signed_out_accounts.empty());
}

TEST_F(GaiaCookieManagerServiceTest, ListAccountsFindsOneAccount) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  std::vector<gaia::ListedAccount> list_accounts;
  std::vector<gaia::ListedAccount> expected_accounts;
  gaia::ListedAccount listed_account;
  listed_account.email = "a@b.com";
  listed_account.raw_email = "a@b.com";
  listed_account.gaia_id = "8";
  expected_accounts.push_back(listed_account);

  std::vector<gaia::ListedAccount> signed_out_accounts;
  std::vector<gaia::ListedAccount> expected_signed_out_accounts;

  EXPECT_CALL(helper, StartFetchingListAccounts());
  EXPECT_CALL(observer, OnGaiaAccountsInCookieUpdated(
                            ListedAccountEquals(expected_accounts),
                            ListedAccountEquals(expected_signed_out_accounts),
                            no_error()));

  ASSERT_FALSE(helper.ListAccounts(&list_accounts, &signed_out_accounts,
                                   GaiaConstants::kChromeSource));

  SimulateListAccountsSuccess(&helper,
      "[\"f\", [[\"b\", 0, \"n\", \"a@b.com\", \"p\", 0, 0, 0, 0, 1, \"8\"]]]");
}

TEST_F(GaiaCookieManagerServiceTest, ListAccountsFindsSignedOutAccounts) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  std::vector<gaia::ListedAccount> list_accounts;
  std::vector<gaia::ListedAccount> expected_accounts;
  gaia::ListedAccount listed_account;
  listed_account.email = "a@b.com";
  listed_account.raw_email = "a@b.com";
  listed_account.gaia_id = "8";
  expected_accounts.push_back(listed_account);

  std::vector<gaia::ListedAccount> signed_out_accounts;
  std::vector<gaia::ListedAccount> expected_signed_out_accounts;
  gaia::ListedAccount signed_out_account;
  signed_out_account.email = "c@d.com";
  signed_out_account.raw_email = "c@d.com";
  signed_out_account.gaia_id = "9";
  signed_out_account.signed_out = true;
  expected_signed_out_accounts.push_back(signed_out_account);

  EXPECT_CALL(helper, StartFetchingListAccounts());
  EXPECT_CALL(observer, OnGaiaAccountsInCookieUpdated(
                            ListedAccountEquals(expected_accounts),
                            ListedAccountEquals(expected_signed_out_accounts),
                            no_error()));

  ASSERT_FALSE(helper.ListAccounts(&list_accounts, &signed_out_accounts,
                                   GaiaConstants::kChromeSource));

  SimulateListAccountsSuccess(&helper,
      "[\"f\","
      "[[\"b\", 0, \"n\", \"a@b.com\", \"p\", 0, 0, 0, 0, 1, \"8\"],"
      " [\"b\", 0, \"n\", \"c@d.com\", \"p\", 0, 0, 0, 0, 1, \"9\","
          "null,null,null,1]]]");
}

TEST_F(GaiaCookieManagerServiceTest, ListAccountsAcceptsNull) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  ASSERT_FALSE(helper.ListAccounts(nullptr, nullptr,
                                   GaiaConstants::kChromeSource));

  SimulateListAccountsSuccess(&helper,
      "[\"f\","
      "[[\"b\", 0, \"n\", \"a@b.com\", \"p\", 0, 0, 0, 0, 1, \"8\"],"
      " [\"b\", 0, \"n\", \"c@d.com\", \"p\", 0, 0, 0, 0, 1, \"9\","
          "null,null,null,1]]]");

  std::vector<gaia::ListedAccount> signed_out_accounts;
  ASSERT_TRUE(helper.ListAccounts(nullptr, &signed_out_accounts,
                                  GaiaConstants::kChromeSource));
  ASSERT_EQ(1u, signed_out_accounts.size());

  std::vector<gaia::ListedAccount> accounts;
  ASSERT_TRUE(helper.ListAccounts(&accounts, nullptr,
                                  GaiaConstants::kChromeSource));
  ASSERT_EQ(1u, accounts.size());
}

TEST_F(GaiaCookieManagerServiceTest, ListAccountsAfterOnCookieChange) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  std::vector<gaia::ListedAccount> list_accounts;
  std::vector<gaia::ListedAccount> empty_list_accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;
  std::vector<gaia::ListedAccount> empty_signed_out_accounts;

  std::vector<gaia::ListedAccount> nonempty_list_accounts;
  gaia::ListedAccount listed_account;
  listed_account.email = "a@b.com";
  listed_account.raw_email = "a@b.com";
  listed_account.gaia_id = "8";
  nonempty_list_accounts.push_back(listed_account);

  // Add a single account.
  EXPECT_CALL(helper, StartFetchingListAccounts());
  EXPECT_CALL(observer,
              OnGaiaAccountsInCookieUpdated(
                  ListedAccountEquals(nonempty_list_accounts),
                  ListedAccountEquals(empty_signed_out_accounts), no_error()));
  ASSERT_FALSE(helper.ListAccounts(&list_accounts, &signed_out_accounts,
                                   GaiaConstants::kChromeSource));
  ASSERT_TRUE(list_accounts.empty());
  ASSERT_TRUE(signed_out_accounts.empty());
  SimulateListAccountsSuccess(
      &helper,
      "[\"f\", [[\"b\", 0, \"n\", \"a@b.com\", \"p\", 0, 0, 0, 0, 1, \"8\"]]]");

  // Sanity-check that ListAccounts returns the cached data.
  ASSERT_TRUE(helper.ListAccounts(&list_accounts, &signed_out_accounts,
                                  GaiaConstants::kChromeSource));
  ASSERT_TRUE(AreAccountListsEqual(nonempty_list_accounts, list_accounts));
  ASSERT_TRUE(signed_out_accounts.empty());

  EXPECT_CALL(helper, StartFetchingListAccounts());
  EXPECT_CALL(observer,
              OnGaiaAccountsInCookieUpdated(
                  ListedAccountEquals(empty_list_accounts),
                  ListedAccountEquals(empty_signed_out_accounts), no_error()));
  helper.ForceOnCookieChangeProcessing();

  // OnCookieChange should invalidate the cached data.

  // Clear the list before calling |ListAccounts()| to make sure that
  // GaiaCookieManagerService repopulates it with the stale cached information.
  list_accounts.clear();

  ASSERT_FALSE(helper.ListAccounts(&list_accounts, &signed_out_accounts,
                                   GaiaConstants::kChromeSource));
  ASSERT_TRUE(AreAccountListsEqual(nonempty_list_accounts, list_accounts));
  ASSERT_TRUE(signed_out_accounts.empty());
  SimulateListAccountsSuccess(&helper, "[\"f\",[]]");
}

TEST_F(GaiaCookieManagerServiceTest, ExternalCcResultFetcher) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  GaiaCookieManagerService::ExternalCcResultFetcher result_fetcher(&helper);
  EXPECT_CALL(helper, StartFetchingMergeSession());
  result_fetcher.Start();

  // Simulate a successful completion of GetCheckConnctionInfo.
  SimulateGetCheckConnectionInfoSuccess(
      "[{\"carryBackToken\": \"yt\", \"url\": \"http://www.yt.com\"},"
      " {\"carryBackToken\": \"bl\", \"url\": \"http://www.bl.com\"}]");

  // Simulate responses for the two connection URLs.
  GaiaCookieManagerService::ExternalCcResultFetcher::LoaderToToken loaders =
      result_fetcher.get_loader_map_for_testing();
  ASSERT_EQ(2u, loaders.size());
  ASSERT_TRUE(IsLoadPending("http://www.yt.com"));
  ASSERT_TRUE(IsLoadPending("http://www.bl.com"));

  ASSERT_EQ("bl:null,yt:null", result_fetcher.GetExternalCcResult());
  SimulateGetCheckConnectionInfoResult("http://www.yt.com", "yt_result");
  ASSERT_EQ("bl:null,yt:yt_result", result_fetcher.GetExternalCcResult());
  SimulateGetCheckConnectionInfoResult("http://www.bl.com", "bl_result");
  ASSERT_EQ("bl:bl_result,yt:yt_result", result_fetcher.GetExternalCcResult());
}

TEST_F(GaiaCookieManagerServiceTest, ExternalCcResultFetcherTimeout) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  GaiaCookieManagerService::ExternalCcResultFetcher result_fetcher(&helper);
  EXPECT_CALL(helper, StartFetchingMergeSession());
  result_fetcher.Start();

  // Simulate a successful completion of GetCheckConnctionInfo.
  SimulateGetCheckConnectionInfoSuccess(
      "[{\"carryBackToken\": \"yt\", \"url\": \"http://www.yt.com\"},"
      " {\"carryBackToken\": \"bl\", \"url\": \"http://www.bl.com\"}]");

  GaiaCookieManagerService::ExternalCcResultFetcher::LoaderToToken loaders =
      result_fetcher.get_loader_map_for_testing();
  ASSERT_EQ(2u, loaders.size());
  ASSERT_TRUE(IsLoadPending("http://www.yt.com"));
  ASSERT_TRUE(IsLoadPending("http://www.bl.com"));

  // Simulate response only for "yt".
  ASSERT_EQ("bl:null,yt:null", result_fetcher.GetExternalCcResult());
  SimulateGetCheckConnectionInfoResult("http://www.yt.com", "yt_result");
  ASSERT_EQ("bl:null,yt:yt_result", result_fetcher.GetExternalCcResult());

  // Now timeout.
  result_fetcher.TimeoutForTests();
  ASSERT_EQ("bl:null,yt:yt_result", result_fetcher.GetExternalCcResult());
  loaders = result_fetcher.get_loader_map_for_testing();
  ASSERT_EQ(0u, loaders.size());
}

TEST_F(GaiaCookieManagerServiceTest, ExternalCcResultFetcherTruncate) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  GaiaCookieManagerService::ExternalCcResultFetcher result_fetcher(&helper);
  EXPECT_CALL(helper, StartFetchingMergeSession());
  result_fetcher.Start();

  // Simulate a successful completion of GetCheckConnctionInfo.
  SimulateGetCheckConnectionInfoSuccess(
      "[{\"carryBackToken\": \"yt\", \"url\": \"http://www.yt.com\"}]");

  GaiaCookieManagerService::ExternalCcResultFetcher::LoaderToToken loaders =
      result_fetcher.get_loader_map_for_testing();
  ASSERT_EQ(1u, loaders.size());
  ASSERT_TRUE(IsLoadPending("http://www.yt.com"));

  // Simulate response for "yt" with a string that is too long.
  SimulateGetCheckConnectionInfoResult("http://www.yt.com",
                                       "1234567890123456trunc");
  ASSERT_EQ("yt:1234567890123456", result_fetcher.GetExternalCcResult());
}

TEST_F(GaiaCookieManagerServiceTest, UbertokenSuccessFetchesExternalCC) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());

  EXPECT_CALL(helper, StartFetchingUbertoken());
  helper.AddAccountToCookie("acc1@gmail.com", GaiaConstants::kChromeSource);

  ASSERT_FALSE(IsLoadPending());
  SimulateUbertokenSuccess(&helper, "token");

  // Check there is now a fetcher that belongs to the ExternalCCResultFetcher.
  SimulateGetCheckConnectionInfoSuccess(
      "[{\"carryBackToken\": \"bl\", \"url\": \"http://www.bl.com\"}]");
  GaiaCookieManagerService::ExternalCcResultFetcher* result_fetcher =
      helper.external_cc_result_fetcher_for_testing();
  GaiaCookieManagerService::ExternalCcResultFetcher::LoaderToToken loaders =
      result_fetcher->get_loader_map_for_testing();
  ASSERT_EQ(1u, loaders.size());
  ASSERT_TRUE(IsLoadPending("http://www.bl.com"));
}

TEST_F(GaiaCookieManagerServiceTest, UbertokenSuccessFetchesExternalCCOnce) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());

  helper.external_cc_result_fetcher_for_testing()->Start();

  EXPECT_CALL(helper, StartFetchingUbertoken());
  helper.AddAccountToCookie("acc2@gmail.com", GaiaConstants::kChromeSource);
  // There is already a ExternalCCResultFetch underway. This will trigger
  // StartFetchingMergeSession.
  EXPECT_CALL(helper, StartFetchingMergeSession());
  SimulateUbertokenSuccess(&helper, "token3");
}
