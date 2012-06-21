// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync/glue/ui_model_worker.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::UIModelWorker;
using csync::SyncerError;
using content::BrowserThread;

// Various boilerplate, primarily for the StopWithPendingWork test.

class UIModelWorkerVisitor {
 public:
  UIModelWorkerVisitor(base::WaitableEvent* was_run,
                       bool quit_loop)
     : quit_loop_when_run_(quit_loop),
       was_run_(was_run) { }
  virtual ~UIModelWorkerVisitor() { }

  virtual csync::SyncerError DoWork() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    was_run_->Signal();
    if (quit_loop_when_run_)
      MessageLoop::current()->Quit();
    return csync::SYNCER_OK;
  }

 private:
  bool quit_loop_when_run_;
  base::WaitableEvent* was_run_;
  DISALLOW_COPY_AND_ASSIGN(UIModelWorkerVisitor);
};

// A faux-syncer that only interacts with its model safe worker.
class Syncer {
 public:
  explicit Syncer(UIModelWorker* worker) : worker_(worker) {}
  ~Syncer() {}

  void SyncShare(UIModelWorkerVisitor* visitor) {
    // We wait until the callback is executed. So it is safe to use Unretained.
    csync::WorkCallback c = base::Bind(&UIModelWorkerVisitor::DoWork,
                                       base::Unretained(visitor));
    worker_->DoWorkAndWaitUntilDone(c);
  }
 private:
  scoped_refptr<UIModelWorker> worker_;
  DISALLOW_COPY_AND_ASSIGN(Syncer);
};

// A callback run from the CoreThread to simulate terminating syncapi.
void FakeSyncapiShutdownCallback(base::Thread* syncer_thread,
                                 UIModelWorker* worker,
                                 base::WaitableEvent** jobs,
                                 size_t job_count) {
  base::WaitableEvent all_jobs_done(false, false);

  // In real life, we would try and close a sync directory, which would
  // result in the syncer calling it's own destructor, which results in
  // the SyncerThread::HaltSyncer being called, which sets the
  // syncer in RequestEarlyExit mode and waits until the Syncer finishes
  // SyncShare to remove the syncer from its watch. Here we just manually
  // wait until all outstanding jobs are done to simulate what happens in
  // SyncerThread::HaltSyncer.
  all_jobs_done.WaitMany(jobs, job_count);

  // These two calls are made from SyncBackendHost::Core::DoShutdown.
  syncer_thread->Stop();
  worker->OnSyncerShutdownComplete();
}

class SyncUIModelWorkerTest : public testing::Test {
 public:
  SyncUIModelWorkerTest() : faux_syncer_thread_("FauxSyncerThread"),
                            faux_core_thread_("FauxCoreThread") { }

  virtual void SetUp() {
    faux_syncer_thread_.Start();
    ui_thread_.reset(new content::TestBrowserThread(BrowserThread::UI,
                                                    &faux_ui_loop_));
    bmw_ = new UIModelWorker();
    syncer_.reset(new Syncer(bmw_.get()));
  }

  Syncer* syncer() { return syncer_.get(); }
  UIModelWorker* bmw() { return bmw_.get(); }
  base::Thread* core_thread() { return &faux_core_thread_; }
  base::Thread* syncer_thread() { return &faux_syncer_thread_; }
 private:
  MessageLoop faux_ui_loop_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;
  base::Thread faux_syncer_thread_;
  base::Thread faux_core_thread_;
  scoped_refptr<UIModelWorker> bmw_;
  scoped_ptr<Syncer> syncer_;
};

TEST_F(SyncUIModelWorkerTest, ScheduledWorkRunsOnUILoop) {
  base::WaitableEvent v_was_run(false, false);
  scoped_ptr<UIModelWorkerVisitor> v(
      new UIModelWorkerVisitor(&v_was_run, true));

  syncer_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&Syncer::SyncShare, base::Unretained(syncer()), v.get()));

  // We are on the UI thread, so run our loop to process the
  // (hopefully) scheduled task from a SyncShare invocation.
  MessageLoop::current()->Run();

  bmw()->OnSyncerShutdownComplete();
  bmw()->Stop();
  syncer_thread()->Stop();
}

