// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"

namespace {

using apps_helper::AllProfilesHaveSameApps;
using apps_helper::InstallApp;
using apps_helper::InstallPlatformApp;

// Class that enables or disables USS based on test parameter. Must be the first
// base class of the test fixture.
class UssSwitchToggler : public testing::WithParamInterface<bool> {
 public:
  UssSwitchToggler() {
    if (GetParam()) {
      override_features_.InitAndEnableFeature(switches::kSyncPseudoUSSApps);
    } else {
      override_features_.InitAndDisableFeature(switches::kSyncPseudoUSSApps);
    }
  }

 private:
  base::test::ScopedFeatureList override_features_;
};

class SingleClientAppsSyncTest : public UssSwitchToggler, public SyncTest {
 public:
  SingleClientAppsSyncTest() : SyncTest(SINGLE_CLIENT) {}

  ~SingleClientAppsSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientAppsSyncTest);
};

IN_PROC_BROWSER_TEST_P(SingleClientAppsSyncTest, StartWithNoApps) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_P(SingleClientAppsSyncTest, StartWithSomeLegacyApps) {
  ASSERT_TRUE(SetupClients());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallApp(GetProfile(0), i);
    InstallApp(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_P(SingleClientAppsSyncTest, StartWithSomePlatformApps) {
  ASSERT_TRUE(SetupClients());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallPlatformApp(GetProfile(0), i);
    InstallPlatformApp(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_P(SingleClientAppsSyncTest, InstallSomeLegacyApps) {
  ASSERT_TRUE(SetupSync());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallApp(GetProfile(0), i);
    InstallApp(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_P(SingleClientAppsSyncTest, InstallSomePlatformApps) {
  ASSERT_TRUE(SetupSync());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallPlatformApp(GetProfile(0), i);
    InstallPlatformApp(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_P(SingleClientAppsSyncTest, InstallSomeApps) {
  ASSERT_TRUE(SetupSync());

  int i = 0;

  const int kNumApps = 5;
  for (int j = 0; j < kNumApps; ++i, ++j) {
    InstallApp(GetProfile(0), i);
    InstallApp(verifier(), i);
  }

  const int kNumPlatformApps = 5;
  for (int j = 0; j < kNumPlatformApps; ++i, ++j) {
    InstallPlatformApp(GetProfile(0), i);
    InstallPlatformApp(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

INSTANTIATE_TEST_CASE_P(USS,
                        SingleClientAppsSyncTest,
                        ::testing::Values(false, true));

}  // namespace
