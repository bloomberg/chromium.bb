// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_preferences_sync_test.h"

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kHomePageIsNewTabPage),
            GetPrefs(1)->GetBoolean(prefs::kHomePageIsNewTabPage));

  bool new_value = !GetVerifierPrefs()->GetBoolean(
      prefs::kHomePageIsNewTabPage);
  GetVerifierPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage, new_value);
  GetPrefs(0)->SetBoolean(prefs::kHomePageIsNewTabPage, new_value);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage),
            GetPrefs(0)->GetBoolean(prefs::kHomePageIsNewTabPage));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage),
            GetPrefs(1)->GetBoolean(prefs::kHomePageIsNewTabPage));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, Race) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  GetPrefs(0)->SetString(prefs::kHomePage, "http://www.google.com/1");
  GetPrefs(1)->SetString(prefs::kHomePage, "http://www.google.com/2");
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  EXPECT_EQ(GetPrefs(0)->GetString(prefs::kHomePage),
            GetPrefs(1)->GetString(prefs::kHomePage));
}
