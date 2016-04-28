// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/scheduler_worker_thread.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/task_scheduler/scheduler_lock.h"
#include "base/task_scheduler/sequence.h"
#include "base/task_scheduler/task.h"
#include "base/task_scheduler/task_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {
namespace {

const size_t kNumSequencesPerTest = 150;

// The test parameter is the number of Tasks per Sequence returned by GetWork().
class TaskSchedulerWorkerThreadTest : public testing::TestWithParam<size_t> {
 protected:
  TaskSchedulerWorkerThreadTest()
      : main_entry_called_(true, false),
        num_get_work_cv_(lock_.CreateConditionVariable()),
        worker_thread_set_(true, false) {}

  void SetUp() override {
    worker_thread_ = SchedulerWorkerThread::Create(
        ThreadPriority::NORMAL,
        WrapUnique(new TestSchedulerWorkerThreadDelegate(this)),
        &task_tracker_);
    ASSERT_TRUE(worker_thread_);
    worker_thread_set_.Signal();
    main_entry_called_.Wait();
  }

  void TearDown() override {
    worker_thread_->JoinForTesting();
  }

  size_t TasksPerSequence() const { return GetParam(); }

  // Wait until GetWork() has been called |num_get_work| times.
  void WaitForNumGetWork(size_t num_get_work) {
    AutoSchedulerLock auto_lock(lock_);
    while (num_get_work_ < num_get_work)
      num_get_work_cv_->Wait();
  }

  void SetMaxGetWork(size_t max_get_work) {
    AutoSchedulerLock auto_lock(lock_);
    max_get_work_ = max_get_work;
  }

  void SetNumSequencesToCreate(size_t num_sequences_to_create) {
    AutoSchedulerLock auto_lock(lock_);
    EXPECT_EQ(0U, num_sequences_to_create_);
    num_sequences_to_create_ = num_sequences_to_create;
  }

  size_t NumRunTasks() {
    AutoSchedulerLock auto_lock(lock_);
    return num_run_tasks_;
  }

  std::vector<scoped_refptr<Sequence>> CreatedSequences() {
    AutoSchedulerLock auto_lock(lock_);
    return created_sequences_;
  }

  std::vector<scoped_refptr<Sequence>> EnqueuedSequences() {
    AutoSchedulerLock auto_lock(lock_);
    return re_enqueued_sequences_;
  }

  std::unique_ptr<SchedulerWorkerThread> worker_thread_;

 private:
  class TestSchedulerWorkerThreadDelegate
      : public SchedulerWorkerThread::Delegate {
   public:
    TestSchedulerWorkerThreadDelegate(TaskSchedulerWorkerThreadTest* outer)
        : outer_(outer) {}

    // SchedulerWorkerThread::Delegate:
    void OnMainEntry(SchedulerWorkerThread* worker_thread) override {
      outer_->worker_thread_set_.Wait();
      EXPECT_EQ(outer_->worker_thread_.get(), worker_thread);

      // Without synchronization, OnMainEntry() could be called twice without
      // generating an error.
      AutoSchedulerLock auto_lock(outer_->lock_);
      EXPECT_FALSE(outer_->main_entry_called_.IsSignaled());
      outer_->main_entry_called_.Signal();
    }

    scoped_refptr<Sequence> GetWork(
        SchedulerWorkerThread* worker_thread) override {
      EXPECT_EQ(outer_->worker_thread_.get(), worker_thread);

      {
        AutoSchedulerLock auto_lock(outer_->lock_);

        // Increment the number of times that this method has been called.
        ++outer_->num_get_work_;
        outer_->num_get_work_cv_->Signal();

        // Verify that this method isn't called more times than expected.
        EXPECT_LE(outer_->num_get_work_, outer_->max_get_work_);

        // Check if a Sequence should be returned.
        if (outer_->num_sequences_to_create_ == 0)
          return nullptr;
        --outer_->num_sequences_to_create_;
      }

      // Create a Sequence with TasksPerSequence() Tasks.
      scoped_refptr<Sequence> sequence(new Sequence);
      for (size_t i = 0; i < outer_->TasksPerSequence(); ++i) {
        std::unique_ptr<Task> task(new Task(
            FROM_HERE, Bind(&TaskSchedulerWorkerThreadTest::RunTaskCallback,
                            Unretained(outer_)),
            TaskTraits(), TimeDelta()));
        EXPECT_TRUE(outer_->task_tracker_.WillPostTask(task.get()));
        sequence->PushTask(std::move(task));
      }

      {
        // Add the Sequence to the vector of created Sequences.
        AutoSchedulerLock auto_lock(outer_->lock_);
        outer_->created_sequences_.push_back(sequence);
      }

      return sequence;
    }

    // This override verifies that |sequence| contains the expected number of
    // Tasks and adds it to |enqueued_sequences_|. Unlike a normal
    // EnqueueSequence implementation, it doesn't reinsert |sequence| into a
    // queue for further execution.
    void ReEnqueueSequence(scoped_refptr<Sequence> sequence) override {
      EXPECT_GT(outer_->TasksPerSequence(), 1U);

      // Verify that |sequence| contains TasksPerSequence() - 1 Tasks.
      for (size_t i = 0; i < outer_->TasksPerSequence() - 1; ++i) {
        EXPECT_TRUE(sequence->PeekTask());
        sequence->PopTask();
      }
      EXPECT_FALSE(sequence->PeekTask());

      // Add |sequence| to |re_enqueued_sequences_|.
      AutoSchedulerLock auto_lock(outer_->lock_);
      outer_->re_enqueued_sequences_.push_back(std::move(sequence));
      EXPECT_LE(outer_->re_enqueued_sequences_.size(),
                outer_->created_sequences_.size());
    }

   private:
    TaskSchedulerWorkerThreadTest* outer_;
  };

