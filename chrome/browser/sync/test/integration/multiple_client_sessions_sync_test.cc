// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

using sessions_helper::AwaitCheckForeignSessionsAgainst;
using sessions_helper::CheckInitialState;
using sessions_helper::OpenTabAndGetLocalWindows;
using sessions_helper::ScopedWindowMap;
using sessions_helper::SessionWindowMap;
using sessions_helper::SyncedSessionVector;

class MultipleClientSessionsSyncTest : public SyncTest {
 public:
  MultipleClientSessionsSyncTest() : SyncTest(MULTIPLE_CLIENT) {}
  virtual ~MultipleClientSessionsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientSessionsSyncTest);
};

// Timeout on Windows, see http://crbug.com/99819
#if defined(OS_WIN)
#define MAYBE_AllChanged DISABLED_AllChanged
#define MAYBE_EncryptedAndChanged DISABLED_EncryptedAndChanged
#else
#define MAYBE_AllChanged AllChanged
#define MAYBE_EncryptedAndChanged EncryptedAndChanged
#endif

IN_PROC_BROWSER_TEST_F(MultipleClientSessionsSyncTest, MAYBE_AllChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  std::vector<ScopedWindowMap> client_windows(num_clients());

  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(CheckInitialState(i));
  }

  // Open tabs on all clients and retain window information.
  for (int i = 0; i < num_clients(); ++i) {
    SessionWindowMap windows;
    ASSERT_TRUE(OpenTabAndGetLocalWindows(
        i, GURL(base::StringPrintf("http://127.0.0.1/bubba%i", i)), &windows));
    client_windows[i].Reset(&windows);
  }

  // Get foreign session data from all clients and check it against all
  // client_windows.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(AwaitCheckForeignSessionsAgainst(i, client_windows));
  }
}

IN_PROC_BROWSER_TEST_F(MultipleClientSessionsSyncTest,
                       MAYBE_EncryptedAndChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  std::vector<ScopedWindowMap> client_windows(num_clients());

  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(CheckInitialState(i));
  }

  // Enable encryption on client 0, should propagate to all other clients.
  ASSERT_TRUE(EnableEncryption(0));

  // Wait for sync.
  // TODO(zea): Fix sync completion detection so we don't need this. For now,
  // the profile sync service harness detects completion before all encryption
  // changes are propagated.
  ASSERT_TRUE(GetClient(0)->AwaitGroupSyncCycleCompletion(clients()));

  // Open tabs on all clients and retain window information.
  for (int i = 0; i < num_clients(); ++i) {
    SessionWindowMap windows;
    ASSERT_TRUE(OpenTabAndGetLocalWindows(
        i, GURL(base::StringPrintf("http://127.0.0.1/bubba%i", i)), &windows));
    client_windows[i].Reset(&windows);
  }

  // Get foreign session data from all clients and check it against all
  // client_windows.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(AwaitCheckForeignSessionsAgainst(i, client_windows));
    ASSERT_TRUE(IsEncryptionComplete(i));
  }
}
