// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "base/stringprintf.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/live_sync_test.h"
#include "chrome/test/live_sync/sessions_helper.h"

using sessions_helper::CheckForeignSessionsAgainst;
using sessions_helper::CheckInitialState;
using sessions_helper::OpenTabAndGetLocalWindows;

class MultipleClientSessionsSyncTest : public LiveSyncTest {
 public:
  MultipleClientSessionsSyncTest() : LiveSyncTest(MULTIPLE_CLIENT) {}
  virtual ~MultipleClientSessionsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientSessionsSyncTest);
};

IN_PROC_BROWSER_TEST_F(MultipleClientSessionsSyncTest, AllChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ScopedVector<SessionWindowVector> client_windows;

  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(CheckInitialState(i));
  }

  // Open tabs on all clients and retain window information.
  for (int i = 0; i < num_clients(); ++i) {
    SessionWindowVector* windows = new SessionWindowVector();
    ASSERT_TRUE(OpenTabAndGetLocalWindows(
        i, GURL(StringPrintf("about:bubba%i", i)), *windows));
    client_windows.push_back(windows);
  }

  // Wait for sync.
  ASSERT_TRUE(AwaitQuiescence());

  // Get foreign session data from all clients and check it against all
  // client_windows.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(CheckForeignSessionsAgainst(i, client_windows.get()));
  }
}

IN_PROC_BROWSER_TEST_F(MultipleClientSessionsSyncTest,
                       EncryptedAndChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ScopedVector<SessionWindowVector> client_windows;

  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(CheckInitialState(i));
  }

  // Enable encryption on client 0, should propagate to all other clients.
  ASSERT_TRUE(EnableEncryption(0, syncable::SESSIONS));

  // Wait for sync.
  // TODO(zea): Fix sync completion detection so we don't need this. For now,
  // the profile sync service harness detects completion before all encryption
  // changes are propagated.
  ASSERT_TRUE(GetClient(0)->AwaitGroupSyncCycleCompletion(clients()));

  // Open tabs on all clients and retain window information.
  for (int i = 0; i < num_clients(); ++i) {
    SessionWindowVector* windows = new SessionWindowVector();
    ASSERT_TRUE(OpenTabAndGetLocalWindows(
        i, GURL(StringPrintf("about:bubba%i", i)), *windows));
    client_windows.push_back(windows);
  }

  // Wait for sync.
  ASSERT_TRUE(AwaitQuiescence());

  // Get foreign session data from all clients and check it against all
  // client_windows.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(IsEncrypted(i, syncable::SESSIONS));
    ASSERT_TRUE(CheckForeignSessionsAgainst(i, client_windows.get()));
  }
}
