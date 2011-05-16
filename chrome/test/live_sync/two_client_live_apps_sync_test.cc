// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/live_apps_sync_test.h"

class TwoClientLiveAppsSyncTest : public LiveAppsSyncTest {
 public:
  TwoClientLiveAppsSyncTest()
      : LiveAppsSyncTest(TWO_CLIENT) {}

  virtual ~TwoClientLiveAppsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientLiveAppsSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientLiveAppsSyncTest,
                       StartWithNoApps) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveAppsSyncTest,
                       StartWithSameApps) {
  ASSERT_TRUE(SetupClients());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallApp(GetProfile(0), i);
    InstallApp(GetProfile(1), i);
    InstallApp(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveAppsSyncTest,
                       StartWithDifferentApps) {
  ASSERT_TRUE(SetupClients());

  int i = 0;

  const int kNumCommonApps = 5;
  for (int j = 0; j < kNumCommonApps; ++i, ++j) {
    InstallApp(GetProfile(0), i);
    InstallApp(GetProfile(1), i);
    InstallApp(verifier(), i);
  }

  const int kNumProfile0Apps = 10;
  for (int j = 0; j < kNumProfile0Apps; ++i, ++j) {
    InstallApp(GetProfile(0), i);
    InstallApp(verifier(), i);
  }

  const int kNumProfile1Apps = 10;
  for (int j = 0; j < kNumProfile1Apps; ++i, ++j) {
    InstallApp(GetProfile(1), i);
    InstallApp(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));

  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveAppsSyncTest,
                       InstallDifferentApps) {
  ASSERT_TRUE(SetupClients());

  int i = 0;

  const int kNumCommonApps = 5;
  for (int j = 0; j < kNumCommonApps; ++i, ++j) {
    InstallApp(GetProfile(0), i);
    InstallApp(GetProfile(1), i);
    InstallApp(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitQuiescence());

  const int kNumProfile0Apps = 10;
  for (int j = 0; j < kNumProfile0Apps; ++i, ++j) {
    InstallApp(GetProfile(0), i);
    InstallApp(verifier(), i);
  }

  const int kNumProfile1Apps = 10;
  for (int j = 0; j < kNumProfile1Apps; ++i, ++j) {
    InstallApp(GetProfile(1), i);
    InstallApp(verifier(), i);
  }

  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));

  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// TCM ID - 3711279.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAppsSyncTest, Add) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  InstallApp(GetProfile(0), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// TCM ID - 3706267.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAppsSyncTest, Uninstall) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  InstallApp(GetProfile(0), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  UninstallApp(GetProfile(0), 0);
  UninstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// TCM ID - 3718276.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAppsSyncTest, DisableApps) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForDatatype(syncable::APPS));
  InstallApp(GetProfile(0), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(HasSameAppsAsVerifier(0));
  ASSERT_FALSE(HasSameAppsAsVerifier(1));

  ASSERT_TRUE(GetClient(1)->EnableSyncForDatatype(syncable::APPS));
  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// TCM ID - 3720303.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAppsSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  InstallApp(GetProfile(0), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion("Installed an app."));
  ASSERT_TRUE(HasSameAppsAsVerifier(0));
  ASSERT_FALSE(HasSameAppsAsVerifier(1));

  ASSERT_TRUE(GetClient(1)->EnableSyncForAllDatatypes());
  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// TODO(akalin): Add tests exercising:
//   - Offline installation/uninstallation behavior
//   - App-specific properties
