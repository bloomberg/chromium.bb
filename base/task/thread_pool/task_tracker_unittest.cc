// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/task_tracker.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/sequence_token.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/common/checked_lock.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/task.h"
#include "base/task/thread_pool/test_utils.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

constexpr size_t kLoadTestNumIterations = 75;

// Invokes a closure asynchronously.
class CallbackThread : public SimpleThread {
 public:
  explicit CallbackThread(const Closure& closure)
      : SimpleThread("CallbackThread"), closure_(closure) {}

  // Returns true once the callback returns.
  bool has_returned() { return has_returned_.IsSet(); }

 private:
  void Run() override {
    closure_.Run();
    has_returned_.Set();
  }

  const Closure closure_;
  AtomicFlag has_returned_;

  DISALLOW_COPY_AND_ASSIGN(CallbackThread);
};

class ThreadPostingAndRunningTask : public SimpleThread {
 public:
  enum class Action {
    WILL_POST,
    RUN,
    WILL_POST_AND_RUN,
  };

  ThreadPostingAndRunningTask(TaskTracker* tracker,
                              Task* task,
                              const TaskTraits& traits,
                              Action action,
                              bool expect_post_succeeds)
      : SimpleThread("ThreadPostingAndRunningTask"),
        tracker_(tracker),
        owned_task_(FROM_HERE, OnceClosure(), TimeDelta()),
        task_(task),
        traits_(traits),
        action_(action),
        expect_post_succeeds_(expect_post_succeeds) {
    EXPECT_TRUE(task_);

    // Ownership of the Task is required to run it.
    EXPECT_NE(Action::RUN, action_);
    EXPECT_NE(Action::WILL_POST_AND_RUN, action_);
  }

  ThreadPostingAndRunningTask(TaskTracker* tracker,
                              Task task,
                              const TaskTraits& traits,
                              Action action,
                              bool expect_post_succeeds)
      : SimpleThread("ThreadPostingAndRunningTask"),
        tracker_(tracker),
        owned_task_(std::move(task)),
        task_(&owned_task_),
        traits_(traits),
        action_(action),
        expect_post_succeeds_(expect_post_succeeds) {
    EXPECT_TRUE(owned_task_.task);
  }

 private:
  void Run() override {
    bool post_succeeded = true;
    if (action_ == Action::WILL_POST || action_ == Action::WILL_POST_AND_RUN) {
      post_succeeded =
          tracker_->WillPostTask(task_, traits_.shutdown_behavior());
      EXPECT_EQ(expect_post_succeeds_, post_succeeded);
    }
    if (post_succeeded &&
        (action_ == Action::RUN || action_ == Action::WILL_POST_AND_RUN)) {
      EXPECT_TRUE(owned_task_.task);

      // Expect RunAndPopNextTask to return nullptr since |sequence| is empty
      // after popping a task from it.
      EXPECT_FALSE(tracker_->RunAndPopNextTask(
          test::CreateSequenceWithTask(std::move(owned_task_), traits_)));
    }
  }

  TaskTracker* const tracker_;
  Task owned_task_;
  Task* task_;
  const TaskTraits traits_;
  const Action action_;
  const bool expect_post_succeeds_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPostingAndRunningTask);
};

class ScopedSetSingletonAllowed {
 public:
  ScopedSetSingletonAllowed(bool singleton_allowed)
      : previous_value_(
            ThreadRestrictions::SetSingletonAllowed(singleton_allowed)) {}
  ~ScopedSetSingletonAllowed() {
    ThreadRestrictions::SetSingletonAllowed(previous_value_);
  }

 private:
  const bool previous_value_;
};

class ThreadPoolTaskTrackerTest
    : public testing::TestWithParam<TaskShutdownBehavior> {
 protected:
  ThreadPoolTaskTrackerTest() = default;

  // Creates a task.
  Task CreateTask() {
    return Task(
        FROM_HERE,
        Bind(&ThreadPoolTaskTrackerTest::RunTaskCallback, Unretained(this)),
        TimeDelta());
  }

  void DispatchAndRunTaskWithTracker(Task task, const TaskTraits& traits) {
    tracker_.RunAndPopNextTask(
        test::CreateSequenceWithTask(std::move(task), traits));
  }

  // Calls tracker_->CompleteShutdown() on a new thread and expects it to block.
  void ExpectAsyncCompleteShutdownBlocks() {
    ASSERT_FALSE(thread_calling_shutdown_);
    ASSERT_TRUE(tracker_.HasShutdownStarted());
    thread_calling_shutdown_ = std::make_unique<CallbackThread>(
        Bind(&TaskTracker::CompleteShutdown, Unretained(&tracker_)));
    thread_calling_shutdown_->Start();
    PlatformThread::Sleep(TestTimeouts::tiny_timeout());
    VerifyAsyncShutdownInProgress();
  }

  void WaitForAsyncIsShutdownComplete() {
    ASSERT_TRUE(thread_calling_shutdown_);
    thread_calling_shutdown_->Join();
    EXPECT_TRUE(thread_calling_shutdown_->has_returned());
    EXPECT_TRUE(tracker_.IsShutdownComplete());
  }

  void VerifyAsyncShutdownInProgress() {
    ASSERT_TRUE(thread_calling_shutdown_);
    EXPECT_FALSE(thread_calling_shutdown_->has_returned());
    EXPECT_TRUE(tracker_.HasShutdownStarted());
    EXPECT_FALSE(tracker_.IsShutdownComplete());
  }

  // Calls tracker_->FlushForTesting() on a new thread.
  void CallFlushFromAnotherThread() {
    ASSERT_FALSE(thread_calling_flush_);
    thread_calling_flush_.reset(new CallbackThread(
        Bind(&TaskTracker::FlushForTesting, Unretained(&tracker_))));
    thread_calling_flush_->Start();
  }

  void WaitForAsyncFlushReturned() {
    ASSERT_TRUE(thread_calling_flush_);
    thread_calling_flush_->Join();
    EXPECT_TRUE(thread_calling_flush_->has_returned());
  }

  void VerifyAsyncFlushInProgress() {
    ASSERT_TRUE(thread_calling_flush_);
    EXPECT_FALSE(thread_calling_flush_->has_returned());
  }

  size_t NumTasksExecuted() {
    CheckedAutoLock auto_lock(lock_);
    return num_tasks_executed_;
  }

  TaskTracker tracker_ = {"Test"};

 private:
  void RunTaskCallback() {
    CheckedAutoLock auto_lock(lock_);
    ++num_tasks_executed_;
  }

  std::unique_ptr<CallbackThread> thread_calling_shutdown_;
  std::unique_ptr<CallbackThread> thread_calling_flush_;

  // Synchronizes accesses to |num_tasks_executed_|.
  CheckedLock lock_;

  size_t num_tasks_executed_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ThreadPoolTaskTrackerTest);
};

