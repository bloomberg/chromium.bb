// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/ui_model_worker.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

// Makes a Closure into a WorkCallback.
// Does |work| and checks that we're on the |thread_verifier| thread.
SyncerError DoWork(
    const scoped_refptr<base::SingleThreadTaskRunner>& thread_verifier,
    base::Closure work) {
  DCHECK(thread_verifier->BelongsToCurrentThread());
  work.Run();
  return SYNCER_OK;
}

// Converts |work| to a WorkCallback that will verify that it's run on the
// thread it was constructed on.
WorkCallback ClosureToWorkCallback(base::Closure work) {
  return base::Bind(&DoWork, base::ThreadTaskRunnerHandle::Get(), work);
}

class SyncUIModelWorkerTest : public testing::Test {
 public:
  SyncUIModelWorkerTest() : sync_thread_("SyncThreadForTest") {
    sync_thread_.Start();
    worker_ = new UIModelWorker(base::ThreadTaskRunnerHandle::Get(), nullptr);
  }

  void PostWorkToSyncThread(WorkCallback work) {
    sync_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&UIModelWorker::DoWorkAndWaitUntilDone),
                   worker_, work));
  }

 private:
  base::MessageLoop ui_loop_;
  base::Thread sync_thread_;
  scoped_refptr<UIModelWorker> worker_;
};

TEST_F(SyncUIModelWorkerTest, ScheduledWorkRunsOnUILoop) {
  base::RunLoop run_loop;
  PostWorkToSyncThread(ClosureToWorkCallback(run_loop.QuitClosure()));
  // This won't quit until the QuitClosure is run.
  run_loop.Run();
}

}  // namespace
}  // namespace syncer
