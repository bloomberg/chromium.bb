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
                       StartWithNoExtensions_E2ETest) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       StartWithSameExtensions_E2ETest) {
  ASSERT_TRUE(SetupClients());

  const int kNumExtensions = 5;
  for (int i = 0; i < kNumExtensions; ++i) {
    InstallExtension(GetProfile(0), i);
    InstallExtension(GetProfile(1), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       StartWithDifferentExtensions_E2ETest) {
  ASSERT_TRUE(SetupClients());

  int i = 0;

  const int kNumCommonExtensions = 5;
  for (int j = 0; j < kNumCommonExtensions; ++i, ++j) {
    InstallExtension(GetProfile(0), i);
    InstallExtension(GetProfile(1), i);
  }

  const int kNumProfile0Extensions = 10;
  for (int j = 0; j < kNumProfile0Extensions; ++i, ++j) {
    InstallExtension(GetProfile(0), i);
  }

  const int kNumProfile1Extensions = 10;
  for (int j = 0; j < kNumProfile1Extensions; ++i, ++j) {
    InstallExtension(GetProfile(1), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       InstallDifferentExtensions_E2ETest) {
  ASSERT_TRUE(SetupClients());

  int i = 0;

  const int kNumCommonExtensions = 5;
  for (int j = 0; j < kNumCommonExtensions; ++i, ++j) {
    InstallExtension(GetProfile(0), i);
    InstallExtension(GetProfile(1), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());

  const int kNumProfile0Extensions = 10;
  for (int j = 0; j < kNumProfile0Extensions; ++i, ++j) {
    InstallExtension(GetProfile(0), i);
  }

  const int kNumProfile1Extensions = 10;
  for (int j = 0; j < kNumProfile1Extensions; ++i, ++j) {
    InstallExtension(GetProfile(1), i);
  }

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());
}

// TCM ID - 3637311.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest, Add_E2ETest) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensions());

  InstallExtension(GetProfile(0), 0);

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());
}

// TCM ID - 3724281.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest, Uninstall) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensions());

  InstallExtension(GetProfile(0), 0);
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());

  UninstallExtension(GetProfile(0), 0);
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());
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

  UninstallExtension(GetProfile(0), 0);

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensions());
}

// TODO(akalin): Add tests exercising:
//   - Offline installation/uninstallation behavior
