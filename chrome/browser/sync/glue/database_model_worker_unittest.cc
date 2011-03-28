// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/timer.h"
#include "chrome/browser/sync/glue/database_model_worker.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::OneShotTimer;
using base::Thread;
using base::TimeDelta;
using browser_sync::DatabaseModelWorker;

namespace {

class DatabaseModelWorkerTest : public testing::Test {
 public:
  DatabaseModelWorkerTest() :
      did_do_work_(false),
      db_thread_(BrowserThread::DB),
      io_thread_(BrowserThread::IO, &io_loop_),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {}

  bool did_do_work() { return did_do_work_; }
  DatabaseModelWorker* worker() { return worker_.get(); }
  OneShotTimer<DatabaseModelWorkerTest>* timer() { return &timer_; }
  ScopedRunnableMethodFactory<DatabaseModelWorkerTest>* factory() {
    return &method_factory_;
  }

  // Schedule DoWork to be executed on the DB thread and have the test fail if
  // DoWork hasn't executed within 10 seconds.
  void ScheduleWork() {
    scoped_ptr<Callback0::Type> c(NewCallback(this,
        &DatabaseModelWorkerTest::DoWork));
    timer()->Start(TimeDelta::FromSeconds(10),
                   this, &DatabaseModelWorkerTest::Timeout);
    worker()->DoWorkAndWaitUntilDone(c.get());
  }

  // This is the work that will be scheduled to be done on the DB thread.
  void DoWork() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::DB));
    timer_.Stop();  // Stop the failure timer so the test succeeds.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, new MessageLoop::QuitTask());
    did_do_work_ = true;
  }

  // This will be called by the OneShotTimer and make the test fail unless
  // DoWork is called first.
  void Timeout() {
    ADD_FAILURE() << "Timed out waiting for work to be done on the DB thread.";
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, new MessageLoop::QuitTask());
  }

 protected:
  virtual void SetUp() {
    db_thread_.Start();
    worker_ = new DatabaseModelWorker();
  }

  virtual void Teardown() {
    worker_.release();
    db_thread_.Stop();
  }

 private:
  bool did_do_work_;
  scoped_refptr<DatabaseModelWorker> worker_;
  OneShotTimer<DatabaseModelWorkerTest> timer_;

  BrowserThread db_thread_;
  MessageLoopForIO io_loop_;
  BrowserThread io_thread_;

  ScopedRunnableMethodFactory<DatabaseModelWorkerTest> method_factory_;
};

TEST_F(DatabaseModelWorkerTest, DoesWorkOnDatabaseThread) {
  MessageLoop::current()->PostTask(FROM_HERE, factory()->NewRunnableMethod(
      &DatabaseModelWorkerTest::ScheduleWork));
  MessageLoop::current()->Run();
  EXPECT_TRUE(did_do_work());
}

}  // namespace
