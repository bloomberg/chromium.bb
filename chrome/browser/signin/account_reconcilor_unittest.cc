// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/fake_gaia_cookie_manager_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_switches.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/fake_oauth2_token_service_delegate.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockAccountReconcilor : public testing::StrictMock<AccountReconcilor> {
 public:
  static std::unique_ptr<KeyedService> Build(content::BrowserContext* context);

  MockAccountReconcilor(ProfileOAuth2TokenService* token_service,
                        SigninManagerBase* signin_manager,
                        SigninClient* client,
                        GaiaCookieManagerService* cookie_manager_service);
  ~MockAccountReconcilor() override {}

  MOCK_METHOD1(PerformMergeAction, void(const std::string& account_id));
  MOCK_METHOD0(PerformLogoutAllAccountsAction, void());
};

// static
std::unique_ptr<KeyedService> MockAccountReconcilor::Build(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<AccountReconcilor> reconcilor(new MockAccountReconcilor(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      SigninManagerFactory::GetForProfile(profile),
      ChromeSigninClientFactory::GetForProfile(profile),
      GaiaCookieManagerServiceFactory::GetForProfile(profile)));
  reconcilor->Initialize(false /* start_reconcile_if_tokens_available */);
  return std::move(reconcilor);
}

MockAccountReconcilor::MockAccountReconcilor(
    ProfileOAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    SigninClient* client,
    GaiaCookieManagerService* cookie_manager_service)
    : testing::StrictMock<AccountReconcilor>(token_service,
                                             signin_manager,
                                             client,
                                             cookie_manager_service) {}

}  // namespace

class AccountReconcilorTest : public ::testing::TestWithParam<bool> {
 public:
  AccountReconcilorTest();
  void SetUp() override;

  TestingProfile* profile() { return profile_; }
  FakeSigninManagerForTesting* signin_manager() { return signin_manager_; }
  FakeProfileOAuth2TokenService* token_service() { return token_service_; }
  FakeOAuth2TokenServiceDelegate* token_service_delegate() {
    return static_cast<FakeOAuth2TokenServiceDelegate*>(
        token_service_->GetDelegate());
  }
  TestSigninClient* test_signin_client() { return test_signin_client_; }
  AccountTrackerService* account_tracker() { return account_tracker_; }
  FakeGaiaCookieManagerService* cookie_manager_service() {
    return cookie_manager_service_;
  }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  void SetFakeResponse(const std::string& url,
                       const std::string& data,
                       net::HttpStatusCode code,
                       net::URLRequestStatus::Status status) {
    url_fetcher_factory_.SetFakeResponse(GURL(url), data, code, status);
  }

  MockAccountReconcilor* GetMockReconcilor();

  std::string ConnectProfileToAccount(const std::string& gaia_id,
                                      const std::string& username);

  std::string PickAccountIdForAccount(const std::string& gaia_id,
                                      const std::string& username);

  void SimulateAddAccountToCookieCompleted(
      GaiaCookieManagerService::Observer* observer,
      const std::string& account_id,
      const GoogleServiceAuthError& error);

  void SimulateCookieContentSettingsChanged(
      content_settings::Observer* observer,
      const ContentSettingsPattern& primary_pattern);

  GURL get_check_connection_info_url() {
    return get_check_connection_info_url_;
  }

 private:
  content::TestBrowserThreadBundle bundle_;
  TestingProfile* profile_;
  FakeSigninManagerForTesting* signin_manager_;
  FakeProfileOAuth2TokenService* token_service_;
  TestSigninClient* test_signin_client_;
  AccountTrackerService* account_tracker_;
  FakeGaiaCookieManagerService* cookie_manager_service_;
  MockAccountReconcilor* mock_reconcilor_;
  net::FakeURLFetcherFactory url_fetcher_factory_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  base::HistogramTester histogram_tester_;
  GURL get_check_connection_info_url_;

  DISALLOW_COPY_AND_ASSIGN(AccountReconcilorTest);
};

