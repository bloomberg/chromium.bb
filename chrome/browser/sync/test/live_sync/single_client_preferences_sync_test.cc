// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pref_names.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/live_sync/preferences_helper.h"
#include "chrome/browser/sync/test/live_sync/live_sync_test.h"

using preferences_helper::BooleanPrefMatches;
using preferences_helper::ChangeBooleanPref;

class SingleClientPreferencesSyncTest : public LiveSyncTest {
 public:
  SingleClientPreferencesSyncTest() : LiveSyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientPreferencesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientPreferencesSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientPreferencesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BooleanPrefMatches(prefs::kHomePageIsNewTabPage));
  ChangeBooleanPref(0, prefs::kHomePageIsNewTabPage);
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Waiting for prefs change."));
  ASSERT_TRUE(BooleanPrefMatches(prefs::kHomePageIsNewTabPage));
}
