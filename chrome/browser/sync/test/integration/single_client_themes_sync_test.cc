// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/themes_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

using themes_helper::GetCustomTheme;
using themes_helper::GetThemeID;
using themes_helper::UseCustomTheme;
using themes_helper::UseDefaultTheme;
using themes_helper::UseNativeTheme;
using themes_helper::UsingCustomTheme;
using themes_helper::UsingDefaultTheme;
using themes_helper::UsingNativeTheme;

class SingleClientThemesSyncTest : public SyncTest {
 public:
  SingleClientThemesSyncTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientThemesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientThemesSyncTest);
};

// TODO(akalin): Add tests for model association (i.e., tests that
// start with SetupClients(), change the theme state, then call
// SetupSync()).

IN_PROC_BROWSER_TEST_F(SingleClientThemesSyncTest, CustomTheme) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_FALSE(UsingCustomTheme(GetProfile(0)));
  ASSERT_FALSE(UsingCustomTheme(verifier()));

  UseCustomTheme(GetProfile(0), 0);
  UseCustomTheme(verifier(), 0);
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(verifier()));

  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion(
      "Waiting for custom themes change."));

  ASSERT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(verifier()));
}

// TODO(sync): Fails on Chrome OS. See http://crbug.com/84575.
#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientThemesSyncTest, DISABLED_NativeTheme) {
#else
IN_PROC_BROWSER_TEST_F(SingleClientThemesSyncTest, NativeTheme) {
#endif  // OS_CHROMEOS
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  UseCustomTheme(GetProfile(0), 0);
  UseCustomTheme(verifier(), 0);
  ASSERT_FALSE(UsingNativeTheme(GetProfile(0)));
  ASSERT_FALSE(UsingNativeTheme(verifier()));

  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion(
      "Waiting for custom themes change."));

  UseNativeTheme(GetProfile(0));
  UseNativeTheme(verifier());
  ASSERT_TRUE(UsingNativeTheme(GetProfile(0)));
  ASSERT_TRUE(UsingNativeTheme(verifier()));

  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion(
      "Waiting for native themes change."));

  ASSERT_TRUE(UsingNativeTheme(GetProfile(0)));
  ASSERT_TRUE(UsingNativeTheme(verifier()));
}

IN_PROC_BROWSER_TEST_F(SingleClientThemesSyncTest, DefaultTheme) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  UseCustomTheme(GetProfile(0), 0);
  UseCustomTheme(verifier(), 0);
  ASSERT_FALSE(UsingDefaultTheme(GetProfile(0)));
  ASSERT_FALSE(UsingDefaultTheme(verifier()));

  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion(
      "Waiting for custom themes change."));

  UseDefaultTheme(GetProfile(0));
  UseDefaultTheme(verifier());
  ASSERT_TRUE(UsingDefaultTheme(GetProfile(0)));
  ASSERT_TRUE(UsingDefaultTheme(verifier()));

  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion(
      "Waiting for native themes change."));

  ASSERT_TRUE(UsingDefaultTheme(GetProfile(0)));
  ASSERT_TRUE(UsingDefaultTheme(verifier()));
}
