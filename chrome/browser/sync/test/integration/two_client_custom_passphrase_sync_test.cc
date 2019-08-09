// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static const int kEncryptingClientId = 0;
static const int kDecryptingClientId = 1;

using bookmarks_helper::AddURL;
using bookmarks_helper::AllModelsMatchVerifier;

// These tests consider the client as a black-box; they are not concerned with
// whether the data is committed to the server correctly encrypted. Rather, they
// test the end-to-end behavior of two clients when a custom passphrase is set,
// i.e. whether the second client can see data that was committed by the first
// client. To test proper encryption behavior, a separate single-client test is
// used.
class TwoClientCustomPassphraseSyncTest : public SyncTest {
 public:
  TwoClientCustomPassphraseSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientCustomPassphraseSyncTest() override {}

  bool WaitForBookmarksToMatchVerifier() {
    return BookmarksMatchVerifierChecker().Wait();
  }

  bool WaitForPassphraseRequiredState(int index, bool desired_state) {
    return PassphraseRequiredStateChecker(GetSyncService(index), desired_state)
        .Wait();
  }

  void AddTestBookmarksToClient(int index) {
    ASSERT_TRUE(AddURL(index, 0, "What are you syncing about?",
                       GURL("https://google.com/synced-bookmark-1")));
    ASSERT_TRUE(AddURL(index, 1, "Test bookmark",
                       GURL("https://google.com/synced-bookmark-2")));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientCustomPassphraseSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientCustomPassphraseSyncTest,
                       DecryptionFailsWhenIncorrectPassphraseProvided) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GetSyncService(kEncryptingClientId)
      ->GetUserSettings()
      ->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/true));
  EXPECT_FALSE(GetSyncService(kDecryptingClientId)
                   ->GetUserSettings()
                   ->SetDecryptionPassphrase("incorrect passphrase"));
  EXPECT_TRUE(GetSyncService(kDecryptingClientId)
                  ->GetUserSettings()
                  ->IsPassphraseRequiredForDecryption());
}

IN_PROC_BROWSER_TEST_F(TwoClientCustomPassphraseSyncTest, ClientsCanSyncData) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GetSyncService(kEncryptingClientId)
      ->GetUserSettings()
      ->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/true));
  EXPECT_TRUE(GetSyncService(kDecryptingClientId)
                  ->GetUserSettings()
                  ->SetDecryptionPassphrase("hunter2"));
  EXPECT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/false));
  AddTestBookmarksToClient(kEncryptingClientId);

  ASSERT_TRUE(
      GetClient(kEncryptingClientId)
          ->AwaitMutualSyncCycleCompletion(GetClient(kDecryptingClientId)));
  EXPECT_TRUE(WaitForBookmarksToMatchVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientCustomPassphraseSyncTest,
                       SetPassphraseAndThenSetupSync) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(kEncryptingClientId)->SetupSync());

  // Set up a sync client with custom passphrase and one bookmark.
  GetSyncService(kEncryptingClientId)
      ->GetUserSettings()
      ->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(
      PassphraseAcceptedChecker(GetSyncService(kEncryptingClientId)).Wait());
  AddTestBookmarksToClient(kEncryptingClientId);
  // Wait for the client to commit the update.
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kEncryptingClientId)).Wait());

  // Set up a new sync client.
  ASSERT_TRUE(GetClient(kDecryptingClientId)
                  ->SetupSyncNoWaitForCompletion(
                      GetRegisteredSelectableTypes(kDecryptingClientId)));
  ASSERT_TRUE(
      PassphraseRequiredChecker(GetSyncService(kDecryptingClientId)).Wait());

  // Get client |kDecryptingClientId| out of the passphrase required state.
  ASSERT_TRUE(GetSyncService(kDecryptingClientId)
                  ->GetUserSettings()
                  ->SetDecryptionPassphrase("hunter2"));
  ASSERT_TRUE(
      PassphraseAcceptedChecker(GetSyncService(kDecryptingClientId)).Wait());
  GetClient(kDecryptingClientId)->FinishSyncSetup();

  // Wait for bookmarks to converge.
  EXPECT_TRUE(WaitForBookmarksToMatchVerifier());
}

class TwoClientCustomPassphraseSyncTestWithScryptEncryptionNotEnabled
    : public TwoClientCustomPassphraseSyncTest {
 public:
  TwoClientCustomPassphraseSyncTestWithScryptEncryptionNotEnabled()
      : toggler_(/*force_disabled=*/false,
                 /*use_for_new_passphrases=*/false) {}
  ~TwoClientCustomPassphraseSyncTestWithScryptEncryptionNotEnabled() override {}

 private:
  ScopedScryptFeatureToggler toggler_;
  DISALLOW_COPY_AND_ASSIGN(
      TwoClientCustomPassphraseSyncTestWithScryptEncryptionNotEnabled);
};

