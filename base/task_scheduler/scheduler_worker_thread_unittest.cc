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

class TaskSchedulerWorkerThreadTest : public testing::Test {
 protected:
  TaskSchedulerWorkerThreadTest()
      : num_main_entry_callback_cv_(lock_.CreateConditionVariable()),
        num_get_work_callback_cv_(lock_.CreateConditionVariable()),
        run_sequences_cv_(lock_.CreateConditionVariable()) {}

  void SetUp() override {
    worker_thread_ = SchedulerWorkerThread::CreateSchedulerWorkerThread(
        ThreadPriority::NORMAL,
        Bind(&TaskSchedulerWorkerThreadTest::MainEntryCallback,
             Unretained(this)),
        Bind(&TaskSchedulerWorkerThreadTest::GetWorkCallback, Unretained(this)),
        Bind(&TaskSchedulerWorkerThreadTest::RanTaskFromSequenceCallback,
             Unretained(this)),
        &task_tracker_);
    ASSERT_TRUE(worker_thread_);
    WaitForNumMainEntryCallback(1);
  }

  void TearDown() override {
    worker_thread_->JoinForTesting();

    AutoSchedulerLock auto_lock(lock_);
    EXPECT_EQ(1U, num_main_entry_callback_);
  }

  // Wait until MainEntryCallback() has been called |num_main_entry_callback|
  // times.
  void WaitForNumMainEntryCallback(size_t num_main_entry_callback) {
    AutoSchedulerLock auto_lock(lock_);
    while (num_main_entry_callback_ < num_main_entry_callback)
      num_main_entry_callback_cv_->Wait();
  }

  // Wait until GetWorkCallback() has been called |num_get_work_callback| times.
  void WaitForNumGetWorkCallback(size_t num_get_work_callback) {
    AutoSchedulerLock auto_lock(lock_);
    while (num_get_work_callback_ < num_get_work_callback)
      num_get_work_callback_cv_->Wait();
  }

  // Wait until there is no more Sequences to create and
  // RanTaskFromSequenceCallback() has been invoked once for each Sequence
  // returned by GetWorkCallback().
  void WaitForAllSequencesToRun() {
    AutoSchedulerLock auto_lock(lock_);

    while (num_sequences_to_create_ > 0 ||
           run_sequences_.size() < created_sequences_.size()) {
      run_sequences_cv_->Wait();
    }

    // Verify that RanTaskFromSequenceCallback() has been invoked with the
    // same Sequences that were returned by GetWorkCallback().
    EXPECT_EQ(created_sequences_, run_sequences_);

    // Verify that RunTaskCallback() has been invoked once for each Sequence
    // returned by GetWorkCallback().
    EXPECT_EQ(created_sequences_.size(), num_run_tasks_);
  }

  void SetNumSequencesToCreate(size_t num_sequences_to_create) {
    AutoSchedulerLock auto_lock(lock_);
    EXPECT_EQ(0U, num_sequences_to_create_);
    num_sequences_to_create_ = num_sequences_to_create;
  }

  size_t NumGetWorkCallback() const {
    AutoSchedulerLock auto_lock(lock_);
    return num_get_work_callback_;
  }

  std::unique_ptr<SchedulerWorkerThread> worker_thread_;

 private:
  void MainEntryCallback() {
    AutoSchedulerLock auto_lock(lock_);
    ++num_main_entry_callback_;
    num_main_entry_callback_cv_->Signal();
  }

