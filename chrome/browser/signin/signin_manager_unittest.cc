// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_manager.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
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

KeyedService* SigninManagerBuild(content::BrowserContext* context) {
  SigninManager* service = NULL;
  Profile* profile = static_cast<Profile*>(context);
  service = new SigninManager(
      ChromeSigninClientFactory::GetInstance()->GetForProfile(profile),
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile));
  service->Initialize(NULL);
  return service;
}

class TestSigninManagerObserver : public SigninManagerBase::Observer {
 public:
  TestSigninManagerObserver() : num_failed_signins_(0),
                                num_successful_signins_(0),
                                num_signouts_(0) {
  }

  virtual ~TestSigninManagerObserver() {}

  int num_failed_signins_;
  int num_successful_signins_;
  int num_signouts_;

 private:
  // SigninManagerBase::Observer:
  virtual void GoogleSigninFailed(
      const GoogleServiceAuthError& error) OVERRIDE {
    num_failed_signins_++;
  }

  virtual void GoogleSigninSucceeded(
      const std::string& username, const std::string& password) OVERRIDE {
    num_successful_signins_++;
  }

  virtual void GoogleSignedOut(const std::string& username) OVERRIDE {
    num_signouts_++;
  }
};

}  // namespace


class SigninManagerTest : public testing::Test {
 public:
  SigninManagerTest() : manager_(NULL) {}
  virtual ~SigninManagerTest() {}

  virtual void SetUp() OVERRIDE {
    manager_ = NULL;
    prefs_.reset(new TestingPrefServiceSimple);
    chrome::RegisterLocalState(prefs_->registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(
        prefs_.get());
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                              BuildFakeProfileOAuth2TokenService);
    builder.AddTestingFactory(ChromeSigninClientFactory::GetInstance(),
                              signin::BuildTestSigninClient);
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              SigninManagerBuild);
    profile_ = builder.Build();

    static_cast<TestSigninClient*>(
        ChromeSigninClientFactory::GetInstance()->GetForProfile(profile()))->
            SetURLRequestContext(profile_->GetRequestContext());
  }

  virtual void TearDown() OVERRIDE {
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
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile())));

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
    EXPECT_FALSE(manager_->GetAuthenticatedUsername().empty());

    ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile());
    EXPECT_TRUE(token_service->RefreshTokenIsAvailable(
        manager_->GetAuthenticatedUsername()));

    // Should go into token service and stop.
    EXPECT_EQ(1, test_observer_.num_successful_signins_);
    EXPECT_EQ(0, test_observer_.num_failed_signins_);
  }

  void CompleteSigninCallback(const std::string& oauth_token) {
    oauth_tokens_fetched_.push_back(oauth_token);
    manager_->CompletePendingSignin();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  net::TestURLFetcherFactory factory_;
  scoped_ptr<SigninManager> naked_manager_;
  SigninManager* manager_;
  TestSigninManagerObserver test_observer_;
  scoped_ptr<TestingProfile> profile_;
  std::vector<std::string> oauth_tokens_fetched_;
  scoped_ptr<TestingPrefServiceSimple> prefs_;
  std::vector<std::string> cookies_;
};

TEST_F(SigninManagerTest, SignInWithRefreshToken) {
  SetUpSigninManagerAsService();
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  manager_->StartSignInWithRefreshToken(
      "rt1",
      "user@gmail.com",
      "password",
      SigninManager::OAuthTokenFetchedCallback());

  ExpectSignInWithRefreshTokenSuccess();

  // Should persist across resets.
  ShutDownManager();
  CreateNakedSigninManager();
  manager_->Initialize(NULL);
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedUsername());
}

TEST_F(SigninManagerTest, SignInWithRefreshTokenCallbackComplete) {
  SetUpSigninManagerAsService();
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  // Since the password is empty, must verify the gaia cookies first.
  SigninManager::OAuthTokenFetchedCallback callback =
      base::Bind(&SigninManagerTest::CompleteSigninCallback,
                 base::Unretained(this));
  manager_->StartSignInWithRefreshToken(
      "rt1",
      "user@gmail.com",
      "password",
      callback);

  ExpectSignInWithRefreshTokenSuccess();
  ASSERT_EQ(1U, oauth_tokens_fetched_.size());
  EXPECT_EQ(oauth_tokens_fetched_[0], "rt1");
}

