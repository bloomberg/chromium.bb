// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/sync/test/integration/extensions_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

using extensions_helper::AwaitAllProfilesHaveSameExtensions;
using extensions_helper::AllProfilesHaveSameExtensions;
using extensions_helper::DisableExtension;
using extensions_helper::EnableExtension;
using extensions_helper::GetInstalledExtensions;
using extensions_helper::HasSameExtensions;
using extensions_helper::IncognitoDisableExtension;
using extensions_helper::IncognitoEnableExtension;
using extensions_helper::InstallExtension;
using extensions_helper::UninstallExtension;

class TwoClientExtensionsSyncTest : public SyncTest {
 public:
  TwoClientExtensionsSyncTest() : SyncTest(TWO_CLIENT) {}

  ~TwoClientExtensionsSyncTest() override {}
  bool TestUsesSelfNotifications() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientExtensionsSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       E2E_ENABLED(StartWithNoExtensions)) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());
}

// Flaky on Mac: http://crbug.com/535996
#if defined(OS_MACOSX)
#define MAYBE_StartWithSameExtensions DISABLED_StartWithSameExtensions
#else
#define MAYBE_StartWithSameExtensions StartWithSameExtensions
#endif
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       E2E_ENABLED(MAYBE_StartWithSameExtensions)) {
  ASSERT_TRUE(SetupClients());

  const int kNumExtensions = 5;
  for (int i = 0; i < kNumExtensions; ++i) {
    InstallExtension(GetProfile(0), i);
    InstallExtension(GetProfile(1), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());
  EXPECT_EQ(kNumExtensions,
            static_cast<int>(GetInstalledExtensions(GetProfile(0)).size()));
}

// Disabled as an E2ETest crbug.com/532202
// Flaky on Mac: http://crbug.com/535996
#if defined(OS_MACOSX)
#define MAYBE_StartWithDifferentExtensions DISABLED_StartWithDifferentExtensions
#else
#define MAYBE_StartWithDifferentExtensions StartWithDifferentExtensions
#endif
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       MAYBE_StartWithDifferentExtensions) {
  ASSERT_TRUE(SetupClients());

  int extension_index = 0;

  const int kNumCommonExtensions = 5;
  for (int i = 0; i < kNumCommonExtensions; ++extension_index, ++i) {
    InstallExtension(GetProfile(0), extension_index);
    InstallExtension(GetProfile(1), extension_index);
  }

  const int kNumProfile0Extensions = 10;
  for (int i = 0; i < kNumProfile0Extensions; ++extension_index, ++i) {
    InstallExtension(GetProfile(0), extension_index);
  }

  const int kNumProfile1Extensions = 10;
  for (int i = 0; i < kNumProfile1Extensions; ++extension_index, ++i) {
    InstallExtension(GetProfile(1), extension_index);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());
  EXPECT_EQ(
      kNumCommonExtensions + kNumProfile0Extensions + kNumProfile1Extensions,
      static_cast<int>(GetInstalledExtensions(GetProfile(0)).size()));
}

// Disabled as an E2ETest crbug.com/532202
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       InstallDifferentExtensions) {
  ASSERT_TRUE(SetupClients());

  int extension_index = 0;

  const int kNumCommonExtensions = 5;
  for (int i = 0; i < kNumCommonExtensions; ++extension_index, ++i) {
    InstallExtension(GetProfile(0), extension_index);
    InstallExtension(GetProfile(1), extension_index);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());

  const int kNumProfile0Extensions = 10;
  for (int i = 0; i < kNumProfile0Extensions; ++extension_index, ++i) {
    InstallExtension(GetProfile(0), extension_index);
  }

  const int kNumProfile1Extensions = 10;
  for (int i = 0; i < kNumProfile1Extensions; ++extension_index, ++i) {
    InstallExtension(GetProfile(1), extension_index);
  }

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());
  EXPECT_EQ(
      kNumCommonExtensions + kNumProfile0Extensions + kNumProfile1Extensions,
      static_cast<int>(GetInstalledExtensions(GetProfile(0)).size()));
}

// TCM ID - 3637311.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest, E2E_ENABLED(Add)) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensions());

  InstallExtension(GetProfile(0), 0);

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());
  EXPECT_EQ(1u, GetInstalledExtensions(GetProfile(0)).size());
}

// TCM ID - 3724281.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest, Uninstall) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensions());

  InstallExtension(GetProfile(0), 0);
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());

  UninstallExtension(GetProfile(0), 0);
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());
  EXPECT_TRUE(GetInstalledExtensions(GetProfile(0)).empty());
}

// TCM ID - 3605300.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       UpdateEnableDisableExtension) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensions());

  InstallExtension(GetProfile(0), 0);
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());

  DisableExtension(GetProfile(0), 0);
  ASSERT_FALSE(HasSameExtensions(0, 1));

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());

  EnableExtension(GetProfile(1), 0);
  ASSERT_FALSE(HasSameExtensions(0, 1));

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());
}

// TCM ID - 3728322.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       UpdateIncognitoEnableDisable) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensions());

  InstallExtension(GetProfile(0), 0);
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());

  IncognitoEnableExtension(GetProfile(0), 0);
  ASSERT_FALSE(HasSameExtensions(0, 1));

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());

  IncognitoDisableExtension(GetProfile(1), 0);
  ASSERT_FALSE(HasSameExtensions(0, 1));

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());
}

// Regression test for bug 104399: ensure that an extension installed prior to
// setting up sync, when uninstalled, is also uninstalled from sync.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       UninstallPreinstalledExtensions) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(AllProfilesHaveSameExtensions());

  InstallExtension(GetProfile(0), 0);

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());
  ASSERT_EQ(1u, GetInstalledExtensions(GetProfile(0)).size());

  UninstallExtension(GetProfile(0), 0);

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());
  EXPECT_TRUE(GetInstalledExtensions(GetProfile(0)).empty());
}

// TODO(akalin): Add tests exercising:
//   - Offline installation/uninstallation behavior
