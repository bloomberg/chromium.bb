// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/utility/mutex.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

TEST(MutexTest, TrivialSingleThreaded) {
  Mutex mutex;

  mutex.Lock();
  mutex.AssertHeld();
  mutex.Unlock();

  EXPECT_TRUE(mutex.TryLock());
  mutex.AssertHeld();
  mutex.Unlock();

  {
    MutexLock lock(&mutex);
    mutex.AssertHeld();
  }

  EXPECT_TRUE(mutex.TryLock());
  mutex.Unlock();
}

// Tests of assertions for Debug builds.
#if !defined(NDEBUG)
// Test |AssertHeld()| (which is an actual user API).
TEST(MutexTest, DebugAssertHeldFailure) {
  Mutex mutex;
  EXPECT_DEATH(mutex.AssertHeld(), "");
}

// Test other consistency checks.
TEST(MutexTest, DebugAssertionFailures) {
  // Unlock without lock held.
  EXPECT_DEATH({
    Mutex mutex;
    mutex.Unlock();
  }, "");

  // Lock with lock held (on same thread).
  EXPECT_DEATH({
    Mutex mutex;
    mutex.Lock();
    mutex.Lock();
  }, "");

  // Try lock with lock held.
  EXPECT_DEATH({
    Mutex mutex;
    mutex.Lock();
    mutex.TryLock();
  }, "");

  // Destroy lock with lock held.
  EXPECT_DEATH({
    Mutex mutex;
    mutex.Lock();
  }, "");
}
#endif  // !defined(NDEBUG)

// TODO(vtl): Add nontrivial tests when we have threads.

}  // namespace
}  // namespace mojo
