// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/live_sync/profile_sync_service_test_harness.h"
#include "chrome/test/live_sync/live_sync_test.h"

class TwoClientLivePreferencesSyncTest : public LiveSyncTest {
 protected:
  TwoClientLivePreferencesSyncTest() {
    // This makes sure browser is visible and active while running test.
    InProcessBrowserTest::set_show_window(true);
    // Set the initial timeout value to 5 min.
    InProcessBrowserTest::SetInitialTimeoutInMS(300000);
  }
  ~TwoClientLivePreferencesSyncTest() {}

  void SetupSync() {
    client1_.reset(new ProfileSyncServiceTestHarness(
        browser()->profile(), username_, password_));
    profile2_.reset(MakeProfile(L"client2"));
    client2_.reset(new ProfileSyncServiceTestHarness(
        profile2_.get(), username_, password_));
    EXPECT_TRUE(client1_->SetupSync());
    EXPECT_TRUE(client1_->AwaitSyncCycleCompletion("Initial setup 1"));
    EXPECT_TRUE(client2_->SetupSync());
    EXPECT_TRUE(client2_->AwaitSyncCycleCompletion("Initial setup 2"));
  }

  void Cleanup() {
    client2_.reset();
    profile2_.reset();
  }

  ProfileSyncServiceTestHarness* client1() { return client1_.get(); }
  ProfileSyncServiceTestHarness* client2() { return client2_.get(); }

  PrefService* prefs1() { return browser()->profile()->GetPrefs(); }
  PrefService* prefs2() { return profile2_->GetPrefs(); }

 private:
  scoped_ptr<ProfileSyncServiceTestHarness> client1_;
  scoped_ptr<ProfileSyncServiceTestHarness> client2_;
  scoped_ptr<Profile> profile2_;

  DISALLOW_COPY_AND_ASSIGN(TwoClientLivePreferencesSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, Sanity) {
  SetupSync();

  EXPECT_EQ(false, prefs1()->GetBoolean(prefs::kHomePageIsNewTabPage));
  EXPECT_EQ(false, prefs2()->GetBoolean(prefs::kHomePageIsNewTabPage));

  PrefService* expected = LiveSyncTest::MakeProfile(L"verifier")->GetPrefs();
  expected->SetBoolean(prefs::kHomePageIsNewTabPage, true);

  prefs1()->SetBoolean(prefs::kHomePageIsNewTabPage, true);
  EXPECT_TRUE(client2()->AwaitMutualSyncCycleCompletion(client1()));

  EXPECT_EQ(expected->GetBoolean(prefs::kHomePageIsNewTabPage),
            prefs1()->GetBoolean(prefs::kHomePageIsNewTabPage));
  EXPECT_EQ(expected->GetBoolean(prefs::kHomePageIsNewTabPage),
            prefs2()->GetBoolean(prefs::kHomePageIsNewTabPage));

  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, Race) {
  SetupSync();

  prefs1()->SetString(prefs::kHomePage, L"http://www.google.com/1");
  prefs2()->SetString(prefs::kHomePage, L"http://www.google.com/2");

  EXPECT_TRUE(client2()->AwaitMutualSyncCycleCompletion(client1()));

  EXPECT_EQ(prefs1()->GetString(prefs::kHomePage),
            prefs2()->GetString(prefs::kHomePage));

  Cleanup();
}
