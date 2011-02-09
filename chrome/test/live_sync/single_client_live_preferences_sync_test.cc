// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pref_names.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/live_preferences_sync_test.h"

IN_PROC_BROWSER_TEST_F(SingleClientLivePreferencesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  bool new_value = !GetVerifierPrefs()->GetBoolean(
      prefs::kHomePageIsNewTabPage);
  GetVerifierPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage, new_value);
  GetPrefs(0)->SetBoolean(prefs::kHomePageIsNewTabPage, new_value);
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Waiting for prefs change."));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage),
      GetPrefs(0)->GetBoolean(prefs::kHomePageIsNewTabPage));
}