  // Returns a Sequence that contains 1 Task if |num_sequences_to_create_| is
  // greater than 0.
  scoped_refptr<Sequence> GetWorkCallback(
      SchedulerWorkerThread* worker_thread) {
    EXPECT_EQ(worker_thread_.get(), worker_thread);

    {
      AutoSchedulerLock auto_lock(lock_);

      // Increment the number of times that this callback has been invoked.
      ++num_get_work_callback_;
      num_get_work_callback_cv_->Signal();

      // Check if a Sequence should be returned.
      if (num_sequences_to_create_ == 0)
        return nullptr;
      --num_sequences_to_create_;
    }

    // Create a Sequence that contains 1 Task.
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

  void RanTaskFromSequenceCallback(const SchedulerWorkerThread* worker_thread,
                                   scoped_refptr<Sequence> sequence) {
    EXPECT_EQ(worker_thread_.get(), worker_thread);

    AutoSchedulerLock auto_lock(lock_);
    run_sequences_.push_back(std::move(sequence));
    run_sequences_cv_->Signal();
  }

  void RunTaskCallback() {
    AutoSchedulerLock auto_lock(lock_);
    ++num_run_tasks_;
  }

  TaskTracker task_tracker_;

  // Synchronizes access to all members below.
  mutable SchedulerLock lock_;

  // Number of times that MainEntryCallback() has been called.
  size_t num_main_entry_callback_ = 0;

  // Condition variable signaled when |num_main_entry_callback_| is incremented.
  std::unique_ptr<ConditionVariable> num_main_entry_callback_cv_;

  // Number of Sequences that should be created by GetWorkCallback(). When this
  // is 0, GetWorkCallback() returns nullptr.
  size_t num_sequences_to_create_ = 0;

  // Number of times that GetWorkCallback() has been called.
  size_t num_get_work_callback_ = 0;

  // Condition variable signaled when |num_get_work_callback_| is incremented.
  std::unique_ptr<ConditionVariable> num_get_work_callback_cv_;

  // Sequences created by GetWorkCallback().
  std::vector<scoped_refptr<Sequence>> created_sequences_;

  // Sequences passed to RanTaskFromSequenceCallback().
  std::vector<scoped_refptr<Sequence>> run_sequences_;

  // Condition variable signaled when a Sequence is added to |run_sequences_|.
  std::unique_ptr<ConditionVariable> run_sequences_cv_;

  // Number of times that RunTaskCallback() has been called.
  size_t num_run_tasks_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerWorkerThreadTest);
};

// Verify that when GetWorkCallback() continuously returns Sequences, all Tasks
// in these Sequences run successfully. The SchedulerWorkerThread is woken up
// once.
TEST_F(TaskSchedulerWorkerThreadTest, ContinousWork) {
  // Set GetWorkCallback() to return |kNumSequencesPerTest| Sequences before
  // starting to return nullptr.
  SetNumSequencesToCreate(kNumSequencesPerTest);

  // Wake up |worker_thread_| and wait until it has run all the Tasks returned
  // by GetWorkCallback().
  worker_thread_->WakeUp();
  WaitForAllSequencesToRun();

  // Expect |kNumSequencesPerTest| calls to GetWorkCallback() in which it
  // returned a Sequence and 1 call in which it returned nullptr.
  const size_t expected_num_get_work_callback = kNumSequencesPerTest + 1;
  WaitForNumGetWorkCallback(expected_num_get_work_callback);
  EXPECT_EQ(expected_num_get_work_callback, NumGetWorkCallback());
}

// Verify that when GetWorkCallback() alternates between returning a Sequence
// and returning nullptr, all Tasks in the returned Sequences run successfully.
// The SchedulerWorkerThread is woken up once for each Sequence.
TEST_F(TaskSchedulerWorkerThreadTest, IntermittentWork) {
  for (size_t i = 0; i < kNumSequencesPerTest; ++i) {
    // Set GetWorkCallback() to return 1 Sequence before starting to return
    // nullptr.
    SetNumSequencesToCreate(1);

    // Wake up |worker_thread_| and wait until it has run all the Tasks returned
    // by GetWorkCallback().
    worker_thread_->WakeUp();
    WaitForAllSequencesToRun();

    // Expect |i| calls to GetWorkCallback() in which it returned a Sequence and
    // |i| calls in which it returned nullptr.
    const size_t expected_num_get_work_callback = 2 * (i + 1);
    WaitForNumGetWorkCallback(expected_num_get_work_callback);
    EXPECT_EQ(expected_num_get_work_callback, NumGetWorkCallback());
  }
}

}  // namespace
}  // namespace internal
}  // namespace base
