// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/browser_thread_model_worker.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Thread;
using base::TimeDelta;

namespace syncer {

namespace {

class SyncBrowserThreadModelWorkerTest : public testing::Test {
 public:
  SyncBrowserThreadModelWorkerTest()
      : db_thread_("DB_Thread"), did_do_work_(false), weak_factory_(this) {}

  bool did_do_work() { return did_do_work_; }
  BrowserThreadModelWorker* worker() { return worker_.get(); }
  base::OneShotTimer* timer() { return &timer_; }
  base::WeakPtrFactory<SyncBrowserThreadModelWorkerTest>* factory() {
    return &weak_factory_;
  }

  // Schedule DoWork to be executed on the DB thread and have the test fail if
  // DoWork hasn't executed within action_timeout().
  void ScheduleWork() {
    // We wait until the callback is done. So it is safe to use unretained.
    WorkCallback c = base::Bind(&SyncBrowserThreadModelWorkerTest::DoWork,
                                base::Unretained(this));
    timer()->Start(FROM_HERE, TestTimeouts::action_timeout(), this,
                   &SyncBrowserThreadModelWorkerTest::Timeout);
    worker()->DoWorkAndWaitUntilDone(c);
  }

  // This is the work that will be scheduled to be done on the DB thread.
  SyncerError DoWork() {
    EXPECT_TRUE(db_thread_.task_runner()->BelongsToCurrentThread());
    main_message_loop_.task_runner()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
    did_do_work_ = true;
    return SYNCER_OK;
  }

  // This will be called by the OneShotTimer and make the test fail unless
  // DoWork is called first.
  void Timeout() {
    ADD_FAILURE() << "Timed out waiting for work to be done on the DB thread.";
    main_message_loop_.task_runner()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  }

 protected:
  void SetUp() override {
    db_thread_.Start();
    worker_ = new BrowserThreadModelWorker(db_thread_.task_runner(), GROUP_DB);
  }

  virtual void Teardown() {
    worker_ = nullptr;
    db_thread_.Stop();
  }

 private:
  base::MessageLoop main_message_loop_;
  Thread db_thread_;
  bool did_do_work_;
  scoped_refptr<BrowserThreadModelWorker> worker_;
  base::OneShotTimer timer_;

  base::WeakPtrFactory<SyncBrowserThreadModelWorkerTest> weak_factory_;
};

TEST_F(SyncBrowserThreadModelWorkerTest, DoesWorkOnDatabaseThread) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&SyncBrowserThreadModelWorkerTest::ScheduleWork,
                            factory()->GetWeakPtr()));
  base::RunLoop().Run();
  EXPECT_TRUE(did_do_work());
}

}  // namespace

}  // namespace syncer
