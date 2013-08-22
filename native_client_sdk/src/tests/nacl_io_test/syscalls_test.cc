// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"

using namespace nacl_io;
using namespace sdk_util;

namespace {
class TestsWithKI : public ::testing::Test {
 public:
  TestsWithKI() {}

  void SetUp() {
    ki_init(&kp_);
  }

  void TearDown() {
    ki_uninit();
  }

 protected:
  KernelProxy kp_;
};

class SyscallsKill : public TestsWithKI {
};

class SyscallsSignal : public TestsWithKI {
};

TEST_F(SyscallsKill, KillSignals) {
  // SIGSEGV can't be sent via kill(2)
  ASSERT_EQ(-1, kill(0, SIGSEGV)) << "kill(SEGV) failed to return an error";
  ASSERT_EQ(EINVAL, errno) << "kill(SEGV) failedd to set errno to EINVAL";

  // Our implemenation should understand SIGWINCH
  ASSERT_EQ(0, kill(0, SIGWINCH)) << "kill(SIGWINCH) failed: " << errno;

  // And USR1/USR2
  ASSERT_EQ(0, kill(0, SIGUSR1)) << "kill(SIGUSR1) failed: " << errno;
  ASSERT_EQ(0, kill(0, SIGUSR2)) << "kill(SIGUSR2) failed: " << errno;
}

TEST_F(SyscallsKill, KillPIDValues) {
  // Any PID other than 0, -1 and getpid() should yield ESRCH
  // since there is only one valid process under NaCl
  int mypid = getpid();
  ASSERT_EQ(0, kill(0, SIGWINCH));
  ASSERT_EQ(0, kill(-1, SIGWINCH));
  ASSERT_EQ(0, kill(mypid, SIGWINCH));

  // Don't use mypid + 1 since getpid() actually returns -1
  // when the IRT interface is missing (e.g. within chrome),
  // and 0 is always a valid PID when calling kill().
  int invalid_pid = mypid + 10;
  ASSERT_EQ(-1, kill(invalid_pid, SIGWINCH));
  ASSERT_EQ(ESRCH, errno);
}

static bool g_handler_called = false;
void sighandler(int) {
  g_handler_called = true;
}

TEST_F(SyscallsSignal, SignalValues) {
  ASSERT_EQ(signal(SIGSEGV, sighandler), SIG_ERR)
      << "registering SEGV handler didn't fail";
  ASSERT_EQ(errno, EINVAL) << "signal(SEGV) failed to set errno to EINVAL";

  ASSERT_EQ(signal(-1, sighandler), SIG_ERR)
      << "registering handler for invalid signal didn't fail";
  ASSERT_EQ(errno, EINVAL) << "signal(-1) failed to set errno to EINVAL";
}

TEST_F(SyscallsSignal, HandlerValues) {
  // Unsupported signal.
  ASSERT_NE(SIG_ERR, signal(SIGSEGV, SIG_DFL));
  ASSERT_EQ(SIG_ERR, signal(SIGSEGV, SIG_IGN));
  ASSERT_EQ(SIG_ERR, signal(SIGSEGV, sighandler));

  // Supported signal.
  ASSERT_NE(SIG_ERR, signal(SIGWINCH, SIG_DFL));
  ASSERT_NE(SIG_ERR, signal(SIGWINCH, SIG_IGN));
  ASSERT_NE(SIG_ERR, signal(SIGWINCH, sighandler));
}

TEST_F(SyscallsSignal, Sigwinch) {
  g_handler_called = false;

  // Register WINCH handler
  sighandler_t newsig = sighandler;
  sighandler_t oldsig = signal(SIGWINCH, newsig);
  ASSERT_NE(oldsig, SIG_ERR);

  // Send signal.
  kill(0, SIGWINCH);

  // Verify that handler was called
  EXPECT_TRUE(g_handler_called);

  // Restore existing handler
  oldsig = signal(SIGWINCH, oldsig);

  // Verify the our newsig was returned as previous handler
  ASSERT_EQ(oldsig, newsig);
}

}  // namespace
