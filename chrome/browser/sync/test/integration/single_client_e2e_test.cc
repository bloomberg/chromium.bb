// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test.h"

class SingleClientE2ETest : public SyncTest {
 public:
  SingleClientE2ETest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientE2ETest() override {}
  bool TestUsesSelfNotifications() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientE2ETest);
};

// http://crbug.com/431366
IN_PROC_BROWSER_TEST_F(SingleClientE2ETest, DISABLED_SanitySetup) {
  ASSERT_TRUE(SetupSync()) <<  "SetupSync() failed.";
  // TODO(shadi): Add AwaitCommitActivityCompletion() once GCM servers are added
  // to E2E setup.
  // ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
}
