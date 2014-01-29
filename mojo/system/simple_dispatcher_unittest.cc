// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE(vtl): Some of these tests are inherently flaky (e.g., if run on a
// heavily-loaded system). Sorry. |kEpsilonMicros| may be increased to increase
// tolerance and reduce observed flakiness.

#include "mojo/system/simple_dispatcher.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"  // For |Sleep()|.
#include "base/time/time.h"
#include "mojo/system/test_utils.h"
#include "mojo/system/waiter.h"
#include "mojo/system/waiter_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

const int64_t kMicrosPerMs = 1000;
const int64_t kEpsilonMicros = 15 * kMicrosPerMs;  // 15 ms.

class MockSimpleDispatcher : public SimpleDispatcher {
 public:
  MockSimpleDispatcher()
      : satisfied_flags_(MOJO_WAIT_FLAG_NONE),
        satisfiable_flags_(MOJO_WAIT_FLAG_READABLE | MOJO_WAIT_FLAG_WRITABLE) {}

  void SetSatisfiedFlags(MojoWaitFlags new_satisfied_flags) {
    base::AutoLock locker(lock());

    // Any new flags that are set should be satisfiable.
    CHECK_EQ(new_satisfied_flags & ~satisfied_flags_,
             new_satisfied_flags & ~satisfied_flags_ & satisfiable_flags_);

    if (new_satisfied_flags == satisfied_flags_)
      return;

    satisfied_flags_ = new_satisfied_flags;
    StateChangedNoLock();
  }

  void SetSatisfiableFlags(MojoWaitFlags new_satisfiable_flags) {
    base::AutoLock locker(lock());

    if (new_satisfiable_flags == satisfiable_flags_)
      return;

    satisfiable_flags_ = new_satisfiable_flags;
    StateChangedNoLock();
  }

  virtual Type GetType() OVERRIDE {
    return kTypeUnknown;
  }

 private:
  friend class base::RefCountedThreadSafe<MockSimpleDispatcher>;
  virtual ~MockSimpleDispatcher() {}

  virtual scoped_refptr<Dispatcher>
      CreateEquivalentDispatcherAndCloseImplNoLock() OVERRIDE {
    scoped_refptr<MockSimpleDispatcher> rv(new MockSimpleDispatcher());
    rv->satisfied_flags_ = satisfied_flags_;
    rv->satisfiable_flags_ = satisfiable_flags_;
    return scoped_refptr<Dispatcher>(rv.get());
  }

  // |SimpleDispatcher| implementation:
  virtual MojoWaitFlags SatisfiedFlagsNoLock() const OVERRIDE {
    lock().AssertAcquired();
    return satisfied_flags_;
  }

  virtual MojoWaitFlags SatisfiableFlagsNoLock() const OVERRIDE {
    lock().AssertAcquired();
    return satisfiable_flags_;
  }

  // Protected by |lock()|:
  MojoWaitFlags satisfied_flags_;
  MojoWaitFlags satisfiable_flags_;

  DISALLOW_COPY_AND_ASSIGN(MockSimpleDispatcher);
};

