// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/pref_names.h"

using preferences_helper::ChangeListPref;
using preferences_helper::AwaitListPrefMatches;

class MultipleClientPreferencesSyncTest : public SyncTest {
 public:
  MultipleClientPreferencesSyncTest() : SyncTest(MULTIPLE_CLIENT) {}
  virtual ~MultipleClientPreferencesSyncTest() {}

  virtual bool TestUsesSelfNotifications() OVERRIDE {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientPreferencesSyncTest);
};

IN_PROC_BROWSER_TEST_F(MultipleClientPreferencesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();

  for (int i = 0; i < num_clients(); ++i) {
    base::ListValue urls;
    urls.Append(new base::StringValue(
        base::StringPrintf("http://www.google.com/%d", i)));
    ChangeListPref(i, prefs::kURLsToRestoreOnStartup, urls);
  }

  ASSERT_TRUE(AwaitListPrefMatches(prefs::kURLsToRestoreOnStartup));
}
