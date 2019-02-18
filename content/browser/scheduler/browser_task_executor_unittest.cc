// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_task_executor.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/test/scoped_task_environment.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_content_browser_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class BrowserTaskExecutorTest : public testing::Test {
 public:
  BrowserTaskExecutorTest() {
    old_browser_client_ = SetBrowserClientForTesting(&browser_client_);
  }

  ~BrowserTaskExecutorTest() override {
    SetBrowserClientForTesting(old_browser_client_);
  }

 protected:
  class AfterStartupBrowserClient : public TestContentBrowserClient {
   public:
    void PostAfterStartupTask(
        const base::Location& from_here,
        const scoped_refptr<base::TaskRunner>& task_runner,
        base::OnceClosure task) override {
      // The tests only post from UI thread.
      DCHECK_CURRENTLY_ON(BrowserThread::UI);
      tasks_.emplace_back(TaskEntry{from_here, task_runner, std::move(task)});
    }

    void RunTasks() {
      DCHECK_CURRENTLY_ON(BrowserThread::UI);
      for (TaskEntry& task : tasks_) {
        task.task_runner->PostTask(task.from_here, std::move(task.task));
      }
      tasks_.clear();
    }

    struct TaskEntry {
      base::Location from_here;
      scoped_refptr<base::TaskRunner> task_runner;
      base::OnceClosure task;
    };
    std::vector<TaskEntry> tasks_;
  };

  TestBrowserThreadBundle thread_bundle_{
      base::test::ScopedTaskEnvironment::MainThreadType::UI_MOCK_TIME};
  AfterStartupBrowserClient browser_client_;
  ContentBrowserClient* old_browser_client_;
};

TEST_F(BrowserTaskExecutorTest, EnsureUIThreadTraitPointsToExpectedQueue) {
  EXPECT_EQ(base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}),
            thread_bundle_.GetMainThreadTaskRunner());
}

TEST_F(BrowserTaskExecutorTest, EnsureIOThreadTraitPointsToExpectedQueue) {
  EXPECT_EQ(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
      BrowserTaskExecutor::GetProxyTaskRunnerForThread(BrowserThread::IO));
}

namespace {
void SetBoolFlag(bool* flag) {
  *flag = true;
}
}  // namespace

TEST_F(BrowserTaskExecutorTest, UserVisibleOrBlockingTasksRunDuringStartup) {
  bool ran_best_effort = false;
  bool ran_user_visible = false;
  bool ran_user_blocking = false;

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&SetBoolFlag, base::Unretained(&ran_best_effort)));
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&SetBoolFlag, base::Unretained(&ran_user_visible)));
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&SetBoolFlag, base::Unretained(&ran_user_blocking)));

  thread_bundle_.RunUntilIdle();

  EXPECT_FALSE(ran_best_effort);
  EXPECT_TRUE(ran_user_visible);
  EXPECT_TRUE(ran_user_blocking);
}

