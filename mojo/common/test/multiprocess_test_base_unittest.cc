// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/test/multiprocess_test_base.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "mojo/common/test/test_utils.h"
#include "mojo/system/embedder/scoped_platform_handle.h"

#if defined(OS_POSIX)
#include <fcntl.h>
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace mojo {
namespace test {
namespace {

// Returns true and logs a warning on Windows prior to Vista.
bool SkipTest() {
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    LOG(WARNING) << "Test skipped: Vista or later needed.";
    return true;
  }
#endif

  return false;
}

bool IsNonBlocking(const embedder::PlatformHandle& handle) {
#if defined(OS_WIN)
  // Haven't figured out a way to query whether a HANDLE was created with
  // FILE_FLAG_OVERLAPPED.
  return true;
#else
  return fcntl(handle.fd, F_GETFL) & O_NONBLOCK;
#endif
}

bool WriteByte(const embedder::PlatformHandle& handle, char c) {
  size_t bytes_written = 0;
  BlockingWrite(handle, &c, 1, &bytes_written);
  return bytes_written == 1;
}

bool ReadByte(const embedder::PlatformHandle& handle, char* c) {
  size_t bytes_read = 0;
  BlockingRead(handle, c, 1, &bytes_read);
  return bytes_read == 1;
}

typedef MultiprocessTestBase MultiprocessTestBaseTest;

TEST_F(MultiprocessTestBaseTest, RunChild) {
  if (SkipTest())
    return;

  EXPECT_TRUE(server_platform_handle.is_valid());

  StartChild("RunChild");
  EXPECT_EQ(123, WaitForChildShutdown());
}

MOJO_MULTIPROCESS_TEST_CHILD_MAIN(RunChild) {
  CHECK(MultiprocessTestBaseTest::client_platform_handle.is_valid());
  return 123;
}

TEST_F(MultiprocessTestBaseTest, TestChildMainNotFound) {
  if (SkipTest())
    return;

  StartChild("NoSuchTestChildMain");
  int result = WaitForChildShutdown();
  EXPECT_FALSE(result >= 0 && result <= 127);
}

TEST_F(MultiprocessTestBaseTest, PassedChannel) {
  if (SkipTest())
    return;

  EXPECT_TRUE(server_platform_handle.is_valid());
  StartChild("PassedChannel");

  // Take ownership of the handle.
  embedder::ScopedPlatformHandle handle = server_platform_handle.Pass();

  // The handle should be non-blocking.
  EXPECT_TRUE(IsNonBlocking(handle.get()));

  // Write a byte.
  const char c = 'X';
  EXPECT_TRUE(WriteByte(handle.get(), c));

  // It'll echo it back to us, incremented.
  char d = 0;
  EXPECT_TRUE(ReadByte(handle.get(), &d));
  EXPECT_EQ(c + 1, d);

  // And return it, incremented again.
  EXPECT_EQ(c + 2, WaitForChildShutdown());
}

MOJO_MULTIPROCESS_TEST_CHILD_MAIN(PassedChannel) {
  CHECK(MultiprocessTestBaseTest::client_platform_handle.is_valid());

  // Take ownership of the handle.
  embedder::ScopedPlatformHandle handle =
      MultiprocessTestBaseTest::client_platform_handle.Pass();

  // The handle should be non-blocking.
  EXPECT_TRUE(IsNonBlocking(handle.get()));

  // Read a byte.
  char c = 0;
  EXPECT_TRUE(ReadByte(handle.get(), &c));

  // Write it back, incremented.
  c++;
  EXPECT_TRUE(WriteByte(handle.get(), c));

  // And return it, incremented again.
  c++;
  return static_cast<int>(c);
}

}  // namespace
}  // namespace test
}  // namespace mojo