#define WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED() \
  do {                                      \
    SCOPED_TRACE("");                       \
    WaitForAsyncIsShutdownComplete();       \
  } while (false)

#define VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS() \
  do {                                      \
    SCOPED_TRACE("");                       \
    VerifyAsyncShutdownInProgress();        \
  } while (false)

#define WAIT_FOR_ASYNC_FLUSH_RETURNED() \
  do {                                  \
    SCOPED_TRACE("");                   \
    WaitForAsyncFlushReturned();        \
  } while (false)

#define VERIFY_ASYNC_FLUSH_IN_PROGRESS() \
  do {                                   \
    SCOPED_TRACE("");                    \
    VerifyAsyncFlushInProgress();        \
  } while (false)

}  // namespace

TEST_P(ThreadPoolTaskTrackerTest, WillPostAndRunBeforeShutdown) {
  Task task(CreateTask());

  // Inform |task_tracker_| that |task| will be posted.
  EXPECT_TRUE(tracker_.WillPostTask(&task, GetParam()));

  // Run the task.
  EXPECT_EQ(0U, NumTasksExecuted());

  DispatchAndRunTaskWithTracker(std::move(task), GetParam());
  EXPECT_EQ(1U, NumTasksExecuted());

  // Shutdown() shouldn't block.
  test::ShutdownTaskTracker(&tracker_);
}

TEST_P(ThreadPoolTaskTrackerTest, WillPostAndRunLongTaskBeforeShutdown) {
  // Create a task that signals |task_running| and blocks until |task_barrier|
  // is signaled.
  WaitableEvent task_running(WaitableEvent::ResetPolicy::AUTOMATIC,
                             WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent task_barrier(WaitableEvent::ResetPolicy::AUTOMATIC,
                             WaitableEvent::InitialState::NOT_SIGNALED);
  Task blocked_task(
      FROM_HERE,
      Bind(
          [](WaitableEvent* task_running, WaitableEvent* task_barrier) {
            task_running->Signal();
            task_barrier->Wait();
          },
          Unretained(&task_running), Unretained(&task_barrier)),
      TimeDelta());

  // Inform |task_tracker_| that |blocked_task| will be posted.
  EXPECT_TRUE(tracker_.WillPostTask(&blocked_task, GetParam()));

  // Create a thread to run the task. Wait until the task starts running.
  ThreadPostingAndRunningTask thread_running_task(
      &tracker_, std::move(blocked_task),
      TaskTraits(WithBaseSyncPrimitives(), GetParam()),
      ThreadPostingAndRunningTask::Action::RUN, false);
  thread_running_task.Start();
  task_running.Wait();

  // Initiate shutdown after the task has started to run.
  tracker_.StartShutdown();

  if (GetParam() == TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN) {
    // Shutdown should complete even with a CONTINUE_ON_SHUTDOWN in progress.
    tracker_.CompleteShutdown();
  } else {
    // Shutdown should block with any non CONTINUE_ON_SHUTDOWN task in progress.
    ExpectAsyncCompleteShutdownBlocks();
  }

  // Unblock the task.
  task_barrier.Signal();
  thread_running_task.Join();

  // Shutdown should now complete for a non CONTINUE_ON_SHUTDOWN task.
  if (GetParam() != TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
    WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();
}

TEST_P(ThreadPoolTaskTrackerTest, WillPostBeforeShutdownRunDuringShutdown) {
  // Inform |task_tracker_| that a task will be posted.
  Task task(CreateTask());
  EXPECT_TRUE(tracker_.WillPostTask(&task, GetParam()));

  // Inform |task_tracker_| that a BLOCK_SHUTDOWN task will be posted just to
  // block shutdown.
  Task block_shutdown_task(CreateTask());
  EXPECT_TRUE(tracker_.WillPostTask(&block_shutdown_task,
                                    TaskShutdownBehavior::BLOCK_SHUTDOWN));

  // Start shutdown and try to complete it asynchronously.
  tracker_.StartShutdown();
  ExpectAsyncCompleteShutdownBlocks();

  // Try to run |task|. It should only run it it's BLOCK_SHUTDOWN. Otherwise it
  // should be discarded.
  EXPECT_EQ(0U, NumTasksExecuted());
  const bool should_run = GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN;

  DispatchAndRunTaskWithTracker(std::move(task), GetParam());
  EXPECT_EQ(should_run ? 1U : 0U, NumTasksExecuted());
  VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS();

  // Unblock shutdown by running the remaining BLOCK_SHUTDOWN task.
  DispatchAndRunTaskWithTracker(std::move(block_shutdown_task),
                                TaskShutdownBehavior::BLOCK_SHUTDOWN);
  EXPECT_EQ(should_run ? 2U : 1U, NumTasksExecuted());
  WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();
}

TEST_P(ThreadPoolTaskTrackerTest, WillPostBeforeShutdownRunAfterShutdown) {
  // Inform |task_tracker_| that a task will be posted.
  Task task(CreateTask());
  EXPECT_TRUE(tracker_.WillPostTask(&task, GetParam()));

  // Start shutdown.
  tracker_.StartShutdown();
  EXPECT_EQ(0U, NumTasksExecuted());

  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN) {
    // Verify that CompleteShutdown() blocks.
    ExpectAsyncCompleteShutdownBlocks();

    // Run the task to unblock shutdown.
    DispatchAndRunTaskWithTracker(std::move(task), GetParam());
    EXPECT_EQ(1U, NumTasksExecuted());
    WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();

    // It is not possible to test running a BLOCK_SHUTDOWN task posted before
    // shutdown after shutdown because Shutdown() won't return if there are
    // pending BLOCK_SHUTDOWN tasks.
  } else {
    tracker_.CompleteShutdown();

    // The task shouldn't be allowed to run after shutdown.
    DispatchAndRunTaskWithTracker(std::move(task), GetParam());
    EXPECT_EQ(0U, NumTasksExecuted());
  }
}

TEST_P(ThreadPoolTaskTrackerTest, WillPostAndRunDuringShutdown) {
  // Inform |task_tracker_| that a BLOCK_SHUTDOWN task will be posted just to
  // block shutdown.
  Task block_shutdown_task(CreateTask());
  EXPECT_TRUE(tracker_.WillPostTask(&block_shutdown_task,
                                    TaskShutdownBehavior::BLOCK_SHUTDOWN));

  // Start shutdown.
  tracker_.StartShutdown();

  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN) {
    // Inform |task_tracker_| that a BLOCK_SHUTDOWN task will be posted.
    Task task(CreateTask());
    EXPECT_TRUE(tracker_.WillPostTask(&task, GetParam()));

    // Run the BLOCK_SHUTDOWN task.
    EXPECT_EQ(0U, NumTasksExecuted());
    DispatchAndRunTaskWithTracker(std::move(task), GetParam());
    EXPECT_EQ(1U, NumTasksExecuted());
  } else {
    // It shouldn't be allowed to post a non BLOCK_SHUTDOWN task.
    Task task(CreateTask());
    EXPECT_FALSE(tracker_.WillPostTask(&task, GetParam()));

    // Don't try to run the task, because it wasn't allowed to be posted.
  }

  // Verify that CompleteShutdown() blocks.
  ExpectAsyncCompleteShutdownBlocks();

  // Unblock shutdown by running |block_shutdown_task|.
  DispatchAndRunTaskWithTracker(std::move(block_shutdown_task),
                                TaskShutdownBehavior::BLOCK_SHUTDOWN);
  EXPECT_EQ(GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN ? 2U : 1U,
            NumTasksExecuted());
  WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();
}