TEST_F(BrowserTaskExecutorTest, BestEffortTasksRunAfterStartup) {
  auto ui_best_effort_runner = base::CreateSingleThreadTaskRunnerWithTraits(
      {BrowserThread::UI, base::TaskPriority::BEST_EFFORT});

  // The TaskRunner shouldn't post directly to the proxy task runner.
  EXPECT_NE(
      ui_best_effort_runner,
      BrowserTaskExecutor::GetProxyTaskRunnerForThread(BrowserThread::UI));

  // Posting BEST_EFFORT tasks before startup should go to the browser_client_.
  bool ran_first_task = false;
  ui_best_effort_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&SetBoolFlag, base::Unretained(&ran_first_task)));

  ui_best_effort_runner->PostDelayedTask(
      FROM_HERE, base::DoNothing(), base::TimeDelta::FromMilliseconds(100));
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
      base::DoNothing(), base::TimeDelta::FromMilliseconds(200));
  PostDelayedTaskWithTraits(
      FROM_HERE, {BrowserThread::IO, base::TaskPriority::BEST_EFFORT},
      base::DoNothing(), base::TimeDelta::FromMilliseconds(300));

  // There should be a pending tasks, one from each thread's
  // AfterStartupTaskRunner.
  EXPECT_EQ(browser_client_.tasks_.size(), 2u);

  EXPECT_EQ(thread_bundle_.GetPendingMainThreadTaskCount(), 0u);

  // Emulate startup complete after 1 sec - this should post the two tasks to
  // the UI thread.
  thread_bundle_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  browser_client_.RunTasks();
  EXPECT_EQ(thread_bundle_.GetPendingMainThreadTaskCount(), 2u);

  // Run the two tasks including the first BEST_EFFORT task posted as immediate.
  // The three other BEST_EFFORT tasks should remain since they are delayed
  // tasks. They should have been posted with their original delays.
  thread_bundle_.RunUntilIdle();
  EXPECT_TRUE(ran_first_task);
  EXPECT_EQ(thread_bundle_.GetPendingMainThreadTaskCount(), 3u);

  // Run the delayed tasks one by one.
  for (size_t pending_tasks = 3; pending_tasks > 0; pending_tasks--) {
    EXPECT_EQ(thread_bundle_.NextMainThreadPendingTaskDelay(),
              base::TimeDelta::FromMilliseconds(100));
    thread_bundle_.FastForwardBy(base::TimeDelta::FromMilliseconds(100));
    EXPECT_EQ(thread_bundle_.GetPendingMainThreadTaskCount(),
              pending_tasks - 1u);
  }

  // Posting another BEST_EFFORT task should bypass the browser_client_.
  ui_best_effort_runner->PostTask(FROM_HERE, base::DoNothing());
  EXPECT_EQ(browser_client_.tasks_.size(), 0u);
  EXPECT_EQ(thread_bundle_.GetPendingMainThreadTaskCount(), 1u);
}

class BrowserTaskExecutorWithCustomSchedulerTest : public testing::Test {
 private:
  class ScopedTaskEnvironmentWithCustomScheduler
      : public base::test::ScopedTaskEnvironment {
   public:
    ScopedTaskEnvironmentWithCustomScheduler()
        : base::test::ScopedTaskEnvironment(
              SubclassCreatesDefaultTaskRunner{},
              base::test::ScopedTaskEnvironment::MainThreadType::UI_MOCK_TIME) {
      std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler =
          BrowserUIThreadScheduler::CreateForTesting(sequence_manager(),
                                                     GetTimeDomain());
      DeferredInitFromSubclass(
          browser_ui_thread_scheduler->GetTaskRunnerForTesting(
              QueueType::kDefault));
      browser_ui_thread_scheduler_ = browser_ui_thread_scheduler.get();
      BrowserTaskExecutor::CreateWithBrowserUIThreadSchedulerForTesting(
          std::move(browser_ui_thread_scheduler));
    }

    BrowserUIThreadScheduler* browser_ui_thread_scheduler() const {
      return browser_ui_thread_scheduler_;
    }

   private:
    BrowserUIThreadScheduler* browser_ui_thread_scheduler_;
  };

 public:
  using QueueType = BrowserUIThreadTaskQueue::QueueType;

  ~BrowserTaskExecutorWithCustomSchedulerTest() override {
    BrowserTaskExecutor::ResetForTesting();
  }

 protected:
  ScopedTaskEnvironmentWithCustomScheduler scoped_task_environment_;
};

TEST_F(BrowserTaskExecutorWithCustomSchedulerTest,
       EnsureUIThreadTraitPointsToExpectedQueue) {
  EXPECT_EQ(base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}),
            scoped_task_environment_.browser_ui_thread_scheduler()
                ->GetTaskRunnerForTesting(QueueType::kDefault));
  EXPECT_EQ(base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}),
            scoped_task_environment_.GetMainThreadTaskRunner());
}

}  // namespace content
