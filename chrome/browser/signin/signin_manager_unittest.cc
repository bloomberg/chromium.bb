// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/account_fetcher_service_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/fake_account_fetcher_service_builder.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_account_fetcher_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<KeyedService> SigninManagerBuild(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  std::unique_ptr<SigninManager> service(new SigninManager(
      ChromeSigninClientFactory::GetInstance()->GetForProfile(profile),
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      AccountTrackerServiceFactory::GetForProfile(profile),
      GaiaCookieManagerServiceFactory::GetForProfile(profile)));
  service->Initialize(NULL);
  return std::move(service);
}

class TestSigninManagerObserver : public SigninManagerBase::Observer {
 public:
  TestSigninManagerObserver()
      : num_failed_signins_(0),
        num_successful_signins_(0),
        num_successful_signins_with_password_(0),
        num_signouts_(0) {}

  ~TestSigninManagerObserver() override {}

  int num_failed_signins_;
  int num_successful_signins_;
  int num_successful_signins_with_password_;
  int num_signouts_;

 private:
  // SigninManagerBase::Observer:
  void GoogleSigninFailed(const GoogleServiceAuthError& error) override {
    num_failed_signins_++;
  }

  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username) override {
    num_successful_signins_++;
  }

  void GoogleSigninSucceededWithPassword(const std::string& account_id,
                                         const std::string& username,
                                         const std::string& password) override {
    num_successful_signins_with_password_++;
  }

  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override {
    num_signouts_++;
  }
};

}  // namespace


class SigninManagerTest : public testing::Test {
 public:
  SigninManagerTest() : manager_(NULL) {}
  ~SigninManagerTest() override {}

  void SetUp() override {
    manager_ = NULL;
    prefs_.reset(new TestingPrefServiceSimple);
    RegisterLocalState(prefs_->registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(
        prefs_.get());
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                              BuildFakeProfileOAuth2TokenService);
    builder.AddTestingFactory(ChromeSigninClientFactory::GetInstance(),
                              signin::BuildTestSigninClient);
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              SigninManagerBuild);
    builder.AddTestingFactory(AccountFetcherServiceFactory::GetInstance(),
                              FakeAccountFetcherServiceBuilder::BuildForTests);
    profile_ = builder.Build();

