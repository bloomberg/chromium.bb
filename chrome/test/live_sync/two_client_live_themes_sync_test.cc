// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/live_themes_sync_test.h"

class TwoClientLiveThemesSyncTest : public LiveThemesSyncTest {
 public:
  TwoClientLiveThemesSyncTest() : LiveThemesSyncTest(TWO_CLIENT) {}
  virtual ~TwoClientLiveThemesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientLiveThemesSyncTest);
};

// TODO(akalin): Add tests for model association (i.e., tests that
// start with SetupClients(), change the theme state, then call
// SetupSync()).

IN_PROC_BROWSER_TEST_F(TwoClientLiveThemesSyncTest, CustomTheme) {
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

IN_PROC_BROWSER_TEST_F(TwoClientLiveThemesSyncTest, NativeTheme) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  UseCustomTheme(GetProfile(0), 0);
  UseCustomTheme(GetProfile(1), 0);
  UseCustomTheme(verifier(), 0);

  ASSERT_TRUE(AwaitQuiescence());

  UseNativeTheme(GetProfile(0));
  UseNativeTheme(verifier());
  ASSERT_TRUE(UsingNativeTheme(GetProfile(0)));
  ASSERT_TRUE(UsingNativeTheme(verifier()));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_TRUE(UsingNativeTheme(GetProfile(0)));
  ASSERT_TRUE(UsingNativeTheme(GetProfile(1)));
  ASSERT_TRUE(UsingNativeTheme(verifier()));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveThemesSyncTest, DefaultTheme) {
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

IN_PROC_BROWSER_TEST_F(TwoClientLiveThemesSyncTest, NativeDefaultRace) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  UseNativeTheme(GetProfile(0));
  UseDefaultTheme(GetProfile(1));
  ASSERT_TRUE(UsingNativeTheme(GetProfile(0)));
  ASSERT_TRUE(UsingDefaultTheme(GetProfile(1)));

  ASSERT_TRUE(AwaitQuiescence());

  // TODO(akalin): Add function that compares two profiles to see if
  // they're at the same state.

  ASSERT_EQ(UsingNativeTheme(GetProfile(0)),
            UsingNativeTheme(GetProfile(1)));
  ASSERT_EQ(UsingDefaultTheme(GetProfile(0)),
            UsingDefaultTheme(GetProfile(1)));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveThemesSyncTest, CustomNativeRace) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  UseCustomTheme(GetProfile(0), 0);
  UseNativeTheme(GetProfile(1));
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  ASSERT_TRUE(UsingNativeTheme(GetProfile(1)));

  ASSERT_TRUE(AwaitQuiescence());

  // TODO(akalin): Add function to wait for pending extensions to be
  // installed.

  ASSERT_EQ(HasOrWillHaveCustomTheme(GetProfile(0), GetCustomTheme(0)),
            HasOrWillHaveCustomTheme(GetProfile(1), GetCustomTheme(0)));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveThemesSyncTest, CustomDefaultRace) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  UseCustomTheme(GetProfile(0), 0);
  UseDefaultTheme(GetProfile(1));
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  ASSERT_TRUE(UsingDefaultTheme(GetProfile(1)));

  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(HasOrWillHaveCustomTheme(GetProfile(0), GetCustomTheme(0)),
            HasOrWillHaveCustomTheme(GetProfile(1), GetCustomTheme(0)));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveThemesSyncTest, CustomCustomRace) {
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
