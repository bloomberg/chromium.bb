// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE(vtl): These tests are inherently flaky (e.g., if run on a heavily-loaded
// system). Sorry. |kEpsilonMicros| may be increased to increase tolerance and
// reduce observed flakiness.

#include "mojo/system/waiter.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"  // For |Sleep()|.
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "mojo/system/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

const int64_t kMicrosPerMs = 1000;
const int64_t kEpsilonMicros = 15 * kMicrosPerMs;  // 15 ms.
const int64_t kPollTimeMicros = 10 * kMicrosPerMs;  // 10 ms.

class WaitingThread : public base::SimpleThread {
 public:
  explicit WaitingThread(MojoDeadline deadline)
      : base::SimpleThread("waiting_thread"),
        deadline_(deadline),
        done_(false),
        result_(MOJO_RESULT_UNKNOWN),
        elapsed_micros_(-1) {
    waiter_.Init();
  }

  virtual ~WaitingThread() {
    Join();
  }

  void WaitUntilDone(MojoResult* result, int64_t* elapsed_micros) {
    for (;;) {
      {
        base::AutoLock locker(lock_);
        if (done_) {
          *result = result_;
          *elapsed_micros = elapsed_micros_;
          break;
        }
      }

      base::PlatformThread::Sleep(
          base::TimeDelta::FromMicroseconds(kPollTimeMicros));
    }
  }

  Waiter* waiter() { return &waiter_; }

 private:
  virtual void Run() OVERRIDE {
    test::Stopwatch stopwatch;
    MojoResult result;
    int64_t elapsed_micros;

    stopwatch.Start();
    result = waiter_.Wait(deadline_);
    elapsed_micros = stopwatch.Elapsed();

    {
      base::AutoLock locker(lock_);
      done_ = true;
      result_ = result;
      elapsed_micros_ = elapsed_micros;
    }
  }

  const MojoDeadline deadline_;
  Waiter waiter_;  // Thread-safe.

  base::Lock lock_;  // Protects the following members.
  bool done_;
  MojoResult result_;
  int64_t elapsed_micros_;

  DISALLOW_COPY_AND_ASSIGN(WaitingThread);
};

TEST(WaiterTest, Basic) {
  MojoResult result;
  int64_t elapsed_micros;

  // Finite deadline.

  // Awake immediately after thread start.
  {
    WaitingThread thread(static_cast<MojoDeadline>(10 * kEpsilonMicros));
    thread.Start();
    thread.waiter()->Awake(0);
    thread.WaitUntilDone(&result, &elapsed_micros);
    EXPECT_EQ(0, result);
    EXPECT_LT(elapsed_micros, kEpsilonMicros);
  }

  // Awake before after thread start.
  {
    WaitingThread thread(static_cast<MojoDeadline>(10 * kEpsilonMicros));
    thread.waiter()->Awake(MOJO_RESULT_CANCELLED);
    thread.Start();
    thread.WaitUntilDone(&result, &elapsed_micros);
    EXPECT_EQ(MOJO_RESULT_CANCELLED, result);
    EXPECT_LT(elapsed_micros, kEpsilonMicros);
  }

  // Awake some time after thread start.
  {
    WaitingThread thread(static_cast<MojoDeadline>(10 * kEpsilonMicros));
    thread.Start();
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(2 * kEpsilonMicros));
    thread.waiter()->Awake(1);
    thread.WaitUntilDone(&result, &elapsed_micros);
    EXPECT_EQ(1, result);
    EXPECT_GT(elapsed_micros, (2-1) * kEpsilonMicros);
    EXPECT_LT(elapsed_micros, (2+1) * kEpsilonMicros);
  }

  // Awake some longer time after thread start.
  {
    WaitingThread thread(static_cast<MojoDeadline>(10 * kEpsilonMicros));
    thread.Start();
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(5 * kEpsilonMicros));
    thread.waiter()->Awake(1);
    thread.WaitUntilDone(&result, &elapsed_micros);
    EXPECT_EQ(1, result);
    EXPECT_GT(elapsed_micros, (5-1) * kEpsilonMicros);
    EXPECT_LT(elapsed_micros, (5+1) * kEpsilonMicros);
  }

  // Don't awake -- time out (on another thread).
  {
    WaitingThread thread(static_cast<MojoDeadline>(2 * kEpsilonMicros));
    thread.Start();
    thread.WaitUntilDone(&result, &elapsed_micros);
    EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, result);
    EXPECT_GT(elapsed_micros, (2-1) * kEpsilonMicros);
    EXPECT_LT(elapsed_micros, (2+1) * kEpsilonMicros);
  }

  // No (indefinite) deadline.

  // Awake immediately after thread start.
  {
    WaitingThread thread(MOJO_DEADLINE_INDEFINITE);
    thread.Start();
    thread.waiter()->Awake(0);
    thread.WaitUntilDone(&result, &elapsed_micros);
    EXPECT_EQ(0, result);
    EXPECT_LT(elapsed_micros, kEpsilonMicros);
  }

  // Awake before after thread start.
  {
    WaitingThread thread(MOJO_DEADLINE_INDEFINITE);
    thread.waiter()->Awake(MOJO_RESULT_CANCELLED);
    thread.Start();
    thread.WaitUntilDone(&result, &elapsed_micros);
    EXPECT_EQ(MOJO_RESULT_CANCELLED, result);
    EXPECT_LT(elapsed_micros, kEpsilonMicros);
  }

  // Awake some time after thread start.
  {
    WaitingThread thread(MOJO_DEADLINE_INDEFINITE);
    thread.Start();
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(2 * kEpsilonMicros));
    thread.waiter()->Awake(1);
    thread.WaitUntilDone(&result, &elapsed_micros);
    EXPECT_EQ(1, result);
    EXPECT_GT(elapsed_micros, (2-1) * kEpsilonMicros);
    EXPECT_LT(elapsed_micros, (2+1) * kEpsilonMicros);
  }

  // Awake some longer time after thread start.
  {
    WaitingThread thread(MOJO_DEADLINE_INDEFINITE);
    thread.Start();
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(5 * kEpsilonMicros));
    thread.waiter()->Awake(1);
    thread.WaitUntilDone(&result, &elapsed_micros);
    EXPECT_EQ(1, result);
    EXPECT_GT(elapsed_micros, (5-1) * kEpsilonMicros);
    EXPECT_LT(elapsed_micros, (5+1) * kEpsilonMicros);
  }
}