TEST_P(ThreadPoolTaskTrackerTest, WillPostAfterShutdown) {
  test::ShutdownTaskTracker(&tracker_);

  Task task(CreateTask());

  // |task_tracker_| shouldn't allow a task to be posted after shutdown.
  EXPECT_FALSE(tracker_.WillPostTask(&task, GetParam()));
}

// Verify that BLOCK_SHUTDOWN and SKIP_ON_SHUTDOWN tasks can
// AssertSingletonAllowed() but CONTINUE_ON_SHUTDOWN tasks can't.
TEST_P(ThreadPoolTaskTrackerTest, SingletonAllowed) {
  const bool can_use_singletons =
      (GetParam() != TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN);

  Task task(FROM_HERE, BindOnce(&ThreadRestrictions::AssertSingletonAllowed),
            TimeDelta());
  EXPECT_TRUE(tracker_.WillPostTask(&task, GetParam()));

  // Set the singleton allowed bit to the opposite of what it is expected to be
  // when |tracker| runs |task| to verify that |tracker| actually sets the
  // correct value.
  ScopedSetSingletonAllowed scoped_singleton_allowed(!can_use_singletons);

  // Running the task should fail iff the task isn't allowed to use singletons.
  if (can_use_singletons) {
    DispatchAndRunTaskWithTracker(std::move(task), GetParam());
  } else {
    EXPECT_DCHECK_DEATH(
        { DispatchAndRunTaskWithTracker(std::move(task), GetParam()); });
  }
}

// Verify that AssertIOAllowed() succeeds only for a MayBlock() task.
TEST_P(ThreadPoolTaskTrackerTest, IOAllowed) {
  // Unset the IO allowed bit. Expect TaskTracker to set it before running a
  // task with the MayBlock() trait.
  ThreadRestrictions::SetIOAllowed(false);
  Task task_with_may_block(FROM_HERE, Bind([]() {
                             // Shouldn't fail.
                             ScopedBlockingCall scope_blocking_call(
                                 FROM_HERE, BlockingType::WILL_BLOCK);
                           }),
                           TimeDelta());
  TaskTraits traits_with_may_block = TaskTraits(MayBlock(), GetParam());
  EXPECT_TRUE(tracker_.WillPostTask(&task_with_may_block,
                                    traits_with_may_block.shutdown_behavior()));
  DispatchAndRunTaskWithTracker(std::move(task_with_may_block),
                                traits_with_may_block);

  // Set the IO allowed bit. Expect TaskTracker to unset it before running a
  // task without the MayBlock() trait.
  ThreadRestrictions::SetIOAllowed(true);
  Task task_without_may_block(FROM_HERE, Bind([]() {
                                EXPECT_DCHECK_DEATH({
                                  ScopedBlockingCall scope_blocking_call(
                                      FROM_HERE, BlockingType::WILL_BLOCK);
                                });
                              }),
                              TimeDelta());
  TaskTraits traits_without_may_block = TaskTraits(GetParam());
  EXPECT_TRUE(tracker_.WillPostTask(
      &task_without_may_block, traits_without_may_block.shutdown_behavior()));
  DispatchAndRunTaskWithTracker(std::move(task_without_may_block),
                                traits_without_may_block);
}

