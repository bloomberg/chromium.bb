// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"

using sessions_helper::CheckInitialState;
using sessions_helper::GetLocalWindows;
using sessions_helper::GetSessionData;
using sessions_helper::OpenTabAndGetLocalWindows;
using sessions_helper::WindowsMatch;

class TwoClientSessionsSyncTest : public SyncTest {
 public:
  TwoClientSessionsSyncTest() : SyncTest(TWO_CLIENT) {}
  virtual ~TwoClientSessionsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientSessionsSyncTest);
};

static const char* kValidPassphrase = "passphrase!";
static const char* kURL1 = "chrome://sync";
static const char* kURL2 = "chrome://version";

// TODO(zea): Test each individual session command we care about separately.
// (as well as multi-window). We're currently only checking basic single-window/
// single-tab functionality.

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest, SingleClientChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ScopedVector<SessionWindow> client0_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(0, GURL(kURL1), client0_windows.get()));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Get foreign session data from client 1.
  SyncedSessionVector sessions1;
  ASSERT_TRUE(GetSessionData(1, &sessions1));

  // Verify client 1's foreign session matches client 0 current window.
  ASSERT_EQ(1U, sessions1.size());
  ASSERT_TRUE(WindowsMatch(sessions1[0]->windows, client0_windows.get()));
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest,
                       SingleClientEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ASSERT_TRUE(EnableEncryption(0, syncable::SESSIONS));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncrypted(0, syncable::SESSIONS));
  ASSERT_TRUE(IsEncrypted(1, syncable::SESSIONS));

  // Should enable encryption for all other types as well. Just check a subset.
  ASSERT_TRUE(IsEncrypted(1, syncable::PREFERENCES));
  ASSERT_TRUE(IsEncrypted(1, syncable::BOOKMARKS));
  ASSERT_TRUE(IsEncrypted(1, syncable::APPS));
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest,
                       SingleClientEnabledEncryptionAndChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ScopedVector<SessionWindow> client0_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(0, GURL(kURL1), client0_windows.get()));
  ASSERT_TRUE(EnableEncryption(0, syncable::SESSIONS));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Get foreign session data from client 1.
  ASSERT_TRUE(IsEncrypted(1, syncable::SESSIONS));
  SyncedSessionVector sessions1;
  ASSERT_TRUE(GetSessionData(1, &sessions1));

  // Verify client 1's foreign session matches client 0 current window.
  ASSERT_EQ(1U, sessions1.size());
  ASSERT_TRUE(WindowsMatch(sessions1[0]->windows, client0_windows.get()));
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest,
                       BothClientsEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ASSERT_TRUE(EnableEncryption(0, syncable::SESSIONS));
  ASSERT_TRUE(EnableEncryption(1, syncable::SESSIONS));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(IsEncrypted(0, syncable::SESSIONS));
  ASSERT_TRUE(IsEncrypted(1, syncable::SESSIONS));
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest, BothChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  // Open tabs on both clients and retain window information.
  ScopedVector<SessionWindow> client0_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(0, GURL(kURL2), client0_windows.get()));
  ScopedVector<SessionWindow> client1_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(1, GURL(kURL1), client1_windows.get()));

  // Wait for sync.
  ASSERT_TRUE(AwaitQuiescence());

  // Get foreign session data from client 0 and 1.
  SyncedSessionVector sessions0;
  SyncedSessionVector sessions1;
  ASSERT_TRUE(GetSessionData(0, &sessions0));
  ASSERT_TRUE(GetSessionData(1, &sessions1));

  // Verify client 1's foreign session matches client 0's current window and
  // vice versa.
  ASSERT_EQ(1U, sessions0.size());
  ASSERT_EQ(1U, sessions1.size());
  ASSERT_TRUE(WindowsMatch(sessions1[0]->windows, client0_windows.get()));
  ASSERT_TRUE(WindowsMatch(sessions0[0]->windows, client1_windows.get()));
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest,
                       FirstChangesAndSetsPassphrase) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ScopedVector<SessionWindow> client0_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(0, GURL(kURL1), client0_windows.get()));

  ASSERT_TRUE(EnableEncryption(0, syncable::SESSIONS));
  GetClient(0)->service()->SetPassphrase(kValidPassphrase, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseRequired());
  ASSERT_EQ(0, GetClient(1)->GetLastSessionSnapshot()->
      num_blocking_conflicting_updates);
  // We have 6 non-blocking conflicts due to the two meta nodes (one for each
  // client), the one tab node, and the three basic preference/themes.
  ASSERT_EQ(6, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);  // The encrypted nodes.

  GetClient(1)->service()->SetPassphrase(kValidPassphrase, true);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(1)->WaitForTypeEncryption(syncable::SESSIONS));

  ASSERT_TRUE(IsEncrypted(0, syncable::SESSIONS));
  ASSERT_TRUE(IsEncrypted(1, syncable::SESSIONS));
  // Get foreign session data from client 0 and 1.
  SyncedSessionVector sessions1;
  ASSERT_TRUE(GetSessionData(1, &sessions1));

  // Verify client 1's foreign session matches client 0's current window and
  // vice versa.
  ASSERT_EQ(1U, sessions1.size());
  ASSERT_TRUE(WindowsMatch(sessions1[0]->windows, client0_windows.get()));
}