TEST(WaiterTest, TimeOut) {
  test::Stopwatch stopwatch;
  int64_t elapsed_micros;

  Waiter waiter;

  waiter.Init();
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, waiter.Wait(0));
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_LT(elapsed_micros, kEpsilonMicros);

  waiter.Init();
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED,
            waiter.Wait(static_cast<MojoDeadline>(2 * kEpsilonMicros)));
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_GT(elapsed_micros, (2-1) * kEpsilonMicros);
  EXPECT_LT(elapsed_micros, (2+1) * kEpsilonMicros);

  waiter.Init();
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED,
            waiter.Wait(static_cast<MojoDeadline>(5 * kEpsilonMicros)));
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_GT(elapsed_micros, (5-1) * kEpsilonMicros);
  EXPECT_LT(elapsed_micros, (5+1) * kEpsilonMicros);
}

// The first |Awake()| should always win.
TEST(WaiterTest, MultipleAwakes) {
  MojoResult result;
  int64_t elapsed_micros;

  {
    WaitingThread thread(MOJO_DEADLINE_INDEFINITE);
    thread.Start();
    thread.waiter()->Awake(0);
    thread.waiter()->Awake(1);
    thread.WaitUntilDone(&result, &elapsed_micros);
    EXPECT_EQ(0, result);
    EXPECT_LT(elapsed_micros, kEpsilonMicros);
  }

  {
    WaitingThread thread(MOJO_DEADLINE_INDEFINITE);
    thread.waiter()->Awake(1);
    thread.Start();
    thread.waiter()->Awake(0);
    thread.WaitUntilDone(&result, &elapsed_micros);
    EXPECT_EQ(1, result);
    EXPECT_LT(elapsed_micros, kEpsilonMicros);
  }

  {
    WaitingThread thread(MOJO_DEADLINE_INDEFINITE);
    thread.Start();
    thread.waiter()->Awake(10);
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(2 * kEpsilonMicros));
    thread.waiter()->Awake(20);
    thread.WaitUntilDone(&result, &elapsed_micros);
    EXPECT_EQ(10, result);
    EXPECT_LT(elapsed_micros, kEpsilonMicros);
  }

  {
    WaitingThread thread(static_cast<MojoDeadline>(10 * kEpsilonMicros));
    thread.Start();
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(1 * kEpsilonMicros));
    thread.waiter()->Awake(MOJO_RESULT_FAILED_PRECONDITION);
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(2 * kEpsilonMicros));
    thread.waiter()->Awake(0);
    thread.WaitUntilDone(&result, &elapsed_micros);
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
    EXPECT_GT(elapsed_micros, (1-1) * kEpsilonMicros);
    EXPECT_LT(elapsed_micros, (1+1) * kEpsilonMicros);
  }
}

}  // namespace
}  // namespace system
}  // namespace mojo
