// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/mutable_profile_oauth2_token_service_delegate.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/browser/webdata/token_web_data.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_token_service_test_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include "components/os_crypt/os_crypt.h"
#endif

// Defining constant here to handle backward compatiblity tests, but this
// constant is no longer used in current versions of chrome.
static const char kLSOService[] = "lso";
static const char kEmail[] = "user@gmail.com";

class MutableProfileOAuth2TokenServiceDelegateTest
    : public testing::Test,
      public OAuth2AccessTokenConsumer,
      public OAuth2TokenService::Observer {
 public:
  MutableProfileOAuth2TokenServiceDelegateTest()
      : factory_(NULL),
        access_token_success_count_(0),
        access_token_failure_count_(0),
        access_token_failure_(GoogleServiceAuthError::NONE),
        token_available_count_(0),
        token_revoked_count_(0),
        tokens_loaded_count_(0),
        start_batch_changes_(0),
        end_batch_changes_(0) {}

  void SetUp() override {
#if defined(OS_MACOSX)
    OSCrypt::UseMockKeychain(true);
#endif

    factory_.SetFakeResponse(GaiaUrls::GetInstance()->oauth2_revoke_url(), "",
                             net::HTTP_OK, net::URLRequestStatus::SUCCESS);

    pref_service_.registry()->RegisterListPref(
        AccountTrackerService::kAccountInfoPref);
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kAccountIdMigrationState,
        AccountTrackerService::MIGRATION_NOT_STARTED);
    client_.reset(new TestSigninClient(&pref_service_));
    client_->SetURLRequestContext(new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get()));
    client_->LoadTokenDatabase();
    account_tracker_service_.Initialize(client_.get());
    oauth2_service_delegate_.reset(new MutableProfileOAuth2TokenServiceDelegate(
        client_.get(), &signin_error_controller_, &account_tracker_service_));
    // Make sure PO2TS has a chance to load itself before continuing.
    base::RunLoop().RunUntilIdle();
    oauth2_service_delegate_->AddObserver(this);
  }

  void TearDown() override {
    oauth2_service_delegate_->RemoveObserver(this);
    oauth2_service_delegate_->Shutdown();
  }

  void AddAuthTokenManually(const std::string& service,
                            const std::string& value) {
    scoped_refptr<TokenWebData> token_web_data = client_->GetDatabase();
    if (token_web_data.get())
      token_web_data->SetTokenForService(service, value);
  }

  // OAuth2AccessTokenConusmer implementation
  void OnGetTokenSuccess(const std::string& access_token,
                         const base::Time& expiration_time) override {
    ++access_token_success_count_;
  }

  void OnGetTokenFailure(const GoogleServiceAuthError& error) override {
    ++access_token_failure_count_;
    access_token_failure_ = error;
  }

  // OAuth2TokenService::Observer implementation.
  void OnRefreshTokenAvailable(const std::string& account_id) override {
    ++token_available_count_;
  }
  void OnRefreshTokenRevoked(const std::string& account_id) override {
    ++token_revoked_count_;
  }
  void OnRefreshTokensLoaded() override { ++tokens_loaded_count_; }

  void OnStartBatchChanges() override { ++start_batch_changes_; }

  void OnEndBatchChanges() override { ++end_batch_changes_; }

  void ResetObserverCounts() {
    token_available_count_ = 0;
    token_revoked_count_ = 0;
    tokens_loaded_count_ = 0;
    start_batch_changes_ = 0;
    end_batch_changes_ = 0;
  }

  void ExpectNoNotifications() {
    EXPECT_EQ(0, token_available_count_);
    EXPECT_EQ(0, token_revoked_count_);
    EXPECT_EQ(0, tokens_loaded_count_);
    ResetObserverCounts();
  }

  void ExpectOneTokenAvailableNotification() {
    EXPECT_EQ(1, token_available_count_);
    EXPECT_EQ(0, token_revoked_count_);
    EXPECT_EQ(0, tokens_loaded_count_);
    ResetObserverCounts();
  }

  void ExpectOneTokenRevokedNotification() {
    EXPECT_EQ(0, token_available_count_);
    EXPECT_EQ(1, token_revoked_count_);
    EXPECT_EQ(0, tokens_loaded_count_);
    ResetObserverCounts();
  }

  void ExpectOneTokensLoadedNotification() {
    EXPECT_EQ(0, token_available_count_);
    EXPECT_EQ(0, token_revoked_count_);
    EXPECT_EQ(1, tokens_loaded_count_);
    ResetObserverCounts();
  }

 protected:
  base::MessageLoop message_loop_;
  net::FakeURLFetcherFactory factory_;
  std::unique_ptr<TestSigninClient> client_;
  std::unique_ptr<MutableProfileOAuth2TokenServiceDelegate>
      oauth2_service_delegate_;
  TestingOAuth2TokenServiceConsumer consumer_;
  SigninErrorController signin_error_controller_;
  TestingPrefServiceSimple pref_service_;
  AccountTrackerService account_tracker_service_;
  int access_token_success_count_;
  int access_token_failure_count_;
  GoogleServiceAuthError access_token_failure_;
  int token_available_count_;
  int token_revoked_count_;
  int tokens_loaded_count_;
  int start_batch_changes_;
  int end_batch_changes_;
};

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, PersistenceDBUpgrade) {
  std::string main_account_id(kEmail);
  std::string main_refresh_token("old_refresh_token");

  // Populate DB with legacy tokens.
  AddAuthTokenManually(GaiaConstants::kSyncService, "syncServiceToken");
  AddAuthTokenManually(kLSOService, "lsoToken");
  AddAuthTokenManually(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                       main_refresh_token);

  switches::EnableAccountConsistencyForTesting(
      base::CommandLine::ForCurrentProcess());

  // Force LoadCredentials.
  oauth2_service_delegate_->LoadCredentials(main_account_id);
  base::RunLoop().RunUntilIdle();

  // Legacy tokens get discarded, but the old refresh token is kept.
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(main_account_id));
  EXPECT_EQ(1U, oauth2_service_delegate_->refresh_tokens_.size());
  EXPECT_EQ(main_refresh_token,
            oauth2_service_delegate_->refresh_tokens_[main_account_id]
                ->refresh_token());

  // Add an old legacy token to the DB, to ensure it will not overwrite existing
  // credentials for main account.
  AddAuthTokenManually(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                       "secondOldRefreshToken");
  // Add some other legacy token. (Expected to get discarded).
  AddAuthTokenManually(kLSOService, "lsoToken");
  // Also add a token using PO2TS.UpdateCredentials and make sure upgrade does
  // not wipe it.
  std::string other_account_id("other_account_id");
  std::string other_refresh_token("other_refresh_token");
  oauth2_service_delegate_->UpdateCredentials(other_account_id,
                                              other_refresh_token);
  ResetObserverCounts();

  // Force LoadCredentials.
  oauth2_service_delegate_->LoadCredentials(main_account_id);
  base::RunLoop().RunUntilIdle();

  // Again legacy tokens get discarded, but since the main porfile account
  // token is present it is not overwritten.
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(main_refresh_token,
            oauth2_service_delegate_->GetRefreshToken(main_account_id));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(main_account_id));
  // TODO(fgorski): cover both using RefreshTokenIsAvailable() and then get the
  // tokens using GetRefreshToken()
  EXPECT_EQ(2U, oauth2_service_delegate_->refresh_tokens_.size());
  EXPECT_EQ(main_refresh_token,
            oauth2_service_delegate_->refresh_tokens_[main_account_id]
                ->refresh_token());
  EXPECT_EQ(other_refresh_token,
            oauth2_service_delegate_->refresh_tokens_[other_account_id]
                ->refresh_token());

  oauth2_service_delegate_->RevokeAllCredentials();
  EXPECT_EQ(2, start_batch_changes_);
  EXPECT_EQ(2, end_batch_changes_);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       PersistenceRevokeCredentials) {
  std::string account_id_1 = "account_id_1";
  std::string refresh_token_1 = "refresh_token_1";
  std::string account_id_2 = "account_id_2";
  std::string refresh_token_2 = "refresh_token_2";

  // TODO(fgorski): Enable below when implemented:
  // EXPECT_FALSE(oauth2_servive_->RefreshTokenIsAvailable(account_id_1));
  // EXPECT_FALSE(oauth2_servive_->RefreshTokenIsAvailable(account_id_2));

  oauth2_service_delegate_->UpdateCredentials(account_id_1, refresh_token_1);
  oauth2_service_delegate_->UpdateCredentials(account_id_2, refresh_token_2);
  EXPECT_EQ(2, start_batch_changes_);
  EXPECT_EQ(2, end_batch_changes_);

  // TODO(fgorski): Enable below when implemented:
  // EXPECT_TRUE(oauth2_servive_->RefreshTokenIsAvailable(account_id_1));
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(account_id_2));

  ResetObserverCounts();
  oauth2_service_delegate_->RevokeCredentials(account_id_1);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  ExpectOneTokenRevokedNotification();

  // TODO(fgorski): Enable below when implemented:
  // EXPECT_FALSE(oauth2_servive_->RefreshTokenIsAvailable(account_id_1));
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(account_id_2));

  oauth2_service_delegate_->RevokeAllCredentials();
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  ResetObserverCounts();
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       PersistenceLoadCredentials) {
  switches::EnableAccountConsistencyForTesting(
      base::CommandLine::ForCurrentProcess());

  // Ensure DB is clean.
  oauth2_service_delegate_->RevokeAllCredentials();
  ResetObserverCounts();
  // Perform a load from an empty DB.
  oauth2_service_delegate_->LoadCredentials("account_id");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  ExpectOneTokensLoadedNotification();
  // LoadCredentials() guarantees that the account given to it as argument
  // is in the refresh_token map.
  EXPECT_EQ(1U, oauth2_service_delegate_->refresh_tokens_.size());
  EXPECT_TRUE(oauth2_service_delegate_->refresh_tokens_["account_id"]
                  ->refresh_token()
                  .empty());
  // Setup a DB with tokens that don't require upgrade and clear memory.
  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token");
  oauth2_service_delegate_->UpdateCredentials("account_id2", "refresh_token2");
  oauth2_service_delegate_->refresh_tokens_.clear();
  EXPECT_EQ(2, start_batch_changes_);
  EXPECT_EQ(2, end_batch_changes_);
  ResetObserverCounts();

  oauth2_service_delegate_->LoadCredentials("account_id");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  ResetObserverCounts();

  // TODO(fgorski): Enable below when implemented:
  // EXPECT_TRUE(oauth2_servive_->RefreshTokenIsAvailable("account_id"));
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable("account_id2"));

  oauth2_service_delegate_->RevokeAllCredentials();
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(2, token_revoked_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  ResetObserverCounts();
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, PersistanceNotifications) {
  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token");
  ExpectOneTokenAvailableNotification();

  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token");
  ExpectNoNotifications();

  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token2");
  ExpectOneTokenAvailableNotification();

  oauth2_service_delegate_->RevokeCredentials("account_id");
  ExpectOneTokenRevokedNotification();

  oauth2_service_delegate_->UpdateCredentials("account_id", "refresh_token2");
  ExpectOneTokenAvailableNotification();

  oauth2_service_delegate_->RevokeAllCredentials();
  ResetObserverCounts();
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, GetAccounts) {
  EXPECT_TRUE(oauth2_service_delegate_->GetAccounts().empty());
  oauth2_service_delegate_->UpdateCredentials("account_id1", "refresh_token1");
  oauth2_service_delegate_->UpdateCredentials("account_id2", "refresh_token2");
  std::vector<std::string> accounts = oauth2_service_delegate_->GetAccounts();
  EXPECT_EQ(2u, accounts.size());
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), "account_id1"));
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), "account_id2"));
  oauth2_service_delegate_->RevokeCredentials("account_id2");
  accounts = oauth2_service_delegate_->GetAccounts();
  EXPECT_EQ(1u, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), "account_id1"));
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, FetchPersistentError) {
  oauth2_service_delegate_->UpdateCredentials(kEmail, "refreshToken");
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            signin_error_controller_.auth_error());

  GoogleServiceAuthError authfail(GoogleServiceAuthError::ACCOUNT_DELETED);
  oauth2_service_delegate_->UpdateAuthError(kEmail, authfail);
  EXPECT_NE(GoogleServiceAuthError::AuthErrorNone(),
            signin_error_controller_.auth_error());

  // Create a "success" fetch we don't expect to get called.
  factory_.SetFakeResponse(GaiaUrls::GetInstance()->oauth2_token_url(),
                           GetValidTokenResponse("token", 3600), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(0, access_token_failure_count_);
  std::vector<std::string> scope_list;
  scope_list.push_back("scope");
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher(
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          kEmail, oauth2_service_delegate_->GetRequestContext(), this));
  fetcher->Start("foo", "bar", scope_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(1, access_token_failure_count_);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, RetryBackoff) {
  oauth2_service_delegate_->UpdateCredentials(kEmail, "refreshToken");
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            signin_error_controller_.auth_error());

  GoogleServiceAuthError authfail(GoogleServiceAuthError::SERVICE_UNAVAILABLE);
  oauth2_service_delegate_->UpdateAuthError(kEmail, authfail);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            signin_error_controller_.auth_error());

  // Create a "success" fetch we don't expect to get called just yet.
  factory_.SetFakeResponse(GaiaUrls::GetInstance()->oauth2_token_url(),
                           GetValidTokenResponse("token", 3600), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  // Transient error will repeat until backoff period expires.
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(0, access_token_failure_count_);
  std::vector<std::string> scope_list;
  scope_list.push_back("scope");
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher1(
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          kEmail, oauth2_service_delegate_->GetRequestContext(), this));
  fetcher1->Start("foo", "bar", scope_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(1, access_token_failure_count_);
  // Expect a positive backoff time.
  EXPECT_GT(oauth2_service_delegate_->backoff_entry_.GetTimeUntilRelease(),
      TimeDelta());

  // Pretend that backoff has expired and try again.
  oauth2_service_delegate_->backoff_entry_.SetCustomReleaseTime(
      base::TimeTicks());
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher2(
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          kEmail, oauth2_service_delegate_->GetRequestContext(), this));
  fetcher2->Start("foo", "bar", scope_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, access_token_success_count_);
  EXPECT_EQ(1, access_token_failure_count_);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, ResetBackoff) {
  oauth2_service_delegate_->UpdateCredentials(kEmail, "refreshToken");
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            signin_error_controller_.auth_error());

  GoogleServiceAuthError authfail(GoogleServiceAuthError::SERVICE_UNAVAILABLE);
  oauth2_service_delegate_->UpdateAuthError(kEmail, authfail);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            signin_error_controller_.auth_error());

  // Create a "success" fetch we don't expect to get called just yet.
  factory_.SetFakeResponse(GaiaUrls::GetInstance()->oauth2_token_url(),
                           GetValidTokenResponse("token", 3600), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  // Transient error will repeat until backoff period expires.
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(0, access_token_failure_count_);
  std::vector<std::string> scope_list;
  scope_list.push_back("scope");
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher1(
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          kEmail, oauth2_service_delegate_->GetRequestContext(), this));
  fetcher1->Start("foo", "bar", scope_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(1, access_token_failure_count_);

  // Notify of network change and ensure that request now runs.
  oauth2_service_delegate_->OnNetworkChanged(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher2(
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          kEmail, oauth2_service_delegate_->GetRequestContext(), this));
  fetcher2->Start("foo", "bar", scope_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, access_token_success_count_);
  EXPECT_EQ(1, access_token_failure_count_);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, CanonicalizeAccountId) {
  std::map<std::string, std::string> tokens;
  tokens["AccountId-user@gmail.com"] = "refresh_token";
  tokens["AccountId-Foo.Bar@gmail.com"] = "refresh_token";
  tokens["AccountId-12345"] = "refresh_token";

  switches::EnableAccountConsistencyForTesting(
      base::CommandLine::ForCurrentProcess());
  oauth2_service_delegate_->LoadAllCredentialsIntoMemory(tokens);

  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("user@gmail.com"));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("foobar@gmail.com"));
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable("12345"));
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       CanonAndNonCanonAccountId) {
  std::map<std::string, std::string> tokens;
  tokens["AccountId-Foo.Bar@gmail.com"] = "bad_token";
  tokens["AccountId-foobar@gmail.com"] = "good_token";

  switches::EnableAccountConsistencyForTesting(
      base::CommandLine::ForCurrentProcess());
  oauth2_service_delegate_->LoadAllCredentialsIntoMemory(tokens);

  EXPECT_EQ(1u, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("foobar@gmail.com"));
  EXPECT_STREQ(
      "good_token",
      oauth2_service_delegate_->GetRefreshToken("foobar@gmail.com").c_str());
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, ShutdownService) {
  switches::EnableAccountConsistencyForTesting(
      base::CommandLine::ForCurrentProcess());
  EXPECT_TRUE(oauth2_service_delegate_->GetAccounts().empty());
  oauth2_service_delegate_->UpdateCredentials("account_id1", "refresh_token1");
  oauth2_service_delegate_->UpdateCredentials("account_id2", "refresh_token2");
  std::vector<std::string> accounts = oauth2_service_delegate_->GetAccounts();
  EXPECT_EQ(2u, accounts.size());
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), "account_id1"));
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), "account_id2"));
  oauth2_service_delegate_->LoadCredentials("account_id1");
  oauth2_service_delegate_->UpdateCredentials("account_id1", "refresh_token3");
  oauth2_service_delegate_->Shutdown();
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  EXPECT_TRUE(oauth2_service_delegate_->refresh_tokens_.empty());
  EXPECT_EQ(0, oauth2_service_delegate_->web_data_service_request_);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, GaiaIdMigration) {
  if (account_tracker_service_.GetMigrationState() !=
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    std::string email = "foo@gmail.com";
    std::string gaia_id = "foo's gaia id";

    switches::EnableAccountConsistencyForTesting(
        base::CommandLine::ForCurrentProcess());
    pref_service_.SetInteger(prefs::kAccountIdMigrationState,
                             AccountTrackerService::MIGRATION_NOT_STARTED);

    ListPrefUpdate update(&pref_service_,
                          AccountTrackerService::kAccountInfoPref);
    update->Clear();
    base::DictionaryValue* dict = new base::DictionaryValue();
    update->Append(dict);
    dict->SetString("account_id", base::UTF8ToUTF16(email));
    dict->SetString("email", base::UTF8ToUTF16(email));
    dict->SetString("gaia", base::UTF8ToUTF16(gaia_id));
    account_tracker_service_.Shutdown();
    account_tracker_service_.Initialize(client_.get());

    AddAuthTokenManually("AccountId-" + email, "refresh_token");
    oauth2_service_delegate_->LoadCredentials(gaia_id);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, tokens_loaded_count_);
    EXPECT_EQ(1, token_available_count_);
    EXPECT_EQ(1, start_batch_changes_);
    EXPECT_EQ(1, end_batch_changes_);

    std::vector<std::string> accounts = oauth2_service_delegate_->GetAccounts();
    EXPECT_EQ(1u, accounts.size());

    EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(email));
    EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(gaia_id));

    account_tracker_service_.SetMigrationDone();
    oauth2_service_delegate_->Shutdown();
    ResetObserverCounts();

    oauth2_service_delegate_->LoadCredentials(gaia_id);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, tokens_loaded_count_);
    EXPECT_EQ(1, token_available_count_);
    EXPECT_EQ(1, start_batch_changes_);
    EXPECT_EQ(1, end_batch_changes_);

    EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(email));
    EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(gaia_id));
    accounts = oauth2_service_delegate_->GetAccounts();
    EXPECT_EQ(1u, accounts.size());
  }
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       GaiaIdMigrationCrashInTheMiddle) {
  if (account_tracker_service_.GetMigrationState() !=
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    std::string email1 = "foo@gmail.com";
    std::string gaia_id1 = "foo's gaia id";
    std::string email2 = "bar@gmail.com";
    std::string gaia_id2 = "bar's gaia id";

    switches::EnableAccountConsistencyForTesting(
        base::CommandLine::ForCurrentProcess());
    pref_service_.SetInteger(prefs::kAccountIdMigrationState,
                             AccountTrackerService::MIGRATION_NOT_STARTED);

    ListPrefUpdate update(&pref_service_,
                          AccountTrackerService::kAccountInfoPref);
    update->Clear();
    base::DictionaryValue* dict = new base::DictionaryValue();
    update->Append(dict);
    dict->SetString("account_id", base::UTF8ToUTF16(email1));
    dict->SetString("email", base::UTF8ToUTF16(email1));
    dict->SetString("gaia", base::UTF8ToUTF16(gaia_id1));
    dict = new base::DictionaryValue();
    update->Append(dict);
    dict->SetString("account_id", base::UTF8ToUTF16(email2));
    dict->SetString("email", base::UTF8ToUTF16(email2));
    dict->SetString("gaia", base::UTF8ToUTF16(gaia_id2));
    account_tracker_service_.Shutdown();
    account_tracker_service_.Initialize(client_.get());

    AddAuthTokenManually("AccountId-" + email1, "refresh_token");
    AddAuthTokenManually("AccountId-" + email2, "refresh_token");
    AddAuthTokenManually("AccountId-" + gaia_id1, "refresh_token");
    oauth2_service_delegate_->LoadCredentials(gaia_id1);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, tokens_loaded_count_);
    EXPECT_EQ(2, token_available_count_);
    EXPECT_EQ(1, start_batch_changes_);
    EXPECT_EQ(1, end_batch_changes_);

    std::vector<std::string> accounts = oauth2_service_delegate_->GetAccounts();
    EXPECT_EQ(2u, accounts.size());

    EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(email1));
    EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(gaia_id1));
    EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(email2));
    EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(gaia_id2));

    account_tracker_service_.SetMigrationDone();
    oauth2_service_delegate_->Shutdown();
    ResetObserverCounts();

    oauth2_service_delegate_->LoadCredentials(gaia_id1);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, tokens_loaded_count_);
    EXPECT_EQ(2, token_available_count_);
    EXPECT_EQ(1, start_batch_changes_);
    EXPECT_EQ(1, end_batch_changes_);

    EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(email1));
    EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(gaia_id1));
    EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(email2));
    EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(gaia_id2));
    accounts = oauth2_service_delegate_->GetAccounts();
    EXPECT_EQ(2u, accounts.size());
  }
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       LoadPrimaryAccountOnlyWhenAccountConsistencyDisabled) {
  std::string primary_account = "primaryaccount";
  std::string secondary_account = "secondaryaccount";

  oauth2_service_delegate_->RevokeAllCredentials();
  ResetObserverCounts();
  AddAuthTokenManually("AccountId-" + primary_account, "refresh_token");
  AddAuthTokenManually("AccountId-" + secondary_account, "refresh_token");
  oauth2_service_delegate_->LoadCredentials(primary_account);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(primary_account));
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(secondary_account));

  oauth2_service_delegate_->RevokeAllCredentials();
  ResetObserverCounts();
  AddAuthTokenManually("AccountId-" + primary_account, "refresh_token");
  AddAuthTokenManually("AccountId-" + secondary_account, "refresh_token");
  switches::EnableAccountConsistencyForTesting(
      base::CommandLine::ForCurrentProcess());
  oauth2_service_delegate_->LoadCredentials(primary_account);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(1, start_batch_changes_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(primary_account));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(secondary_account));
}
