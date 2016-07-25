// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/atomic_flag.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

// Death tests misbehave on Android.
// TODO(fdoray): Remove this when https://codereview.chromium.org/2162053006
// lands.
#if DCHECK_IS_ON() && defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)
#define EXPECT_DCHECK_DEATH(statement, regex) EXPECT_DEATH(statement, regex)
#else
#define EXPECT_DCHECK_DEATH(statement, regex)
#endif

namespace base {

namespace {

void ExpectSetFlagDeath(AtomicFlag* flag) {
  ASSERT_TRUE(flag);
  EXPECT_DCHECK_DEATH(flag->Set(), "");
}

void BusyWaitUntilFlagIsSet(AtomicFlag* flag) {
  while (!flag->IsSet())
    PlatformThread::YieldCurrentThread();
}

}  // namespace

TEST(AtomicFlagTest, SimpleSingleThreadedTest) {
  AtomicFlag flag;
  ASSERT_FALSE(flag.IsSet());
  flag.Set();
  ASSERT_TRUE(flag.IsSet());
}

TEST(AtomicFlagTest, DoubleSetTest) {
  AtomicFlag flag;
  ASSERT_FALSE(flag.IsSet());
  flag.Set();
  ASSERT_TRUE(flag.IsSet());
  flag.Set();
  ASSERT_TRUE(flag.IsSet());
}

TEST(AtomicFlagTest, ReadFromDifferentThread) {
  AtomicFlag flag;

  Thread thread("AtomicFlagTest.ReadFromDifferentThread");
  ASSERT_TRUE(thread.Start());
  thread.task_runner()->PostTask(FROM_HERE,
                                 Bind(&BusyWaitUntilFlagIsSet, &flag));

  // To verify that IsSet() fetches the flag's value from memory every time it
  // is called (not just the first time that it is called on a thread), sleep
  // before setting the flag.
  PlatformThread::Sleep(TimeDelta::FromMilliseconds(25));

  flag.Set();

  // The |thread|'s destructor will block until the posted task completes, so
  // the test will time out if it fails to see the flag be set.
}

TEST(AtomicFlagTest, SetOnDifferentThreadDeathTest) {
  // Checks that Set() can't be called from any other thread. AtomicFlag should
  // die on a DCHECK if Set() is called from other thread.
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  Thread t("AtomicFlagTest.SetOnDifferentThreadDeathTest");
  ASSERT_TRUE(t.Start());

  AtomicFlag flag;
  t.task_runner()->PostTask(FROM_HERE, Bind(&ExpectSetFlagDeath, &flag));
}

}  // namespace base
