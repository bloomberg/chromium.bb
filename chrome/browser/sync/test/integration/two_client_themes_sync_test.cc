// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/themes_helper.h"

using themes_helper::AwaitThemeIsPendingInstall;
using themes_helper::AwaitUsingSystemTheme;
using themes_helper::AwaitUsingDefaultTheme;
using themes_helper::GetCustomTheme;
using themes_helper::GetThemeID;
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

  virtual bool TestUsesSelfNotifications() OVERRIDE {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientThemesSyncTest);
};

// Starts with default themes, then sets up sync and uses it to set all
// profiles to use a custom theme.  Does not actually install any themes, but
// instead verifies the custom theme is pending for install.
IN_PROC_BROWSER_TEST_F(TwoClientThemesSyncTest, DefaultThenSyncCustom) {
  ASSERT_TRUE(SetupSync());

  ASSERT_FALSE(UsingCustomTheme(GetProfile(0)));
  ASSERT_FALSE(UsingCustomTheme(GetProfile(1)));
  ASSERT_FALSE(UsingCustomTheme(verifier()));

  UseCustomTheme(GetProfile(0), 0);
  UseCustomTheme(verifier(), 0);
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(verifier()));

  // TODO(sync): Add functions to simulate when a pending extension
  // is installed as well as when a pending extension fails to
  // install.
  ASSERT_TRUE(AwaitThemeIsPendingInstall(GetProfile(1), GetCustomTheme(0)));

  EXPECT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  EXPECT_FALSE(UsingCustomTheme(GetProfile(1)));
  EXPECT_EQ(GetCustomTheme(0), GetThemeID(verifier()));
}

// Starts with custom themes, then sets up sync and uses it to set all profiles
// to the system theme.
IN_PROC_BROWSER_TEST_F(TwoClientThemesSyncTest, CustomThenSyncNative) {
  ASSERT_TRUE(SetupClients());

  UseCustomTheme(GetProfile(0), 0);
  UseCustomTheme(GetProfile(1), 0);
  UseCustomTheme(verifier(), 0);

  ASSERT_TRUE(SetupSync());

  UseSystemTheme(GetProfile(0));
  UseSystemTheme(verifier());
  ASSERT_TRUE(UsingSystemTheme(GetProfile(0)));
  ASSERT_TRUE(UsingSystemTheme(verifier()));

  ASSERT_TRUE(AwaitUsingSystemTheme(GetProfile(1)));

  EXPECT_TRUE(UsingSystemTheme(GetProfile(0)));
  EXPECT_TRUE(UsingSystemTheme(GetProfile(1)));
  EXPECT_TRUE(UsingSystemTheme(verifier()));
}

// Starts with custom themes, then sets up sync and uses it to set all profiles
// to the default theme.
IN_PROC_BROWSER_TEST_F(TwoClientThemesSyncTest, CustomThenSyncDefault) {
  ASSERT_TRUE(SetupClients());

  UseCustomTheme(GetProfile(0), 0);
  UseCustomTheme(GetProfile(1), 0);
  UseCustomTheme(verifier(), 0);

  ASSERT_TRUE(SetupSync());

  UseDefaultTheme(GetProfile(0));
  UseDefaultTheme(verifier());
  EXPECT_TRUE(UsingDefaultTheme(GetProfile(0)));
  EXPECT_TRUE(UsingDefaultTheme(verifier()));

  ASSERT_TRUE(AwaitUsingDefaultTheme(GetProfile(1)));
  EXPECT_TRUE(UsingDefaultTheme(GetProfile(0)));
  EXPECT_TRUE(UsingDefaultTheme(GetProfile(1)));
  EXPECT_TRUE(UsingDefaultTheme(verifier()));
}

// Cycles through a set of options.
//
// Most other tests have significant coverage of model association.  This test
// is intended to test steady-state scenarios.
IN_PROC_BROWSER_TEST_F(TwoClientThemesSyncTest, CycleOptions) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  UseCustomTheme(GetProfile(0), 0);
  UseCustomTheme(verifier(), 0);

  ASSERT_TRUE(AwaitThemeIsPendingInstall(GetProfile(1), GetCustomTheme(0)));
  EXPECT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  EXPECT_EQ(GetCustomTheme(0), GetThemeID(verifier()));

  UseSystemTheme(GetProfile(0));
  UseSystemTheme(verifier());

  ASSERT_TRUE(AwaitUsingSystemTheme(GetProfile(1)));
  EXPECT_TRUE(UsingSystemTheme(GetProfile(0)));
  EXPECT_TRUE(UsingSystemTheme(GetProfile(1)));
  EXPECT_TRUE(UsingSystemTheme(verifier()));

  UseDefaultTheme(GetProfile(0));
  UseDefaultTheme(verifier());

  ASSERT_TRUE(AwaitUsingDefaultTheme(GetProfile(1)));
  EXPECT_TRUE(UsingDefaultTheme(GetProfile(0)));
  EXPECT_TRUE(UsingDefaultTheme(GetProfile(1)));
  EXPECT_TRUE(UsingDefaultTheme(verifier()));

  UseCustomTheme(GetProfile(0), 1);
  UseCustomTheme(verifier(), 1);
  ASSERT_TRUE(AwaitThemeIsPendingInstall(GetProfile(1), GetCustomTheme(1)));
  EXPECT_EQ(GetCustomTheme(1), GetThemeID(GetProfile(0)));
  EXPECT_EQ(GetCustomTheme(1), GetThemeID(verifier()));
}
