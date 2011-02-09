// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_vector.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/live_sessions_sync_test.h"

IN_PROC_BROWSER_TEST_F(SingleClientLiveSessionsSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  std::vector<SessionWindow*>* old_windows =
      InitializeNewWindowWithTab(0, GURL("about:bubba"));
  ASSERT_TRUE(old_windows);

  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Waiting for session change."));

  // Get foreign session data from client 0.
  std::vector<const ForeignSession*> sessions;
  ASSERT_FALSE(GetSessionData(0, &sessions));
  ASSERT_EQ(0U, sessions.size());

  // Verify client didn't change.
  std::vector<SessionWindow*>* new_windows = GetHelper(0)->ReadWindows();
  ASSERT_TRUE(new_windows);
  ASSERT_TRUE(WindowsMatch(*old_windows, *new_windows));
}

