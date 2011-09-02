// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/live_sync/live_sync_test.h"
#include "chrome/browser/sync/test/live_sync/sessions_helper.h"

using sessions_helper::CheckInitialState;
using sessions_helper::GetLocalWindows;
using sessions_helper::GetSessionData;
using sessions_helper::OpenTabAndGetLocalWindows;
using sessions_helper::WindowsMatch;

class SingleClientSessionsSyncTest : public LiveSyncTest {
 public:
  SingleClientSessionsSyncTest() : LiveSyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientSessionsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientSessionsSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  ScopedVector<SessionWindow> old_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(0,
                                        GURL("about:bubba"),
                                        old_windows.get()));

  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Waiting for session change."));

  // Get foreign session data from client 0.
  SyncedSessionVector sessions;
  ASSERT_FALSE(GetSessionData(0, &sessions));
  ASSERT_EQ(0U, sessions.size());

  // Verify client didn't change.
  ScopedVector<SessionWindow> new_windows;
  ASSERT_TRUE(GetLocalWindows(0, new_windows.get()));
  ASSERT_TRUE(WindowsMatch(old_windows.get(), new_windows.get()));
}
