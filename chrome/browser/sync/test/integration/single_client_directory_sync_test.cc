// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "sync/syncable/directory.h"
#include "sync/test/directory_backing_store_corruption_testing.h"
#include "url/gurl.h"

using content::BrowserThread;
using sync_integration_test_util::AwaitCommitActivityCompletion;
using syncer::syncable::corruption_testing::kNumEntriesRequiredForCorruption;
using syncer::syncable::corruption_testing::CorruptDatabase;

class SingleClientDirectorySyncTest : public SyncTest {
 public:
  SingleClientDirectorySyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientDirectorySyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientDirectorySyncTest);
};

void SignalEvent(base::WaitableEvent* e) {
  e->Signal();
}

bool WaitForExistingTasksOnLoop(base::MessageLoop* loop) {
  base::WaitableEvent e(true, false);
  loop->task_runner()->PostTask(FROM_HERE, base::Bind(&SignalEvent, &e));
  // Timeout stolen from StatusChangeChecker::GetTimeoutDuration().
  return e.TimedWait(base::TimeDelta::FromSeconds(45));
}

// A status change checker that waits for an unrecoverable sync error to occur.
class SyncUnrecoverableErrorChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncUnrecoverableErrorChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied() override {
    return service()->HasUnrecoverableError();
  }

  std::string GetDebugMessage() const override {
    return "Sync Unrecoverable Error";
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientDirectorySyncTest,
                       StopThenDisableDeletesDirectory) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ProfileSyncService* sync_service = GetSyncService(0);
  base::FilePath directory_path = sync_service->GetDirectoryPathForTest();
  ASSERT_TRUE(base::DirectoryExists(directory_path));
  sync_service->RequestStop(ProfileSyncService::CLEAR_DATA);

  // Wait for StartupController::StartUp()'s tasks to finish.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  // Wait for the directory deletion to finish.
  base::MessageLoop* sync_loop = sync_service->GetSyncLoopForTest();
  ASSERT_TRUE(WaitForExistingTasksOnLoop(sync_loop));

  ASSERT_FALSE(base::DirectoryExists(directory_path));
}

// Verify that when the sync directory's backing store becomes corrupted, we
// trigger an unrecoverable error and delete the database.
//
// If this test fails, see the definition of kNumEntriesRequiredForCorruption
// for one possible cause.
IN_PROC_BROWSER_TEST_F(SingleClientDirectorySyncTest,
                       DeleteDirectoryWhenCorrupted) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  // Sync and wait for syncing to complete.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(bookmarks_helper::ModelMatchesVerifier(0));

  // Flush the directory to the backing store and wait until the flush
  // completes.
  ProfileSyncService* sync_service = GetSyncService(0);
  sync_service->FlushDirectory();
  base::MessageLoop* sync_loop = sync_service->GetSyncLoopForTest();
  ASSERT_TRUE(WaitForExistingTasksOnLoop(sync_loop));

  // Now corrupt the database.
  const base::FilePath directory_path(sync_service->GetDirectoryPathForTest());
  const base::FilePath sync_db(directory_path.Append(
      syncer::syncable::Directory::kSyncDatabaseFilename));
  ASSERT_TRUE(CorruptDatabase(sync_db));

  // Write a bunch of bookmarks and flush the directory to ensure sync notices
  // the corruption. The key here is to force sync to actually write a lot of
  // data to its DB so it will see the corruption we introduced above.
  const GURL url(
      "https://"
      "www."
      "gooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooogle.com");
  const bookmarks::BookmarkNode* top = bookmarks_helper::AddFolder(
      0, bookmarks_helper::GetOtherNode(0), 0, "top");
  for (int i = 0; i < kNumEntriesRequiredForCorruption; ++i) {
    ASSERT_TRUE(
        bookmarks_helper::AddURL(0, top, 0, base::Int64ToString(i), url));
  }
  sync_service->FlushDirectory();

  // Wait for an unrecoverable error to occur.
  SyncUnrecoverableErrorChecker checker(sync_service);
  checker.Wait();
  ASSERT_TRUE(!checker.TimedOut());
  ASSERT_TRUE(sync_service->HasUnrecoverableError());

  // Wait until the sync loop has processed any existing tasks and see that the
  // directory no longer exists.
  ASSERT_TRUE(WaitForExistingTasksOnLoop(sync_loop));
  ASSERT_FALSE(base::DirectoryExists(directory_path));
}
