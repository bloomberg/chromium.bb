// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/ui_model_worker.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class UIModelWorkerVisitor {
 public:
  UIModelWorkerVisitor(base::WaitableEvent* was_run, bool quit_loop)
      : quit_loop_when_run_(quit_loop), was_run_(was_run) {}
  virtual ~UIModelWorkerVisitor() {}

  virtual SyncerError DoWork() {
    was_run_->Signal();
    if (quit_loop_when_run_)
      base::MessageLoop::current()->QuitWhenIdle();
    return SYNCER_OK;
  }

 private:
  bool quit_loop_when_run_;
  base::WaitableEvent* was_run_;
  DISALLOW_COPY_AND_ASSIGN(UIModelWorkerVisitor);
};

// A fake syncer that only interacts with its model safe worker.
class FakeSyncer {
 public:
  explicit FakeSyncer(UIModelWorker* worker) : worker_(worker) {}
  ~FakeSyncer() {}

  void SyncShare(UIModelWorkerVisitor* visitor) {
    // We wait until the callback is executed. So it is safe to use Unretained.
    WorkCallback c =
        base::Bind(&UIModelWorkerVisitor::DoWork, base::Unretained(visitor));
    worker_->DoWorkAndWaitUntilDone(c);
  }

 private:
  scoped_refptr<UIModelWorker> worker_;
  DISALLOW_COPY_AND_ASSIGN(FakeSyncer);
};

class SyncUIModelWorkerTest : public testing::Test {
 public:
  SyncUIModelWorkerTest()
      : faux_syncer_thread_("FauxSyncerThread"),
        faux_core_thread_("FauxCoreThread") {}

  void SetUp() override {
    faux_syncer_thread_.Start();
    bmw_ = new UIModelWorker(base::ThreadTaskRunnerHandle::Get(), nullptr);
    syncer_.reset(new FakeSyncer(bmw_.get()));
  }

  FakeSyncer* syncer() { return syncer_.get(); }
  UIModelWorker* bmw() { return bmw_.get(); }
  base::Thread* core_thread() { return &faux_core_thread_; }
  base::Thread* syncer_thread() { return &faux_syncer_thread_; }

 private:
  base::MessageLoop faux_ui_loop_;
  base::Thread faux_syncer_thread_;
  base::Thread faux_core_thread_;
  scoped_refptr<UIModelWorker> bmw_;
  std::unique_ptr<FakeSyncer> syncer_;
};

TEST_F(SyncUIModelWorkerTest, ScheduledWorkRunsOnUILoop) {
  base::WaitableEvent v_was_run(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  std::unique_ptr<UIModelWorkerVisitor> v(
      new UIModelWorkerVisitor(&v_was_run, true));

  syncer_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&FakeSyncer::SyncShare, base::Unretained(syncer()), v.get()));

  // We are on the UI thread, so run our loop to process the
  // (hopefully) scheduled task from a SyncShare invocation.
  base::RunLoop().Run();
  syncer_thread()->Stop();
}

}  // namespace
}  // namespace syncer