static void RunTaskRunnerHandleVerificationTask(
    TaskTracker* tracker,
    Task verify_task,
    TaskTraits traits,
    scoped_refptr<TaskRunner> task_runner,
    TaskSourceExecutionMode execution_mode) {
  // Pretend |verify_task| is posted to respect TaskTracker's contract.
  EXPECT_TRUE(tracker->WillPostTask(&verify_task, traits.shutdown_behavior()));

  // Confirm that the test conditions are right (no TaskRunnerHandles set
  // already).
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
  EXPECT_FALSE(SequencedTaskRunnerHandle::IsSet());

  auto sequence = test::CreateSequenceWithTask(
      std::move(verify_task), traits, std::move(task_runner), execution_mode);

  tracker->RunAndPopNextTask(std::move(sequence));

  // TaskRunnerHandle state is reset outside of task's scope.
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
  EXPECT_FALSE(SequencedTaskRunnerHandle::IsSet());
}

static void VerifyNoTaskRunnerHandle() {
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
  EXPECT_FALSE(SequencedTaskRunnerHandle::IsSet());
}

TEST_P(ThreadPoolTaskTrackerTest, TaskRunnerHandleIsNotSetOnParallel) {
  // Create a task that will verify that TaskRunnerHandles are not set in its
  // scope per no TaskRunner ref being set to it.
  Task verify_task(FROM_HERE, BindOnce(&VerifyNoTaskRunnerHandle), TimeDelta());

  RunTaskRunnerHandleVerificationTask(&tracker_, std::move(verify_task),
                                      TaskTraits(GetParam()), nullptr,
                                      TaskSourceExecutionMode::kParallel);
}

static void VerifySequencedTaskRunnerHandle(
    const SequencedTaskRunner* expected_task_runner) {
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
  EXPECT_TRUE(SequencedTaskRunnerHandle::IsSet());
  EXPECT_EQ(expected_task_runner, SequencedTaskRunnerHandle::Get());
}

TEST_P(ThreadPoolTaskTrackerTest, SequencedTaskRunnerHandleIsSetOnSequenced) {
  scoped_refptr<SequencedTaskRunner> test_task_runner(new TestSimpleTaskRunner);

  // Create a task that will verify that SequencedTaskRunnerHandle is properly
  // set to |test_task_runner| in its scope per |sequenced_task_runner_ref|
  // being set to it.
  Task verify_task(FROM_HERE,
                   BindOnce(&VerifySequencedTaskRunnerHandle,
                            Unretained(test_task_runner.get())),
                   TimeDelta());

  RunTaskRunnerHandleVerificationTask(
      &tracker_, std::move(verify_task), TaskTraits(GetParam()),
      std::move(test_task_runner), TaskSourceExecutionMode::kSequenced);
}

static void VerifyThreadTaskRunnerHandle(
    const SingleThreadTaskRunner* expected_task_runner) {
  EXPECT_TRUE(ThreadTaskRunnerHandle::IsSet());
  // SequencedTaskRunnerHandle inherits ThreadTaskRunnerHandle for thread.
  EXPECT_TRUE(SequencedTaskRunnerHandle::IsSet());
  EXPECT_EQ(expected_task_runner, ThreadTaskRunnerHandle::Get());
}

TEST_P(ThreadPoolTaskTrackerTest, ThreadTaskRunnerHandleIsSetOnSingleThreaded) {
  scoped_refptr<SingleThreadTaskRunner> test_task_runner(
      new TestSimpleTaskRunner);

  // Create a task that will verify that ThreadTaskRunnerHandle is properly set
  // to |test_task_runner| in its scope per |single_thread_task_runner_ref|
  // being set on it.
  Task verify_task(FROM_HERE,
                   BindOnce(&VerifyThreadTaskRunnerHandle,
                            Unretained(test_task_runner.get())),
                   TimeDelta());

  RunTaskRunnerHandleVerificationTask(
      &tracker_, std::move(verify_task), TaskTraits(GetParam()),
      std::move(test_task_runner), TaskSourceExecutionMode::kSingleThread);
}

TEST_P(ThreadPoolTaskTrackerTest, FlushPendingDelayedTask) {
  Task delayed_task(FROM_HERE, DoNothing(), TimeDelta::FromDays(1));
  tracker_.WillPostTask(&delayed_task, GetParam());
  // FlushForTesting() should return even if the delayed task didn't run.
  tracker_.FlushForTesting();
}

TEST_P(ThreadPoolTaskTrackerTest, FlushAsyncForTestingPendingDelayedTask) {
  Task delayed_task(FROM_HERE, DoNothing(), TimeDelta::FromDays(1));
  tracker_.WillPostTask(&delayed_task, GetParam());
  // FlushAsyncForTesting() should callback even if the delayed task didn't run.
  bool called_back = false;
  tracker_.FlushAsyncForTesting(
      BindOnce([](bool* called_back) { *called_back = true; },
               Unretained(&called_back)));
  EXPECT_TRUE(called_back);
}

TEST_P(ThreadPoolTaskTrackerTest, FlushPendingUndelayedTask) {
  Task undelayed_task(FROM_HERE, DoNothing(), TimeDelta());
  tracker_.WillPostTask(&undelayed_task, GetParam());

  // FlushForTesting() shouldn't return before the undelayed task runs.
  CallFlushFromAnotherThread();
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  VERIFY_ASYNC_FLUSH_IN_PROGRESS();

  // FlushForTesting() should return after the undelayed task runs.
  DispatchAndRunTaskWithTracker(std::move(undelayed_task), GetParam());
  WAIT_FOR_ASYNC_FLUSH_RETURNED();
}

