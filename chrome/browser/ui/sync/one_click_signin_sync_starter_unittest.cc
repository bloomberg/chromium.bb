// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/scoped_account_consistency.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestingGaiaId[] = "gaia_id";
const char kTestingRefreshToken[] = "refresh_token";
const char kTestingUsername[] = "fake_username";

// Test signin manager creating testing profiles.
class UnittestProfileManager : public ProfileManagerWithoutInit {
 public:
  explicit UnittestProfileManager(const base::FilePath& user_data_dir)
      : ProfileManagerWithoutInit(user_data_dir) {}

 protected:
  Profile* CreateProfileHelper(const base::FilePath& file_path) override {
    if (!base::PathExists(file_path)) {
      if (!base::CreateDirectory(file_path))
        return nullptr;
    }
    return new TestingProfile(file_path, nullptr);
  }

  Profile* CreateProfileAsyncHelper(const base::FilePath& path,
                                    Delegate* delegate) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&base::CreateDirectory), path));

    return new TestingProfile(path, this);
  }
};

}  // namespace

class OneClickSigninSyncStarterTest : public ChromeRenderViewHostTestHarness {
 public:
  OneClickSigninSyncStarterTest()
      : sync_starter_(nullptr), failed_count_(0), succeeded_count_(0) {}

  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Disable sync to simplify the creation of a OneClickSigninSyncStarter.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableSync);

    SigninManagerBase* signin_manager =
        SigninManagerFactory::GetForProfile(profile());

    signin_manager->Initialize(nullptr);
    signin_manager->SetAuthenticatedAccountInfo(kTestingGaiaId,
                                                kTestingUsername);
  }

  void Callback(OneClickSigninSyncStarter::SyncSetupResult result) {
    if (result == OneClickSigninSyncStarter::SYNC_SETUP_SUCCESS)
      ++succeeded_count_;
    else
      ++failed_count_;
  }

  // ChromeRenderViewHostTestHarness:
  content::BrowserContext* CreateBrowserContext() override {
    // Create the sign in manager required by OneClickSigninSyncStarter.
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        SigninManagerFactory::GetInstance(),
        &OneClickSigninSyncStarterTest::BuildSigninManager);
    return builder.Build().release();
  }

 protected:
  void CreateSyncStarter(OneClickSigninSyncStarter::Callback callback) {
    sync_starter_ = new OneClickSigninSyncStarter(
        profile(), nullptr, kTestingGaiaId, kTestingUsername, std::string(),
        kTestingRefreshToken, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
        signin_metrics::Reason::REASON_UNKNOWN_REASON,
        OneClickSigninSyncStarter::CURRENT_PROFILE,
        OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS,
        OneClickSigninSyncStarter::NO_CONFIRMATION, callback);
  }

  // Deletes itself when SigninFailed() or SigninSuccess() is called.
  OneClickSigninSyncStarter* sync_starter_;

  // Number of times that the callback is called with SYNC_SETUP_FAILURE.
  int failed_count_;

  // Number of times that the callback is called with SYNC_SETUP_SUCCESS.
  int succeeded_count_;

 private:
  static std::unique_ptr<KeyedService> BuildSigninManager(
      content::BrowserContext* context) {
    Profile* profile = static_cast<Profile*>(context);
    return std::make_unique<FakeSigninManager>(
        ChromeSigninClientFactory::GetForProfile(profile),
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
        AccountTrackerServiceFactory::GetForProfile(profile),
        GaiaCookieManagerServiceFactory::GetForProfile(profile));
  }

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninSyncStarterTest);
};

// Verifies that the callback is invoked when sync setup fails.
TEST_F(OneClickSigninSyncStarterTest, CallbackSigninFailed) {
  CreateSyncStarter(base::Bind(&OneClickSigninSyncStarterTest::Callback,
                               base::Unretained(this)));
  sync_starter_->SigninFailed(GoogleServiceAuthError(
      GoogleServiceAuthError::REQUEST_CANCELED));
  EXPECT_EQ(1, failed_count_);
  EXPECT_EQ(0, succeeded_count_);
}

// Verifies that there is no crash when the callback is NULL.
TEST_F(OneClickSigninSyncStarterTest, CallbackNull) {
  CreateSyncStarter(OneClickSigninSyncStarter::Callback());
  sync_starter_->SigninFailed(GoogleServiceAuthError(
      GoogleServiceAuthError::REQUEST_CANCELED));
  EXPECT_EQ(0, failed_count_);
  EXPECT_EQ(0, succeeded_count_);
}

class OneClickSigninSyncStarterTestDice
    : public ChromeRenderViewHostTestHarness {
 public:
  OneClickSigninSyncStarterTestDice()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        new UnittestProfileManager(temp_dir_.GetPath()));

    // Disable sync to simplify the creation of a OneClickSigninSyncStarter.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableSync);

    account_id_ =
        AccountTrackerServiceFactory::GetForProfile(profile())
            ->PickAccountIdForAccount(kTestingGaiaId, kTestingUsername);
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // ChromeRenderViewHostTestHarness:
  content::BrowserContext* CreateBrowserContext() override {
    // Create the sign in manager required by OneClickSigninSyncStarter.
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                              BuildFakeProfileOAuth2TokenService);
    return builder.Build().release();
  }

  std::string account_id_;

 private:
  base::ScopedTempDir temp_dir_;
  ScopedTestingLocalState local_state_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninSyncStarterTestDice);
};

// Checks that the token is revoked when signin is canceled while authentication
// is in progress.
TEST_F(OneClickSigninSyncStarterTestDice, RemoveAccountOnCancel) {
  signin::ScopedAccountConsistencyDicePrepareMigration prepare_migration;
  SigninManager* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  // Start from signed-out state.
  signin_manager->SignOut(signin_metrics::SIGNOUT_TEST,
                          signin_metrics::SignoutDelete::IGNORE_METRIC);
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile());
  token_service->UpdateCredentials(account_id_, kTestingRefreshToken);
  ASSERT_TRUE(token_service->RefreshTokenIsAvailable(account_id_));

  // Use NEW_PROFILE to pause the OneClickSynStarter in the middle of the
  // signin, while it is waiting for a new Profile to be created.
  // Deletes itself when SigninFailed() or SigninSuccess() is called.
  OneClickSigninSyncStarter* sync_starter = new OneClickSigninSyncStarter(
      profile(), nullptr, kTestingGaiaId, kTestingUsername, std::string(),
      kTestingRefreshToken, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::Reason::REASON_UNKNOWN_REASON,
      OneClickSigninSyncStarter::NEW_PROFILE,
      OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS,
      OneClickSigninSyncStarter::CONFIRM_UNTRUSTED_SIGNIN,
      OneClickSigninSyncStarter::Callback());

  // Check that the token is deleted.
  ASSERT_TRUE(signin_manager->AuthInProgress());
  sync_starter->CancelSigninAndDelete();
  EXPECT_FALSE(token_service->RefreshTokenIsAvailable(account_id_));
}