AccountReconcilorTest::AccountReconcilorTest()
    : signin_manager_(NULL),
      token_service_(NULL),
      test_signin_client_(NULL),
      cookie_manager_service_(NULL),
      mock_reconcilor_(NULL),
      url_fetcher_factory_(NULL) {}

void AccountReconcilorTest::SetUp() {
  // If it's a non-parameterized test, or we have a parameter of true, set flag.
  if (!::testing::UnitTest::GetInstance()->current_test_info()->value_param() ||
      GetParam()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableNewProfileManagement);
  }

  get_check_connection_info_url_ =
      GaiaUrls::GetInstance()->GetCheckConnectionInfoURLWithSource(
          GaiaConstants::kChromeSource);

  testing_profile_manager_.reset(
      new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
  ASSERT_TRUE(testing_profile_manager_.get()->SetUp());

  TestingProfile::TestingFactories factories;
  factories.push_back(std::make_pair(ChromeSigninClientFactory::GetInstance(),
      signin::BuildTestSigninClient));
  factories.push_back(std::make_pair(
      ProfileOAuth2TokenServiceFactory::GetInstance(),
      BuildFakeProfileOAuth2TokenService));
  factories.push_back(std::make_pair(
      GaiaCookieManagerServiceFactory::GetInstance(),
      FakeGaiaCookieManagerService::Build));
  factories.push_back(std::make_pair(SigninManagerFactory::GetInstance(),
                                     BuildFakeSigninManagerBase));
  factories.push_back(std::make_pair(AccountReconcilorFactory::GetInstance(),
      MockAccountReconcilor::Build));

  profile_ = testing_profile_manager_.get()->CreateTestingProfile(
      "name", std::unique_ptr<syncable_prefs::PrefServiceSyncable>(),
      base::UTF8ToUTF16("name"), 0, std::string(), factories);

  test_signin_client_ =
      static_cast<TestSigninClient*>(
          ChromeSigninClientFactory::GetForProfile(profile()));

  token_service_ =
      static_cast<FakeProfileOAuth2TokenService*>(
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile()));

  account_tracker_ =
      AccountTrackerServiceFactory::GetForProfile(profile());

  signin_manager_ =
      static_cast<FakeSigninManagerForTesting*>(
          SigninManagerFactory::GetForProfile(profile()));

  test_signin_client_ =
      static_cast<TestSigninClient*>(
          ChromeSigninClientFactory::GetForProfile(profile()));

  cookie_manager_service_ =
      static_cast<FakeGaiaCookieManagerService*>(
        GaiaCookieManagerServiceFactory::GetForProfile(profile()));
  cookie_manager_service_->Init(&url_fetcher_factory_);

  cookie_manager_service_->SetListAccountsResponseHttpNotFound();
}

MockAccountReconcilor* AccountReconcilorTest::GetMockReconcilor() {
  if (!mock_reconcilor_) {
    mock_reconcilor_ =
        static_cast<MockAccountReconcilor*>(
            AccountReconcilorFactory::GetForProfile(profile()));
  }

  return mock_reconcilor_;
}

std::string AccountReconcilorTest::ConnectProfileToAccount(
    const std::string& gaia_id,
    const std::string& username) {
  const std::string account_id = PickAccountIdForAccount(gaia_id, username);
#if !defined(OS_CHROMEOS)
  signin_manager()->set_password("password");
#endif
  signin_manager()->SetAuthenticatedAccountInfo(gaia_id, username);
  token_service()->UpdateCredentials(account_id, "refresh_token");
  return account_id;
}

std::string AccountReconcilorTest::PickAccountIdForAccount(
    const std::string& gaia_id,
    const std::string& username) {
  return account_tracker()->PickAccountIdForAccount(gaia_id, username);
}

void AccountReconcilorTest::SimulateAddAccountToCookieCompleted(
    GaiaCookieManagerService::Observer* observer,
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  observer->OnAddAccountToCookieCompleted(account_id, error);
}