TEST_P(ThreadPoolTaskTrackerTest, FlushAsyncForTestingPendingUndelayedTask) {
  Task undelayed_task(FROM_HERE, DoNothing(), TimeDelta());
  tracker_.WillPostTask(&undelayed_task, GetParam());

  // FlushAsyncForTesting() shouldn't callback before the undelayed task runs.
  WaitableEvent event;
  tracker_.FlushAsyncForTesting(
      BindOnce(&WaitableEvent::Signal, Unretained(&event)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(event.IsSignaled());

  // FlushAsyncForTesting() should callback after the undelayed task runs.
  DispatchAndRunTaskWithTracker(std::move(undelayed_task), GetParam());
  event.Wait();
}

TEST_P(ThreadPoolTaskTrackerTest, PostTaskDuringFlush) {
  Task undelayed_task(FROM_HERE, DoNothing(), TimeDelta());
  tracker_.WillPostTask(&undelayed_task, GetParam());

  // FlushForTesting() shouldn't return before the undelayed task runs.
  CallFlushFromAnotherThread();
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  VERIFY_ASYNC_FLUSH_IN_PROGRESS();

  // Simulate posting another undelayed task.
  Task other_undelayed_task(FROM_HERE, DoNothing(), TimeDelta());
  tracker_.WillPostTask(&other_undelayed_task, GetParam());

  // Run the first undelayed task.
  DispatchAndRunTaskWithTracker(std::move(undelayed_task), GetParam());

  // FlushForTesting() shouldn't return before the second undelayed task runs.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  VERIFY_ASYNC_FLUSH_IN_PROGRESS();

  // FlushForTesting() should return after the second undelayed task runs.
  DispatchAndRunTaskWithTracker(std::move(other_undelayed_task), GetParam());
  WAIT_FOR_ASYNC_FLUSH_RETURNED();
}

TEST_P(ThreadPoolTaskTrackerTest, PostTaskDuringFlushAsyncForTesting) {
  Task undelayed_task(FROM_HERE, DoNothing(), TimeDelta());
  tracker_.WillPostTask(&undelayed_task, GetParam());

  // FlushAsyncForTesting() shouldn't callback before the undelayed task runs.
  WaitableEvent event;
  tracker_.FlushAsyncForTesting(
      BindOnce(&WaitableEvent::Signal, Unretained(&event)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(event.IsSignaled());

  // Simulate posting another undelayed task.
  Task other_undelayed_task(FROM_HERE, DoNothing(), TimeDelta());
  tracker_.WillPostTask(&other_undelayed_task, GetParam());

  // Run the first undelayed task.
  DispatchAndRunTaskWithTracker(std::move(undelayed_task), GetParam());

  // FlushAsyncForTesting() shouldn't callback before the second undelayed task
  // runs.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(event.IsSignaled());

  // FlushAsyncForTesting() should callback after the second undelayed task
  // runs.
  DispatchAndRunTaskWithTracker(std::move(other_undelayed_task), GetParam());
  event.Wait();
}

TEST_P(ThreadPoolTaskTrackerTest, RunDelayedTaskDuringFlush) {
  // Simulate posting a delayed and an undelayed task.
  Task delayed_task(FROM_HERE, DoNothing(), TimeDelta::FromDays(1));
  tracker_.WillPostTask(&delayed_task, GetParam());
  Task undelayed_task(FROM_HERE, DoNothing(), TimeDelta());
  tracker_.WillPostTask(&undelayed_task, GetParam());

  // FlushForTesting() shouldn't return before the undelayed task runs.
  CallFlushFromAnotherThread();
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  VERIFY_ASYNC_FLUSH_IN_PROGRESS();

  // Run the delayed task.
  DispatchAndRunTaskWithTracker(std::move(delayed_task), GetParam());

  // FlushForTesting() shouldn't return since there is still a pending undelayed
  // task.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  VERIFY_ASYNC_FLUSH_IN_PROGRESS();

  // Run the undelayed task.
  DispatchAndRunTaskWithTracker(std::move(undelayed_task), GetParam());

  // FlushForTesting() should now return.
  WAIT_FOR_ASYNC_FLUSH_RETURNED();
}

TEST_P(ThreadPoolTaskTrackerTest, RunDelayedTaskDuringFlushAsyncForTesting) {
  // Simulate posting a delayed and an undelayed task.
  Task delayed_task(FROM_HERE, DoNothing(), TimeDelta::FromDays(1));
  tracker_.WillPostTask(&delayed_task, GetParam());
  Task undelayed_task(FROM_HERE, DoNothing(), TimeDelta());
  tracker_.WillPostTask(&undelayed_task, GetParam());

  // FlushAsyncForTesting() shouldn't callback before the undelayed task runs.
  WaitableEvent event;
  tracker_.FlushAsyncForTesting(
      BindOnce(&WaitableEvent::Signal, Unretained(&event)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(event.IsSignaled());

  // Run the delayed task.
  DispatchAndRunTaskWithTracker(std::move(delayed_task), GetParam());

  // FlushAsyncForTesting() shouldn't callback since there is still a pending
  // undelayed task.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(event.IsSignaled());

  // Run the undelayed task.
  DispatchAndRunTaskWithTracker(std::move(undelayed_task), GetParam());

  // FlushAsyncForTesting() should now callback.
  event.Wait();
}

TEST_P(ThreadPoolTaskTrackerTest, FlushAfterShutdown) {
  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN)
    return;

  // Simulate posting a task.
  Task undelayed_task(FROM_HERE, DoNothing(), TimeDelta());
  tracker_.WillPostTask(&undelayed_task, GetParam());

  // Shutdown() should return immediately since there are no pending
  // BLOCK_SHUTDOWN tasks.
  test::ShutdownTaskTracker(&tracker_);

  // FlushForTesting() should return immediately after shutdown, even if an
  // undelayed task hasn't run.
  tracker_.FlushForTesting();
}

TEST_P(ThreadPoolTaskTrackerTest, FlushAfterShutdownAsync) {
  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN)
    return;

  // Simulate posting a task.
  Task undelayed_task(FROM_HERE, DoNothing(), TimeDelta());
  tracker_.WillPostTask(&undelayed_task, GetParam());

  // Shutdown() should return immediately since there are no pending
  // BLOCK_SHUTDOWN tasks.
  test::ShutdownTaskTracker(&tracker_);

  // FlushForTesting() should callback immediately after shutdown, even if an
  // undelayed task hasn't run.
  bool called_back = false;
  tracker_.FlushAsyncForTesting(
      BindOnce([](bool* called_back) { *called_back = true; },
               Unretained(&called_back)));
  EXPECT_TRUE(called_back);
}

TEST_P(ThreadPoolTaskTrackerTest, ShutdownDuringFlush) {
  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN)
    return;

  // Simulate posting a task.
  Task undelayed_task(FROM_HERE, DoNothing(), TimeDelta());
  tracker_.WillPostTask(&undelayed_task, GetParam());

  // FlushForTesting() shouldn't return before the undelayed task runs or
  // shutdown completes.
  CallFlushFromAnotherThread();
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  VERIFY_ASYNC_FLUSH_IN_PROGRESS();

  // Shutdown() should return immediately since there are no pending
  // BLOCK_SHUTDOWN tasks.
  test::ShutdownTaskTracker(&tracker_);

  // FlushForTesting() should now return, even if an undelayed task hasn't run.
  WAIT_FOR_ASYNC_FLUSH_RETURNED();
}

TEST_P(ThreadPoolTaskTrackerTest, ShutdownDuringFlushAsyncForTesting) {
  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN)
    return;

  // Simulate posting a task.
  Task undelayed_task(FROM_HERE, DoNothing(), TimeDelta());
  tracker_.WillPostTask(&undelayed_task, GetParam());

  // FlushAsyncForTesting() shouldn't callback before the undelayed task runs or
  // shutdown completes.
  WaitableEvent event;
  tracker_.FlushAsyncForTesting(
      BindOnce(&WaitableEvent::Signal, Unretained(&event)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(event.IsSignaled());

  // Shutdown() should return immediately since there are no pending
  // BLOCK_SHUTDOWN tasks.
  test::ShutdownTaskTracker(&tracker_);

  // FlushAsyncForTesting() should now callback, even if an undelayed task
  // hasn't run.
  event.Wait();
}

TEST_P(ThreadPoolTaskTrackerTest, DoublePendingFlushAsyncForTestingFails) {
  Task undelayed_task(FROM_HERE, DoNothing(), TimeDelta());
  tracker_.WillPostTask(&undelayed_task, GetParam());

  // FlushAsyncForTesting() shouldn't callback before the undelayed task runs.
  bool called_back = false;
  tracker_.FlushAsyncForTesting(
      BindOnce([](bool* called_back) { *called_back = true; },
               Unretained(&called_back)));
  EXPECT_FALSE(called_back);
  EXPECT_DCHECK_DEATH({ tracker_.FlushAsyncForTesting(BindOnce([]() {})); });
}

// Verify that a delayed task does not block shutdown, regardless of its
// shutdown behavior.
TEST_P(ThreadPoolTaskTrackerTest, DelayedTasksDoNotBlockShutdown) {
  // Simulate posting a delayed task.
  Task delayed_task(FROM_HERE, DoNothing(), TimeDelta::FromDays(1));
  EXPECT_TRUE(tracker_.WillPostTask(&delayed_task, GetParam()));

  // Since the delayed task doesn't block shutdown, a call to Shutdown() should
  // not hang.
  test::ShutdownTaskTracker(&tracker_);
}

INSTANTIATE_TEST_SUITE_P(
    ContinueOnShutdown,
    ThreadPoolTaskTrackerTest,
    ::testing::Values(TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN));
INSTANTIATE_TEST_SUITE_P(
    SkipOnShutdown,
    ThreadPoolTaskTrackerTest,
    ::testing::Values(TaskShutdownBehavior::SKIP_ON_SHUTDOWN));
INSTANTIATE_TEST_SUITE_P(
    BlockShutdown,
    ThreadPoolTaskTrackerTest,
    ::testing::Values(TaskShutdownBehavior::BLOCK_SHUTDOWN));

namespace {

void ExpectSequenceToken(SequenceToken sequence_token) {
  EXPECT_EQ(sequence_token, SequenceToken::GetForCurrentThread());
}

}  // namespace

// Verify that SequenceToken::GetForCurrentThread() returns the Sequence's token
// when a Task runs.
TEST_F(ThreadPoolTaskTrackerTest, CurrentSequenceToken) {
  scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(
      TaskTraits(), nullptr, TaskSourceExecutionMode::kParallel);

  const SequenceToken sequence_token = sequence->token();
  Task task(FROM_HERE, Bind(&ExpectSequenceToken, sequence_token), TimeDelta());
  tracker_.WillPostTask(&task, sequence->shutdown_behavior());

  {
    Sequence::Transaction sequence_transaction(sequence->BeginTransaction());
    sequence_transaction.PushTask(std::move(task));

    EXPECT_FALSE(SequenceToken::GetForCurrentThread().IsValid());
  }

  tracker_.RunAndPopNextTask(std::move(sequence));
  EXPECT_FALSE(SequenceToken::GetForCurrentThread().IsValid());
}

TEST_F(ThreadPoolTaskTrackerTest, LoadWillPostAndRunBeforeShutdown) {
  // Post and run tasks asynchronously.
  std::vector<std::unique_ptr<ThreadPostingAndRunningTask>> threads;

  for (size_t i = 0; i < kLoadTestNumIterations; ++i) {
    threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_, CreateTask(),
        TaskTraits(TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN),
        ThreadPostingAndRunningTask::Action::WILL_POST_AND_RUN, true));
    threads.back()->Start();

    threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_, CreateTask(),
        TaskTraits(TaskShutdownBehavior::SKIP_ON_SHUTDOWN),
        ThreadPostingAndRunningTask::Action::WILL_POST_AND_RUN, true));
    threads.back()->Start();

    threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_, CreateTask(),
        TaskTraits(TaskShutdownBehavior::BLOCK_SHUTDOWN),
        ThreadPostingAndRunningTask::Action::WILL_POST_AND_RUN, true));
    threads.back()->Start();
  }

  for (const auto& thread : threads)
    thread->Join();

  // Expect all tasks to be executed.
  EXPECT_EQ(kLoadTestNumIterations * 3, NumTasksExecuted());

  // Should return immediately because no tasks are blocking shutdown.
  test::ShutdownTaskTracker(&tracker_);
}

