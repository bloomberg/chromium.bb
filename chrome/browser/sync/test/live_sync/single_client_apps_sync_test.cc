// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/live_sync/apps_helper.h"
#include "chrome/browser/sync/test/live_sync/live_sync_test.h"

using apps_helper::AllProfilesHaveSameAppsAsVerifier;
using apps_helper::InstallApp;

class SingleClientAppsSyncTest : public LiveSyncTest {
 public:
  SingleClientAppsSyncTest() : LiveSyncTest(SINGLE_CLIENT) {}

  virtual ~SingleClientAppsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientAppsSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientAppsSyncTest, StartWithNoApps) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(SingleClientAppsSyncTest, StartWithSomeApps) {
  ASSERT_TRUE(SetupClients());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallApp(GetProfile(0), i);
    InstallApp(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(SingleClientAppsSyncTest, InstallSomeApps) {
  ASSERT_TRUE(SetupSync());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallApp(GetProfile(0), i);
    InstallApp(verifier(), i);
  }

  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Waiting for app changes."));

  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}
