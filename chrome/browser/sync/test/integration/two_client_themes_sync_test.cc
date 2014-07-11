// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/themes_helper.h"

using sync_integration_test_util::AwaitCommitActivityCompletion;
using themes_helper::GetCustomTheme;
using themes_helper::GetThemeID;
using themes_helper::HasOrWillHaveCustomTheme;
using themes_helper::ThemeIsPendingInstall;
using themes_helper::UseCustomTheme;
using themes_helper::UseDefaultTheme;
using themes_helper::UseSystemTheme;
using themes_helper::UsingCustomTheme;
using themes_helper::UsingDefaultTheme;
using themes_helper::UsingSystemTheme;

class TwoClientThemesSyncTest : public SyncTest {
 public:
  TwoClientThemesSyncTest() : SyncTest(TWO_CLIENT) {}
  virtual ~TwoClientThemesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientThemesSyncTest);
};

class LegacyTwoClientThemesSyncTest : public SyncTest {
 public:
  LegacyTwoClientThemesSyncTest() : SyncTest(TWO_CLIENT_LEGACY) {}
  virtual ~LegacyTwoClientThemesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(LegacyTwoClientThemesSyncTest);
};

// TODO(akalin): Add tests for model association (i.e., tests that
// start with SetupClients(), change the theme state, then call
// SetupSync()).

// TCM ID - 3667311.
IN_PROC_BROWSER_TEST_F(TwoClientThemesSyncTest, CustomTheme) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_FALSE(UsingCustomTheme(GetProfile(0)));
  ASSERT_FALSE(UsingCustomTheme(GetProfile(1)));
  ASSERT_FALSE(UsingCustomTheme(verifier()));

  UseCustomTheme(GetProfile(0), 0);
  UseCustomTheme(verifier(), 0);
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(verifier()));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  ASSERT_FALSE(UsingCustomTheme(GetProfile(1)));
  // TODO(akalin): Add functions to simulate when a pending extension
  // is installed as well as when a pending extension fails to
  // install.
  ASSERT_TRUE(ThemeIsPendingInstall(GetProfile(1), GetCustomTheme(0)));
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(verifier()));
}

// TCM ID - 3599303.
// TODO(sync): Fails on Chrome OS. See http://crbug.com/84575.
#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(TwoClientThemesSyncTest, DISABLED_NativeTheme) {
#else
IN_PROC_BROWSER_TEST_F(TwoClientThemesSyncTest, NativeTheme) {
#endif  // OS_CHROMEOS
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  UseCustomTheme(GetProfile(0), 0);
  UseCustomTheme(GetProfile(1), 0);
  UseCustomTheme(verifier(), 0);

  ASSERT_TRUE(AwaitQuiescence());

  UseSystemTheme(GetProfile(0));
  UseSystemTheme(verifier());
  ASSERT_TRUE(UsingSystemTheme(GetProfile(0)));
  ASSERT_TRUE(UsingSystemTheme(verifier()));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_TRUE(UsingSystemTheme(GetProfile(0)));
  ASSERT_TRUE(UsingSystemTheme(GetProfile(1)));
  ASSERT_TRUE(UsingSystemTheme(verifier()));
}

// TCM ID - 7247455.
IN_PROC_BROWSER_TEST_F(TwoClientThemesSyncTest, DefaultTheme) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  UseCustomTheme(GetProfile(0), 0);
  UseCustomTheme(GetProfile(1), 0);
  UseCustomTheme(verifier(), 0);

  ASSERT_TRUE(AwaitQuiescence());

  UseDefaultTheme(GetProfile(0));
  UseDefaultTheme(verifier());
  ASSERT_TRUE(UsingDefaultTheme(GetProfile(0)));
  ASSERT_TRUE(UsingDefaultTheme(verifier()));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_TRUE(UsingDefaultTheme(GetProfile(0)));
  ASSERT_TRUE(UsingDefaultTheme(GetProfile(1)));
  ASSERT_TRUE(UsingDefaultTheme(verifier()));
}