    TestSigninClient* client = static_cast<TestSigninClient*>(
        ChromeSigninClientFactory::GetForProfile(profile()));
    client->SetURLRequestContext(profile_->GetRequestContext());
  }

  void TearDown() override {
    if (manager_)
      manager_->RemoveObserver(&test_observer_);

    // Destroy the SigninManager here, because it relies on profile() which is
    // freed in the base class.
    if (naked_manager_) {
      naked_manager_->Shutdown();
      naked_manager_.reset(NULL);
    }
    TestingBrowserProcess::GetGlobal()->SetLocalState(NULL);

    // Manually destroy PrefService and Profile so that they are shutdown
    // in the correct order.  Both need to be destroyed before the
    // |thread_bundle_| member.
    profile_.reset();
    prefs_.reset();  // LocalState needs to outlive the profile.
  }

  TestingProfile* profile() { return profile_.get(); }

  TestSigninClient* signin_client() {
      return static_cast<TestSigninClient*>(
          ChromeSigninClientFactory::GetInstance()->GetForProfile(profile()));
  }

  // Seed the account tracker with information from logged in user.  Normally
  // this is done by UI code before calling SigninManager.  Returns the string
  // to use as the account_id.
  std::string AddToAccountTracker(const std::string& gaia_id,
                                  const std::string& email) {
    AccountTrackerService* service =
        AccountTrackerServiceFactory::GetForProfile(profile());
    service->SeedAccountInfo(gaia_id, email);
    return service->PickAccountIdForAccount(gaia_id, email);
  }

  // Sets up the signin manager as a service if other code will try to get it as
  // a PKS.
  void SetUpSigninManagerAsService() {
    DCHECK(!manager_);
    DCHECK(!naked_manager_);
    manager_ = static_cast<SigninManager*>(
        SigninManagerFactory::GetForProfile(profile()));
    manager_->AddObserver(&test_observer_);
  }

  // Create a naked signin manager if integration with PKSs is not needed.
  void CreateNakedSigninManager() {
    DCHECK(!manager_);
    naked_manager_.reset(new SigninManager(
        ChromeSigninClientFactory::GetInstance()->GetForProfile(profile()),
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile()),
        AccountTrackerServiceFactory::GetForProfile(profile()),
        GaiaCookieManagerServiceFactory::GetForProfile(profile())));

    manager_ = naked_manager_.get();
    manager_->AddObserver(&test_observer_);
  }

  // Shuts down |manager_|.
  void ShutDownManager() {
    DCHECK(manager_);
    manager_->RemoveObserver(&test_observer_);
    manager_->Shutdown();
    if (naked_manager_)
      naked_manager_.reset(NULL);
    manager_ = NULL;
  }

  void ExpectSignInWithRefreshTokenSuccess() {
    EXPECT_TRUE(manager_->IsAuthenticated());
    EXPECT_FALSE(manager_->GetAuthenticatedAccountId().empty());
    EXPECT_FALSE(manager_->GetAuthenticatedAccountInfo().email.empty());

    ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile());
    EXPECT_TRUE(token_service->RefreshTokenIsAvailable(
        manager_->GetAuthenticatedAccountId()));

    // Should go into token service and stop.
    EXPECT_EQ(1, test_observer_.num_successful_signins_);
    EXPECT_EQ(1, test_observer_.num_successful_signins_with_password_);
    EXPECT_EQ(0, test_observer_.num_failed_signins_);
  }

  void CompleteSigninCallback(const std::string& oauth_token) {
    oauth_tokens_fetched_.push_back(oauth_token);
    manager_->CompletePendingSignin();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  net::TestURLFetcherFactory factory_;
  std::unique_ptr<SigninManager> naked_manager_;
  SigninManager* manager_;
  TestSigninManagerObserver test_observer_;
  std::unique_ptr<TestingProfile> profile_;
  std::vector<std::string> oauth_tokens_fetched_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::vector<std::string> cookies_;
};

TEST_F(SigninManagerTest, SignInWithRefreshToken) {
  SetUpSigninManagerAsService();
  EXPECT_FALSE(manager_->IsAuthenticated());

  std::string account_id = AddToAccountTracker("gaia_id", "user@gmail.com");
  manager_->StartSignInWithRefreshToken(
      "rt",
      "gaia_id",
      "user@gmail.com",
      "password",
      SigninManager::OAuthTokenFetchedCallback());

  ExpectSignInWithRefreshTokenSuccess();

  // Should persist across resets.
  ShutDownManager();
  CreateNakedSigninManager();
  manager_->Initialize(NULL);
  EXPECT_EQ(account_id, manager_->GetAuthenticatedAccountId());
}

TEST_F(SigninManagerTest, SignInWithRefreshTokenCallbackComplete) {
  SetUpSigninManagerAsService();
  EXPECT_FALSE(manager_->IsAuthenticated());

  // Since the password is empty, must verify the gaia cookies first.
  SigninManager::OAuthTokenFetchedCallback callback =
      base::Bind(&SigninManagerTest::CompleteSigninCallback,
                 base::Unretained(this));
  manager_->StartSignInWithRefreshToken(
      "rt",
      "gaia_id",
      "user@gmail.com",
      "password",
      callback);

  ExpectSignInWithRefreshTokenSuccess();
  ASSERT_EQ(1U, oauth_tokens_fetched_.size());
  EXPECT_EQ(oauth_tokens_fetched_[0], "rt");
}