  void RunTaskCallback() {
    AutoSchedulerLock auto_lock(lock_);
    ++num_run_tasks_;
    EXPECT_LE(num_run_tasks_, created_sequences_.size());
  }

  TaskTracker task_tracker_;

  // Synchronizes access to all members below.
  mutable SchedulerLock lock_;

  // Signaled once OnMainEntry() has been called.
  WaitableEvent main_entry_called_;

  // Number of Sequences that should be created by GetWork(). When this
  // is 0, GetWork() returns nullptr.
  size_t num_sequences_to_create_ = 0;

  // Number of times that GetWork() has been called.
  size_t num_get_work_ = 0;

  // Maximum number of times that GetWork() can be called.
  size_t max_get_work_ = 0;

  // Condition variable signaled when |num_get_work_| is incremented.
  std::unique_ptr<ConditionVariable> num_get_work_cv_;

  // Sequences created by GetWork().
  std::vector<scoped_refptr<Sequence>> created_sequences_;

  // Sequences passed to EnqueueSequence().
  std::vector<scoped_refptr<Sequence>> re_enqueued_sequences_;

  // Number of times that RunTaskCallback() has been called.
  size_t num_run_tasks_ = 0;

  // Signaled after |worker_thread_| is set.
  WaitableEvent worker_thread_set_;

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerWorkerThreadTest);
};

// Verify that when GetWork() continuously returns Sequences, all Tasks in these
// Sequences run successfully. The test wakes up the SchedulerWorkerThread once.
TEST_P(TaskSchedulerWorkerThreadTest, ContinuousWork) {
  // Set GetWork() to return |kNumSequencesPerTest| Sequences before starting to
  // return nullptr.
  SetNumSequencesToCreate(kNumSequencesPerTest);

  // Expect |kNumSequencesPerTest| calls to GetWork() in which it returns a
  // Sequence and one call in which its returns nullptr.
  const size_t kExpectedNumGetWork = kNumSequencesPerTest + 1;
  SetMaxGetWork(kExpectedNumGetWork);

  // Wake up |worker_thread_| and wait until GetWork() has been invoked the
  // expected amount of times.
  worker_thread_->WakeUp();
  WaitForNumGetWork(kExpectedNumGetWork);

  // All tasks should have run.
  EXPECT_EQ(kNumSequencesPerTest, NumRunTasks());

  // If Sequences returned by GetWork() contain more than one Task, they aren't
  // empty after the worker thread pops Tasks from them and thus should be
  // returned to EnqueueSequence().
  if (TasksPerSequence() > 1)
    EXPECT_EQ(CreatedSequences(), EnqueuedSequences());
  else
    EXPECT_TRUE(EnqueuedSequences().empty());
}

// Verify that when GetWork() alternates between returning a Sequence and
// returning nullptr, all Tasks in the returned Sequences run successfully. The
// test wakes up the SchedulerWorkerThread once for each Sequence.
TEST_P(TaskSchedulerWorkerThreadTest, IntermittentWork) {
  for (size_t i = 0; i < kNumSequencesPerTest; ++i) {
    // Set GetWork() to return 1 Sequence before starting to return
    // nullptr.
    SetNumSequencesToCreate(1);

    // Expect |i + 1| calls to GetWork() in which it returns a Sequence and
    // |i + 1| calls in which it returns nullptr.
    const size_t expected_num_get_work = 2 * (i + 1);
    SetMaxGetWork(expected_num_get_work);

    // Wake up |worker_thread_| and wait until GetWork() has been invoked
    // the expected amount of times.
    worker_thread_->WakeUp();
    WaitForNumGetWork(expected_num_get_work);

    // The Task should have run
    EXPECT_EQ(i + 1, NumRunTasks());

    // If Sequences returned by GetWork() contain more than one Task, they
    // aren't empty after the worker thread pops Tasks from them and thus should
    // be returned to EnqueueSequence().
    if (TasksPerSequence() > 1)
      EXPECT_EQ(CreatedSequences(), EnqueuedSequences());
    else
      EXPECT_TRUE(EnqueuedSequences().empty());
  }
}

INSTANTIATE_TEST_CASE_P(OneTaskPerSequence,
                        TaskSchedulerWorkerThreadTest,
                        ::testing::Values(1));
INSTANTIATE_TEST_CASE_P(TwoTasksPerSequence,
                        TaskSchedulerWorkerThreadTest,
                        ::testing::Values(2));

}  // namespace
}  // namespace internal
}  // namespace base
