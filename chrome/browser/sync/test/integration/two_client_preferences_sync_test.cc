// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/guid.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

using preferences_helper::AwaitBooleanPrefMatches;
using preferences_helper::AwaitIntegerPrefMatches;
using preferences_helper::AwaitListPrefMatches;
using preferences_helper::AwaitStringPrefMatches;
using preferences_helper::BooleanPrefMatches;
using preferences_helper::ChangeBooleanPref;
using preferences_helper::ChangeIntegerPref;
using preferences_helper::ChangeListPref;
using preferences_helper::ChangeStringPref;
using preferences_helper::GetPrefs;

class TwoClientPreferencesSyncTest : public SyncTest {
 public:
  TwoClientPreferencesSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientPreferencesSyncTest() override {}

  bool TestUsesSelfNotifications() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientPreferencesSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, E2E_ONLY(Sanity)) {
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

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, BooleanPref) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitBooleanPrefMatches(prefs::kHomePageIsNewTabPage));

  ChangeBooleanPref(0, prefs::kHomePageIsNewTabPage);
  ASSERT_TRUE(AwaitBooleanPrefMatches(prefs::kHomePageIsNewTabPage));
}

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, Bidirectional) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitStringPrefMatches(prefs::kHomePage));

  ChangeStringPref(0, prefs::kHomePage, "http://www.google.com/0");
  ASSERT_TRUE(AwaitStringPrefMatches(prefs::kHomePage));
  EXPECT_EQ("http://www.google.com/0", GetPrefs(0)->GetString(prefs::kHomePage));

  ChangeStringPref(1, prefs::kHomePage, "http://www.google.com/1");
  ASSERT_TRUE(AwaitStringPrefMatches(prefs::kHomePage));
  EXPECT_EQ("http://www.google.com/1", GetPrefs(0)->GetString(prefs::kHomePage));
}

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, UnsyncableBooleanPref) {
  ASSERT_TRUE(SetupSync());
  DisableVerifier();
  ASSERT_TRUE(AwaitStringPrefMatches(prefs::kHomePage));
  ASSERT_TRUE(AwaitBooleanPrefMatches(prefs::kCheckDefaultBrowser));

  // This pref is not syncable.
  ChangeBooleanPref(0, prefs::kCheckDefaultBrowser);

  // This pref is syncable.
  ChangeStringPref(0, prefs::kHomePage, "http://news.google.com");

  // Wait until the syncable pref is synced, then expect that the non-syncable
  // one is still out of sync.
  ASSERT_TRUE(AwaitStringPrefMatches(prefs::kHomePage));
  ASSERT_FALSE(BooleanPrefMatches(prefs::kCheckDefaultBrowser));
}

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, StringPref) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitStringPrefMatches(prefs::kHomePage));

  ChangeStringPref(0, prefs::kHomePage, "http://news.google.com");
  ASSERT_TRUE(AwaitStringPrefMatches(prefs::kHomePage));
}

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, ComplexPrefs) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitIntegerPrefMatches(prefs::kRestoreOnStartup));
  ASSERT_TRUE(AwaitListPrefMatches(prefs::kURLsToRestoreOnStartup));

  ChangeIntegerPref(0, prefs::kRestoreOnStartup, 0);
  ASSERT_TRUE(AwaitIntegerPrefMatches(prefs::kRestoreOnStartup));

  base::ListValue urls;
  urls.Append(new base::StringValue("http://www.google.com/"));
  urls.Append(new base::StringValue("http://www.flickr.com/"));
  ChangeIntegerPref(0, prefs::kRestoreOnStartup, 4);
  ChangeListPref(0, prefs::kURLsToRestoreOnStartup, urls);
  ASSERT_TRUE(AwaitIntegerPrefMatches(prefs::kRestoreOnStartup));
  ASSERT_TRUE(AwaitListPrefMatches(prefs::kURLsToRestoreOnStartup));
}

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       SingleClientEnabledEncryptionBothChanged) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitBooleanPrefMatches(prefs::kHomePageIsNewTabPage));
  ASSERT_TRUE(AwaitStringPrefMatches(prefs::kHomePage));

  ASSERT_TRUE(EnableEncryption(0));
  ChangeBooleanPref(0, prefs::kHomePageIsNewTabPage);
  ChangeStringPref(1, prefs::kHomePage, "http://www.google.com/1");
  ASSERT_TRUE(AwaitEncryptionComplete(0));
  ASSERT_TRUE(AwaitEncryptionComplete(1));
  ASSERT_TRUE(AwaitStringPrefMatches(prefs::kHomePage));
  ASSERT_TRUE(BooleanPrefMatches(prefs::kHomePageIsNewTabPage));
}

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       BothClientsEnabledEncryptionAndChangedMultipleTimes) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitBooleanPrefMatches(prefs::kHomePageIsNewTabPage));

  ChangeBooleanPref(0, prefs::kHomePageIsNewTabPage);
  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(EnableEncryption(1));
  ASSERT_TRUE(AwaitBooleanPrefMatches(prefs::kHomePageIsNewTabPage));

  ASSERT_TRUE(AwaitBooleanPrefMatches(prefs::kShowHomeButton));
  ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(AwaitBooleanPrefMatches(prefs::kShowHomeButton));
}