TEST_F(ThreadPoolTaskTrackerTest,
       LoadWillPostBeforeShutdownAndRunDuringShutdown) {
  // Post tasks asynchronously.
  std::vector<Task> tasks_continue_on_shutdown;
  std::vector<Task> tasks_skip_on_shutdown;
  std::vector<Task> tasks_block_shutdown;
  for (size_t i = 0; i < kLoadTestNumIterations; ++i) {
    tasks_continue_on_shutdown.push_back(CreateTask());
    tasks_skip_on_shutdown.push_back(CreateTask());
    tasks_block_shutdown.push_back(CreateTask());
  }

  TaskTraits traits_continue_on_shutdown =
      TaskTraits(TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN);
  TaskTraits traits_skip_on_shutdown =
      TaskTraits(TaskShutdownBehavior::SKIP_ON_SHUTDOWN);
  TaskTraits traits_block_shutdown =
      TaskTraits(TaskShutdownBehavior::BLOCK_SHUTDOWN);

  std::vector<std::unique_ptr<ThreadPostingAndRunningTask>> post_threads;
  for (size_t i = 0; i < kLoadTestNumIterations; ++i) {
    post_threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_, &tasks_continue_on_shutdown[i], traits_continue_on_shutdown,
        ThreadPostingAndRunningTask::Action::WILL_POST, true));
    post_threads.back()->Start();

    post_threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_, &tasks_skip_on_shutdown[i], traits_skip_on_shutdown,
        ThreadPostingAndRunningTask::Action::WILL_POST, true));
    post_threads.back()->Start();

    post_threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_, &tasks_block_shutdown[i], traits_block_shutdown,
        ThreadPostingAndRunningTask::Action::WILL_POST, true));
    post_threads.back()->Start();
  }

  for (const auto& thread : post_threads)
    thread->Join();

  // Start shutdown and try to complete shutdown asynchronously.
  tracker_.StartShutdown();
  ExpectAsyncCompleteShutdownBlocks();

  // Run tasks asynchronously.
  std::vector<std::unique_ptr<ThreadPostingAndRunningTask>> run_threads;
  for (size_t i = 0; i < kLoadTestNumIterations; ++i) {
    run_threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_, std::move(tasks_continue_on_shutdown[i]),
        traits_continue_on_shutdown, ThreadPostingAndRunningTask::Action::RUN,
        false));
    run_threads.back()->Start();

    run_threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_, std::move(tasks_skip_on_shutdown[i]),
        traits_skip_on_shutdown, ThreadPostingAndRunningTask::Action::RUN,
        false));
    run_threads.back()->Start();

    run_threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_, std::move(tasks_block_shutdown[i]), traits_block_shutdown,
        ThreadPostingAndRunningTask::Action::RUN, false));
    run_threads.back()->Start();
  }

  for (const auto& thread : run_threads)
    thread->Join();

  WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();

  // Expect BLOCK_SHUTDOWN tasks to have been executed.
  EXPECT_EQ(kLoadTestNumIterations, NumTasksExecuted());
}

