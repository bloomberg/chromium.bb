// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/retry_verifier.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

namespace {

using bookmarks_helper::AddFolder;
using bookmarks_helper::ModelMatchesVerifier;
using syncer::sessions::SyncSessionSnapshot;

class SyncExponentialBackoffTest : public SyncTest {
 public:
  SyncExponentialBackoffTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~SyncExponentialBackoffTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncExponentialBackoffTest);
};

// Helper class that checks if a sync client has successfully gone through
// exponential backoff after it encounters an error.
class ExponentialBackoffChecker : public StatusChangeChecker {
 public:
  explicit ExponentialBackoffChecker(const ProfileSyncServiceHarness* harness)
      : StatusChangeChecker("ExponentialBackoffChecker"),
        harness_(harness) {
    DCHECK(harness);
    const SyncSessionSnapshot& snap = harness_->GetLastSessionSnapshot();
    retry_verifier_.Initialize(snap);
  }

  virtual ~ExponentialBackoffChecker() {}

  // Checks if backoff is complete. Called repeatedly each time PSS notifies
  // observers of a state change.
  virtual bool IsExitConditionSatisfied() OVERRIDE {
    const SyncSessionSnapshot& snap = harness_->GetLastSessionSnapshot();
    retry_verifier_.VerifyRetryInterval(snap);
    return (retry_verifier_.done() && retry_verifier_.Succeeded());
  }

 private:
  // The sync client for which backoff is being verified.
  const ProfileSyncServiceHarness* harness_;

  // Keeps track of the number of attempts at exponential backoff and its
  // related bookkeeping information for verification.
  RetryVerifier retry_verifier_;

  DISALLOW_COPY_AND_ASSIGN(ExponentialBackoffChecker);
};

IN_PROC_BROWSER_TEST_F(SyncExponentialBackoffTest, OfflineToOnline) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add an item and ensure that sync is successful.
  ASSERT_TRUE(AddFolder(0, 0, L"folder1"));
  ASSERT_TRUE(GetClient(0)->AwaitCommitActivityCompletion());

  // Trigger a network error at the client side.
  DisableNetwork(GetProfile(0));

  // Add a new item to trigger another sync cycle.
  ASSERT_TRUE(AddFolder(0, 0, L"folder2"));

  // Verify that the client goes into exponential backoff while it is unable to
  // reach the sync server.
  ExponentialBackoffChecker exponential_backoff_checker(GetClient(0));
  ASSERT_TRUE(GetClient(0)->AwaitStatusChange(&exponential_backoff_checker,
                                              "Checking exponential backoff"));

  // Recover from the network error.
  EnableNetwork(GetProfile(0));

  // Verify that sync was able to recover.
  ASSERT_TRUE(GetClient(0)->AwaitCommitActivityCompletion());
  ASSERT_TRUE(ModelMatchesVerifier(0));
}

IN_PROC_BROWSER_TEST_F(SyncExponentialBackoffTest, TransientErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add an item and ensure that sync is successful.
  ASSERT_TRUE(AddFolder(0, 0, L"folder1"));
  ASSERT_TRUE(GetClient(0)->AwaitCommitActivityCompletion());

  // Trigger a transient error on the server.
  TriggerTransientError();

  // Add a new item to trigger another sync cycle.
  ASSERT_TRUE(AddFolder(0, 0, L"folder2"));

  // Verify that the client goes into exponential backoff while it is unable to
  // reach the sync server.
  ExponentialBackoffChecker exponential_backoff_checker(GetClient(0));
  ASSERT_TRUE(GetClient(0)->AwaitStatusChange(&exponential_backoff_checker,
                                              "Checking exponential backoff"));
}

}  // namespace
