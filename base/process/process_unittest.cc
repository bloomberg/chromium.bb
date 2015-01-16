// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_LINUX)
#include <errno.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

#if defined(OS_WIN)
const int kExpectedStillRunningExitCode = 0x102;
#else
const int kExpectedStillRunningExitCode = 0;
#endif

}  // namespace

namespace base {

class ProcessTest : public MultiProcessTest {
};

TEST_F(ProcessTest, Create) {
  Process process(SpawnChild("SimpleChildProcess"));
  ASSERT_TRUE(process.IsValid());
  ASSERT_FALSE(process.is_current());
  process.Close();
  ASSERT_FALSE(process.IsValid());
}

TEST_F(ProcessTest, CreateCurrent) {
  Process process = Process::Current();
  ASSERT_TRUE(process.IsValid());
  ASSERT_TRUE(process.is_current());
  process.Close();
  ASSERT_FALSE(process.IsValid());
}

TEST_F(ProcessTest, Move) {
  Process process1(SpawnChild("SimpleChildProcess"));
  EXPECT_TRUE(process1.IsValid());

  Process process2;
  EXPECT_FALSE(process2.IsValid());

  process2 = process1.Pass();
  EXPECT_TRUE(process2.IsValid());
  EXPECT_FALSE(process1.IsValid());
  EXPECT_FALSE(process2.is_current());

  Process process3 = Process::Current();
  process2 = process3.Pass();
  EXPECT_TRUE(process2.is_current());
  EXPECT_TRUE(process2.IsValid());
  EXPECT_FALSE(process3.IsValid());
}

TEST_F(ProcessTest, Duplicate) {
  Process process1(SpawnChild("SimpleChildProcess"));
  ASSERT_TRUE(process1.IsValid());

  Process process2 = process1.Duplicate();
  ASSERT_TRUE(process1.IsValid());
  ASSERT_TRUE(process2.IsValid());
  EXPECT_EQ(process1.pid(), process2.pid());
  EXPECT_FALSE(process1.is_current());
  EXPECT_FALSE(process2.is_current());

  process1.Close();
  ASSERT_TRUE(process2.IsValid());
}

TEST_F(ProcessTest, DuplicateCurrent) {
  Process process1 = Process::Current();
  ASSERT_TRUE(process1.IsValid());

  Process process2 = process1.Duplicate();
  ASSERT_TRUE(process1.IsValid());
  ASSERT_TRUE(process2.IsValid());
  EXPECT_EQ(process1.pid(), process2.pid());
  EXPECT_TRUE(process1.is_current());
  EXPECT_TRUE(process2.is_current());

  process1.Close();
  ASSERT_TRUE(process2.IsValid());
}

TEST_F(ProcessTest, DeprecatedGetProcessFromHandle) {
  Process process1(SpawnChild("SimpleChildProcess"));
  ASSERT_TRUE(process1.IsValid());

  Process process2 = Process::DeprecatedGetProcessFromHandle(process1.Handle());
  ASSERT_TRUE(process1.IsValid());
  ASSERT_TRUE(process2.IsValid());
  EXPECT_EQ(process1.pid(), process2.pid());
  EXPECT_FALSE(process1.is_current());
  EXPECT_FALSE(process2.is_current());

  process1.Close();
  ASSERT_TRUE(process2.IsValid());
}

MULTIPROCESS_TEST_MAIN(SleepyChildProcess) {
  PlatformThread::Sleep(TestTimeouts::action_max_timeout());
  return 0;
}

TEST_F(ProcessTest, Terminate) {
  Process process(SpawnChild("SleepyChildProcess"));
  ASSERT_TRUE(process.IsValid());

  const int kDummyExitCode = 42;
  int exit_code = kDummyExitCode;
  EXPECT_EQ(TERMINATION_STATUS_STILL_RUNNING,
            GetTerminationStatus(process.Handle(), &exit_code));
  EXPECT_EQ(kExpectedStillRunningExitCode, exit_code);

  exit_code = kDummyExitCode;
  int kExpectedExitCode = 250;
  process.Terminate(kExpectedExitCode);
  WaitForSingleProcess(process.Handle(), TestTimeouts::action_max_timeout());

  EXPECT_NE(TERMINATION_STATUS_STILL_RUNNING,
            GetTerminationStatus(process.Handle(), &exit_code));
#if !defined(OS_POSIX)
  // The POSIX implementation actually ignores the exit_code.
  EXPECT_EQ(kExpectedExitCode, exit_code);
#endif
}

MULTIPROCESS_TEST_MAIN(FastSleepyChildProcess) {
  PlatformThread::Sleep(TestTimeouts::tiny_timeout() * 10);
  return 0;
}

TEST_F(ProcessTest, WaitForExit) {
  Process process(SpawnChild("FastSleepyChildProcess"));
  ASSERT_TRUE(process.IsValid());

  const int kDummyExitCode = 42;
  int exit_code = kDummyExitCode;
  EXPECT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(0, exit_code);
}

TEST_F(ProcessTest, WaitForExitWithTimeout) {
  Process process(SpawnChild("SleepyChildProcess"));
  ASSERT_TRUE(process.IsValid());

  const int kDummyExitCode = 42;
  int exit_code = kDummyExitCode;
  TimeDelta timeout = TestTimeouts::tiny_timeout();
  EXPECT_FALSE(process.WaitForExitWithTimeout(timeout, &exit_code));
  EXPECT_EQ(kDummyExitCode, exit_code);

  process.Terminate(kDummyExitCode);
}

// Ensure that the priority of a process is restored correctly after
// backgrounding and restoring.
// Note: a platform may not be willing or able to lower the priority of
// a process. The calls to SetProcessBackground should be noops then.
TEST_F(ProcessTest, SetProcessBackgrounded) {
  Process process(SpawnChild("SimpleChildProcess"));
  int old_priority = process.GetPriority();
#if defined(OS_WIN)
  EXPECT_TRUE(process.SetProcessBackgrounded(true));
  EXPECT_TRUE(process.IsProcessBackgrounded());
  EXPECT_TRUE(process.SetProcessBackgrounded(false));
  EXPECT_FALSE(process.IsProcessBackgrounded());
#else
  process.SetProcessBackgrounded(true);
  process.SetProcessBackgrounded(false);
#endif
  int new_priority = process.GetPriority();
  EXPECT_EQ(old_priority, new_priority);
}

// Same as SetProcessBackgrounded but to this very process. It uses
// a different code path at least for Windows.
TEST_F(ProcessTest, SetProcessBackgroundedSelf) {
  Process process = Process::Current();
  int old_priority = process.GetPriority();
#if defined(OS_WIN)
  EXPECT_TRUE(process.SetProcessBackgrounded(true));
  EXPECT_TRUE(process.IsProcessBackgrounded());
  EXPECT_TRUE(process.SetProcessBackgrounded(false));
  EXPECT_FALSE(process.IsProcessBackgrounded());
#else
  process.SetProcessBackgrounded(true);
  process.SetProcessBackgrounded(false);
#endif
  int new_priority = process.GetPriority();
  EXPECT_EQ(old_priority, new_priority);
}

#if defined(OS_LINUX)
const int kSuccess = 0;

MULTIPROCESS_TEST_MAIN(CheckPidProcess) {
  const pid_t kInitPid = 1;
  const pid_t pid = syscall(__NR_getpid);
  CHECK(pid == kInitPid);
  CHECK(getpid() == pid);
  return kSuccess;
}

TEST_F(ProcessTest, CloneFlags) {
  if (RunningOnValgrind() || !PathExists(FilePath("/proc/self/ns/user")) ||
      !PathExists(FilePath("/proc/self/ns/pid"))) {
    // User or PID namespaces are not supported.
    return;
  }

  LaunchOptions options;
  options.clone_flags = CLONE_NEWUSER | CLONE_NEWPID;

  Process process(SpawnChildWithOptions("CheckPidProcess", options));
  ASSERT_TRUE(process.IsValid());

  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(kSuccess, exit_code);
}

TEST(ForkWithFlagsTest, UpdatesPidCache) {
  // The libc clone function, which allows ForkWithFlags to keep the pid cache
  // up to date, does not work on Valgrind.
  if (RunningOnValgrind()) {
    return;
  }

  // Warm up the libc pid cache, if there is one.
  ASSERT_EQ(syscall(__NR_getpid), getpid());

  pid_t ctid = 0;
  const pid_t pid = ForkWithFlags(SIGCHLD | CLONE_CHILD_SETTID, nullptr, &ctid);
  if (pid == 0) {
    // In child.  Check both the raw getpid syscall and the libc getpid wrapper
    // (which may rely on a pid cache).
    RAW_CHECK(syscall(__NR_getpid) == ctid);
    RAW_CHECK(getpid() == ctid);
    _exit(kSuccess);
  }

  ASSERT_NE(-1, pid);
  int status = 42;
  ASSERT_EQ(pid, HANDLE_EINTR(waitpid(pid, &status, 0)));
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(kSuccess, WEXITSTATUS(status));
}
#endif

#if defined(OS_POSIX)
const char kPipeValue = '\xcc';

class ReadFromPipeDelegate : public LaunchOptions::PreExecDelegate {
 public:
  explicit ReadFromPipeDelegate(int fd) : fd_(fd) {}
  ~ReadFromPipeDelegate() override {}
  void RunAsyncSafe() override {
    char c;
    RAW_CHECK(HANDLE_EINTR(read(fd_, &c, 1)) == 1);
    RAW_CHECK(IGNORE_EINTR(close(fd_)) == 0);
    RAW_CHECK(c == kPipeValue);
  }

 private:
  int fd_;
  DISALLOW_COPY_AND_ASSIGN(ReadFromPipeDelegate);
};

TEST_F(ProcessTest, PreExecHook) {
  int pipe_fds[2];
  ASSERT_EQ(0, pipe(pipe_fds));

  ScopedFD read_fd(pipe_fds[0]);
  ScopedFD write_fd(pipe_fds[1]);
  base::FileHandleMappingVector fds_to_remap;
  fds_to_remap.push_back(std::make_pair(read_fd.get(), read_fd.get()));

  ReadFromPipeDelegate read_from_pipe_delegate(read_fd.get());
  LaunchOptions options;
  options.fds_to_remap = &fds_to_remap;
  options.pre_exec_delegate = &read_from_pipe_delegate;
  Process process(SpawnChildWithOptions("SimpleChildProcess", options));
  ASSERT_TRUE(process.IsValid());

  read_fd.reset();
  ASSERT_EQ(1, HANDLE_EINTR(write(write_fd.get(), &kPipeValue, 1)));

  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(0, exit_code);
}
#endif  // defined(OS_POSIX)

}  // namespace base
