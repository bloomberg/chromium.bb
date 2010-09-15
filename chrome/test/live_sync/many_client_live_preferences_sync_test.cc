// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_preferences_sync_test.h"

IN_PROC_BROWSER_TEST_F(ManyClientLivePreferencesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  bool new_value = !GetVerifierPrefs()->GetBoolean(
      prefs::kHomePageIsNewTabPage);
  GetVerifierPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage, new_value);
  GetPrefs(0)->SetBoolean(prefs::kHomePageIsNewTabPage, new_value);

  GetClient(0)->AwaitGroupSyncCycleCompletion(clients());

  for (int i = 0; i < num_clients(); ++i) {
    EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage),
        GetPrefs(i)->GetBoolean(prefs::kHomePageIsNewTabPage));
  }
}
