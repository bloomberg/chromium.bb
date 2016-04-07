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

class TaskSchedulerWorkerThreadTest : public testing::Test,
                                      public SchedulerWorkerThread::Delegate {
 protected:
  TaskSchedulerWorkerThreadTest()
      : main_entry_called_(true, false),
        num_get_work_cv_(lock_.CreateConditionVariable()),
        run_sequences_cv_(lock_.CreateConditionVariable()) {}

  void SetUp() override {
    worker_thread_ = SchedulerWorkerThread::CreateSchedulerWorkerThread(
        ThreadPriority::NORMAL, this, &task_tracker_);
    ASSERT_TRUE(worker_thread_);
    main_entry_called_.Wait();
  }

  void TearDown() override {
    {
      AutoSchedulerLock auto_lock(lock_);
      EXPECT_FALSE(main_exit_called_);
    }

    worker_thread_->JoinForTesting();

    AutoSchedulerLock auto_lock(lock_);
    EXPECT_TRUE(main_exit_called_);
  }

  // Wait until GetWork() has been called |num_get_work| times.
  void WaitForNumGetWork(size_t num_get_work) {
    AutoSchedulerLock auto_lock(lock_);
    while (num_get_work_ < num_get_work)
      num_get_work_cv_->Wait();
  }

  // Wait until there are no more Sequences to create and RanTaskFromSequence()
  // has been invoked once for each Sequence returned by GetWork().
  void WaitForAllSequencesToRun() {
    AutoSchedulerLock auto_lock(lock_);

    while (num_sequences_to_create_ > 0 ||
           run_sequences_.size() < created_sequences_.size()) {
      run_sequences_cv_->Wait();
    }

    // Verify that RanTaskFromSequence() has been invoked with the same
    // Sequences that were returned by GetWork().
    EXPECT_EQ(created_sequences_, run_sequences_);

    // Verify that RunTaskCallback() has been invoked once for each Sequence
    // returned by GetWork().
    EXPECT_EQ(created_sequences_.size(), num_run_tasks_);
  }

  void SetNumSequencesToCreate(size_t num_sequences_to_create) {
    AutoSchedulerLock auto_lock(lock_);
    EXPECT_EQ(0U, num_sequences_to_create_);
    num_sequences_to_create_ = num_sequences_to_create;
  }

  void SetMaxGetWork(size_t max_get_work) {
    AutoSchedulerLock auto_lock(lock_);
    max_get_work_ = max_get_work;
  }

  std::unique_ptr<SchedulerWorkerThread> worker_thread_;

 private:
  // SchedulerWorkerThread::Delegate:
  void OnMainEntry() override {
    // Without this |auto_lock|, OnMainEntry() could be called twice without
    // generating an error.
    AutoSchedulerLock auto_lock(lock_);
    EXPECT_FALSE(main_entry_called_.IsSignaled());
    main_entry_called_.Signal();
  }

  void OnMainExit() override {
    AutoSchedulerLock auto_lock(lock_);
    EXPECT_FALSE(main_exit_called_);
    main_exit_called_ = true;
  }

  scoped_refptr<Sequence> GetWork(
      SchedulerWorkerThread* worker_thread) override {
    EXPECT_EQ(worker_thread_.get(), worker_thread);

    {
      AutoSchedulerLock auto_lock(lock_);

      // Increment the number of times that this method has been called.
      ++num_get_work_;
      num_get_work_cv_->Signal();

      // Verify that this method isn't called more times than expected.
      EXPECT_LE(num_get_work_, max_get_work_);

      // Check if a Sequence should be returned.
      if (num_sequences_to_create_ == 0)
        return nullptr;
      --num_sequences_to_create_;
    }

    // Create a Sequence that contains one Task.
    scoped_refptr<Sequence> sequence(new Sequence);
    task_tracker_.PostTask(
        Bind(IgnoreResult(&Sequence::PushTask), Unretained(sequence.get())),
        WrapUnique(new Task(
            FROM_HERE, Bind(&TaskSchedulerWorkerThreadTest::RunTaskCallback,
                            Unretained(this)),
            TaskTraits())));

    {
      // Add the Sequence to the vector of created Sequences.
      AutoSchedulerLock auto_lock(lock_);
      created_sequences_.push_back(sequence);
    }

    return sequence;
  }

  void RanTaskFromSequence(scoped_refptr<Sequence> sequence) override {
    AutoSchedulerLock auto_lock(lock_);
    run_sequences_.push_back(std::move(sequence));
    EXPECT_LE(run_sequences_.size(), created_sequences_.size());
    run_sequences_cv_->Signal();
  }

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

  // True once OnMainExit() has been called.
  bool main_exit_called_ = false;

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

  // Sequences passed to RanTaskFromSequence().
  std::vector<scoped_refptr<Sequence>> run_sequences_;

  // Condition variable signaled when a Sequence is added to |run_sequences_|.
  std::unique_ptr<ConditionVariable> run_sequences_cv_;

  // Number of times that RunTaskCallback() has been called.
  size_t num_run_tasks_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerWorkerThreadTest);
};

// Verify that when GetWork() continuously returns Sequences, all Tasks in these
// Sequences run successfully. The test wakes up the SchedulerWorkerThread once.
TEST_F(TaskSchedulerWorkerThreadTest, ContinuousWork) {
  // Set GetWork() to return |kNumSequencesPerTest| Sequences before starting to
  // return nullptr.
  SetNumSequencesToCreate(kNumSequencesPerTest);

  // Expect |kNumSequencesPerTest| calls to GetWork() in which it returns a
  // Sequence and one call in which its returns nullptr.
  const size_t kExpectedNumGetWork = kNumSequencesPerTest + 1;
  SetMaxGetWork(kExpectedNumGetWork);

  // Wake up |worker_thread_| and wait until it has run all the Tasks returned
  // by GetWork().
  worker_thread_->WakeUp();
  WaitForAllSequencesToRun();
  WaitForNumGetWork(kExpectedNumGetWork);
}

// Verify that when GetWork() alternates between returning a Sequence and
// returning nullptr, all Tasks in the returned Sequences run successfully. The
// test wakes up the SchedulerWorkerThread once for each Sequence.
TEST_F(TaskSchedulerWorkerThreadTest, IntermittentWork) {
  for (size_t i = 0; i < kNumSequencesPerTest; ++i) {
    // Set GetWork() to return 1 Sequence before starting to return
    // nullptr.
    SetNumSequencesToCreate(1);

    // Expect |i + 1| calls to GetWork() in which it returns a Sequence and
    // |i + 1| calls in which it returns nullptr.
    const size_t expected_num_get_work = 2 * (i + 1);
    SetMaxGetWork(expected_num_get_work);

    // Wake up |worker_thread_| and wait until it has run the Task returned by
    // GetWork().
    worker_thread_->WakeUp();
    WaitForAllSequencesToRun();
    WaitForNumGetWork(expected_num_get_work);
  }
}

}  // namespace
}  // namespace internal
}  // namespace base