void AccountReconcilorTest::SimulateCookieContentSettingsChanged(
    content_settings::Observer* observer,
    const ContentSettingsPattern& primary_pattern) {
  observer->OnContentSettingChanged(
      primary_pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES,
      std::string());
}

TEST_F(AccountReconcilorTest, Basic) {
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
}

#if !defined(OS_CHROMEOS)

// This method requires the use of the |TestSigninClient| to be created from the
// |ChromeSigninClientFactory| because it overrides the |GoogleSigninSucceeded|
// method with an empty implementation. On MacOS, the normal implementation
// causes the try_bots to time out.
TEST_F(AccountReconcilorTest, SigninManagerRegistration) {
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
  ASSERT_FALSE(reconcilor->IsRegisteredWithTokenService());

  account_tracker()->SeedAccountInfo("12345", "user@gmail.com");
  signin_manager()->SignIn("12345", "user@gmail.com", "password");
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());

  signin_manager()->SignOut(signin_metrics::SIGNOUT_TEST,
                            signin_metrics::SignoutDelete::IGNORE_METRIC);
  ASSERT_FALSE(reconcilor->IsRegisteredWithTokenService());
}

// This method requires the use of the |TestSigninClient| to be created from the
// |ChromeSigninClientFactory| because it overrides the |GoogleSigninSucceeded|
// method with an empty implementation. On MacOS, the normal implementation
// causes the try_bots to time out.
TEST_F(AccountReconcilorTest, Reauth) {
  const std::string email = "user@gmail.com";
  const std::string account_id =
      ConnectProfileToAccount("12345", email);

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());

  // Simulate reauth.  The state of the reconcilor should not change.
  signin_manager()->OnExternalSigninCompleted(email);
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());
}

#endif  // !defined(OS_CHROMEOS)

TEST_F(AccountReconcilorTest, ProfileAlreadyConnected) {
  ConnectProfileToAccount("12345", "user@gmail.com");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());
}

TEST_F(AccountReconcilorTest, GetAccountsFromCookieSuccess) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseOneAccountWithExpiry(
      "user@gmail.com", "12345", true);
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
  reconcilor->StartReconcile();
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_RUNNING,
            reconcilor->GetState());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_RUNNING, reconcilor->GetState());

  std::vector<gaia::ListedAccount> accounts;
  ASSERT_TRUE(cookie_manager_service()->ListAccounts(&accounts));
  ASSERT_EQ(1u, accounts.size());
  ASSERT_EQ(account_id, accounts[0].id);
}

TEST_F(AccountReconcilorTest, GetAccountsFromCookieFailure) {
  ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseWebLoginRequired();

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
  reconcilor->StartReconcile();
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_RUNNING,
            reconcilor->GetState());
  base::RunLoop().RunUntilIdle();

  std::vector<gaia::ListedAccount> accounts;
  ASSERT_FALSE(cookie_manager_service()->ListAccounts(&accounts));
  ASSERT_EQ(0u, accounts.size());

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_ERROR,
            reconcilor->GetState());
}

TEST_P(AccountReconcilorTest, StartReconcileNoop) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  cookie_manager_service()->SetListAccountsResponseOneAccount(
      "user@gmail.com", "12345");

  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectTotalCount(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun", 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
}

TEST_P(AccountReconcilorTest, StartReconcileCookiesDisabled) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");
  test_signin_client()->set_are_signin_cookies_allowed(false);

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  std::vector<gaia::ListedAccount> accounts;
  // This will be the first call to ListAccounts.
  ASSERT_FALSE(cookie_manager_service()->ListAccounts(&accounts));
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_P(AccountReconcilorTest, StartReconcileContentSettings) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  test_signin_client()->set_are_signin_cookies_allowed(false);
  SimulateCookieContentSettingsChanged(reconcilor,
                                       ContentSettingsPattern::Wildcard());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  test_signin_client()->set_are_signin_cookies_allowed(true);
  SimulateCookieContentSettingsChanged(reconcilor,
                                       ContentSettingsPattern::Wildcard());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
}