TEST_F(SigninManagerTest, SignInWithRefreshTokenCallsPostSignout) {
  SetUpSigninManagerAsService();
  EXPECT_FALSE(manager_->IsAuthenticated());

  std::string gaia_id = "12345";
  std::string email = "user@google.com";

  AccountTrackerService* account_tracker_service =
      AccountTrackerServiceFactory::GetForProfile(profile());
  account_tracker_service->SeedAccountInfo(gaia_id, email);
  FakeAccountFetcherService* account_fetcher_service =
      static_cast<FakeAccountFetcherService*>(
          AccountFetcherServiceFactory::GetForProfile(profile()));
  account_fetcher_service->OnRefreshTokensLoaded();

  ASSERT_TRUE(signin_client()->get_signed_in_password().empty());

  manager_->StartSignInWithRefreshToken(
      "rt1",
      gaia_id,
      email,
      "password",
      SigninManager::OAuthTokenFetchedCallback());

  // PostSignedIn is not called until the AccountTrackerService returns.
  ASSERT_EQ("", signin_client()->get_signed_in_password());

  account_fetcher_service->FakeUserInfoFetchSuccess(
      account_tracker_service->PickAccountIdForAccount(gaia_id, email), email,
      gaia_id, "google.com", "full_name", "given_name", "locale",
      "http://www.google.com");

  // AccountTracker and SigninManager are both done and PostSignedIn was called.
  ASSERT_EQ("password", signin_client()->get_signed_in_password());

  ExpectSignInWithRefreshTokenSuccess();
}

TEST_F(SigninManagerTest, SignOut) {
  SetUpSigninManagerAsService();
  manager_->StartSignInWithRefreshToken(
      "rt",
      "gaia_id",
      "user@gmail.com",
      "password",
      SigninManager::OAuthTokenFetchedCallback());
  manager_->SignOut(signin_metrics::SIGNOUT_TEST,
                    signin_metrics::SignoutDelete::IGNORE_METRIC);
  EXPECT_FALSE(manager_->IsAuthenticated());
  EXPECT_TRUE(manager_->GetAuthenticatedAccountInfo().email.empty());
  EXPECT_TRUE(manager_->GetAuthenticatedAccountId().empty());
  // Should not be persisted anymore
  ShutDownManager();
  CreateNakedSigninManager();
  manager_->Initialize(NULL);
  EXPECT_FALSE(manager_->IsAuthenticated());
  EXPECT_TRUE(manager_->GetAuthenticatedAccountInfo().email.empty());
  EXPECT_TRUE(manager_->GetAuthenticatedAccountId().empty());
}

TEST_F(SigninManagerTest, SignOutWhileProhibited) {
  SetUpSigninManagerAsService();
  EXPECT_FALSE(manager_->IsAuthenticated());
  EXPECT_TRUE(manager_->GetAuthenticatedAccountInfo().email.empty());
  EXPECT_TRUE(manager_->GetAuthenticatedAccountId().empty());

  manager_->SetAuthenticatedAccountInfo("gaia_id", "user@gmail.com");
  manager_->ProhibitSignout(true);
  manager_->SignOut(signin_metrics::SIGNOUT_TEST,
                    signin_metrics::SignoutDelete::IGNORE_METRIC);
  EXPECT_TRUE(manager_->IsAuthenticated());
  manager_->ProhibitSignout(false);
  manager_->SignOut(signin_metrics::SIGNOUT_TEST,
                    signin_metrics::SignoutDelete::IGNORE_METRIC);
  EXPECT_FALSE(manager_->IsAuthenticated());
}

