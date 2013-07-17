// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "chrome/browser/sync/glue/browser_thread_model_worker.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::OneShotTimer;
using base::Thread;
using base::TimeDelta;
using content::BrowserThread;

namespace browser_sync {

namespace {

class SyncBrowserThreadModelWorkerTest : public testing::Test {
 public:
  SyncBrowserThreadModelWorkerTest() :
      did_do_work_(false),
      thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP |
                     content::TestBrowserThreadBundle::REAL_DB_THREAD),
      weak_factory_(this) {}

  bool did_do_work() { return did_do_work_; }
  BrowserThreadModelWorker* worker() { return worker_.get(); }
  OneShotTimer<SyncBrowserThreadModelWorkerTest>* timer() { return &timer_; }
  base::WeakPtrFactory<SyncBrowserThreadModelWorkerTest>* factory() {
    return &weak_factory_;
  }

  // Schedule DoWork to be executed on the DB thread and have the test fail if
  // DoWork hasn't executed within action_timeout().
  void ScheduleWork() {
   // We wait until the callback is done. So it is safe to use unretained.
    syncer::WorkCallback c =
        base::Bind(&SyncBrowserThreadModelWorkerTest::DoWork,
                   base::Unretained(this));
    timer()->Start(
        FROM_HERE,
        TestTimeouts::action_timeout(),
        this,
        &SyncBrowserThreadModelWorkerTest::Timeout);
    worker()->DoWorkAndWaitUntilDone(c);
  }

  // This is the work that will be scheduled to be done on the DB thread.
  syncer::SyncerError DoWork() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::DB));
    timer_.Stop();  // Stop the failure timer so the test succeeds.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, base::MessageLoop::QuitClosure());
    did_do_work_ = true;
    return syncer::SYNCER_OK;
  }

  // This will be called by the OneShotTimer and make the test fail unless
  // DoWork is called first.
  void Timeout() {
    ADD_FAILURE() << "Timed out waiting for work to be done on the DB thread.";
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, base::MessageLoop::QuitClosure());
  }

 protected:
  virtual void SetUp() {
    worker_ = new DatabaseModelWorker(NULL);
  }

  virtual void Teardown() {
    worker_ = NULL;
  }

 private:
  bool did_do_work_;
  scoped_refptr<BrowserThreadModelWorker> worker_;
  OneShotTimer<SyncBrowserThreadModelWorkerTest> timer_;

  content::TestBrowserThreadBundle thread_bundle_;

  base::WeakPtrFactory<SyncBrowserThreadModelWorkerTest> weak_factory_;
};

TEST_F(SyncBrowserThreadModelWorkerTest, DoesWorkOnDatabaseThread) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&SyncBrowserThreadModelWorkerTest::ScheduleWork,
                 factory()->GetWeakPtr()));
  base::MessageLoop::current()->Run();
  EXPECT_TRUE(did_do_work());
}

}  // namespace

}  // namespace browser_sync