TEST_P(AccountReconcilorTest, StartReconcileContentSettingsGaiaUrl) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SimulateCookieContentSettingsChanged(
      reconcilor,
      ContentSettingsPattern::FromURL(GaiaUrls::GetInstance()->gaia_url()));
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
}

TEST_P(AccountReconcilorTest, StartReconcileContentSettingsNonGaiaUrl) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SimulateCookieContentSettingsChanged(
      reconcilor,
      ContentSettingsPattern::FromURL(GURL("http://www.example.com")));
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_P(AccountReconcilorTest, StartReconcileContentSettingsInvalidPattern) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  std::unique_ptr<ContentSettingsPattern::BuilderInterface> builder(
      ContentSettingsPattern::CreateBuilder(false));
  builder->Invalid();

  SimulateCookieContentSettingsChanged(reconcilor, builder->Build());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
}

// This test is needed until chrome changes to use gaia obfuscated id.
// The signin manager and token service use the gaia "email" property, which
// preserves dots in usernames and preserves case. gaia::ParseListAccountsData()
// however uses gaia "displayEmail" which does not preserve case, and then
// passes the string through gaia::CanonicalizeEmail() which removes dots.  This
// tests makes sure that an email like "Dot.S@hmail.com", as seen by the
// token service, will be considered the same as "dots@gmail.com" as returned
// by gaia::ParseListAccountsData().
TEST_P(AccountReconcilorTest, StartReconcileNoopWithDots) {
  if (account_tracker()->GetMigrationState() !=
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    return;
  }

  const std::string account_id =
      ConnectProfileToAccount("12345", "Dot.S@gmail.com");
  cookie_manager_service()->SetListAccountsResponseOneAccount(
      "dot.s@gmail.com", "12345");
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
}

TEST_P(AccountReconcilorTest, StartReconcileNoopMultiple) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      "user@gmail.com", "12345", "other@gmail.com", "67890");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectTotalCount(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun", 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
}

TEST_P(AccountReconcilorTest, StartReconcileAddToCookie) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseOneAccount(
      "user@gmail.com", "12345");

  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id2));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateAddAccountToCookieCompleted(reconcilor, account_id2,
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.FirstRun", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 0, 1);

  base::HistogramTester::CountsMap expected_counts;
  expected_counts["Signin.Reconciler.Duration.Success"] = 1;
  EXPECT_THAT(histogram_tester()->GetTotalCountsForPrefix(
      "Signin.Reconciler.Duration.Success"),
      testing::ContainerEq(expected_counts));
}

#if !defined(OS_CHROMEOS)
// This test does not run on ChromeOS because it calls
// FakeSigninManagerForTesting::SignOut() which doesn't exist for ChromeOS.

TEST_F(AccountReconcilorTest, SignoutAfterErrorDoesNotRecordUma) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseOneAccount(
      "user@gmail.com", "12345");

  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id2));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  GoogleServiceAuthError
    error(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  SimulateAddAccountToCookieCompleted(reconcilor, account_id2, error);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
  signin_manager()->SignOut(signin_metrics::SIGNOUT_TEST,
                            signin_metrics::SignoutDelete::IGNORE_METRIC);

  base::HistogramTester::CountsMap expected_counts;
  expected_counts["Signin.Reconciler.Duration.Failure"] = 1;
  EXPECT_THAT(histogram_tester()->GetTotalCountsForPrefix(
      "Signin.Reconciler.Duration.Failure"),
      testing::ContainerEq(expected_counts));
}

#endif  // !defined(OS_CHROMEOS)

TEST_P(AccountReconcilorTest, StartReconcileRemoveFromCookie) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      "user@gmail.com", "12345", "other@gmail.com", "67890");

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  SimulateAddAccountToCookieCompleted(reconcilor, account_id,
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.FirstRun", 0, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 1, 1);
}

