// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/live_sync/live_preferences_sync_test.h"

// TODO(rsimha): Enable once http://crbug.com/69604 is fixed.
IN_PROC_BROWSER_TEST_F(ManyClientLivePreferencesSyncTest, DISABLED_Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  bool new_value = !GetVerifierPrefs()->GetBoolean(
      prefs::kHomePageIsNewTabPage);
  GetVerifierPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage, new_value);
  GetPrefs(0)->SetBoolean(prefs::kHomePageIsNewTabPage, new_value);

  ASSERT_TRUE(GetClient(0)->AwaitGroupSyncCycleCompletion(clients()));

  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage),
        GetPrefs(i)->GetBoolean(prefs::kHomePageIsNewTabPage));
  }
}
