// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/common/extensions/extension.h"
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

  ASSERT_EQ(NULL, GetCustomTheme(GetProfile(0)));
  ASSERT_EQ(NULL, GetCustomTheme(GetProfile(1)));
  ASSERT_EQ(NULL, GetCustomTheme(verifier()));

  SetTheme(GetProfile(0), GetTheme(0));
  SetTheme(verifier(), GetTheme(0));
  ASSERT_EQ(GetTheme(0), GetCustomTheme(GetProfile(0)));
  ASSERT_EQ(NULL, GetCustomTheme(GetProfile(1)));
  ASSERT_EQ(GetTheme(0), GetCustomTheme(verifier()));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetTheme(0), GetCustomTheme(GetProfile(0)));
  // TODO(akalin): Add functions to simulate when a pending extension
  // is installed as well as when a pending extension fails to
  // install.
  ASSERT_TRUE(ExtensionIsPendingInstall(GetProfile(1), GetTheme(0)));
  ASSERT_EQ(GetTheme(0), GetCustomTheme(verifier()));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveThemesSyncTest, NativeTheme) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  SetTheme(GetProfile(0), GetTheme(0));
  SetTheme(GetProfile(1), GetTheme(0));
  SetTheme(verifier(), GetTheme(0));

  ASSERT_TRUE(AwaitQuiescence());

  GetProfile(0)->SetNativeTheme();
  verifier()->SetNativeTheme();
  ASSERT_TRUE(UsingNativeTheme(GetProfile(0)));
  ASSERT_FALSE(UsingNativeTheme(GetProfile(1)));
  ASSERT_TRUE(UsingNativeTheme(verifier()));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_TRUE(UsingNativeTheme(GetProfile(0)));
  ASSERT_TRUE(UsingNativeTheme(GetProfile(1)));
  ASSERT_TRUE(UsingNativeTheme(verifier()));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveThemesSyncTest, DefaultTheme) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  SetTheme(GetProfile(0), GetTheme(0));
  SetTheme(GetProfile(1), GetTheme(0));
  SetTheme(verifier(), GetTheme(0));

  ASSERT_TRUE(AwaitQuiescence());

  GetProfile(0)->ClearTheme();
  verifier()->ClearTheme();
  ASSERT_TRUE(UsingDefaultTheme(GetProfile(0)));
  ASSERT_FALSE(UsingDefaultTheme(GetProfile(1)));
  ASSERT_TRUE(UsingDefaultTheme(verifier()));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_TRUE(UsingDefaultTheme(GetProfile(0)));
  ASSERT_TRUE(UsingDefaultTheme(GetProfile(1)));
  ASSERT_TRUE(UsingDefaultTheme(verifier()));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveThemesSyncTest, NativeDefaultRace) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  GetProfile(0)->SetNativeTheme();
  GetProfile(1)->ClearTheme();
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

  SetTheme(GetProfile(0), GetTheme(0));
  GetProfile(1)->SetNativeTheme();
  ASSERT_EQ(GetTheme(0), GetCustomTheme(GetProfile(0)));
  ASSERT_TRUE(UsingNativeTheme(GetProfile(1)));

  ASSERT_TRUE(AwaitQuiescence());

  // TODO(akalin): Add function to wait for pending extensions to be
  // installed.

  ASSERT_EQ(HasOrWillHaveCustomTheme(GetProfile(0), GetTheme(0)),
            HasOrWillHaveCustomTheme(GetProfile(1), GetTheme(0)));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveThemesSyncTest, CustomDefaultRace) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  SetTheme(GetProfile(0), GetTheme(0));
  GetProfile(1)->ClearTheme();
  ASSERT_EQ(GetTheme(0), GetCustomTheme(GetProfile(0)));
  ASSERT_TRUE(UsingDefaultTheme(GetProfile(1)));

  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(HasOrWillHaveCustomTheme(GetProfile(0), GetTheme(0)),
            HasOrWillHaveCustomTheme(GetProfile(1), GetTheme(0)));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveThemesSyncTest, CustomCustomRace) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // TODO(akalin): Generalize this to n clients.

  SetTheme(GetProfile(0), GetTheme(0));
  SetTheme(GetProfile(1), GetTheme(1));
  ASSERT_EQ(GetTheme(0), GetCustomTheme(GetProfile(0)));
  ASSERT_EQ(GetTheme(1), GetCustomTheme(GetProfile(1)));

  ASSERT_TRUE(AwaitQuiescence());

  bool using_theme_0 =
      (GetCustomTheme(GetProfile(0)) == GetTheme(0)) &&
      HasOrWillHaveCustomTheme(GetProfile(1), GetTheme(0));
  bool using_theme_1 =
      HasOrWillHaveCustomTheme(GetProfile(0), GetTheme(1)) &&
      (GetCustomTheme(GetProfile(1)) == GetTheme(1));

  // Equivalent to using_theme_0 xor using_theme_1.
  ASSERT_NE(using_theme_0, using_theme_1);
}
