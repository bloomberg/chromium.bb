// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

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
  loop->PostTask(FROM_HERE, base::Bind(&SignalEvent, &e));
  // Timeout stolen from StatusChangeChecker::GetTimeoutDuration().
  return e.TimedWait(base::TimeDelta::FromSeconds(45));
}

IN_PROC_BROWSER_TEST_F(SingleClientDirectorySyncTest,
                       StopThenDisableDeletesDirectory) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ProfileSyncService* sync_service = GetSyncService(0);
  base::FilePath directory_path = sync_service->GetDirectoryPathForTest();
  ASSERT_TRUE(base::DirectoryExists(directory_path));
  sync_service->StopAndSuppress();
  sync_service->DisableForUser();

  // Wait for StartupController::StartUp()'s tasks to finish.
  base::RunLoop run_loop;
  base::MessageLoop::current()->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  // Wait for the directory deletion to finish.
  base::MessageLoop* sync_loop = sync_service->GetSyncLoopForTest();
  ASSERT_TRUE(WaitForExistingTasksOnLoop(sync_loop));

  ASSERT_FALSE(base::DirectoryExists(directory_path));
}