// Flaky (number of conflicting nodes is off). http://crbug.com/89604.
IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest,
                       FLAKY_FirstChangesWhileSecondWaitingForPassphrase) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ASSERT_TRUE(EnableEncryption(0, syncable::SESSIONS));
  GetClient(0)->service()->SetPassphrase(kValidPassphrase, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseRequired());
  ASSERT_EQ(0, GetClient(1)->GetLastSessionSnapshot()->
      num_blocking_conflicting_updates);
  // We have five non-blocking conflicts due to the two meta nodes (one for each
  // client), and the 3 basic preference/themes nodes.
  ASSERT_EQ(5, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);  // The encrypted nodes.

  ScopedVector<SessionWindow> client0_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(0, GURL(kURL1), client0_windows.get()));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_EQ(0, GetClient(1)->GetLastSessionSnapshot()->
      num_blocking_conflicting_updates);
  ASSERT_EQ(6, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);  // The encrypted nodes.

  GetClient(1)->service()->SetPassphrase(kValidPassphrase, true);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(1)->WaitForTypeEncryption(syncable::SESSIONS));

  ASSERT_TRUE(IsEncrypted(0, syncable::SESSIONS));
  ASSERT_TRUE(IsEncrypted(1, syncable::SESSIONS));
  // Get foreign session data from client 0 and 1.
  SyncedSessionVector sessions1;
  ASSERT_TRUE(GetSessionData(1, &sessions1));

  // Verify client 1's foreign session matches client 0's current window and
  // vice versa.
  ASSERT_EQ(1U, sessions1.size());
  ASSERT_TRUE(WindowsMatch(sessions1[0]->windows, client0_windows.get()));
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest,
                       SecondChangesAfterEncrAndPassphraseChange) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ASSERT_TRUE(EnableEncryption(0, syncable::SESSIONS));
  GetClient(0)->service()->SetPassphrase(kValidPassphrase, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseRequired());
  ASSERT_EQ(0, GetClient(1)->GetLastSessionSnapshot()->
      num_blocking_conflicting_updates);
  // We have two non-blocking conflicts due to the two meta nodes (one for each
  // client), and the 3 basic preference/themes nodes..
  ASSERT_EQ(5, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);  // The encrypted nodes.

  // These changes are either made with the old passphrase or not encrypted at
  // all depending on when client 0's changes are propagated.
  ScopedVector<SessionWindow> client1_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(1, GURL(kURL1), client1_windows.get()));
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_EQ(0, GetClient(1)->GetLastSessionSnapshot()->
      num_blocking_conflicting_updates);
  ASSERT_EQ(5, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);  // The same encrypted nodes.

  // At this point we enter the passphrase, triggering a resync, in which the
  // local changes of client 1 get overwritten for now.
  GetClient(1)->service()->SetPassphrase(kValidPassphrase, true);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(1)->WaitForTypeEncryption(syncable::SESSIONS));

  ASSERT_TRUE(IsEncrypted(0, syncable::SESSIONS));
  ASSERT_TRUE(IsEncrypted(1, syncable::SESSIONS));
  // The session data from client 1 got overwritten. As a result, client 0
  // should have no foreign session data.
  SyncedSessionVector sessions0;
  SyncedSessionVector sessions1;
  ASSERT_FALSE(GetSessionData(0, &sessions0));
  ASSERT_FALSE(GetSessionData(1, &sessions1));
}

