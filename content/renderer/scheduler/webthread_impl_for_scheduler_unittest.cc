// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/webthread_impl_for_scheduler.h"

#include "base/run_loop.h"
#include "content/renderer/scheduler/renderer_scheduler_impl.h"
#include "content/renderer/scheduler/renderer_scheduler_message_loop_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace content {
namespace {

const int kWorkBatchSize = 2;

class MockTask : public blink::WebThread::Task {
 public:
  MOCK_METHOD0(run, void());
};

class MockTaskObserver : public blink::WebThread::TaskObserver {
 public:
  MOCK_METHOD0(willProcessTask, void());
  MOCK_METHOD0(didProcessTask, void());
};
}  // namespace

class WebThreadImplForSchedulerTest : public testing::Test {
 public:
  WebThreadImplForSchedulerTest()
      : scheduler_(
            RendererSchedulerMessageLoopDelegate::Create(&message_loop_)),
        default_task_runner_(scheduler_.DefaultTaskRunner()),
        thread_(&scheduler_) {}

  ~WebThreadImplForSchedulerTest() override {}

 protected:
  void EatDefaultTask(MockTaskObserver* observer) {
    // The scheduler posts one extra DoWork() task automatically.
    EXPECT_CALL(*observer, willProcessTask());
    EXPECT_CALL(*observer, didProcessTask());
  }

  base::MessageLoop message_loop_;
  RendererSchedulerImpl scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  WebThreadImplForScheduler thread_;

  DISALLOW_COPY_AND_ASSIGN(WebThreadImplForSchedulerTest);
};

TEST_F(WebThreadImplForSchedulerTest, TestTaskObserver) {
  MockTaskObserver observer;
  thread_.addTaskObserver(&observer);
  scoped_ptr<MockTask> task(new MockTask());

  {
    testing::InSequence sequence;
    EXPECT_CALL(observer, willProcessTask());
    EXPECT_CALL(*task, run());
    EXPECT_CALL(observer, didProcessTask());

    EatDefaultTask(&observer);
  }

  thread_.postTask(blink::WebTraceLocation(), task.release());
  message_loop_.RunUntilIdle();
  thread_.removeTaskObserver(&observer);
}

TEST_F(WebThreadImplForSchedulerTest, TestWorkBatchWithOneTask) {
  MockTaskObserver observer;
  thread_.addTaskObserver(&observer);
  scoped_ptr<MockTask> task(new MockTask());

  scheduler_.SetWorkBatchSizeForTesting(kWorkBatchSize);
  {
    testing::InSequence sequence;
    EXPECT_CALL(observer, willProcessTask());
    EXPECT_CALL(*task, run());
    EXPECT_CALL(observer, didProcessTask());

    EatDefaultTask(&observer);
  }

  thread_.postTask(blink::WebTraceLocation(), task.release());
  message_loop_.RunUntilIdle();
  thread_.removeTaskObserver(&observer);
}

TEST_F(WebThreadImplForSchedulerTest, TestWorkBatchWithTwoTasks) {
  MockTaskObserver observer;
  thread_.addTaskObserver(&observer);
  scoped_ptr<MockTask> task1(new MockTask());
  scoped_ptr<MockTask> task2(new MockTask());

  scheduler_.SetWorkBatchSizeForTesting(kWorkBatchSize);
  {
    testing::InSequence sequence;
    EXPECT_CALL(observer, willProcessTask());
    EXPECT_CALL(*task1, run());
    EXPECT_CALL(observer, didProcessTask());

    EXPECT_CALL(observer, willProcessTask());
    EXPECT_CALL(*task2, run());
    EXPECT_CALL(observer, didProcessTask());

    EatDefaultTask(&observer);
  }

  thread_.postTask(blink::WebTraceLocation(), task1.release());
  thread_.postTask(blink::WebTraceLocation(), task2.release());
  message_loop_.RunUntilIdle();
  thread_.removeTaskObserver(&observer);
}

TEST_F(WebThreadImplForSchedulerTest, TestWorkBatchWithThreeTasks) {
  MockTaskObserver observer;
  thread_.addTaskObserver(&observer);
  scoped_ptr<MockTask> task1(new MockTask());
  scoped_ptr<MockTask> task2(new MockTask());
  scoped_ptr<MockTask> task3(new MockTask());

  scheduler_.SetWorkBatchSizeForTesting(kWorkBatchSize);
  {
    testing::InSequence sequence;
    EXPECT_CALL(observer, willProcessTask());
    EXPECT_CALL(*task1, run());
    EXPECT_CALL(observer, didProcessTask());

    EXPECT_CALL(observer, willProcessTask());
    EXPECT_CALL(*task2, run());
    EXPECT_CALL(observer, didProcessTask());

    EXPECT_CALL(observer, willProcessTask());
    EXPECT_CALL(*task3, run());
    EXPECT_CALL(observer, didProcessTask());

    EatDefaultTask(&observer);
  }

  thread_.postTask(blink::WebTraceLocation(), task1.release());
  thread_.postTask(blink::WebTraceLocation(), task2.release());
  thread_.postTask(blink::WebTraceLocation(), task3.release());
  message_loop_.RunUntilIdle();
  thread_.removeTaskObserver(&observer);
}

class ExitRunLoopTask : public blink::WebThread::Task {
 public:
  ExitRunLoopTask(base::RunLoop* run_loop) : run_loop_(run_loop) {}

  virtual void run() { run_loop_->Quit(); }

 private:
  base::RunLoop* run_loop_;
};

void EnterRunLoop(base::MessageLoop* message_loop, blink::WebThread* thread) {
  // Note: WebThreads do not support nested run loops, which is why we use a
  // run loop directly.
  base::RunLoop run_loop;
  thread->postTask(blink::WebTraceLocation(), new ExitRunLoopTask(&run_loop));
  message_loop->SetNestableTasksAllowed(true);
  run_loop.Run();
}

TEST_F(WebThreadImplForSchedulerTest, TestNestedRunLoop) {
  MockTaskObserver observer;
  thread_.addTaskObserver(&observer);

  {
    testing::InSequence sequence;

    // One callback for EnterRunLoop.
    EXPECT_CALL(observer, willProcessTask());

    // A pair for ExitRunLoopTask.
    EXPECT_CALL(observer, willProcessTask());
    EXPECT_CALL(observer, didProcessTask());

    // A final callback for EnterRunLoop.
    EXPECT_CALL(observer, didProcessTask());

    EatDefaultTask(&observer);
  }

  message_loop_.PostTask(
      FROM_HERE, base::Bind(&EnterRunLoop, base::Unretained(&message_loop_),
                            base::Unretained(&thread_)));
  message_loop_.RunUntilIdle();
  thread_.removeTaskObserver(&observer);
}

}  // namespace content
