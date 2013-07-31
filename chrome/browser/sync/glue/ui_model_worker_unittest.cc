// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync/glue/ui_model_worker.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::UIModelWorker;
using syncer::SyncerError;
using content::BrowserThread;

class UIModelWorkerVisitor {
 public:
  UIModelWorkerVisitor(base::WaitableEvent* was_run,
                       bool quit_loop)
     : quit_loop_when_run_(quit_loop),
       was_run_(was_run) { }
  virtual ~UIModelWorkerVisitor() { }

  virtual syncer::SyncerError DoWork() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    was_run_->Signal();
    if (quit_loop_when_run_)
      base::MessageLoop::current()->Quit();
    return syncer::SYNCER_OK;
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
    syncer::WorkCallback c = base::Bind(&UIModelWorkerVisitor::DoWork,
                                       base::Unretained(visitor));
    worker_->DoWorkAndWaitUntilDone(c);
  }
 private:
  scoped_refptr<UIModelWorker> worker_;
  DISALLOW_COPY_AND_ASSIGN(Syncer);
};

class SyncUIModelWorkerTest : public testing::Test {
 public:
  SyncUIModelWorkerTest() : faux_syncer_thread_("FauxSyncerThread"),
                            faux_core_thread_("FauxCoreThread") { }

  virtual void SetUp() {
    faux_syncer_thread_.Start();
    ui_thread_.reset(new content::TestBrowserThread(BrowserThread::UI,
                                                    &faux_ui_loop_));
    bmw_ = new UIModelWorker(NULL);
    syncer_.reset(new Syncer(bmw_.get()));
  }

  Syncer* syncer() { return syncer_.get(); }
  UIModelWorker* bmw() { return bmw_.get(); }
  base::Thread* core_thread() { return &faux_core_thread_; }
  base::Thread* syncer_thread() { return &faux_syncer_thread_; }
 private:
  base::MessageLoop faux_ui_loop_;
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
  base::MessageLoop::current()->Run();
  syncer_thread()->Stop();
}