// Flaky. http://crbug.com/85294
IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest,
                       FLAKY_SecondChangesBeforeEncrAndPassphraseChange) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  // These changes are either made on client 1 without encryption.
  ScopedVector<SessionWindow> client1_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(1, GURL(kURL1), client1_windows.get()));
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));

  // Turn encryption on client 0. Client 1's foreign will be encrypted with the
  // new passphrase and synced back. It will be unable to decrypt it yet.
  ASSERT_TRUE(EnableEncryption(0, syncable::SESSIONS));
  GetClient(0)->service()->SetPassphrase(kValidPassphrase, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseRequired());
  ASSERT_EQ(0, GetClient(1)->GetLastSessionSnapshot()->
      num_blocking_conflicting_updates);
  // We have three non-blocking conflicts due to the two meta nodes (one for
  // each client), the one tab node, and the 3 basic preference/themes nodes.
  ASSERT_GE(6, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);  // The encrypted nodes.

  // At this point we enter the passphrase, triggering a resync.
  GetClient(1)->service()->SetPassphrase(kValidPassphrase, true);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(1)->WaitForTypeEncryption(syncable::SESSIONS));

  ASSERT_TRUE(IsEncrypted(0, syncable::SESSIONS));
  ASSERT_TRUE(IsEncrypted(1, syncable::SESSIONS));
  // Client 0's foreign data should match client 1's local data. Client 1's
  // foreign data is empty because client 0 did not open any tabs.
  SyncedSessionVector sessions0;
  SyncedSessionVector sessions1;
  ASSERT_TRUE(GetSessionData(0, &sessions0));
  ASSERT_FALSE(GetSessionData(1, &sessions1));
  ASSERT_EQ(1U, sessions0.size());
  ASSERT_TRUE(WindowsMatch(sessions0[0]->windows, client1_windows.get()));
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest,
                       BothChangeWithEncryptionAndPassphrase) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  GetClient(0)->service()->SetPassphrase(kValidPassphrase, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseRequired());

  // These changes will sync over to client 1, who will be unable to decrypt
  // them due to the missing passphrase.
  ScopedVector<SessionWindow> client0_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(0, GURL(kURL1), client0_windows.get()));
  ASSERT_TRUE(EnableEncryption(0, syncable::SESSIONS));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_EQ(0, GetClient(1)->GetLastSessionSnapshot()->
      num_blocking_conflicting_updates);
  // We have three non-blocking conflicts due to the two meta nodes (one for
  // each client), the one tab node, and the 3 basic preference/themes nodes.
  ASSERT_EQ(6, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);  // The encrypted nodes.

  GetClient(1)->service()->SetPassphrase(kValidPassphrase, true);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
  ASSERT_FALSE(GetClient(1)->service()->IsPassphraseRequired());
  ASSERT_TRUE(GetClient(1)->WaitForTypeEncryption(syncable::SESSIONS));

  // Open windows on client 1, which should automatically be encrypted.
  ScopedVector<SessionWindow> client1_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(1, GURL(kURL2), client1_windows.get()));
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));

  ASSERT_TRUE(IsEncrypted(0, syncable::SESSIONS));
  ASSERT_TRUE(IsEncrypted(1, syncable::SESSIONS));
  // Get foreign session data from client 0 and 1.
  SyncedSessionVector sessions0;
  SyncedSessionVector sessions1;
  ASSERT_TRUE(GetSessionData(0, &sessions0));
  ASSERT_TRUE(GetSessionData(1, &sessions1));

  // Verify client 1's foreign session matches client 0's current window and
  // vice versa.
  ASSERT_EQ(1U, sessions0.size());
  ASSERT_EQ(1U, sessions1.size());
  ASSERT_TRUE(WindowsMatch(sessions1[0]->windows, client0_windows.get()));
  ASSERT_TRUE(WindowsMatch(sessions0[0]->windows, client1_windows.get()));
}