TEST_F(SigninManagerTest, Prohibited) {
  g_browser_process->local_state()->SetString(
      prefs::kGoogleServicesUsernamePattern, ".*@google.com");
  CreateNakedSigninManager();
  manager_->Initialize(g_browser_process->local_state());
  EXPECT_TRUE(manager_->IsAllowedUsername("test@google.com"));
  EXPECT_TRUE(manager_->IsAllowedUsername("happy@google.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername("test@invalid.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername("test@notgoogle.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername(std::string()));
}

TEST_F(SigninManagerTest, TestAlternateWildcard) {
  // Test to make sure we accept "*@google.com" as a pattern (treat it as if
  // the admin entered ".*@google.com").
  g_browser_process->local_state()->SetString(
      prefs::kGoogleServicesUsernamePattern, "*@google.com");
  CreateNakedSigninManager();
  manager_->Initialize(g_browser_process->local_state());
  EXPECT_TRUE(manager_->IsAllowedUsername("test@google.com"));
  EXPECT_TRUE(manager_->IsAllowedUsername("happy@google.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername("test@invalid.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername("test@notgoogle.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername(std::string()));
}

TEST_F(SigninManagerTest, ProhibitedAtStartup) {
  std::string account_id = AddToAccountTracker("gaia_id", "user@gmail.com");
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesAccountId, account_id);
  g_browser_process->local_state()->SetString(
      prefs::kGoogleServicesUsernamePattern, ".*@google.com");
  CreateNakedSigninManager();
  manager_->Initialize(g_browser_process->local_state());
  // Currently signed in user is prohibited by policy, so should be signed out.
  EXPECT_EQ("", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ("", manager_->GetAuthenticatedAccountId());
}

TEST_F(SigninManagerTest, ProhibitedAfterStartup) {
  std::string account_id = AddToAccountTracker("gaia_id", "user@gmail.com");
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesAccountId, account_id);
  CreateNakedSigninManager();
  manager_->Initialize(g_browser_process->local_state());
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ(account_id, manager_->GetAuthenticatedAccountId());
  // Update the profile - user should be signed out.
  g_browser_process->local_state()->SetString(
      prefs::kGoogleServicesUsernamePattern, ".*@google.com");
  EXPECT_EQ("", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ("", manager_->GetAuthenticatedAccountId());
}

TEST_F(SigninManagerTest, ExternalSignIn) {
  CreateNakedSigninManager();
  manager_->Initialize(g_browser_process->local_state());
  EXPECT_EQ("", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ("", manager_->GetAuthenticatedAccountId());
  EXPECT_EQ(0, test_observer_.num_successful_signins_);
  EXPECT_EQ(0, test_observer_.num_successful_signins_with_password_);

  std::string account_id = AddToAccountTracker("gaia_id", "user@gmail.com");
  manager_->OnExternalSigninCompleted("user@gmail.com");
  EXPECT_EQ(1, test_observer_.num_successful_signins_);
  EXPECT_EQ(1, test_observer_.num_successful_signins_with_password_);
  EXPECT_EQ(0, test_observer_.num_failed_signins_);
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ(account_id, manager_->GetAuthenticatedAccountId());
}

TEST_F(SigninManagerTest, ExternalSignIn_ReauthShouldNotSendNotification) {
  CreateNakedSigninManager();
  manager_->Initialize(g_browser_process->local_state());
  EXPECT_EQ("", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ("", manager_->GetAuthenticatedAccountId());
  EXPECT_EQ(0, test_observer_.num_successful_signins_);
  EXPECT_EQ(0, test_observer_.num_successful_signins_with_password_);

  std::string account_id = AddToAccountTracker("gaia_id", "user@gmail.com");
  manager_->OnExternalSigninCompleted("user@gmail.com");
  EXPECT_EQ(1, test_observer_.num_successful_signins_);
  EXPECT_EQ(1, test_observer_.num_successful_signins_with_password_);
  EXPECT_EQ(0, test_observer_.num_failed_signins_);
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ(account_id, manager_->GetAuthenticatedAccountId());

  manager_->OnExternalSigninCompleted("user@gmail.com");
  EXPECT_EQ(1, test_observer_.num_successful_signins_);
  EXPECT_EQ(1, test_observer_.num_successful_signins_with_password_);
  EXPECT_EQ(0, test_observer_.num_failed_signins_);
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ(account_id, manager_->GetAuthenticatedAccountId());
}

TEST_F(SigninManagerTest, SigninNotAllowed) {
  std::string user("user@google.com");
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesAccountId, user);
  profile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  CreateNakedSigninManager();
  AddToAccountTracker("gaia_id", user);
  manager_->Initialize(g_browser_process->local_state());
  // Currently signing in is prohibited by policy, so should be signed out.
  EXPECT_EQ("", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ("", manager_->GetAuthenticatedAccountId());
}

TEST_F(SigninManagerTest, UpgradeToNewPrefs) {
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                   "user@gmail.com");
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesUserAccountId,
                                   "account_id");
  CreateNakedSigninManager();
  manager_->Initialize(g_browser_process->local_state());
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedAccountInfo().email);

  AccountTrackerService* service =
      AccountTrackerServiceFactory::GetForProfile(profile());
  if (service->GetMigrationState() ==
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    // TODO(rogerta): until the migration to gaia id, the account id will remain
    // the old username.
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedAccountId());
  EXPECT_EQ("user@gmail.com",
            profile()->GetPrefs()->GetString(prefs::kGoogleServicesAccountId));
  } else {
    EXPECT_EQ("account_id", manager_->GetAuthenticatedAccountId());
    EXPECT_EQ("account_id", profile()->GetPrefs()->GetString(
                                prefs::kGoogleServicesAccountId));
  }
  EXPECT_EQ("",
            profile()->GetPrefs()->GetString(prefs::kGoogleServicesUsername));

  // Make sure account tracker was updated.
  AccountInfo info =
      service->GetAccountInfo(manager_->GetAuthenticatedAccountId());
  EXPECT_EQ("user@gmail.com", info.email);
  EXPECT_EQ("account_id", info.gaia);
}

TEST_F(SigninManagerTest, CanonicalizesPrefs) {
  AccountTrackerService* service =
      AccountTrackerServiceFactory::GetForProfile(profile());

  // This unit test is not needed after migrating to gaia id.
  if (service->GetMigrationState() ==
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    profile()->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                     "user.C@gmail.com");

    CreateNakedSigninManager();
    manager_->Initialize(g_browser_process->local_state());
    EXPECT_EQ("user.C@gmail.com",
              manager_->GetAuthenticatedAccountInfo().email);

    // TODO(rogerta): until the migration to gaia id, the account id will remain
    // the old username.
    EXPECT_EQ("userc@gmail.com", manager_->GetAuthenticatedAccountId());
    EXPECT_EQ("userc@gmail.com", profile()->GetPrefs()->GetString(
                                     prefs::kGoogleServicesAccountId));
    EXPECT_EQ("",
              profile()->GetPrefs()->GetString(prefs::kGoogleServicesUsername));

    // Make sure account tracker has a canonicalized username.
    AccountInfo info =
        service->GetAccountInfo(manager_->GetAuthenticatedAccountId());
    EXPECT_EQ("user.C@gmail.com", info.email);
    EXPECT_EQ("userc@gmail.com", info.account_id);
  }
}

TEST_F(SigninManagerTest, GaiaIdMigration) {
  AccountTrackerService* tracker =
      AccountTrackerServiceFactory::GetForProfile(profile());
  if (tracker->GetMigrationState() !=
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    std::string email = "user@gmail.com";
    std::string gaia_id = "account_gaia_id";

    CreateNakedSigninManager();

    PrefService* client_prefs = signin_client()->GetPrefs();
    client_prefs->SetInteger(prefs::kAccountIdMigrationState,
                             AccountTrackerService::MIGRATION_NOT_STARTED);
    ListPrefUpdate update(client_prefs,
                          AccountTrackerService::kAccountInfoPref);
    update->Clear();
    auto dict = base::MakeUnique<base::DictionaryValue>();
    dict->SetString("account_id", base::UTF8ToUTF16(email));
    dict->SetString("email", base::UTF8ToUTF16(email));
    dict->SetString("gaia", base::UTF8ToUTF16(gaia_id));
    update->Append(std::move(dict));

    tracker->Shutdown();
    tracker->Initialize(signin_client());

    client_prefs->SetString(prefs::kGoogleServicesAccountId, email);
    manager_->Initialize(g_browser_process->local_state());

    EXPECT_EQ(gaia_id, manager_->GetAuthenticatedAccountId());
    EXPECT_EQ(gaia_id, profile()->GetPrefs()->GetString(
                           prefs::kGoogleServicesAccountId));
  }
}

TEST_F(SigninManagerTest, VeryOldProfileGaiaIdMigration) {
  AccountTrackerService* tracker =
      AccountTrackerServiceFactory::GetForProfile(profile());
  if (tracker->GetMigrationState() !=
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    std::string email = "user@gmail.com";
    std::string gaia_id = "account_gaia_id";

    CreateNakedSigninManager();

    PrefService* client_prefs = signin_client()->GetPrefs();
    client_prefs->SetInteger(prefs::kAccountIdMigrationState,
                             AccountTrackerService::MIGRATION_NOT_STARTED);
    ListPrefUpdate update(client_prefs,
                          AccountTrackerService::kAccountInfoPref);
    update->Clear();
    auto dict = base::MakeUnique<base::DictionaryValue>();
    dict->SetString("account_id", base::UTF8ToUTF16(email));
    dict->SetString("email", base::UTF8ToUTF16(email));
    dict->SetString("gaia", base::UTF8ToUTF16(gaia_id));
    update->Append(std::move(dict));

    tracker->Shutdown();
    tracker->Initialize(signin_client());

    client_prefs->ClearPref(prefs::kGoogleServicesAccountId);
    client_prefs->SetString(prefs::kGoogleServicesUsername, email);
    manager_->Initialize(g_browser_process->local_state());

    EXPECT_EQ(gaia_id, manager_->GetAuthenticatedAccountId());
    EXPECT_EQ(gaia_id, profile()->GetPrefs()->GetString(
                           prefs::kGoogleServicesAccountId));
  }
}

TEST_F(SigninManagerTest, GaiaIdMigrationCrashInTheMiddle) {
  AccountTrackerService* tracker =
      AccountTrackerServiceFactory::GetForProfile(profile());
  if (tracker->GetMigrationState() !=
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    std::string email = "user@gmail.com";
    std::string gaia_id = "account_gaia_id";

    CreateNakedSigninManager();

    PrefService* client_prefs = signin_client()->GetPrefs();
    client_prefs->SetInteger(prefs::kAccountIdMigrationState,
                             AccountTrackerService::MIGRATION_NOT_STARTED);
    ListPrefUpdate update(client_prefs,
                          AccountTrackerService::kAccountInfoPref);
    update->Clear();
    auto dict = base::MakeUnique<base::DictionaryValue>();
    dict->SetString("account_id", base::UTF8ToUTF16(email));
    dict->SetString("email", base::UTF8ToUTF16(email));
    dict->SetString("gaia", base::UTF8ToUTF16(gaia_id));
    update->Append(std::move(dict));

    tracker->Shutdown();
    tracker->Initialize(signin_client());

    client_prefs->SetString(prefs::kGoogleServicesAccountId, gaia_id);
    manager_->Initialize(g_browser_process->local_state());

    EXPECT_EQ(gaia_id, manager_->GetAuthenticatedAccountId());
    EXPECT_EQ(gaia_id, profile()->GetPrefs()->GetString(
                           prefs::kGoogleServicesAccountId));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(AccountTrackerService::MIGRATION_DONE,
              tracker->GetMigrationState());
  }
}
