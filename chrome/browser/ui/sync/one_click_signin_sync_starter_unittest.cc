// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char* kTestingUsername = "fake_username";
}  // namespace

class OneClickSigninSyncStarterTest : public ChromeRenderViewHostTestHarness {
 public:
  OneClickSigninSyncStarterTest()
      : sync_starter_(NULL),
        failed_count_(0),
        succeeded_count_(0) {}

  // ChromeRenderViewHostTestHarness:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();

    // Disable sync to simplify the creation of a OneClickSigninSyncStarter.
    CommandLine::ForCurrentProcess()->AppendSwitch(switches::kDisableSync);

    SigninManagerBase* signin_manager = static_cast<FakeSigninManager*>(
        SigninManagerFactory::GetForProfile(profile()));

    signin_manager->Initialize(NULL);
    signin_manager->SetAuthenticatedUsername(kTestingUsername);
  }

  void Callback(OneClickSigninSyncStarter::SyncSetupResult result) {
    if (result == OneClickSigninSyncStarter::SYNC_SETUP_SUCCESS)
      ++succeeded_count_;
    else
      ++failed_count_;
  }

  // ChromeRenderViewHostTestHarness:
  virtual content::BrowserContext* CreateBrowserContext() OVERRIDE {
    // Create the sign in manager required by OneClickSigninSyncStarter.
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        SigninManagerFactory::GetInstance(),
        &OneClickSigninSyncStarterTest::BuildSigninManager);
    return builder.Build().release();
  }

 protected:
  void CreateSyncStarter(OneClickSigninSyncStarter::Callback callback,
                         const GURL& continue_url) {
    sync_starter_ = new OneClickSigninSyncStarter(
        profile(),
        NULL,
        kTestingUsername,
        std::string(),
        "refresh_token",
        OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS,
        web_contents(),
        OneClickSigninSyncStarter::NO_CONFIRMATION,
        continue_url,
        callback);
  }

  // Deletes itself when SigninFailed() or SigninSuccess() is called.
  OneClickSigninSyncStarter* sync_starter_;

  // Number of times that the callback is called with SYNC_SETUP_FAILURE.
  int failed_count_;

  // Number of times that the callback is called with SYNC_SETUP_SUCCESS.
  int succeeded_count_;

 private:
  static KeyedService* BuildSigninManager(content::BrowserContext* profile) {
    return new FakeSigninManager(static_cast<Profile*>(profile));
  }

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninSyncStarterTest);
};

// Verifies that the callback is invoked when sync setup fails.
TEST_F(OneClickSigninSyncStarterTest, CallbackSigninFailed) {
  CreateSyncStarter(base::Bind(&OneClickSigninSyncStarterTest::Callback,
                               base::Unretained(this)),
                    GURL());
  sync_starter_->SigninFailed(GoogleServiceAuthError(
      GoogleServiceAuthError::REQUEST_CANCELED));
  EXPECT_EQ(1, failed_count_);
  EXPECT_EQ(0, succeeded_count_);
}

// Verifies that there is no crash when the callback is NULL.
TEST_F(OneClickSigninSyncStarterTest, CallbackNull) {
  CreateSyncStarter(OneClickSigninSyncStarter::Callback(), GURL());
  sync_starter_->SigninFailed(GoogleServiceAuthError(
      GoogleServiceAuthError::REQUEST_CANCELED));
  EXPECT_EQ(0, failed_count_);
  EXPECT_EQ(0, succeeded_count_);
}

// Verifies that the continue URL is loaded once signin completes.
TEST_F(OneClickSigninSyncStarterTest, LoadContinueUrl) {
  content::NavigationController& controller = web_contents()->GetController();
  EXPECT_FALSE(controller.GetPendingEntry());

  const GURL kTestURL = GURL("http://www.example.com");
  CreateSyncStarter(base::Bind(&OneClickSigninSyncStarterTest::Callback,
                               base::Unretained(this)),
                    kTestURL);
  sync_starter_->MergeSessionComplete(
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  EXPECT_EQ(1, succeeded_count_);
  EXPECT_EQ(kTestURL, controller.GetPendingEntry()->GetURL());
}