TEST_F(SigninManagerTest, SignOut) {
  SetUpSigninManagerAsService();
  manager_->StartSignInWithRefreshToken(
      "rt1",
      "user@gmail.com",
      "password",
      SigninManager::OAuthTokenFetchedCallback());
  manager_->SignOut(signin_metrics::SIGNOUT_TEST);
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());
  // Should not be persisted anymore
  ShutDownManager();
  CreateNakedSigninManager();
  manager_->Initialize(NULL);
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());
}

TEST_F(SigninManagerTest, SignOutWhileProhibited) {
  SetUpSigninManagerAsService();
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());

  manager_->SetAuthenticatedUsername("user@gmail.com");
  manager_->ProhibitSignout(true);
  manager_->SignOut(signin_metrics::SIGNOUT_TEST);
  EXPECT_FALSE(manager_->GetAuthenticatedUsername().empty());
  manager_->ProhibitSignout(false);
  manager_->SignOut(signin_metrics::SIGNOUT_TEST);
  EXPECT_TRUE(manager_->GetAuthenticatedUsername().empty());
}

TEST_F(SigninManagerTest, TestIsWebBasedSigninFlowURL) {
  EXPECT_FALSE(SigninManager::IsWebBasedSigninFlowURL(
      GURL("http://www.google.com")));
  EXPECT_TRUE(SigninManager::IsWebBasedSigninFlowURL(
      GURL("https://accounts.google.com/ServiceLogin?service=chromiumsync")));
  EXPECT_FALSE(SigninManager::IsWebBasedSigninFlowURL(
      GURL("http://accounts.google.com/ServiceLogin?service=chromiumsync")));
  // http, not https, should not be treated as web based signin.
  EXPECT_FALSE(SigninManager::IsWebBasedSigninFlowURL(
      GURL("http://accounts.google.com/ServiceLogin?service=googlemail")));
  // chromiumsync is double-embedded in a continue query param.
  EXPECT_TRUE(SigninManager::IsWebBasedSigninFlowURL(
      GURL("https://accounts.google.com/CheckCookie?"
           "continue=https%3A%2F%2Fwww.google.com%2Fintl%2Fen-US%2Fchrome"
           "%2Fblank.html%3Fsource%3D3%26nonadv%3D1&service=chromiumsync")));
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
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                   "monkey@invalid.com");
  g_browser_process->local_state()->SetString(
      prefs::kGoogleServicesUsernamePattern, ".*@google.com");
  CreateNakedSigninManager();
  manager_->Initialize(g_browser_process->local_state());
  // Currently signed in user is prohibited by policy, so should be signed out.
  EXPECT_EQ("", manager_->GetAuthenticatedUsername());
}

TEST_F(SigninManagerTest, ProhibitedAfterStartup) {
  std::string user("monkey@invalid.com");
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesUsername, user);
  CreateNakedSigninManager();
  manager_->Initialize(g_browser_process->local_state());
  EXPECT_EQ(user, manager_->GetAuthenticatedUsername());
  // Update the profile - user should be signed out.
  g_browser_process->local_state()->SetString(
      prefs::kGoogleServicesUsernamePattern, ".*@google.com");
  EXPECT_EQ("", manager_->GetAuthenticatedUsername());
}

TEST_F(SigninManagerTest, ExternalSignIn) {
  CreateNakedSigninManager();
  manager_->Initialize(g_browser_process->local_state());
  EXPECT_EQ("",
            profile()->GetPrefs()->GetString(prefs::kGoogleServicesUsername));
  EXPECT_EQ("", manager_->GetAuthenticatedUsername());
  EXPECT_EQ(0, test_observer_.num_successful_signins_);

  manager_->OnExternalSigninCompleted("external@example.com");
  EXPECT_EQ(1, test_observer_.num_successful_signins_);
  EXPECT_EQ(0, test_observer_.num_failed_signins_);
  EXPECT_EQ("external@example.com",
            profile()->GetPrefs()->GetString(prefs::kGoogleServicesUsername));
  EXPECT_EQ("external@example.com", manager_->GetAuthenticatedUsername());
}

TEST_F(SigninManagerTest, SigninNotAllowed) {
  std::string user("user@google.com");
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesUsername, user);
  profile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  SetUpSigninManagerAsService();
}