TEST_F(ThreadPoolTaskTrackerTest, LoadWillPostAndRunDuringShutdown) {
  // Inform |task_tracker_| that a BLOCK_SHUTDOWN task will be posted just to
  // block shutdown.
  Task block_shutdown_task(CreateTask());
  EXPECT_TRUE(tracker_.WillPostTask(&block_shutdown_task,
                                    TaskShutdownBehavior::BLOCK_SHUTDOWN));

  // Start shutdown and try to complete it asynchronously.
  tracker_.StartShutdown();
  ExpectAsyncCompleteShutdownBlocks();

  // Post and run tasks asynchronously.
  std::vector<std::unique_ptr<ThreadPostingAndRunningTask>> threads;

  for (size_t i = 0; i < kLoadTestNumIterations; ++i) {
    threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_, CreateTask(),
        TaskTraits(TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN),
        ThreadPostingAndRunningTask::Action::WILL_POST_AND_RUN, false));
    threads.back()->Start();

    threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_, CreateTask(),
        TaskTraits(TaskShutdownBehavior::SKIP_ON_SHUTDOWN),
        ThreadPostingAndRunningTask::Action::WILL_POST_AND_RUN, false));
    threads.back()->Start();

    threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_, CreateTask(),
        TaskTraits(TaskShutdownBehavior::BLOCK_SHUTDOWN),
        ThreadPostingAndRunningTask::Action::WILL_POST_AND_RUN, true));
    threads.back()->Start();
  }

  for (const auto& thread : threads)
    thread->Join();

  // Expect BLOCK_SHUTDOWN tasks to have been executed.
  EXPECT_EQ(kLoadTestNumIterations, NumTasksExecuted());

  // Shutdown() shouldn't return before |block_shutdown_task| is executed.
  VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS();

  // Unblock shutdown by running |block_shutdown_task|.
  DispatchAndRunTaskWithTracker(
      std::move(block_shutdown_task),
      TaskTraits(TaskShutdownBehavior::BLOCK_SHUTDOWN));
  EXPECT_EQ(kLoadTestNumIterations + 1, NumTasksExecuted());
  WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();
}

