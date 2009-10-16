// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests of AtomicFlag class.

#include "base/atomic_flag.h"
#include "base/logging.h"
#include "base/spin_wait.h"
#include "base/time.h"
#include "base/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::AtomicFlag;
using base::TimeDelta;
using base::Thread;

namespace {

//------------------------------------------------------------------------------
// Define our test class.
//------------------------------------------------------------------------------

class NotifyTask : public Task {
 public:
  explicit NotifyTask(AtomicFlag* flag) : flag_(flag) {
  }
  virtual void Run() {
    flag_->Set();
  }
 private:
  AtomicFlag* flag_;
};

TEST(AtomicFlagTest, SimpleSingleThreadedTest) {
  AtomicFlag flag;
  CHECK(!flag.IsSet());
  flag.Set();
  CHECK(flag.IsSet());
}

TEST(AtomicFlagTest, SimpleSingleThreadedTestPrenotified) {
  AtomicFlag flag(true);
  CHECK(flag.IsSet());
}

#if defined(OS_WIN)
#define DISABLED_ON_WIN(x) DISABLED_##x
#else
#define DISABLED_ON_WIN(x) x
#endif

// AtomicFlag should die on a DCHECK if Set() is called more than once.
// This test isn't Windows-friendly yet since ASSERT_DEATH doesn't catch tests
// failed on DCHECK. See http://crbug.com/24885 for the details.
TEST(AtomicFlagTest, DISABLED_ON_WIN(DoubleSetDeathTest)) {
  // Checks that Set() can't be called more than once.
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  AtomicFlag flag;
  flag.Set();
  ASSERT_DEBUG_DEATH(flag.Set(), "");
}

TEST(AtomicFlagTest, SimpleThreadedTest) {
  Thread t("AtomicFlagTest.SimpleThreadedTest");
  EXPECT_TRUE(t.Start());
  EXPECT_TRUE(t.message_loop());
  EXPECT_TRUE(t.IsRunning());

  AtomicFlag flag;
  CHECK(!flag.IsSet());
  t.message_loop()->PostTask(FROM_HERE, new NotifyTask(&flag));
  SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(TimeDelta::FromSeconds(10),
                                   flag.IsSet());
  CHECK(flag.IsSet());
}

}  // namespace
