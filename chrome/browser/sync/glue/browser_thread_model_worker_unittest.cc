// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/timer.h"
#include "chrome/browser/sync/glue/browser_thread_model_worker.h"
#include "content/public/test/test_browser_thread.h"
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
      db_thread_(BrowserThread::DB),
      io_thread_(BrowserThread::IO, &io_loop_),
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
        BrowserThread::IO, FROM_HERE, MessageLoop::QuitClosure());
    did_do_work_ = true;
    return syncer::SYNCER_OK;
  }

  // This will be called by the OneShotTimer and make the test fail unless
  // DoWork is called first.
  void Timeout() {
    ADD_FAILURE() << "Timed out waiting for work to be done on the DB thread.";
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, MessageLoop::QuitClosure());
  }

 protected:
  virtual void SetUp() {
    db_thread_.Start();
    worker_ = new DatabaseModelWorker();
  }

  virtual void Teardown() {
    worker_ = NULL;
    db_thread_.Stop();
  }

 private:
  bool did_do_work_;
  scoped_refptr<BrowserThreadModelWorker> worker_;
  OneShotTimer<SyncBrowserThreadModelWorkerTest> timer_;

  content::TestBrowserThread db_thread_;
  MessageLoopForIO io_loop_;
  content::TestBrowserThread io_thread_;

  base::WeakPtrFactory<SyncBrowserThreadModelWorkerTest> weak_factory_;
};

TEST_F(SyncBrowserThreadModelWorkerTest, DoesWorkOnDatabaseThread) {
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&SyncBrowserThreadModelWorkerTest::ScheduleWork,
                 factory()->GetWeakPtr()));
  MessageLoop::current()->Run();
  EXPECT_TRUE(did_do_work());
}

}  // namespace

}  // namespace browser_sync