// Verify that RunAndPopNextTask() returns the sequence from which it ran a task
// when it can be rescheduled.
TEST_F(ThreadPoolTaskTrackerTest,
       RunAndPopNextTaskReturnsSequenceToReschedule) {
  TaskTraits default_traits = {};
  Task task_1(FROM_HERE, DoNothing(), TimeDelta());
  EXPECT_TRUE(
      tracker_.WillPostTask(&task_1, default_traits.shutdown_behavior()));
  Task task_2(FROM_HERE, DoNothing(), TimeDelta());
  EXPECT_TRUE(
      tracker_.WillPostTask(&task_2, default_traits.shutdown_behavior()));

  scoped_refptr<Sequence> sequence =
      test::CreateSequenceWithTask(std::move(task_1), default_traits);
  sequence->BeginTransaction().PushTask(std::move(task_2));
  EXPECT_EQ(sequence, tracker_.RunAndPopNextTask(sequence));
}

namespace {

class WaitAllowedTestThread : public SimpleThread {
 public:
  WaitAllowedTestThread() : SimpleThread("WaitAllowedTestThread") {}

 private:
  void Run() override {
    auto task_tracker = std::make_unique<TaskTracker>("Test");

    // Waiting is allowed by default. Expect TaskTracker to disallow it before
    // running a task without the WithBaseSyncPrimitives() trait.
    internal::AssertBaseSyncPrimitivesAllowed();
    Task task_without_sync_primitives(
        FROM_HERE, Bind([]() {
          EXPECT_DCHECK_DEATH({ internal::AssertBaseSyncPrimitivesAllowed(); });
        }),
        TimeDelta());
    TaskTraits default_traits = {};
    EXPECT_TRUE(task_tracker->WillPostTask(&task_without_sync_primitives,
                                           default_traits.shutdown_behavior()));
    auto sequence_without_sync_primitives = test::CreateSequenceWithTask(
        std::move(task_without_sync_primitives), default_traits);
    task_tracker->RunAndPopNextTask(
        std::move(sequence_without_sync_primitives));

    // Disallow waiting. Expect TaskTracker to allow it before running a task
    // with the WithBaseSyncPrimitives() trait.
    ThreadRestrictions::DisallowWaiting();
    Task task_with_sync_primitives(
        FROM_HERE, Bind([]() {
          // Shouldn't fail.
          internal::AssertBaseSyncPrimitivesAllowed();
        }),
        TimeDelta());
    TaskTraits traits_with_sync_primitives =
        TaskTraits(WithBaseSyncPrimitives());
    EXPECT_TRUE(task_tracker->WillPostTask(
        &task_with_sync_primitives,
        traits_with_sync_primitives.shutdown_behavior()));
    auto sequence_with_sync_primitives = test::CreateSequenceWithTask(
        std::move(task_with_sync_primitives), traits_with_sync_primitives);
    task_tracker->RunAndPopNextTask(std::move(sequence_with_sync_primitives));

    ScopedAllowBaseSyncPrimitivesForTesting
        allow_wait_in_task_tracker_destructor;
    task_tracker.reset();
  }

  DISALLOW_COPY_AND_ASSIGN(WaitAllowedTestThread);
};

}  // namespace

// Verify that AssertIOAllowed() succeeds only for a WithBaseSyncPrimitives()
// task.
TEST(ThreadPoolTaskTrackerWaitAllowedTest, WaitAllowed) {
  // Run the test on the separate thread since it is not possible to reset the
  // "wait allowed" bit of a thread without being a friend of
  // ThreadRestrictions.
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  WaitAllowedTestThread wait_allowed_test_thread;
  wait_allowed_test_thread.Start();
  wait_allowed_test_thread.Join();
}

// Verify that ThreadPool.TaskLatency.* histograms are correctly recorded
// when a task runs.
TEST(ThreadPoolTaskTrackerHistogramTest, TaskLatency) {
  auto statistics_recorder = StatisticsRecorder::CreateTemporaryForTesting();

  TaskTracker tracker("Test");

  struct {
    const TaskTraits traits;
    const char* const expected_histogram;
  } static constexpr kTests[] = {
      {{TaskPriority::BEST_EFFORT},
       "ThreadPool.TaskLatencyMicroseconds.Test."
       "BackgroundTaskPriority"},
      {{MayBlock(), TaskPriority::BEST_EFFORT},
       "ThreadPool.TaskLatencyMicroseconds.Test."
       "BackgroundTaskPriority_MayBlock"},
      {{WithBaseSyncPrimitives(), TaskPriority::BEST_EFFORT},
       "ThreadPool.TaskLatencyMicroseconds.Test."
       "BackgroundTaskPriority_MayBlock"},
      {{TaskPriority::USER_VISIBLE},
       "ThreadPool.TaskLatencyMicroseconds.Test."
       "UserVisibleTaskPriority"},
      {{MayBlock(), TaskPriority::USER_VISIBLE},
       "ThreadPool.TaskLatencyMicroseconds.Test."
       "UserVisibleTaskPriority_MayBlock"},
      {{WithBaseSyncPrimitives(), TaskPriority::USER_VISIBLE},
       "ThreadPool.TaskLatencyMicroseconds.Test."
       "UserVisibleTaskPriority_MayBlock"},
      {{TaskPriority::USER_BLOCKING},
       "ThreadPool.TaskLatencyMicroseconds.Test."
       "UserBlockingTaskPriority"},
      {{MayBlock(), TaskPriority::USER_BLOCKING},
       "ThreadPool.TaskLatencyMicroseconds.Test."
       "UserBlockingTaskPriority_MayBlock"},
      {{WithBaseSyncPrimitives(), TaskPriority::USER_BLOCKING},
       "ThreadPool.TaskLatencyMicroseconds.Test."
       "UserBlockingTaskPriority_MayBlock"}};

  for (const auto& test : kTests) {
    Task task(FROM_HERE, DoNothing(), TimeDelta());
    ASSERT_TRUE(tracker.WillPostTask(&task, test.traits.shutdown_behavior()));

    HistogramTester tester;

    tracker.RunAndPopNextTask(
        test::CreateSequenceWithTask(std::move(task), test.traits));
    tester.ExpectTotalCount(test.expected_histogram, 1);
  }
}

}  // namespace internal
}  // namespace base
