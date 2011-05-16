// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/live_extensions_sync_test.h"

class TwoClientLiveExtensionsSyncTest : public LiveExtensionsSyncTest {
 public:
  TwoClientLiveExtensionsSyncTest()
      : LiveExtensionsSyncTest(TWO_CLIENT) {}

  virtual ~TwoClientLiveExtensionsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientLiveExtensionsSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientLiveExtensionsSyncTest,
                       StartWithNoExtensions) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveExtensionsSyncTest,
                       StartWithSameExtensions) {
  ASSERT_TRUE(SetupClients());

  const int kNumExtensions = 5;
  for (int i = 0; i < kNumExtensions; ++i) {
    InstallExtension(GetProfile(0), i);
    InstallExtension(GetProfile(1), i);
    InstallExtension(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveExtensionsSyncTest,
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

  ASSERT_TRUE(AwaitQuiescence());

  InstallExtensionsPendingForSync(GetProfile(0));
  InstallExtensionsPendingForSync(GetProfile(1));

  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveExtensionsSyncTest,
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

  ASSERT_TRUE(AwaitQuiescence());

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

  ASSERT_TRUE(AwaitQuiescence());

  InstallExtensionsPendingForSync(GetProfile(0));
  InstallExtensionsPendingForSync(GetProfile(1));

  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

// TCM ID - 3637311.
IN_PROC_BROWSER_TEST_F(TwoClientLiveExtensionsSyncTest, Add) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());

  InstallExtension(GetProfile(0), 0);
  InstallExtension(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());

  InstallExtensionsPendingForSync(GetProfile(0));
  InstallExtensionsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

// TCM ID - 3724281.
IN_PROC_BROWSER_TEST_F(TwoClientLiveExtensionsSyncTest, Uninstall) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());

  InstallExtension(GetProfile(0), 0);
  InstallExtension(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());

  InstallExtensionsPendingForSync(GetProfile(0));
  InstallExtensionsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());

  UninstallExtension(GetProfile(0), 0);
  UninstallExtension(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

// TCM ID - 3732278.
IN_PROC_BROWSER_TEST_F(TwoClientLiveExtensionsSyncTest, DisableExtensions) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForDatatype(syncable::EXTENSIONS));
  InstallExtension(GetProfile(0), 1);
  InstallExtension(verifier(), 1);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_FALSE(AllProfilesHaveSameExtensionsAsVerifier());

  ASSERT_TRUE(GetClient(1)->EnableSyncForDatatype(syncable::EXTENSIONS));
  ASSERT_TRUE(AwaitQuiescence());
  InstallExtensionsPendingForSync(GetProfile(0));
  InstallExtensionsPendingForSync(GetProfile(1));
  InstallExtensionsPendingForSync(verifier());
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

// TCM ID - 3606290.
IN_PROC_BROWSER_TEST_F(TwoClientLiveExtensionsSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  InstallExtension(GetProfile(0), 0);
  InstallExtension(verifier(), 0);
  ASSERT_TRUE(
      GetClient(0)->AwaitSyncCycleCompletion("Installed an extension."));
  ASSERT_TRUE(HasSameExtensionsAsVerifier(0));
  ASSERT_FALSE(HasSameExtensionsAsVerifier(1));

  ASSERT_TRUE(GetClient(1)->EnableSyncForAllDatatypes());
  ASSERT_TRUE(AwaitQuiescence());
  InstallExtensionsPendingForSync(GetProfile(0));
  InstallExtensionsPendingForSync(GetProfile(1));
  InstallExtensionsPendingForSync(verifier());
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

// TODO(akalin): Add tests exercising:
//   - Offline installation/uninstallation behavior
