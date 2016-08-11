// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/after_startup_task_utils.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::RunLoop;
using content::BrowserThread;
using content::TestBrowserThreadBundle;

namespace {

class WrappedTaskRunner : public base::TaskRunner {
 public:
  explicit WrappedTaskRunner(const scoped_refptr<TaskRunner>& real_runner)
      : real_task_runner_(real_runner) {}

  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override {
    ++posted_task_count_;
    return real_task_runner_->PostDelayedTask(
        from_here, base::Bind(&WrappedTaskRunner::RunWrappedTask, this, task),
        base::TimeDelta());  // Squash all delays so our tests complete asap.
  }

  bool RunsTasksOnCurrentThread() const override {
    return real_task_runner_->RunsTasksOnCurrentThread();
  }

  base::TaskRunner* real_runner() const { return real_task_runner_.get(); }

  int total_task_count() const { return posted_task_count_ + ran_task_count_; }
  int posted_task_count() const { return posted_task_count_; }
  int ran_task_count() const { return ran_task_count_; }

  void reset_task_counts() {
    posted_task_count_ = 0;
    ran_task_count_ = 0;
  }

 private:
  ~WrappedTaskRunner() override {}

  void RunWrappedTask(const base::Closure& task) {
    ++ran_task_count_;
    task.Run();
  }

  scoped_refptr<TaskRunner> real_task_runner_;
  int posted_task_count_ = 0;
  int ran_task_count_ = 0;
};

}  // namespace

class AfterStartupTaskTest : public testing::Test {
 public:
  AfterStartupTaskTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::REAL_DB_THREAD) {
    ui_thread_ = new WrappedTaskRunner(
        BrowserThread::GetTaskRunnerForThread(BrowserThread::UI));
    db_thread_ = new WrappedTaskRunner(
        BrowserThread::GetTaskRunnerForThread(BrowserThread::DB));
    AfterStartupTaskUtils::UnsafeResetForTesting();
  }

  // Hop to the db thread and call IsBrowserStartupComplete.
  bool GetIsBrowserStartupCompleteFromDBThread() {
    RunLoop run_loop;
    bool is_complete;
    base::PostTaskAndReplyWithResult(
        db_thread_->real_runner(), FROM_HERE,
        base::Bind(&AfterStartupTaskUtils::IsBrowserStartupComplete),
        base::Bind(&AfterStartupTaskTest::GotIsOnBrowserStartupComplete,
                   &run_loop, &is_complete));
    run_loop.Run();
    return is_complete;
  }

  // Hop to the db thread and call PostAfterStartupTask.
  void PostAfterStartupTaskFromDBThread(
      const tracked_objects::Location& from_here,
      const scoped_refptr<base::TaskRunner>& task_runner,
      const base::Closure& task) {
    RunLoop run_loop;
    db_thread_->real_runner()->PostTaskAndReply(
        FROM_HERE, base::Bind(&AfterStartupTaskUtils::PostTask, from_here,
                              task_runner, task),
        base::Bind(&RunLoop::Quit, base::Unretained(&run_loop)));
    run_loop.Run();
  }

  // Make sure all tasks posted to the DB thread get run.
  void FlushDBThread() {
    RunLoop run_loop;
    db_thread_->real_runner()->PostTaskAndReply(
        FROM_HERE, base::Bind(&base::DoNothing),
        base::Bind(&RunLoop::Quit, base::Unretained(&run_loop)));
    run_loop.Run();
  }

  static void VerifyExpectedThread(BrowserThread::ID id) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(id));
  }

 protected:
  scoped_refptr<WrappedTaskRunner> ui_thread_;
  scoped_refptr<WrappedTaskRunner> db_thread_;

 private:
  static void GotIsOnBrowserStartupComplete(RunLoop* loop,
                                            bool* out,
                                            bool is_complete) {
    *out = is_complete;
    loop->Quit();
  }

  TestBrowserThreadBundle browser_thread_bundle_;
};

