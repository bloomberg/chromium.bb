// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE(vtl): These tests are inherently flaky (e.g., if run on a heavily-loaded
// system). Sorry. |kEpsilonMicros| may be increased to increase tolerance and
// reduce observed flakiness.

#include "mojo/system/waiter_list.h"

#include "base/threading/platform_thread.h"  // For |Sleep()|.
#include "base/time/time.h"
#include "mojo/system/waiter.h"
#include "mojo/system/waiter_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

const int64_t kMicrosPerMs = 1000;
const int64_t kEpsilonMicros = 15 * kMicrosPerMs;  // 15 ms.

TEST(WaiterListTest, BasicCancel) {
  MojoResult result;

  // Cancel immediately after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result);
    waiter_list.AddWaiter(thread.waiter(), MOJO_WAIT_FLAG_READABLE, 0);
    thread.Start();
    waiter_list.CancelAllWaiters();
    waiter_list.RemoveWaiter(thread.waiter());  // Double-remove okay.
  }  // Join |thread|.
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result);

  // Cancel before after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result);
    waiter_list.AddWaiter(thread.waiter(), MOJO_WAIT_FLAG_WRITABLE, 1);
    waiter_list.CancelAllWaiters();
    thread.Start();
  }  // Join |thread|.
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result);

  // Cancel some time after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result);
    waiter_list.AddWaiter(thread.waiter(), MOJO_WAIT_FLAG_READABLE, 2);
    thread.Start();
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(2 * kEpsilonMicros));
    waiter_list.CancelAllWaiters();
  }  // Join |thread|.
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result);
}

TEST(WaiterListTest, BasicAwakeSatisfied) {
  MojoResult result;

  // Awake immediately after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result);
    waiter_list.AddWaiter(thread.waiter(), MOJO_WAIT_FLAG_READABLE, 0);
    thread.Start();
    waiter_list.AwakeWaitersForStateChange(MOJO_WAIT_FLAG_READABLE,
                                           MOJO_WAIT_FLAG_READABLE |
                                              MOJO_WAIT_FLAG_WRITABLE);
    waiter_list.RemoveWaiter(thread.waiter());
  }  // Join |thread|.
  EXPECT_EQ(0, result);

  // Awake before after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result);
    waiter_list.AddWaiter(thread.waiter(), MOJO_WAIT_FLAG_WRITABLE, 1);
    waiter_list.AwakeWaitersForStateChange(MOJO_WAIT_FLAG_WRITABLE,
                                           MOJO_WAIT_FLAG_READABLE |
                                              MOJO_WAIT_FLAG_WRITABLE);
    waiter_list.RemoveWaiter(thread.waiter());
    waiter_list.RemoveWaiter(thread.waiter());  // Double-remove okay.
    thread.Start();
  }  // Join |thread|.
  EXPECT_EQ(1, result);

  // Awake some time after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result);
    waiter_list.AddWaiter(thread.waiter(), MOJO_WAIT_FLAG_READABLE, 2);
    thread.Start();
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(2 * kEpsilonMicros));
    waiter_list.AwakeWaitersForStateChange(MOJO_WAIT_FLAG_READABLE,
                                           MOJO_WAIT_FLAG_READABLE |
                                              MOJO_WAIT_FLAG_WRITABLE);
    waiter_list.RemoveWaiter(thread.waiter());
  }  // Join |thread|.
  EXPECT_EQ(2, result);
}

TEST(WaiterListTest, BasicAwakeUnsatisfiable) {
  MojoResult result;

  // Awake (for unsatisfiability) immediately after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result);
    waiter_list.AddWaiter(thread.waiter(), MOJO_WAIT_FLAG_READABLE, 0);
    thread.Start();
    waiter_list.AwakeWaitersForStateChange(0, MOJO_WAIT_FLAG_WRITABLE);
    waiter_list.RemoveWaiter(thread.waiter());
  }  // Join |thread|.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);

  // Awake (for unsatisfiability) before after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result);
    waiter_list.AddWaiter(thread.waiter(), MOJO_WAIT_FLAG_WRITABLE, 1);
    waiter_list.AwakeWaitersForStateChange(MOJO_WAIT_FLAG_READABLE,
                                           MOJO_WAIT_FLAG_READABLE);
    waiter_list.RemoveWaiter(thread.waiter());
    thread.Start();
  }  // Join |thread|.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);

  // Awake (for unsatisfiability) some time after thread start.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread(&result);
    waiter_list.AddWaiter(thread.waiter(), MOJO_WAIT_FLAG_READABLE, 2);
    thread.Start();
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(2 * kEpsilonMicros));
    waiter_list.AwakeWaitersForStateChange(0, MOJO_WAIT_FLAG_WRITABLE);
    waiter_list.RemoveWaiter(thread.waiter());
    waiter_list.RemoveWaiter(thread.waiter());  // Double-remove okay.
  }  // Join |thread|.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
}

