// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/live_sync/live_extensions_sync_test.h"

class SingleClientLiveExtensionsSyncTest : public LiveExtensionsSyncTest {
 public:
  SingleClientLiveExtensionsSyncTest()
      : LiveExtensionsSyncTest(SINGLE_CLIENT) {}

  virtual ~SingleClientLiveExtensionsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientLiveExtensionsSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientLiveExtensionsSyncTest,
                       StartWithNoExtensions) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

// TODO(rsimha): Enable after http://crbug.com/70028 is fixed.
IN_PROC_BROWSER_TEST_F(SingleClientLiveExtensionsSyncTest,
                       DISABLED_StartWithSomeExtensions) {
  ASSERT_TRUE(SetupClients());

  const int kNumExtensions = 5;
  for (int i = 0; i < kNumExtensions; ++i) {
    InstallExtension(GetProfile(0), GetExtension(i));
    InstallExtension(verifier(), GetExtension(i));
  }

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(SingleClientLiveExtensionsSyncTest,
                       InstallSomeExtensions) {
  ASSERT_TRUE(SetupSync());

  const int kNumExtensions = 5;
  for (int i = 0; i < kNumExtensions; ++i) {
    InstallExtension(GetProfile(0), GetExtension(i));
    InstallExtension(verifier(), GetExtension(i));
  }

  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Waiting for extension changes."));

  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}
