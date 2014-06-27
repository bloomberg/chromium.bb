// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/sync/test/integration/extensions_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

using extensions_helper::AwaitAllProfilesHaveSameExtensionsAsVerifier;
using extensions_helper::AllProfilesHaveSameExtensionsAsVerifier;
using extensions_helper::DisableExtension;
using extensions_helper::EnableExtension;
using extensions_helper::HasSameExtensionsAsVerifier;
using extensions_helper::IncognitoDisableExtension;
using extensions_helper::IncognitoEnableExtension;
using extensions_helper::InstallExtension;
using extensions_helper::UninstallExtension;

class TwoClientExtensionsSyncTest : public SyncTest {
 public:
  TwoClientExtensionsSyncTest() : SyncTest(TWO_CLIENT) {}

  virtual ~TwoClientExtensionsSyncTest() {}
  virtual bool TestUsesSelfNotifications() OVERRIDE { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientExtensionsSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest, StartWithNoExtensions) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensionsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest, StartWithSameExtensions) {
  ASSERT_TRUE(SetupClients());

  const int kNumExtensions = 5;
  for (int i = 0; i < kNumExtensions; ++i) {
    InstallExtension(GetProfile(0), i);
    InstallExtension(GetProfile(1), i);
    InstallExtension(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensionsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       StartWithDifferentExtensions) {
  ASSERT_TRUE(SetupClients());

  int i = 0;

  const int kNumCommonExtensions = 5;
  for (int j = 0; j < kNumCommonExtensions; ++i, ++j) {
    InstallExtension(GetProfile(0), i);
    InstallExtension(GetProfile(1), i);
    InstallExtension(verifier(), i);
  }

  const int kNumProfile0Extensions = 10;
  for (int j = 0; j < kNumProfile0Extensions; ++i, ++j) {
    InstallExtension(GetProfile(0), i);
    InstallExtension(verifier(), i);
  }

  const int kNumProfile1Extensions = 10;
  for (int j = 0; j < kNumProfile1Extensions; ++i, ++j) {
    InstallExtension(GetProfile(1), i);
    InstallExtension(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensionsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       InstallDifferentExtensions) {
  ASSERT_TRUE(SetupClients());

  int i = 0;

  const int kNumCommonExtensions = 5;
  for (int j = 0; j < kNumCommonExtensions; ++i, ++j) {
    InstallExtension(GetProfile(0), i);
    InstallExtension(GetProfile(1), i);
    InstallExtension(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensionsAsVerifier());

  const int kNumProfile0Extensions = 10;
  for (int j = 0; j < kNumProfile0Extensions; ++i, ++j) {
    InstallExtension(GetProfile(0), i);
    InstallExtension(verifier(), i);
  }

  const int kNumProfile1Extensions = 10;
  for (int j = 0; j < kNumProfile1Extensions; ++i, ++j) {
    InstallExtension(GetProfile(1), i);
    InstallExtension(verifier(), i);
  }

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensionsAsVerifier());
}

// TCM ID - 3637311.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest, Add) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());

  InstallExtension(GetProfile(0), 0);
  InstallExtension(verifier(), 0);

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensionsAsVerifier());
}

// TCM ID - 3724281.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest, Uninstall) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());

  InstallExtension(GetProfile(0), 0);
  InstallExtension(verifier(), 0);
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensionsAsVerifier());

  UninstallExtension(GetProfile(0), 0);
  UninstallExtension(verifier(), 0);
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensionsAsVerifier());
}

// TCM ID - 3605300.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       UpdateEnableDisableExtension) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());

  InstallExtension(GetProfile(0), 0);
  InstallExtension(verifier(), 0);
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensionsAsVerifier());

  DisableExtension(GetProfile(0), 0);
  DisableExtension(verifier(), 0);
  ASSERT_TRUE(HasSameExtensionsAsVerifier(0));
  ASSERT_FALSE(HasSameExtensionsAsVerifier(1));

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensionsAsVerifier());

  EnableExtension(GetProfile(1), 0);
  EnableExtension(verifier(), 0);
  ASSERT_TRUE(HasSameExtensionsAsVerifier(1));
  ASSERT_FALSE(HasSameExtensionsAsVerifier(0));

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensionsAsVerifier());
}

// TCM ID - 3728322.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       UpdateIncognitoEnableDisable) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());

  InstallExtension(GetProfile(0), 0);
  InstallExtension(verifier(), 0);
  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensionsAsVerifier());

  IncognitoEnableExtension(GetProfile(0), 0);
  IncognitoEnableExtension(verifier(), 0);
  ASSERT_TRUE(HasSameExtensionsAsVerifier(0));
  ASSERT_FALSE(HasSameExtensionsAsVerifier(1));

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensionsAsVerifier());

  IncognitoDisableExtension(GetProfile(1), 0);
  IncognitoDisableExtension(verifier(), 0);
  ASSERT_TRUE(HasSameExtensionsAsVerifier(1));
  ASSERT_FALSE(HasSameExtensionsAsVerifier(0));

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensionsAsVerifier());
}

// Regression test for bug 104399: ensure that an extension installed prior to
// setting up sync, when uninstalled, is also uninstalled from sync.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionsSyncTest,
                       UninstallPreinstalledExtensions) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());

  InstallExtension(GetProfile(0), 0);
  InstallExtension(verifier(), 0);

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensionsAsVerifier());

  UninstallExtension(GetProfile(0), 0);
  UninstallExtension(verifier(), 0);

  ASSERT_TRUE(AwaitAllProfilesHaveSameExtensionsAsVerifier());
}

// TODO(akalin): Add tests exercising:
//   - Offline installation/uninstallation behavior
