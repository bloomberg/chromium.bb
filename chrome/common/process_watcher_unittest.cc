// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/platform_thread.h"
#include "base/multiprocess_test.h"
#include "chrome/common/process_watcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ProcessWatcherTest : public MultiProcessTest {
};

MULTIPROCESS_TEST_MAIN(Sleep1ChildProcess) {
  PlatformThread::Sleep(1000);
  exit(0);
  return 0;
}

MULTIPROCESS_TEST_MAIN(Sleep3ChildProcess) {
  PlatformThread::Sleep(3000);
  exit(0);
  return 0;
}

TEST_F(ProcessWatcherTest, DiesBeforeTermination) {
  base::ProcessHandle handle = this->SpawnChild(L"Sleep1ChildProcess");
  ASSERT_NE(static_cast<base::ProcessHandle>(NULL), handle);

  ProcessWatcher::EnsureProcessTerminated(handle);

  PlatformThread::Sleep(2500);
  // Normally we don't touch |handle| after calling EnsureProcessTerminated,
  // but we know the EnsureProcessTerminated process finishes in 2000 ms, so
  // it's safe to do so now. Same for Terminated test case below.
  EXPECT_FALSE(base::CrashAwareSleep(handle, 0));
}

TEST_F(ProcessWatcherTest, Terminated) {
  base::ProcessHandle handle = this->SpawnChild(L"Sleep3ChildProcess");
  ASSERT_NE(static_cast<base::ProcessHandle>(NULL), handle);

  ProcessWatcher::EnsureProcessTerminated(handle);

  PlatformThread::Sleep(2500);
  EXPECT_FALSE(base::CrashAwareSleep(handle, 0));
}

TEST_F(ProcessWatcherTest, NotTerminated) {
  base::ProcessHandle handle = this->SpawnChild(L"Sleep3ChildProcess");
  ASSERT_NE(static_cast<base::ProcessHandle>(NULL), handle);

  EXPECT_TRUE(base::CrashAwareSleep(handle, 2500));
  EXPECT_FALSE(base::CrashAwareSleep(handle, 1000));
}

}  // namespace
