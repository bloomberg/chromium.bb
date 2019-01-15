// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

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

    IdentityTestEnvironmentProfileAdaptor identity_test_env_profile_adaptor(
        profile());
    account_info_ = identity_test_env_profile_adaptor.identity_test_env()
                        ->MakePrimaryAccountAvailable("test@email.com");
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
    return IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment()
            .release();
  }

 protected:
  void CreateSyncStarter(OneClickSigninSyncStarter::Callback callback) {
    std::string refresh_token =
        "refresh_token_for_" +
        account_info_.account_id;  // This matches the refresh token string set
                                   // in identity::SetRefreshTokenForAccount.
    sync_starter_ = new OneClickSigninSyncStarter(
        profile(), nullptr, account_info_.gaia, account_info_.email,
        std::string(), refresh_token,
        signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
        signin_metrics::Reason::REASON_UNKNOWN_REASON,
        OneClickSigninSyncStarter::CURRENT_PROFILE, callback);
  }

  // Deletes itself when SigninFailed() or SigninSuccess() is called.
  OneClickSigninSyncStarter* sync_starter_;

  // Number of times that the callback is called with SYNC_SETUP_FAILURE.
  int failed_count_;

  // Number of times that the callback is called with SYNC_SETUP_SUCCESS.
  int succeeded_count_;

  // Holds information for the account currently logged in.
  AccountInfo account_info_;

 private:
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
