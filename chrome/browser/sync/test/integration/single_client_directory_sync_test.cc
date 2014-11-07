// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

class SingleClientDirectorySyncTest : public SyncTest {
 public:
  SingleClientDirectorySyncTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientDirectorySyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientDirectorySyncTest);
};

void SignalEvent(base::WaitableEvent* e) {
  e->Signal();
}

void WaitForExistingTasksOnThread(BrowserThread::ID id) {
  base::WaitableEvent e(true, false);
  BrowserThread::PostTask(id, FROM_HERE, base::Bind(&SignalEvent, &e));
  e.Wait();
}

IN_PROC_BROWSER_TEST_F(SingleClientDirectorySyncTest,
                       StopThenDisableDeletesDirectory) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ProfileSyncService* sync_service = GetSyncService(0);
  base::FilePath directory_path = sync_service->GetDirectoryPathForTest();
  ASSERT_TRUE(base::DirectoryExists(directory_path));
  sync_service->StopAndSuppress();
  sync_service->DisableForUser();
  // Wait for the deletion to finish.
  WaitForExistingTasksOnThread(BrowserThread::FILE);
  ASSERT_FALSE(base::DirectoryExists(directory_path));
}