TEST(SimpleDispatcherTest, Basic) {
  test::Stopwatch stopwatch;
  int64_t elapsed_micros;

  scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
  Waiter w;

  // Try adding a readable waiter when already readable.
  w.Init();
  d->SetSatisfiedFlags(MOJO_WAIT_FLAG_READABLE);
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            d->AddWaiter(&w, MOJO_WAIT_FLAG_READABLE, 0));
  // Shouldn't need to remove the waiter (it was not added).

  // Wait (forever) for writable when already writable.
  w.Init();
  d->SetSatisfiedFlags(MOJO_WAIT_FLAG_READABLE);
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_WAIT_FLAG_WRITABLE, 1));
  d->SetSatisfiedFlags(MOJO_WAIT_FLAG_WRITABLE);
  stopwatch.Start();
  EXPECT_EQ(1, w.Wait(MOJO_DEADLINE_INDEFINITE));
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_LT(elapsed_micros, kEpsilonMicros);
  d->RemoveWaiter(&w);

  // Wait for zero time for writable when already writable.
  w.Init();
  d->SetSatisfiedFlags(MOJO_WAIT_FLAG_READABLE);
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_WAIT_FLAG_WRITABLE, 2));
  d->SetSatisfiedFlags(MOJO_WAIT_FLAG_WRITABLE);
  stopwatch.Start();
  EXPECT_EQ(2, w.Wait(0));
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_LT(elapsed_micros, kEpsilonMicros);
  d->RemoveWaiter(&w);

  // Wait for non-zero, finite time for writable when already writable.
  w.Init();
  d->SetSatisfiedFlags(MOJO_WAIT_FLAG_READABLE);
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_WAIT_FLAG_WRITABLE, 3));
  d->SetSatisfiedFlags(MOJO_WAIT_FLAG_WRITABLE);
  stopwatch.Start();
  EXPECT_EQ(3, w.Wait(2 * kEpsilonMicros));
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_LT(elapsed_micros, kEpsilonMicros);
  d->RemoveWaiter(&w);

  // Wait for zero time for writable when not writable (will time out).
  w.Init();
  d->SetSatisfiedFlags(MOJO_WAIT_FLAG_READABLE);
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_WAIT_FLAG_WRITABLE, 4));
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, w.Wait(0));
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_LT(elapsed_micros, kEpsilonMicros);
  d->RemoveWaiter(&w);

  // Wait for non-zero, finite time for writable when not writable (will time
  // out).
  w.Init();
  d->SetSatisfiedFlags(MOJO_WAIT_FLAG_READABLE);
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_WAIT_FLAG_WRITABLE, 4));
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, w.Wait(2 * kEpsilonMicros));
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_GT(elapsed_micros, (2-1) * kEpsilonMicros);
  EXPECT_LT(elapsed_micros, (2+1) * kEpsilonMicros);
  d->RemoveWaiter(&w);

  EXPECT_EQ(MOJO_RESULT_OK, d->Close());
}

TEST(SimpleDispatcherTest, BasicUnsatisfiable) {
  test::Stopwatch stopwatch;
  int64_t elapsed_micros;

  scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
  Waiter w;

  // Try adding a writable waiter when it can never be writable.
  w.Init();
  d->SetSatisfiableFlags(MOJO_WAIT_FLAG_READABLE);
  d->SetSatisfiedFlags(0);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            d->AddWaiter(&w, MOJO_WAIT_FLAG_WRITABLE, 5));
  // Shouldn't need to remove the waiter (it was not added).

  // Wait (forever) for writable and then it becomes never writable.
  w.Init();
  d->SetSatisfiableFlags(MOJO_WAIT_FLAG_READABLE | MOJO_WAIT_FLAG_WRITABLE);
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_WAIT_FLAG_WRITABLE, 6));
  d->SetSatisfiableFlags(MOJO_WAIT_FLAG_READABLE);
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, w.Wait(MOJO_DEADLINE_INDEFINITE));
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_LT(elapsed_micros, kEpsilonMicros);
  d->RemoveWaiter(&w);

  // Wait for zero time for writable and then it becomes never writable.
  w.Init();
  d->SetSatisfiableFlags(MOJO_WAIT_FLAG_READABLE | MOJO_WAIT_FLAG_WRITABLE);
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_WAIT_FLAG_WRITABLE, 6));
  d->SetSatisfiableFlags(MOJO_WAIT_FLAG_READABLE);
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, w.Wait(0));
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_LT(elapsed_micros, kEpsilonMicros);
  d->RemoveWaiter(&w);

  // Wait for non-zero, finite time for writable and then it becomes never
  // writable.
  w.Init();
  d->SetSatisfiableFlags(MOJO_WAIT_FLAG_READABLE | MOJO_WAIT_FLAG_WRITABLE);
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_WAIT_FLAG_WRITABLE, 7));
  d->SetSatisfiableFlags(MOJO_WAIT_FLAG_READABLE);
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, w.Wait(2 * kEpsilonMicros));
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_LT(elapsed_micros, kEpsilonMicros);
  d->RemoveWaiter(&w);

  EXPECT_EQ(MOJO_RESULT_OK, d->Close());
}

