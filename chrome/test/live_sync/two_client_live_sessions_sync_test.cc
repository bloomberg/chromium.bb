// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/test/live_sync/live_sessions_sync_test.h"

static const char* kValidPassphrase = "passphrase!";

// TODO(zea): Test each individual session command we care about separately.
// (as well as multi-window). We're currently only checking basic single-window/
// single-tab functionality.

IN_PROC_BROWSER_TEST_F(TwoClientLiveSessionsSyncTest, SingleClientChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  std::vector<SessionWindow*>* client0_windows =
      InitializeNewWindowWithTab(0, GURL("about:bubba"));
  ASSERT_TRUE(client0_windows);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Get foreign session data from client 1.
  std::vector<const ForeignSession*> sessions1;
  ASSERT_TRUE(GetSessionData(1, &sessions1));

  // Verify client 1's foreign session matches client 0 current window.
  ASSERT_EQ(1U, sessions1.size());
  ASSERT_TRUE(WindowsMatch(sessions1[0]->windows, *client0_windows));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveSessionsSyncTest,
                       SingleClientEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncrypted(0));
  ASSERT_TRUE(IsEncrypted(1));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveSessionsSyncTest,
                       SingleClientEnabledEncryptionAndChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  std::vector<SessionWindow*>* client0_windows =
      InitializeNewWindowWithTab(0, GURL("about:bubba"));
  ASSERT_TRUE(client0_windows);
  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Get foreign session data from client 1.
  ASSERT_TRUE(IsEncrypted(1));
  std::vector<const ForeignSession*> sessions1;
  ASSERT_TRUE(GetSessionData(1, &sessions1));

  // Verify client 1's foreign session matches client 0 current window.
  ASSERT_EQ(1U, sessions1.size());
  ASSERT_TRUE(WindowsMatch(sessions1[0]->windows, *client0_windows));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveSessionsSyncTest,
                       BothClientsEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(EnableEncryption(1));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(IsEncrypted(0));
  ASSERT_TRUE(IsEncrypted(1));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveSessionsSyncTest, BothChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  // Open tabs on both clients and retain window information.
  std::vector<SessionWindow*>* client0_windows =
      InitializeNewWindowWithTab(0, GURL("about:bubba0"));
  ASSERT_TRUE(client0_windows);
  std::vector<SessionWindow*>* client1_windows =
      InitializeNewWindowWithTab(1, GURL("about:bubba1"));
  ASSERT_TRUE(client1_windows);

  // Wait for sync.
  ASSERT_TRUE(AwaitQuiescence());

  // Get foreign session data from client 0 and 1.
  std::vector<const ForeignSession*> sessions0;
  std::vector<const ForeignSession*> sessions1;
  ASSERT_TRUE(GetSessionData(0, &sessions0));
  ASSERT_TRUE(GetSessionData(1, &sessions1));

  // Verify client 1's foreign session matches client 0's current window and
  // vice versa.
  ASSERT_EQ(1U, sessions0.size());
  ASSERT_EQ(1U, sessions1.size());
  ASSERT_TRUE(WindowsMatch(sessions1[0]->windows, *client0_windows));
  ASSERT_TRUE(WindowsMatch(sessions0[0]->windows, *client1_windows));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveSessionsSyncTest,
                       FirstChangesAndSetsPassphrase) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  std::vector<SessionWindow*>* client0_windows =
      InitializeNewWindowWithTab(0, GURL("about:bubba0"));
  ASSERT_TRUE(client0_windows);
  ASSERT_TRUE(EnableEncryption(0));
  GetClient(0)->service()->SetPassphrase(kValidPassphrase, true, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseRequired());
  ASSERT_EQ(0, GetClient(1)->GetLastSessionSnapshot()->
      num_blocking_conflicting_updates);
  // We have 3 non-blocking conflicts due to the two meta nodes (one for each
  // client, and the one tab node).
  ASSERT_EQ(3, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);  // The encrypted nodes.

  GetClient(1)->service()->SetPassphrase(kValidPassphrase, true, true);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(1)->WaitForTypeEncryption(syncable::SESSIONS));

  ASSERT_TRUE(IsEncrypted(0));
  ASSERT_TRUE(IsEncrypted(1));
  // Get foreign session data from client 0 and 1.
  std::vector<const ForeignSession*> sessions1;
  ASSERT_TRUE(GetSessionData(1, &sessions1));

  // Verify client 1's foreign session matches client 0's current window and
  // vice versa.
  ASSERT_EQ(1U, sessions1.size());
  ASSERT_TRUE(WindowsMatch(sessions1[0]->windows, *client0_windows));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveSessionsSyncTest,
                       FirstChangesWhileSecondWaitingForPassphrase) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ASSERT_TRUE(EnableEncryption(0));
  GetClient(0)->service()->SetPassphrase(kValidPassphrase, true, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseRequired());
  ASSERT_EQ(0, GetClient(1)->GetLastSessionSnapshot()->
      num_blocking_conflicting_updates);
  // We have two non-blocking conflicts due to the two meta nodes (one for each
  // client).
  ASSERT_EQ(2, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);  // The encrypted nodes.

  std::vector<SessionWindow*>* client0_windows =
      InitializeNewWindowWithTab(0, GURL("about:bubba0"));
  ASSERT_TRUE(client0_windows);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_EQ(0, GetClient(1)->GetLastSessionSnapshot()->
      num_blocking_conflicting_updates);
  ASSERT_EQ(3, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);  // The encrypted nodes.

  GetClient(1)->service()->SetPassphrase(kValidPassphrase, true, true);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(1)->WaitForTypeEncryption(syncable::SESSIONS));

  ASSERT_TRUE(IsEncrypted(0));
  ASSERT_TRUE(IsEncrypted(1));
  // Get foreign session data from client 0 and 1.
  std::vector<const ForeignSession*> sessions1;
  ASSERT_TRUE(GetSessionData(1, &sessions1));

  // Verify client 1's foreign session matches client 0's current window and
  // vice versa.
  ASSERT_EQ(1U, sessions1.size());
  ASSERT_TRUE(WindowsMatch(sessions1[0]->windows, *client0_windows));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveSessionsSyncTest,
                       SecondChangesAfterEncrAndPassphraseChange) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ASSERT_TRUE(EnableEncryption(0));
  GetClient(0)->service()->SetPassphrase(kValidPassphrase, true, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseRequired());
  ASSERT_EQ(0, GetClient(1)->GetLastSessionSnapshot()->
      num_blocking_conflicting_updates);
  // We have two non-blocking conflicts due to the two meta nodes (one for each
  // client).
  ASSERT_EQ(2, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);  // The encrypted nodes.

  // These changes are either made with the old passphrase or not encrypted at
  // all depending on when client 0's changes are propagated.
  std::vector<SessionWindow*>* client1_windows =
      InitializeNewWindowWithTab(1, GURL("about:bubba1"));
  ASSERT_TRUE(client1_windows);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_EQ(0, GetClient(1)->GetLastSessionSnapshot()->
      num_blocking_conflicting_updates);
  ASSERT_EQ(2, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);  // The same encrypted nodes.

  // At this point we enter the passphrase, triggering a resync, in which the
  // local changes of client 1 get overwritten for now.
  GetClient(1)->service()->SetPassphrase(kValidPassphrase, true, true);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(1)->WaitForTypeEncryption(syncable::SESSIONS));

  ASSERT_TRUE(IsEncrypted(0));
  ASSERT_TRUE(IsEncrypted(1));
  // The session data from client 1 got overwritten. As a result, client 0
  // should have no foreign session data.
  std::vector<const ForeignSession*> sessions0;
  std::vector<const ForeignSession*> sessions1;
  ASSERT_FALSE(GetSessionData(0, &sessions0));
  ASSERT_FALSE(GetSessionData(1, &sessions1));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveSessionsSyncTest,
                       SecondChangesBeforeEncrAndPassphraseChange) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  // These changes are either made on client 1 without encryption.
  std::vector<SessionWindow*>* client1_windows =
      InitializeNewWindowWithTab(1, GURL("about:bubba1"));
  ASSERT_TRUE(client1_windows);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));

  // Turn encryption on client 0. Client 1's foreign will be encrypted with the
  // new passphrase and synced back. It will be unable to decrypt it yet.
  ASSERT_TRUE(EnableEncryption(0));
  GetClient(0)->service()->SetPassphrase(kValidPassphrase, true, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseRequired());
  ASSERT_EQ(0, GetClient(1)->GetLastSessionSnapshot()->
      num_blocking_conflicting_updates);
  // We have three non-blocking conflicts due to the two meta nodes (one for
  // each client) and the one tab node.
  ASSERT_GE(3, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);  // The encrypted nodes.

  // At this point we enter the passphrase, triggering a resync.
  GetClient(1)->service()->SetPassphrase(kValidPassphrase, true, true);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(1)->WaitForTypeEncryption(syncable::SESSIONS));

  ASSERT_TRUE(IsEncrypted(0));
  ASSERT_TRUE(IsEncrypted(1));
  // Client 0's foreign data should match client 1's local data. Client 1's
  // foreign data is empty because client 0 did not open any tabs.
  std::vector<const ForeignSession*> sessions0;
  std::vector<const ForeignSession*> sessions1;
  ASSERT_TRUE(GetSessionData(0, &sessions0));
  ASSERT_FALSE(GetSessionData(1, &sessions1));
  ASSERT_EQ(1U, sessions0.size());
  ASSERT_TRUE(WindowsMatch(sessions0[0]->windows, *client1_windows));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveSessionsSyncTest,
                       BothChangeWithEncryptionAndPassphrase) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  GetClient(0)->service()->SetPassphrase(kValidPassphrase, true, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseRequired());

  // These changes will sync over to client 1, who will be unable to decrypt
  // them due to the missing passphrase.
  std::vector<SessionWindow*>* client0_windows =
      InitializeNewWindowWithTab(0, GURL("about:bubba0"));
  ASSERT_TRUE(client0_windows);
  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_EQ(0, GetClient(1)->GetLastSessionSnapshot()->
      num_blocking_conflicting_updates);
  // We have three non-blocking conflicts due to the two meta nodes (one for
  // each client) and the one tab node.
  ASSERT_EQ(3, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);  // The encrypted nodes.

  GetClient(1)->service()->SetPassphrase(kValidPassphrase, true, true);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
  ASSERT_FALSE(GetClient(1)->service()->IsPassphraseRequired());
  ASSERT_TRUE(GetClient(1)->WaitForTypeEncryption(syncable::SESSIONS));

  // Open windows on client 1, which should automatically be encrypted.
  std::vector<SessionWindow*>* client1_windows =
      InitializeNewWindowWithTab(1, GURL("about:bubba1"));
  ASSERT_TRUE(client1_windows);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));

  ASSERT_TRUE(IsEncrypted(0));
  ASSERT_TRUE(IsEncrypted(1));
  // Get foreign session data from client 0 and 1.
  std::vector<const ForeignSession*> sessions0;
  std::vector<const ForeignSession*> sessions1;
  ASSERT_TRUE(GetSessionData(0, &sessions0));
  ASSERT_TRUE(GetSessionData(1, &sessions1));

  // Verify client 1's foreign session matches client 0's current window and
  // vice versa.
  ASSERT_EQ(1U, sessions0.size());
  ASSERT_EQ(1U, sessions1.size());
  ASSERT_TRUE(WindowsMatch(sessions1[0]->windows, *client0_windows));
  ASSERT_TRUE(WindowsMatch(sessions0[0]->windows, *client1_windows));
}