TEST_F(SyncUIModelWorkerTest, StopWithPendingWork) {
  // What we want to set up is the following:
  // ("ui_thread" is the thread we are currently executing on)
  // 1 - simulate the user shutting down the browser, and the ui thread needing
  //     to terminate the core thread.
  // 2 - the core thread is where the syncapi is accessed from, and so it needs
  //     to shut down the SyncerThread.
  // 3 - the syncer is waiting on the UIModelWorker to
  //     perform a task for it.
  // The UIModelWorker's manual shutdown pump will save the day, as the
  // UI thread is not actually trying to join() the core thread, it is merely
  // waiting for the SyncerThread to give it work or to finish. After that, it
  // will join the core thread which should succeed as the SyncerThread has left
  // the building. Unfortunately this test as written is not provably decidable,
  // as it will always halt on success, but it may not on failure (namely if
  // the task scheduled by the Syncer is _never_ run).
  core_thread()->Start();
  base::WaitableEvent v_ran(false, false);
  scoped_ptr<UIModelWorkerVisitor> v(new UIModelWorkerVisitor(
       &v_ran, false));
  base::WaitableEvent* jobs[] = { &v_ran };

  // The current message loop is not running, so queue a task to cause
  // UIModelWorker::Stop() to play a crucial role. See comment below.
  syncer_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&Syncer::SyncShare, base::Unretained(syncer()), v.get()));

  // This is what gets the core_thread blocked on the syncer_thread.
  core_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&FakeSyncapiShutdownCallback, syncer_thread(),
                 base::Unretained(bmw()),
                 static_cast<base::WaitableEvent**>(jobs), 1));

  // This is what gets the UI thread blocked until NotifyExitRequested,
  // which is called when FakeSyncapiShutdownCallback runs and deletes the
  // syncer.
  bmw()->Stop();

  EXPECT_FALSE(syncer_thread()->IsRunning());
  core_thread()->Stop();
}

TEST_F(SyncUIModelWorkerTest, HypotheticalManualPumpFlooding) {
  // This situation should not happen in real life because the Syncer should
  // never send more than one CallDoWork notification after early_exit_requested
  // has been set, but our UIModelWorker is built to handle this case
  // nonetheless. It may be needed in the future, and since we support it and
  // it is not actually exercised in the wild this test is essential.
  // It is identical to above except we schedule more than one visitor.
  core_thread()->Start();

  // Our ammunition.
  base::WaitableEvent fox1_ran(false, false);
  scoped_ptr<UIModelWorkerVisitor> fox1(new UIModelWorkerVisitor(
      &fox1_ran, false));
  base::WaitableEvent fox2_ran(false, false);
  scoped_ptr<UIModelWorkerVisitor> fox2(new UIModelWorkerVisitor(
      &fox2_ran, false));
  base::WaitableEvent fox3_ran(false, false);
  scoped_ptr<UIModelWorkerVisitor> fox3(new UIModelWorkerVisitor(
      &fox3_ran, false));
  base::WaitableEvent* jobs[] = { &fox1_ran, &fox2_ran, &fox3_ran };

  // The current message loop is not running, so queue a task to cause
  // UIModelWorker::Stop() to play a crucial role. See comment below.
  syncer_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&Syncer::SyncShare, base::Unretained(syncer()), fox1.get()));
  syncer_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&Syncer::SyncShare, base::Unretained(syncer()), fox2.get()));

  // This is what gets the core_thread blocked on the syncer_thread.
  core_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&FakeSyncapiShutdownCallback, syncer_thread(),
                 base::Unretained(bmw()),
                 static_cast<base::WaitableEvent**>(jobs), 3));
  syncer_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&Syncer::SyncShare, base::Unretained(syncer()), fox3.get()));

  // This is what gets the UI thread blocked until NotifyExitRequested,
  // which is called when FakeSyncapiShutdownCallback runs and deletes the
  // syncer.
  bmw()->Stop();

  // Was the thread killed?
  EXPECT_FALSE(syncer_thread()->IsRunning());
  core_thread()->Stop();
}
