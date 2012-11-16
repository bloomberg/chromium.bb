// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/cancelable_task_tracker.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bind;
using base::Closure;
using base::Owned;
using base::TaskRunner;
using base::Thread;
using base::Unretained;
using base::WaitableEvent;

namespace {

class WaitableEventScoper {
 public:
  explicit WaitableEventScoper(WaitableEvent* event) : event_(event) {}
  ~WaitableEventScoper() {
    if (event_)
      event_->Signal();
  }
 private:
  WaitableEvent* event_;
  DISALLOW_COPY_AND_ASSIGN(WaitableEventScoper);
};

class CancelableTaskTrackerTest : public testing::Test {
 protected:
  CancelableTaskTrackerTest()
      : task_id_(CancelableTaskTracker::kBadTaskId),
        test_data_(0),
        task_thread_start_event_(true, false) {}

  virtual void SetUp() {
    task_thread_.reset(new Thread("task thread"));
    client_thread_.reset(new Thread("client thread"));
    task_thread_->Start();
    client_thread_->Start();

    task_thread_runner_ = task_thread_->message_loop_proxy();
    client_thread_runner_ = client_thread_->message_loop_proxy();

    // Create tracker on client thread.
    WaitableEvent tracker_created(true, false);
    client_thread_runner_->PostTask(
        FROM_HERE,
        Bind(&CancelableTaskTrackerTest::CreateTrackerOnClientThread,
             Unretained(this), &tracker_created));
    tracker_created.Wait();

    // Block server thread so we can prepare the test.
    task_thread_runner_->PostTask(
        FROM_HERE,
        Bind(&WaitableEvent::Wait, Unretained(&task_thread_start_event_)));
  }

  virtual void TearDown() {
    UnblockTaskThread();

    // Create tracker on client thread.
    WaitableEvent tracker_destroyed(true, false);
    client_thread_runner_->PostTask(
        FROM_HERE,
        Bind(&CancelableTaskTrackerTest::DestroyTrackerOnClientThread,
             Unretained(this), &tracker_destroyed));
    tracker_destroyed.Wait();

    client_thread_->Stop();
    task_thread_->Stop();
  }

  void RunOnClientAndWait(
      void (*func)(CancelableTaskTrackerTest*, WaitableEvent*)) {
    WaitableEvent event(true, false);
    client_thread_runner_->PostTask(FROM_HERE,
                                    Bind(func, Unretained(this), &event));
    event.Wait();
  }

 public:
  // Client thread posts tasks and runs replies.
  scoped_refptr<TaskRunner> client_thread_runner_;

  // Task thread runs tasks.
  scoped_refptr<TaskRunner> task_thread_runner_;

  // |tracker_| can only live on client thread.
  scoped_ptr<CancelableTaskTracker> tracker_;

  CancelableTaskTracker::TaskId task_id_;

  void UnblockTaskThread() {
    task_thread_start_event_.Signal();
  }

  //////////////////////////////////////////////////////////////////////////////
  // Testing data and related functions
  int test_data_;  // Defaults to 0.

  Closure IncreaseTestDataAndSignalClosure(WaitableEvent* event) {
    return Bind(&CancelableTaskTrackerTest::IncreaseDataAndSignal,
                &test_data_, event);
  }

  Closure DecreaseTestDataClosure(WaitableEvent* event) {
    return Bind(&CancelableTaskTrackerTest::DecreaseData,
                Owned(new WaitableEventScoper(event)), &test_data_);
  }

 private:
  void CreateTrackerOnClientThread(WaitableEvent* event) {
    tracker_.reset(new CancelableTaskTracker());
    event->Signal();
  }

  void DestroyTrackerOnClientThread(WaitableEvent* event) {
    tracker_.reset();
    event->Signal();
  }

  static void IncreaseDataAndSignal(int* data, WaitableEvent* event) {
    (*data)++;
    if (event)
      event->Signal();
  }

  static void DecreaseData(WaitableEventScoper* event_scoper, int* data) {
    (*data) -= 2;
  }

  scoped_ptr<Thread> client_thread_;
  scoped_ptr<Thread> task_thread_;

