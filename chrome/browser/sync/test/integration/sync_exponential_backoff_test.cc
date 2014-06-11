// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/retry_verifier.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

namespace {

using bookmarks_helper::AddFolder;
using bookmarks_helper::ModelMatchesVerifier;
using syncer::sessions::SyncSessionSnapshot;
using sync_integration_test_util::AwaitCommitActivityCompletion;

class SyncExponentialBackoffTest : public SyncTest {
 public:
  // TODO(pvalenzuela): Switch to SINGLE_CLIENT once FakeServer
  // supports this scenario.
  SyncExponentialBackoffTest() : SyncTest(SINGLE_CLIENT_LEGACY) {}
  virtual ~SyncExponentialBackoffTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncExponentialBackoffTest);
};

// Helper class that checks if a sync client has successfully gone through
// exponential backoff after it encounters an error.
class ExponentialBackoffChecker : public SingleClientStatusChangeChecker {
 public:
  explicit ExponentialBackoffChecker(ProfileSyncService* pss)
        : SingleClientStatusChangeChecker(pss) {
    const SyncSessionSnapshot& snap = service()->GetLastSessionSnapshot();
    retry_verifier_.Initialize(snap);
  }

  virtual ~ExponentialBackoffChecker() {}

  // Checks if backoff is complete. Called repeatedly each time PSS notifies
  // observers of a state change.
  virtual bool IsExitConditionSatisfied() OVERRIDE {
    const SyncSessionSnapshot& snap = service()->GetLastSessionSnapshot();
    retry_verifier_.VerifyRetryInterval(snap);
    return (retry_verifier_.done() && retry_verifier_.Succeeded());
  }

  virtual std::string GetDebugMessage() const OVERRIDE {
    return base::StringPrintf("Verifying backoff intervals (%d/%d)",
                              retry_verifier_.retry_count(),
                              RetryVerifier::kMaxRetry);
  }

 private:
  // Keeps track of the number of attempts at exponential backoff and its
  // related bookkeeping information for verification.
  RetryVerifier retry_verifier_;

  DISALLOW_COPY_AND_ASSIGN(ExponentialBackoffChecker);
};

IN_PROC_BROWSER_TEST_F(SyncExponentialBackoffTest, OfflineToOnline) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add an item and ensure that sync is successful.
  ASSERT_TRUE(AddFolder(0, 0, "folder1"));
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));

  // Trigger a network error at the client side.
  DisableNetwork(GetProfile(0));

  // Add a new item to trigger another sync cycle.
  ASSERT_TRUE(AddFolder(0, 0, "folder2"));

  // Verify that the client goes into exponential backoff while it is unable to
  // reach the sync server.
  ExponentialBackoffChecker exponential_backoff_checker(
      GetSyncService((0)));
  exponential_backoff_checker.Wait();
  ASSERT_FALSE(exponential_backoff_checker.TimedOut());

  // Recover from the network error.
  EnableNetwork(GetProfile(0));

  // Verify that sync was able to recover.
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(ModelMatchesVerifier(0));
}

IN_PROC_BROWSER_TEST_F(SyncExponentialBackoffTest, TransientErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add an item and ensure that sync is successful.
  ASSERT_TRUE(AddFolder(0, 0, "folder1"));
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));

  // Trigger a transient error on the server.
  TriggerTransientError();

  // Add a new item to trigger another sync cycle.
  ASSERT_TRUE(AddFolder(0, 0, "folder2"));

  // Verify that the client goes into exponential backoff while it is unable to
  // reach the sync server.
  ExponentialBackoffChecker exponential_backoff_checker(
      GetSyncService((0)));
  exponential_backoff_checker.Wait();
  ASSERT_FALSE(exponential_backoff_checker.TimedOut());
}

}  // namespace