TEST_P(AccountReconcilorTest, StartReconcileAddToCookieTwice) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  const std::string account_id3 =
      PickAccountIdForAccount("34567", "third@gmail.com");

  cookie_manager_service()->SetListAccountsResponseOneAccount(
      "user@gmail.com", "12345");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id2));
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id3));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateAddAccountToCookieCompleted(
      reconcilor, account_id2, GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.FirstRun", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 0, 1);

  // Do another pass after I've added a third account to the token service
  cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      "user@gmail.com", "12345", "other@gmail.com", "67890");
  cookie_manager_service()->set_list_accounts_stale_for_testing(true);

  // This will cause the reconcilor to fire.
  token_service()->UpdateCredentials(account_id3, "refresh_token");
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateAddAccountToCookieCompleted(
      reconcilor, account_id3, GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.FirstRun", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 0, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.SubsequentRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.SubsequentRun", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.SubsequentRun", 0, 1);
}

TEST_P(AccountReconcilorTest, StartReconcileBadPrimary) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");

  token_service()->UpdateCredentials(account_id2, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      "other@gmail.com", "67890", "user@gmail.com", "12345");

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id2));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateAddAccountToCookieCompleted(reconcilor, account_id2,
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateAddAccountToCookieCompleted(reconcilor, account_id,
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::COOKIE_AND_TOKEN_PRIMARIES_DIFFERENT,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.FirstRun", 0, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 0, 1);
}

TEST_P(AccountReconcilorTest, StartReconcileOnlyOnce) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseOneAccount(
      "user@gmail.com", "12345");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_P(AccountReconcilorTest, StartReconcileWithSessionInfoExpiredDefault) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id2, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseTwoAccountsWithExpiry(
      "user@gmail.com", "12345", true, "other@gmail.com", "67890", false);

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  SimulateAddAccountToCookieCompleted(reconcilor, account_id,
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, AddAccountToCookieCompletedWithBogusAccount) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseOneAccountWithExpiry(
      "user@gmail.com", "12345", true);

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();

  // If an unknown account id is sent, it should not upset the state.
  SimulateAddAccountToCookieCompleted(reconcilor, "bogus_account_id",
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateAddAccountToCookieCompleted(reconcilor, account_id,
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, NoLoopWithBadPrimary) {
  // Connect profile to a primary account and then add a secondary account.
  const std::string account_id1 =
  ConnectProfileToAccount("12345", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id1));
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id2));

  // The primary account is in auth error, so it is not in the cookie.
  cookie_manager_service()->SetListAccountsResponseOneAccountWithExpiry(
      "other@gmail.com", "67890", true);

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  GoogleServiceAuthError
      error(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);

  // The primary cannot be added to cookie, so it fails.
  SimulateAddAccountToCookieCompleted(
      reconcilor, account_id1, error);
  SimulateAddAccountToCookieCompleted(reconcilor, account_id2,
                                      GoogleServiceAuthError::AuthErrorNone());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_TRUE(reconcilor->error_during_last_reconcile_);
  testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());

  // Now that we've tried once, the token service knows that the primary
  // account has an auth error.
  token_service_delegate()->SetLastErrorForAccount(account_id1, error);

  // A second attempt to reconcile should be a noop.
  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());
}

TEST_F(AccountReconcilorTest, WontMergeAccountsWithError) {
  // Connect profile to a primary account and then add a secondary account.
  const std::string account_id1 =
  ConnectProfileToAccount("12345", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  // Mark the secondary account in auth error state.
  token_service_delegate()->SetLastErrorForAccount(
      account_id2,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // The cookie starts empty.
  cookie_manager_service()->SetListAccountsResponseNoAccounts();

  // Since the cookie jar starts empty, the reconcilor should attempt to merge
  // accounts into it.  However, it should only try accounts not in auth
  // error state.
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id1));

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateAddAccountToCookieCompleted(
      reconcilor, account_id1, GoogleServiceAuthError::AuthErrorNone());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_FALSE(reconcilor->error_during_last_reconcile_);
}

INSTANTIATE_TEST_CASE_P(AccountReconcilorMaybeEnabled,
                        AccountReconcilorTest,
                        testing::Bool());