  WaitableEvent task_thread_start_event_;
};

#if (!defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)) && GTEST_HAS_DEATH_TEST
typedef CancelableTaskTrackerTest CancelableTaskTrackerDeathTest;

TEST_F(CancelableTaskTrackerDeathTest, PostFromDifferentThread) {
  // The default style "fast" does not support multi-threaded tests.
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  EXPECT_DEATH(
      tracker_->PostTask(task_thread_runner_,
                         FROM_HERE,
                         DecreaseTestDataClosure(NULL)),
      "");
}

void CancelOnDifferentThread_Test(CancelableTaskTrackerTest* test,
                                  WaitableEvent* event) {
  test->task_id_ = test->tracker_->PostTask(
      test->task_thread_runner_,
      FROM_HERE,
      test->DecreaseTestDataClosure(event));
  EXPECT_NE(CancelableTaskTracker::kBadTaskId, test->task_id_);

  // Canceling a non-existed task is noop.
  test->tracker_->TryCancel(test->task_id_ + 1);

  test->UnblockTaskThread();
}

TEST_F(CancelableTaskTrackerDeathTest, CancelOnDifferentThread) {
  // The default style "fast" does not support multi-threaded tests.
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  // Post a task and we'll try canceling it on a different thread.
  RunOnClientAndWait(&CancelOnDifferentThread_Test);

  // Canceling on the wrong thread.
  EXPECT_DEATH(tracker_->TryCancel(task_id_), "");

  // Even canceling a non-existant task will crash.
  EXPECT_DEATH(tracker_->TryCancel(task_id_ + 1), "");
}

void TrackerCancelAllOnDifferentThread_Test(
    CancelableTaskTrackerTest* test, WaitableEvent* event) {
  test->task_id_ = test->tracker_->PostTask(
      test->task_thread_runner_,
      FROM_HERE,
      test->DecreaseTestDataClosure(event));
  EXPECT_NE(CancelableTaskTracker::kBadTaskId, test->task_id_);
  test->UnblockTaskThread();
}

TEST_F(CancelableTaskTrackerDeathTest, TrackerCancelAllOnDifferentThread) {
  // The default style "fast" does not support multi-threaded tests.
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  // |tracker_| can only live on client thread.
  EXPECT_DEATH(tracker_.reset(), "");

  RunOnClientAndWait(&TrackerCancelAllOnDifferentThread_Test);

  EXPECT_DEATH(tracker_->TryCancelAll(), "");
  EXPECT_DEATH(tracker_.reset(), "");
}

#endif  // (!defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)) &&
        //     GTEST_HAS_DEATH_TEST

void Canceled_Test(CancelableTaskTrackerTest* test, WaitableEvent* event) {
  test->task_id_ = test->tracker_->PostTask(
      test->task_thread_runner_,
      FROM_HERE,
      test->DecreaseTestDataClosure(event));
  EXPECT_NE(CancelableTaskTracker::kBadTaskId, test->task_id_);

  test->tracker_->TryCancel(test->task_id_);
  test->UnblockTaskThread();
}

TEST_F(CancelableTaskTrackerTest, Canceled) {
  RunOnClientAndWait(&Canceled_Test);
  EXPECT_EQ(0, test_data_);
}

void SignalAndWaitThenIncrease(WaitableEvent* start_event,
                               WaitableEvent* continue_event,
                               int* data) {
  start_event->Signal();
  continue_event->Wait();
  (*data)++;
}

void CancelWhileTaskRunning_Test(CancelableTaskTrackerTest* test,
                                 WaitableEvent* event) {
  WaitableEvent task_start_event(true, false);
  WaitableEvent* task_continue_event = new WaitableEvent(true, false);

  test->task_id_ = test->tracker_->PostTaskAndReply(
      test->task_thread_runner_,
      FROM_HERE,
      Bind(&SignalAndWaitThenIncrease,
           &task_start_event, Owned(task_continue_event), &test->test_data_),
      test->DecreaseTestDataClosure(event));
  EXPECT_NE(CancelableTaskTracker::kBadTaskId, test->task_id_);

  test->UnblockTaskThread();
  task_start_event.Wait();

  // Now task is running. Let's try to cancel.
  test->tracker_->TryCancel(test->task_id_);

  // Let task continue.
  task_continue_event->Signal();
}

TEST_F(CancelableTaskTrackerTest, CancelWhileTaskRunning) {
  RunOnClientAndWait(&CancelWhileTaskRunning_Test);

  // Task will continue running but reply will be canceled.
  EXPECT_EQ(1, test_data_);
}

void NotCanceled_Test(CancelableTaskTrackerTest* test, WaitableEvent* event) {
  test->task_id_ = test->tracker_->PostTaskAndReply(
      test->task_thread_runner_,
      FROM_HERE,
      test->IncreaseTestDataAndSignalClosure(NULL),
      test->DecreaseTestDataClosure(event));
  EXPECT_NE(CancelableTaskTracker::kBadTaskId, test->task_id_);

  test->UnblockTaskThread();
}

TEST_F(CancelableTaskTrackerTest, NotCanceled) {
  RunOnClientAndWait(&NotCanceled_Test);
  EXPECT_EQ(-1, test_data_);
}

void TrackerDestructed_Test(CancelableTaskTrackerTest* test,
                            WaitableEvent* event) {
  test->task_id_ = test->tracker_->PostTaskAndReply(
      test->task_thread_runner_,
      FROM_HERE,
      test->IncreaseTestDataAndSignalClosure(NULL),
      test->DecreaseTestDataClosure(event));
  EXPECT_NE(CancelableTaskTracker::kBadTaskId, test->task_id_);

  test->tracker_.reset();
  test->UnblockTaskThread();
}

TEST_F(CancelableTaskTrackerTest, TrackerDestructed) {
  RunOnClientAndWait(&TrackerDestructed_Test);
  EXPECT_EQ(0, test_data_);
}

void TrackerDestructedAfterTask_Test(CancelableTaskTrackerTest* test,
                                     WaitableEvent* event) {
  WaitableEvent task_done_event(true, false);
  test->task_id_ = test->tracker_->PostTaskAndReply(
      test->task_thread_runner_,
      FROM_HERE,
      test->IncreaseTestDataAndSignalClosure(&task_done_event),
      test->DecreaseTestDataClosure(event));
  ASSERT_NE(CancelableTaskTracker::kBadTaskId, test->task_id_);

  test->UnblockTaskThread();

  task_done_event.Wait();

  // At this point, task is already finished on task thread but reply has not
  // started yet (because this function is still running on client thread).
  // Now delete the tracker to cancel reply.
  test->tracker_.reset();
}

TEST_F(CancelableTaskTrackerTest, TrackerDestructedAfterTask) {
  RunOnClientAndWait(&TrackerDestructedAfterTask_Test);
  EXPECT_EQ(1, test_data_);
}

}  // namespace