TEST(WaiterListTest, MultipleWaiters) {
  MojoResult result1;
  MojoResult result2;
  MojoResult result3;
  MojoResult result4;

  // Cancel two waiters.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread1(&result1);
    waiter_list.AddWaiter(thread1.waiter(), MOJO_WAIT_FLAG_READABLE, 0);
    thread1.Start();
    test::SimpleWaiterThread thread2(&result2);
    waiter_list.AddWaiter(thread2.waiter(), MOJO_WAIT_FLAG_WRITABLE, 1);
    thread2.Start();
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(2 * kEpsilonMicros));
    waiter_list.CancelAllWaiters();
  }  // Join threads.
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result1);
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result2);

  // Awake one waiter, cancel other.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread1(&result1);
    waiter_list.AddWaiter(thread1.waiter(), MOJO_WAIT_FLAG_READABLE, 2);
    thread1.Start();
    test::SimpleWaiterThread thread2(&result2);
    waiter_list.AddWaiter(thread2.waiter(), MOJO_WAIT_FLAG_WRITABLE, 3);
    thread2.Start();
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(2 * kEpsilonMicros));
    waiter_list.AwakeWaitersForStateChange(MOJO_WAIT_FLAG_READABLE,
                                           MOJO_WAIT_FLAG_READABLE |
                                              MOJO_WAIT_FLAG_WRITABLE);
    waiter_list.RemoveWaiter(thread1.waiter());
    waiter_list.CancelAllWaiters();
  }  // Join threads.
  EXPECT_EQ(2, result1);
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result2);

  // Cancel one waiter, awake other for unsatisfiability.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread1(&result1);
    waiter_list.AddWaiter(thread1.waiter(), MOJO_WAIT_FLAG_READABLE, 4);
    thread1.Start();
    test::SimpleWaiterThread thread2(&result2);
    waiter_list.AddWaiter(thread2.waiter(), MOJO_WAIT_FLAG_WRITABLE, 5);
    thread2.Start();
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(2 * kEpsilonMicros));
    waiter_list.AwakeWaitersForStateChange(0, MOJO_WAIT_FLAG_READABLE);
    waiter_list.RemoveWaiter(thread2.waiter());
    waiter_list.CancelAllWaiters();
  }  // Join threads.
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result1);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result2);

  // Cancel one waiter, awake other for unsatisfiability.
  {
    WaiterList waiter_list;
    test::SimpleWaiterThread thread1(&result1);
    waiter_list.AddWaiter(thread1.waiter(), MOJO_WAIT_FLAG_READABLE, 6);
    thread1.Start();

    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(1 * kEpsilonMicros));

    // Should do nothing.
    waiter_list.AwakeWaitersForStateChange(0,
                                           MOJO_WAIT_FLAG_READABLE |
                                               MOJO_WAIT_FLAG_WRITABLE);

    test::SimpleWaiterThread thread2(&result2);
    waiter_list.AddWaiter(thread2.waiter(), MOJO_WAIT_FLAG_WRITABLE, 7);
    thread2.Start();

    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(1 * kEpsilonMicros));

    // Awake #1.
    waiter_list.AwakeWaitersForStateChange(MOJO_WAIT_FLAG_READABLE,
                                           MOJO_WAIT_FLAG_READABLE |
                                               MOJO_WAIT_FLAG_WRITABLE);
    waiter_list.RemoveWaiter(thread1.waiter());

    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(1 * kEpsilonMicros));

    test::SimpleWaiterThread thread3(&result3);
    waiter_list.AddWaiter(thread3.waiter(), MOJO_WAIT_FLAG_WRITABLE, 8);
    thread3.Start();

    test::SimpleWaiterThread thread4(&result4);
    waiter_list.AddWaiter(thread4.waiter(), MOJO_WAIT_FLAG_READABLE, 9);
    thread4.Start();

    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(1 * kEpsilonMicros));

    // Awake #2 and #3 for unsatisfiability.
    waiter_list.AwakeWaitersForStateChange(0, MOJO_WAIT_FLAG_READABLE);
    waiter_list.RemoveWaiter(thread2.waiter());
    waiter_list.RemoveWaiter(thread3.waiter());

    // Cancel #4.
    waiter_list.CancelAllWaiters();
  }  // Join threads.
  EXPECT_EQ(6, result1);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result2);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result3);
  EXPECT_EQ(MOJO_RESULT_CANCELLED, result4);
}

}  // namespace
}  // namespace system
}  // namespace mojo