TEST(SimpleDispatcherTest, BasicClosed) {
  test::Stopwatch stopwatch;
  int64_t elapsed_micros;

  scoped_refptr<MockSimpleDispatcher> d;
  Waiter w;

  // Try adding a writable waiter when the dispatcher has been closed.
  d = new MockSimpleDispatcher();
  w.Init();
  EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            d->AddWaiter(&w, MOJO_WAIT_FLAG_WRITABLE, 8));
  // Shouldn't need to remove the waiter (it was not added).

  // Wait (forever) for writable and then the dispatcher is closed.
  d = new MockSimpleDispatcher();
  w.Init();
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_WAIT_FLAG_WRITABLE, 9));
  EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_CANCELLED, w.Wait(MOJO_DEADLINE_INDEFINITE));
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_LT(elapsed_micros, kEpsilonMicros);
  // Don't need to remove waiters from closed dispatchers.

  // Wait for zero time for writable and then the dispatcher is closed.
  d = new MockSimpleDispatcher();
  w.Init();
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_WAIT_FLAG_WRITABLE, 10));
  EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_CANCELLED, w.Wait(0));
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_LT(elapsed_micros, kEpsilonMicros);
  // Don't need to remove waiters from closed dispatchers.

  // Wait for non-zero, finite time for writable and then the dispatcher is
  // closed.
  d = new MockSimpleDispatcher();
  w.Init();
  EXPECT_EQ(MOJO_RESULT_OK, d->AddWaiter(&w, MOJO_WAIT_FLAG_WRITABLE, 11));
  EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  stopwatch.Start();
  EXPECT_EQ(MOJO_RESULT_CANCELLED, w.Wait(2 * kEpsilonMicros));
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_LT(elapsed_micros, kEpsilonMicros);
  // Don't need to remove waiters from closed dispatchers.
}

