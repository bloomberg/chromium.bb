// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/common/pref_names.h"

using preferences_helper::BooleanPrefMatches;
using preferences_helper::ChangeBooleanPref;

class ManyClientPreferencesSyncTest : public SyncTest {
 public:
  ManyClientPreferencesSyncTest() : SyncTest(MANY_CLIENT) {}
  virtual ~ManyClientPreferencesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ManyClientPreferencesSyncTest);
};

IN_PROC_BROWSER_TEST_F(ManyClientPreferencesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BooleanPrefMatches(prefs::kHomePageIsNewTabPage));
  ChangeBooleanPref(0, prefs::kHomePageIsNewTabPage);
  ASSERT_TRUE(GetClient(0)->AwaitGroupSyncCycleCompletion(clients()));
  ASSERT_TRUE(BooleanPrefMatches(prefs::kHomePageIsNewTabPage));
}
