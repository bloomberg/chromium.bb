// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"

using sessions_helper::CheckInitialState;
using sessions_helper::GetLocalWindows;
using sessions_helper::GetSessionData;
using sessions_helper::OpenTabAndGetLocalWindows;
using sessions_helper::ScopedWindowMap;
using sessions_helper::SyncedSessionVector;
using sessions_helper::WindowsMatch;

class SingleClientSessionsSyncTest : public SyncTest {
 public:
  SingleClientSessionsSyncTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientSessionsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientSessionsSyncTest);
};

// Timeout on Windows, see http://crbug.com/99819
#if defined(OS_WIN)
#define MAYBE_Sanity DISABLED_Sanity
#else
#define MAYBE_Sanity Sanity
#endif

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, MAYBE_Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  ScopedWindowMap old_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(0,
                                        GURL("http://127.0.0.1/bubba"),
                                        old_windows.GetMutable()));

  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion(
      "Waiting for session change."));

  // Get foreign session data from client 0.
  SyncedSessionVector sessions;
  ASSERT_FALSE(GetSessionData(0, &sessions));
  ASSERT_EQ(0U, sessions.size());

  // Verify client didn't change.
  ScopedWindowMap new_windows;
  ASSERT_TRUE(GetLocalWindows(0, new_windows.GetMutable()));
  ASSERT_TRUE(WindowsMatch(*old_windows.Get(), *new_windows.Get()));
}
