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

class SingleClientLivePreferencesSyncTest : public LiveSyncTest {
 public:
  SingleClientLivePreferencesSyncTest() {
    // This makes sure browser is visible and active while running test.
    InProcessBrowserTest::set_show_window(true);
    // Set the initial timeout value to 5 min.
    InProcessBrowserTest::SetInitialTimeoutInMS(300000);
  }
  ~SingleClientLivePreferencesSyncTest() {}

  void SetupSync() {
    client_.reset(new ProfileSyncServiceTestHarness(
        browser()->profile(), username_, password_));
    EXPECT_TRUE(client_->SetupSync());
    EXPECT_TRUE(client()->AwaitSyncCycleCompletion("Initial setup"));
  }

  ProfileSyncServiceTestHarness* client() { return client_.get(); }
  ProfileSyncService* service() { return client_->service(); }

  PrefService* prefs() { return browser()->profile()->GetPrefs(); }

 private:
  scoped_ptr<ProfileSyncServiceTestHarness> client_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientLivePreferencesSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientLivePreferencesSyncTest, Sanity) {
  SetupSync();

  PrefService* expected = LiveSyncTest::MakeProfile(L"verifier")->GetPrefs();
  expected->SetBoolean(prefs::kHomePageIsNewTabPage, true);

  prefs()->SetBoolean(prefs::kHomePageIsNewTabPage, true);
  ASSERT_TRUE(client()->AwaitSyncCycleCompletion("Pref updated"));

  ASSERT_EQ(expected->GetBoolean(prefs::kHomePageIsNewTabPage),
            prefs()->GetBoolean(prefs::kHomePageIsNewTabPage));
}