TEST_F(AfterStartupTaskTest, IsStartupComplete) {
  // Check IsBrowserStartupComplete on a background thread first to
  // verify that it does not allocate the underlying flag on that thread.
  // That allocation thread correctness part of this test relies on
  // the DCHECK in CancellationFlag::Set().
  EXPECT_FALSE(GetIsBrowserStartupCompleteFromDBThread());
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
  EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  EXPECT_TRUE(GetIsBrowserStartupCompleteFromDBThread());
}

TEST_F(AfterStartupTaskTest, PostTask) {
  // Nothing should be posted prior to startup completion.
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  AfterStartupTaskUtils::PostTask(
      FROM_HERE, ui_thread_,
      base::Bind(&AfterStartupTaskTest::VerifyExpectedThread,
                 BrowserThread::UI));
  AfterStartupTaskUtils::PostTask(
      FROM_HERE, db_thread_,
      base::Bind(&AfterStartupTaskTest::VerifyExpectedThread,
                 BrowserThread::DB));
  PostAfterStartupTaskFromDBThread(
      FROM_HERE, ui_thread_,
      base::Bind(&AfterStartupTaskTest::VerifyExpectedThread,
                 BrowserThread::UI));
  PostAfterStartupTaskFromDBThread(
      FROM_HERE, db_thread_,
      base::Bind(&AfterStartupTaskTest::VerifyExpectedThread,
                 BrowserThread::DB));
  RunLoop().RunUntilIdle();
  EXPECT_EQ(0, db_thread_->total_task_count() + ui_thread_->total_task_count());

  // Queued tasks should be posted upon setting the flag.
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
  EXPECT_EQ(2, db_thread_->posted_task_count());
  EXPECT_EQ(2, ui_thread_->posted_task_count());
  FlushDBThread();
  RunLoop().RunUntilIdle();
  EXPECT_EQ(2, db_thread_->ran_task_count());
  EXPECT_EQ(2, ui_thread_->ran_task_count());

  db_thread_->reset_task_counts();
  ui_thread_->reset_task_counts();
  EXPECT_EQ(0, db_thread_->total_task_count() + ui_thread_->total_task_count());

  // Tasks posted after startup should get posted immediately.
  AfterStartupTaskUtils::PostTask(FROM_HERE, ui_thread_,
                                  base::Bind(&base::DoNothing));
  AfterStartupTaskUtils::PostTask(FROM_HERE, db_thread_,
                                  base::Bind(&base::DoNothing));
  EXPECT_EQ(1, db_thread_->posted_task_count());
  EXPECT_EQ(1, ui_thread_->posted_task_count());
  PostAfterStartupTaskFromDBThread(FROM_HERE, ui_thread_,
                                   base::Bind(&base::DoNothing));
  PostAfterStartupTaskFromDBThread(FROM_HERE, db_thread_,
                                   base::Bind(&base::DoNothing));
  EXPECT_EQ(2, db_thread_->posted_task_count());
  EXPECT_EQ(2, ui_thread_->posted_task_count());
  FlushDBThread();
  RunLoop().RunUntilIdle();
  EXPECT_EQ(2, db_thread_->ran_task_count());
  EXPECT_EQ(2, ui_thread_->ran_task_count());
}

// Verify that posting to an AfterStartupTaskUtils::Runner bound to |db_thread_|
// results in the same behavior as posting via
// AfterStartupTaskUtils::PostTask(..., db_thread_, ...).
TEST_F(AfterStartupTaskTest, AfterStartupTaskUtilsRunner) {
  scoped_refptr<base::TaskRunner> after_startup_runner =
      make_scoped_refptr(new AfterStartupTaskUtils::Runner(db_thread_));

  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  after_startup_runner->PostTask(
      FROM_HERE, base::Bind(&AfterStartupTaskTest::VerifyExpectedThread,
                            BrowserThread::DB));

  RunLoop().RunUntilIdle();
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  EXPECT_EQ(0, db_thread_->total_task_count());

  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
  EXPECT_EQ(1, db_thread_->posted_task_count());

  FlushDBThread();
  RunLoop().RunUntilIdle();
  EXPECT_EQ(1, db_thread_->ran_task_count());

  EXPECT_EQ(0, ui_thread_->total_task_count());
}