IN_PROC_BROWSER_TEST_F(
    TwoClientCustomPassphraseSyncTestWithScryptEncryptionNotEnabled,
    ClientsCanSyncData) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GetSyncService(kEncryptingClientId)
      ->GetUserSettings()
      ->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/true));
  EXPECT_TRUE(GetSyncService(kDecryptingClientId)
                  ->GetUserSettings()
                  ->SetDecryptionPassphrase("hunter2"));
  EXPECT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/false));
  AddTestBookmarksToClient(kEncryptingClientId);

  ASSERT_TRUE(
      GetClient(kEncryptingClientId)
          ->AwaitMutualSyncCycleCompletion(GetClient(kDecryptingClientId)));
  EXPECT_TRUE(WaitForBookmarksToMatchVerifier());
}

class TwoClientCustomPassphraseSyncTestWithScryptEncryptionEnabled
    : public TwoClientCustomPassphraseSyncTest {
 public:
  TwoClientCustomPassphraseSyncTestWithScryptEncryptionEnabled()
      : toggler_(/*force_disabled=*/false,
                 /*use_for_new_passphrases=*/true) {}
  ~TwoClientCustomPassphraseSyncTestWithScryptEncryptionEnabled() override {}

 private:
  ScopedScryptFeatureToggler toggler_;
  DISALLOW_COPY_AND_ASSIGN(
      TwoClientCustomPassphraseSyncTestWithScryptEncryptionEnabled);
};

IN_PROC_BROWSER_TEST_F(
    TwoClientCustomPassphraseSyncTestWithScryptEncryptionEnabled,
    ClientsCanSyncData) {
  ScopedScryptFeatureToggler toggler(/*force_disabled=*/false,
                                     /*use_for_new_passphrases=*/true);
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GetSyncService(kEncryptingClientId)
      ->GetUserSettings()
      ->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/true));
  EXPECT_TRUE(GetSyncService(kDecryptingClientId)
                  ->GetUserSettings()
                  ->SetDecryptionPassphrase("hunter2"));
  EXPECT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/false));
  AddTestBookmarksToClient(kEncryptingClientId);

  ASSERT_TRUE(
      GetClient(kEncryptingClientId)
          ->AwaitMutualSyncCycleCompletion(GetClient(kDecryptingClientId)));
  EXPECT_TRUE(WaitForBookmarksToMatchVerifier());
}

#if defined(THREAD_SANITIZER)
// https://crbug.com/915219. This data race is hard to avoid as overriding
// g_feature_list after it has been used is needed for this test.
#define MAYBE_ClientsCanSyncDataWhenScryptEncryptionEnabledInOne \
  DISABLED_ClientsCanSyncDataWhenScryptEncryptionEnabledInOne
#else
#define MAYBE_ClientsCanSyncDataWhenScryptEncryptionEnabledInOne \
  ClientsCanSyncDataWhenScryptEncryptionEnabledInOne
#endif

IN_PROC_BROWSER_TEST_F(
    TwoClientCustomPassphraseSyncTest,
    MAYBE_ClientsCanSyncDataWhenScryptEncryptionEnabledInOne) {
  ScopedScryptFeatureToggler toggler(/*force_disabled=*/false,
                                     /*use_for_new_passphrases=*/false);
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  {
    ScopedScryptFeatureToggler temporary_toggler(
        /*force_disabled=*/false, /*use_for_new_passphrases=*/true);
    GetSyncService(kEncryptingClientId)
        ->GetUserSettings()
        ->SetEncryptionPassphrase("hunter2");
  }
  ASSERT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/true));
  EXPECT_TRUE(GetSyncService(kDecryptingClientId)
                  ->GetUserSettings()
                  ->SetDecryptionPassphrase("hunter2"));
  EXPECT_TRUE(WaitForPassphraseRequiredState(kDecryptingClientId,
                                             /*desired_state=*/false));
  AddTestBookmarksToClient(kEncryptingClientId);

  ASSERT_TRUE(
      GetClient(kEncryptingClientId)
          ->AwaitMutualSyncCycleCompletion(GetClient(kDecryptingClientId)));
  EXPECT_TRUE(WaitForBookmarksToMatchVerifier());
}

}  // namespace
