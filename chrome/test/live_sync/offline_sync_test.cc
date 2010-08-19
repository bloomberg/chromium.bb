// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/test/live_sync/live_sync_test.h"
#include "chrome/test/live_sync/profile_sync_service_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"

class OfflineSyncTest : public LiveSyncTest {
 public:
  OfflineSyncTest() : LiveSyncTest(LiveSyncTest::SINGLE_CLIENT) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(OfflineSyncTest);
};

IN_PROC_BROWSER_TEST_F(OfflineSyncTest, OfflineStart) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableNetwork(GetProfile(0));

  EXPECT_FALSE(GetClient(0)->SetupSync());
  EXPECT_EQ(GoogleServiceAuthError::CONNECTION_FAILED,
            GetClient(0)->service()->GetAuthError().state());

  EnableNetwork(GetProfile(0));
  EXPECT_TRUE(GetClient(0)->RetryAuthentication());
}
