// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/test/multiprocess_test_base.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "mojo/system/scoped_platform_handle.h"

#if defined(OS_POSIX)
#include <fcntl.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#endif

namespace mojo {
namespace {

typedef test::MultiprocessTestBase MultiprocessTestBaseTest;

TEST_F(MultiprocessTestBaseTest, RunChild) {
// TODO(vtl): Not implemented on Windows yet.
#if defined(OS_POSIX)
  EXPECT_TRUE(server_platform_handle.is_valid());
#endif
  StartChild("RunChild");
  EXPECT_EQ(123, WaitForChildShutdown());
}

MOJO_MULTIPROCESS_TEST_CHILD_MAIN(RunChild) {
// TODO(vtl): Not implemented on Windows yet.
#if defined(OS_POSIX)
  CHECK(MultiprocessTestBaseTest::client_platform_handle.is_valid());
#endif
  return 123;
}

TEST_F(MultiprocessTestBaseTest, TestChildMainNotFound) {
  StartChild("NoSuchTestChildMain");
  int result = WaitForChildShutdown();
  EXPECT_FALSE(result >= 0 && result <= 127);
}

// POSIX-specific test of passed handle ----------------------------------------

#if defined(OS_POSIX)
TEST_F(MultiprocessTestBaseTest, PassedChannelPosix) {
  EXPECT_TRUE(server_platform_handle.is_valid());
  StartChild("PassedChannelPosix");

  // Take ownership of the FD.
  system::ScopedPlatformHandle handle = server_platform_handle.Pass();
  int fd = handle.get().fd;

  // The FD should be non-blocking. Check this.
  CHECK((fcntl(fd, F_GETFL) & O_NONBLOCK));
  // We're lazy. Set it to block.
  PCHECK(fcntl(fd, F_SETFL, 0) == 0);

  // Write a byte.
  const char c = 'X';
  ssize_t write_size = HANDLE_EINTR(write(fd, &c, 1));
  EXPECT_EQ(1, write_size);

  // It'll echo it back to us, incremented.
  char d = 0;
  ssize_t read_size = HANDLE_EINTR(read(fd, &d, 1));
  EXPECT_EQ(1, read_size);
  EXPECT_EQ(c + 1, d);

  // And return it, incremented again.
  EXPECT_EQ(c + 2, WaitForChildShutdown());
}

MOJO_MULTIPROCESS_TEST_CHILD_MAIN(PassedChannelPosix) {
  CHECK(MultiprocessTestBaseTest::client_platform_handle.is_valid());

  // Take ownership of the FD.
  system::ScopedPlatformHandle handle =
      MultiprocessTestBaseTest::client_platform_handle.Pass();
  int fd = handle.get().fd;

  // The FD should still be non-blocking. Check this.
  CHECK((fcntl(fd, F_GETFL) & O_NONBLOCK));
  // We're lazy. Set it to block.
  PCHECK(fcntl(fd, F_SETFL, 0) == 0);

  // Read a byte.
  char c = 0;
  ssize_t read_size = HANDLE_EINTR(read(fd, &c, 1));
  CHECK_EQ(read_size, 1);

  // Write it back, incremented.
  c++;
  ssize_t write_size = HANDLE_EINTR(write(fd, &c, 1));
  CHECK_EQ(write_size, 1);

  // And return it, incremented again.
  c++;
  return static_cast<int>(c);
}
#endif  // defined(OS_POSIX)

}  // namespace
}  // namespace mojo
