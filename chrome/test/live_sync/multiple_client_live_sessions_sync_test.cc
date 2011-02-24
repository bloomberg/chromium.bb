// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/test/live_sync/live_sessions_sync_test.h"

IN_PROC_BROWSER_TEST_F(MultipleClientLiveSessionsSyncTest, AllChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  std::vector<std::vector<SessionWindow*>* > client_windows;

  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(CheckInitialState(i));
  }

  // Open tabs on all clients and retain window information.
  for (int i = 0; i < num_clients(); ++i) {
    std::vector<SessionWindow*>* new_windows =
        InitializeNewWindowWithTab(i, GURL(StringPrintf("about:bubba%i", i)));
    ASSERT_TRUE(new_windows);
    client_windows.push_back(new_windows);
  }

  // Wait for sync.
  ASSERT_TRUE(AwaitQuiescence());

  // Get foreign session data from all clients and check it against all
  // client_windows.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(CheckForeignSessionsAgainst(i, client_windows));
  }
}

IN_PROC_BROWSER_TEST_F(MultipleClientLiveSessionsSyncTest,
    EncryptedAndChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  std::vector<std::vector<SessionWindow*>* > client_windows;

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
    std::vector<SessionWindow*>* new_windows =
        InitializeNewWindowWithTab(i, GURL(StringPrintf("about:bubba%i", i)));
    ASSERT_TRUE(new_windows);
    client_windows.push_back(new_windows);
  }

  // Wait for sync.
  ASSERT_TRUE(AwaitQuiescence());

  // Get foreign session data from all clients and check it against all
  // client_windows.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(IsEncrypted(i));
    ASSERT_TRUE(CheckForeignSessionsAgainst(i, client_windows));
  }
}