TEST(SimpleDispatcherTest, BasicThreaded) {
  test::Stopwatch stopwatch;
  bool did_wait;
  MojoResult result;
  int64_t elapsed_micros;

  // Wait for readable (already readable).
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    {
      d->SetSatisfiedFlags(MOJO_WAIT_FLAG_READABLE);
      test::WaiterThread thread(d,
                                MOJO_WAIT_FLAG_READABLE,
                                MOJO_DEADLINE_INDEFINITE,
                                0,
                                &did_wait, &result);
      stopwatch.Start();
      thread.Start();
    }  // Joins the thread.
    // If we closed earlier, then probably we'd get a |MOJO_RESULT_CANCELLED|.
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_FALSE(did_wait);
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS, result);
  EXPECT_LT(elapsed_micros, kEpsilonMicros);

  // Wait for readable and becomes readable after some time.
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    test::WaiterThread thread(d,
                              MOJO_WAIT_FLAG_READABLE,
                              MOJO_DEADLINE_INDEFINITE,
                              1,
                              &did_wait, &result);
    stopwatch.Start();
    thread.Start();
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(2 * kEpsilonMicros));
    d->SetSatisfiedFlags(MOJO_WAIT_FLAG_READABLE);
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }  // Joins the thread.
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_TRUE(did_wait);
  EXPECT_EQ(1, result);
  EXPECT_GT(elapsed_micros, (2-1) * kEpsilonMicros);
  EXPECT_LT(elapsed_micros, (2+1) * kEpsilonMicros);

  // Wait for readable and becomes never-readable after some time.
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    test::WaiterThread thread(d,
                              MOJO_WAIT_FLAG_READABLE,
                              MOJO_DEADLINE_INDEFINITE,
                              2,
                              &did_wait, &result);
    stopwatch.Start();
    thread.Start();
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(2 * kEpsilonMicros));
    d->SetSatisfiableFlags(MOJO_WAIT_FLAG_NONE);
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }  // Joins the thread.
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_TRUE(did_wait);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
  EXPECT_GT(elapsed_micros, (2-1) * kEpsilonMicros);
  EXPECT_LT(elapsed_micros, (2+1) * kEpsilonMicros);

  // Wait for readable and dispatcher gets closed.
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    test::WaiterThread thread(d,
                              MOJO_WAIT_FLAG_READABLE,
                              MOJO_DEADLINE_INDEFINITE,
                              3,
                              &did_wait, &result);
    stopwatch.Start();
    thread.Start();
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(2 * kEpsilonMicros));
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }  // Joins the thread.
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_TRUE(did_wait);
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result);
  EXPECT_GT(elapsed_micros, (2-1) * kEpsilonMicros);
  EXPECT_LT(elapsed_micros, (2+1) * kEpsilonMicros);

  // Wait for readable and times out.
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    {
      test::WaiterThread thread(d,
                                MOJO_WAIT_FLAG_READABLE,
                                2 * kEpsilonMicros,
                                4,
                                &did_wait, &result);
      stopwatch.Start();
      thread.Start();
      base::PlatformThread::Sleep(
          base::TimeDelta::FromMicroseconds(1 * kEpsilonMicros));
      // Not what we're waiting for.
      d->SetSatisfiedFlags(MOJO_WAIT_FLAG_WRITABLE);
    }  // Joins the thread (after its wait times out).
    // If we closed earlier, then probably we'd get a |MOJO_RESULT_CANCELLED|.
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }
  elapsed_micros = stopwatch.Elapsed();
  EXPECT_TRUE(did_wait);
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, result);
  EXPECT_GT(elapsed_micros, (2-1) * kEpsilonMicros);
  EXPECT_LT(elapsed_micros, (2+1) * kEpsilonMicros);
}

