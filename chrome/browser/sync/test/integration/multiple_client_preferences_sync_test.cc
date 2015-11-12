// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/guid.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/pref_names.h"

using preferences_helper::AwaitStringPrefMatches;
using preferences_helper::ChangeStringPref;
using preferences_helper::GetPrefs;

class MultipleClientPreferencesSyncTest : public SyncTest {
 public:
  MultipleClientPreferencesSyncTest() : SyncTest(MULTIPLE_CLIENT) {}
  ~MultipleClientPreferencesSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientPreferencesSyncTest);
};

IN_PROC_BROWSER_TEST_F(MultipleClientPreferencesSyncTest, E2E_ENABLED(Sanity)) {
  DisableVerifier();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitStringPrefMatches(prefs::kHomePage));
  const std::string new_home_page = base::StringPrintf(
      "https://example.com/%s", base::GenerateGUID().c_str());
  ChangeStringPref(0, prefs::kHomePage, new_home_page);
  ASSERT_TRUE(AwaitStringPrefMatches(prefs::kHomePage));
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_EQ(new_home_page, GetPrefs(i)->GetString(prefs::kHomePage));
  }
}
