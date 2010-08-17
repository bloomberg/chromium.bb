// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/process_watcher.h"

#if defined(OS_POSIX)
#include <sys/wait.h>

#include "base/eintr_wrapper.h"
#include "base/process_util.h"
#include "base/test/multiprocess_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

class ProcessWatcherTest : public base::MultiProcessTest {
};

namespace {

bool IsProcessDead(base::ProcessHandle child) {
  // waitpid() will actually reap the process which is exactly NOT what we
  // want to test for.  The good thing is that if it can't find the process
  // we'll get a nice value for errno which we can test for.
  const pid_t result = HANDLE_EINTR(waitpid(child, NULL, WNOHANG));
  return result == -1 && errno == ECHILD;
}

}  // namespace

TEST_F(ProcessWatcherTest, DelayedTermination) {
  base::ProcessHandle child_process =
      SpawnChild("process_watcher_test_never_die", false);
  ProcessWatcher::EnsureProcessTerminated(child_process);
  base::WaitForSingleProcess(child_process, 5000);

  // Check that process was really killed.
  EXPECT_TRUE(IsProcessDead(child_process));
  base::CloseProcessHandle(child_process);
}

MULTIPROCESS_TEST_MAIN(process_watcher_test_never_die) {
  while (1) {
    sleep(500);
  }
  return 0;
}

TEST_F(ProcessWatcherTest, ImmediateTermination) {
  base::ProcessHandle child_process =
      SpawnChild("process_watcher_test_die_immediately", false);
  // Give it time to die.
  sleep(2);
  ProcessWatcher::EnsureProcessTerminated(child_process);

  // Check that process was really killed.
  EXPECT_TRUE(IsProcessDead(child_process));
  base::CloseProcessHandle(child_process);
}

MULTIPROCESS_TEST_MAIN(process_watcher_test_die_immediately) {
  return 0;
}

#endif  // OS_POSIX