TEST(SimpleDispatcherTest, MultipleWaiters) {
  static const size_t kNumWaiters = 20;

  bool did_wait[kNumWaiters];
  MojoResult result[kNumWaiters];

  // All wait for readable and becomes readable after some time.
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    ScopedVector<test::WaiterThread> threads;
    for (size_t i = 0; i < kNumWaiters; i++) {
      threads.push_back(new test::WaiterThread(d,
                                               MOJO_WAIT_FLAG_READABLE,
                                               MOJO_DEADLINE_INDEFINITE,
                                               static_cast<MojoResult>(i),
                                               &did_wait[i], &result[i]));
      threads.back()->Start();
    }
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(2 * kEpsilonMicros));
    d->SetSatisfiedFlags(MOJO_WAIT_FLAG_READABLE);
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }  // Joins the threads.
  for (size_t i = 0; i < kNumWaiters; i++) {
    EXPECT_TRUE(did_wait[i]);
    EXPECT_EQ(static_cast<MojoResult>(i), result[i]);
  }

  // Some wait for readable, some for writable, and becomes readable after some
  // time.
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    ScopedVector<test::WaiterThread> threads;
    for (size_t i = 0; i < kNumWaiters / 2; i++) {
      threads.push_back(new test::WaiterThread(d,
                                               MOJO_WAIT_FLAG_READABLE,
                                               MOJO_DEADLINE_INDEFINITE,
                                               static_cast<MojoResult>(i),
                                               &did_wait[i], &result[i]));
      threads.back()->Start();
    }
    for (size_t i = kNumWaiters / 2; i < kNumWaiters; i++) {
      threads.push_back(new test::WaiterThread(d,
                                               MOJO_WAIT_FLAG_WRITABLE,
                                               MOJO_DEADLINE_INDEFINITE,
                                               static_cast<MojoResult>(i),
                                               &did_wait[i], &result[i]));
      threads.back()->Start();
    }
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(2 * kEpsilonMicros));
    d->SetSatisfiedFlags(MOJO_WAIT_FLAG_READABLE);
    // This will wake up the ones waiting to write.
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }  // Joins the threads.
  for (size_t i = 0; i < kNumWaiters / 2; i++) {
    EXPECT_TRUE(did_wait[i]);
    EXPECT_EQ(static_cast<MojoResult>(i), result[i]);
  }
  for (size_t i = kNumWaiters / 2; i < kNumWaiters; i++) {
    EXPECT_TRUE(did_wait[i]);
    EXPECT_EQ(MOJO_RESULT_CANCELLED, result[i]);
  }

  // Some wait for readable, some for writable, and becomes readable and
  // never-writable after some time.
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    ScopedVector<test::WaiterThread> threads;
    for (size_t i = 0; i < kNumWaiters / 2; i++) {
      threads.push_back(new test::WaiterThread(d,
                                               MOJO_WAIT_FLAG_READABLE,
                                               MOJO_DEADLINE_INDEFINITE,
                                               static_cast<MojoResult>(i),
                                               &did_wait[i], &result[i]));
      threads.back()->Start();
    }
    for (size_t i = kNumWaiters / 2; i < kNumWaiters; i++) {
      threads.push_back(new test::WaiterThread(d,
                                               MOJO_WAIT_FLAG_WRITABLE,
                                               MOJO_DEADLINE_INDEFINITE,
                                               static_cast<MojoResult>(i),
                                               &did_wait[i], &result[i]));
      threads.back()->Start();
    }
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(1 * kEpsilonMicros));
    d->SetSatisfiableFlags(MOJO_WAIT_FLAG_READABLE);
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(1 * kEpsilonMicros));
    d->SetSatisfiedFlags(MOJO_WAIT_FLAG_READABLE);
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }  // Joins the threads.
  for (size_t i = 0; i < kNumWaiters / 2; i++) {
    EXPECT_TRUE(did_wait[i]);
    EXPECT_EQ(static_cast<MojoResult>(i), result[i]);
  }
  for (size_t i = kNumWaiters / 2; i < kNumWaiters; i++) {
    EXPECT_TRUE(did_wait[i]);
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result[i]);
  }

  // Some wait for readable, some for writable, and becomes readable after some
  // time.
  {
    scoped_refptr<MockSimpleDispatcher> d(new MockSimpleDispatcher());
    ScopedVector<test::WaiterThread> threads;
    for (size_t i = 0; i < kNumWaiters / 2; i++) {
      threads.push_back(new test::WaiterThread(d,
                                               MOJO_WAIT_FLAG_READABLE,
                                               3 * kEpsilonMicros,
                                               static_cast<MojoResult>(i),
                                               &did_wait[i], &result[i]));
      threads.back()->Start();
    }
    for (size_t i = kNumWaiters / 2; i < kNumWaiters; i++) {
      threads.push_back(new test::WaiterThread(d,
                                               MOJO_WAIT_FLAG_WRITABLE,
                                               1 * kEpsilonMicros,
                                               static_cast<MojoResult>(i),
                                               &did_wait[i], &result[i]));
      threads.back()->Start();
    }
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(2 * kEpsilonMicros));
    d->SetSatisfiedFlags(MOJO_WAIT_FLAG_READABLE);
    // All those waiting for writable should have timed out.
    EXPECT_EQ(MOJO_RESULT_OK, d->Close());
  }  // Joins the threads.
  for (size_t i = 0; i < kNumWaiters / 2; i++) {
    EXPECT_TRUE(did_wait[i]);
    EXPECT_EQ(static_cast<MojoResult>(i), result[i]);
  }
  for (size_t i = kNumWaiters / 2; i < kNumWaiters; i++) {
    EXPECT_TRUE(did_wait[i]);
    EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, result[i]);
  }
}

// TODO(vtl): Stress test?

}  // namespace
}  // namespace system
}  // namespace mojo
