// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_task_executor.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_content_browser_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using ::testing::Invoke;
using ::testing::Mock;

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

using MockTask =
    testing::StrictMock<base::MockCallback<base::RepeatingCallback<void()>>>;

TEST_F(BrowserTaskExecutorTest, RunAllPendingTasksForTestingOnUI) {
  MockTask task_1;
  MockTask task_2;
  EXPECT_CALL(task_1, Run).WillOnce(testing::Invoke([&]() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, task_2.Get());
  }));

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, task_1.Get());

  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::UI);

  // Cleanup pending tasks, as TestBrowserThreadBundle will run them.
  Mock::VerifyAndClearExpectations(&task_1);
  EXPECT_CALL(task_2, Run);
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);
}

TEST_F(BrowserTaskExecutorTest, RunAllPendingTasksForTestingOnIO) {
  MockTask task_1;
  MockTask task_2;
  EXPECT_CALL(task_1, Run).WillOnce(testing::Invoke([&]() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, task_2.Get());
  }));

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO}, task_1.Get());

  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);

  // Cleanup pending tasks, as TestBrowserThreadBundle will run them.
  Mock::VerifyAndClearExpectations(&task_1);
  EXPECT_CALL(task_2, Run);
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);
}

TEST_F(BrowserTaskExecutorTest, RunAllPendingTasksForTestingOnIOIsReentrant) {
  MockTask task_1;
  MockTask task_2;
  MockTask task_3;

  EXPECT_CALL(task_1, Run).WillOnce(Invoke([&]() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, task_2.Get());
    BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(
        BrowserThread::IO);
  }));
  EXPECT_CALL(task_2, Run).WillOnce(Invoke([&]() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, task_3.Get());
  }));

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO}, task_1.Get());
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);

  // Cleanup pending tasks, as TestBrowserThreadBundle will run them.
  Mock::VerifyAndClearExpectations(&task_1);
  Mock::VerifyAndClearExpectations(&task_2);
  EXPECT_CALL(task_3, Run);
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);
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
          browser_ui_thread_scheduler->GetHandle().task_runner(
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
  using QueueType = BrowserUIThreadScheduler::QueueType;

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
                ->GetHandle()
                .task_runner(QueueType::kDefault));
  EXPECT_EQ(base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}),
            scoped_task_environment_.GetMainThreadTaskRunner());
}

TEST_F(BrowserTaskExecutorWithCustomSchedulerTest,
       UserVisibleOrBlockingTasksRunDuringStartup) {
  MockTask best_effort;
  MockTask user_visible;
  MockTask user_blocking;

  base::PostTaskWithTraits(FROM_HERE,
                           {BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
                           best_effort.Get());
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      user_visible.Get());
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_BLOCKING},
      user_blocking.Get());

  EXPECT_CALL(user_visible, Run);
  EXPECT_CALL(user_blocking, Run);

  scoped_task_environment_.RunUntilIdle();
}

TEST_F(BrowserTaskExecutorWithCustomSchedulerTest,
       BestEffortTasksRunAfterStartup) {
  auto ui_best_effort_runner = base::CreateSingleThreadTaskRunnerWithTraits(
      {BrowserThread::UI, base::TaskPriority::BEST_EFFORT});

  MockTask best_effort;

  ui_best_effort_runner->PostTask(FROM_HERE, best_effort.Get());
  ui_best_effort_runner->PostDelayedTask(
      FROM_HERE, best_effort.Get(), base::TimeDelta::FromMilliseconds(100));
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
      best_effort.Get(), base::TimeDelta::FromMilliseconds(100));
  base::PostTaskWithTraits(FROM_HERE,
                           {BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
                           best_effort.Get());
  scoped_task_environment_.RunUntilIdle();

  BrowserTaskExecutor::EnableBestEffortQueues();
  EXPECT_CALL(best_effort, Run).Times(4);
  scoped_task_environment_.FastForwardBy(
      base::TimeDelta::FromMilliseconds(100));
}

}  // namespace content
