// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/extensions_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

using extensions_helper::AllProfilesHaveSameExtensionsAsVerifier;
using extensions_helper::InstallExtension;
using sync_integration_test_util::AwaitCommitActivityCompletion;

class SingleClientExtensionsSyncTest : public SyncTest {
 public:
  SingleClientExtensionsSyncTest() : SyncTest(SINGLE_CLIENT) {}

  virtual ~SingleClientExtensionsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientExtensionsSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientExtensionsSyncTest, StartWithNoExtensions) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(SingleClientExtensionsSyncTest,
                       StartWithSomeExtensions) {
  ASSERT_TRUE(SetupClients());

  const int kNumExtensions = 5;
  for (int i = 0; i < kNumExtensions; ++i) {
    InstallExtension(GetProfile(0), i);
    InstallExtension(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(SingleClientExtensionsSyncTest, InstallSomeExtensions) {
  ASSERT_TRUE(SetupSync());

  const int kNumExtensions = 5;
  for (int i = 0; i < kNumExtensions; ++i) {
    InstallExtension(GetProfile(0), i);
    InstallExtension(verifier(), i);
  }

  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));

  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}