// TCM ID - 7292065.
// TODO(sync): Fails on Chrome OS. See http://crbug.com/84575.
// TODO(erg): Fails on linux_aura. See http://crbug.com/304554
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
IN_PROC_BROWSER_TEST_F(TwoClientThemesSyncTest, DISABLED_NativeDefaultRace) {
#else
IN_PROC_BROWSER_TEST_F(TwoClientThemesSyncTest, NativeDefaultRace) {
#endif  // OS_CHROMEOS
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  UseSystemTheme(GetProfile(0));
  UseDefaultTheme(GetProfile(1));
  ASSERT_TRUE(UsingSystemTheme(GetProfile(0)));
  ASSERT_TRUE(UsingDefaultTheme(GetProfile(1)));

  ASSERT_TRUE(AwaitQuiescence());

  // TODO(akalin): Add function that compares two profiles to see if
  // they're at the same state.

  ASSERT_EQ(UsingSystemTheme(GetProfile(0)),
            UsingSystemTheme(GetProfile(1)));
  ASSERT_EQ(UsingDefaultTheme(GetProfile(0)),
            UsingDefaultTheme(GetProfile(1)));
}

// TCM ID - 7294077.
// TODO(sync): Fails on Chrome OS. See http://crbug.com/84575.
#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(TwoClientThemesSyncTest, DISABLED_CustomNativeRace) {
#else
IN_PROC_BROWSER_TEST_F(TwoClientThemesSyncTest, CustomNativeRace) {
#endif  // OS_CHROMEOS
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  UseCustomTheme(GetProfile(0), 0);
  UseSystemTheme(GetProfile(1));
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  ASSERT_TRUE(UsingSystemTheme(GetProfile(1)));

  ASSERT_TRUE(AwaitQuiescence());

  // TODO(akalin): Add function to wait for pending extensions to be
  // installed.

  ASSERT_EQ(HasOrWillHaveCustomTheme(GetProfile(0), GetCustomTheme(0)),
            HasOrWillHaveCustomTheme(GetProfile(1), GetCustomTheme(0)));
}

// TCM ID - 7307225.
IN_PROC_BROWSER_TEST_F(TwoClientThemesSyncTest, CustomDefaultRace) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  UseCustomTheme(GetProfile(0), 0);
  UseDefaultTheme(GetProfile(1));
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  ASSERT_TRUE(UsingDefaultTheme(GetProfile(1)));

  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(HasOrWillHaveCustomTheme(GetProfile(0), GetCustomTheme(0)),
            HasOrWillHaveCustomTheme(GetProfile(1), GetCustomTheme(0)));
}

// TCM ID - 7264758.
IN_PROC_BROWSER_TEST_F(TwoClientThemesSyncTest, CustomCustomRace) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // TODO(akalin): Generalize this to n clients.

  UseCustomTheme(GetProfile(0), 0);
  UseCustomTheme(GetProfile(1), 1);
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  ASSERT_EQ(GetCustomTheme(1), GetThemeID(GetProfile(1)));

  ASSERT_TRUE(AwaitQuiescence());

  bool using_theme_0 =
      (GetThemeID(GetProfile(0)) == GetCustomTheme(0)) &&
      HasOrWillHaveCustomTheme(GetProfile(1), GetCustomTheme(0));
  bool using_theme_1 =
      HasOrWillHaveCustomTheme(GetProfile(0), GetCustomTheme(1)) &&
      (GetThemeID(GetProfile(1)) == GetCustomTheme(1));

  // Equivalent to using_theme_0 xor using_theme_1.
  ASSERT_NE(using_theme_0, using_theme_1);
}

// TCM ID - 3723272.
IN_PROC_BROWSER_TEST_F(LegacyTwoClientThemesSyncTest, DisableThemes) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_FALSE(UsingCustomTheme(GetProfile(0)));
  ASSERT_FALSE(UsingCustomTheme(GetProfile(1)));
  ASSERT_FALSE(UsingCustomTheme(verifier()));

  ASSERT_TRUE(GetClient(1)->DisableSyncForDatatype(syncer::THEMES));
  UseCustomTheme(GetProfile(0), 0);
  UseCustomTheme(verifier(), 0);
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));

  ASSERT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  ASSERT_FALSE(UsingCustomTheme(GetProfile(1)));
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(verifier()));

  ASSERT_TRUE(GetClient(1)->EnableSyncForDatatype(syncer::THEMES));
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  ASSERT_FALSE(UsingCustomTheme(GetProfile(1)));
  ASSERT_TRUE(ThemeIsPendingInstall(GetProfile(1), GetCustomTheme(0)));
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(verifier()));
}

// TCM ID - 3687288.
IN_PROC_BROWSER_TEST_F(TwoClientThemesSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_FALSE(UsingCustomTheme(GetProfile(0)));
  ASSERT_FALSE(UsingCustomTheme(GetProfile(1)));
  ASSERT_FALSE(UsingCustomTheme(verifier()));

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  UseCustomTheme(GetProfile(0), 0);
  UseCustomTheme(verifier(), 0);
  ASSERT_TRUE(
      AwaitCommitActivityCompletion(GetSyncService((0))));

  ASSERT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  ASSERT_FALSE(UsingCustomTheme(GetProfile(1)));
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(verifier()));

  ASSERT_TRUE(GetClient(1)->EnableSyncForAllDatatypes());
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(verifier()));
  ASSERT_FALSE(UsingCustomTheme(GetProfile(1)));
  ASSERT_TRUE(ThemeIsPendingInstall(GetProfile(1), GetCustomTheme(0)));
}
