// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/libc_close_tracking.h"

#include <errno.h>
#include <unistd.h>

#include "base/debug/stack_trace.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestData[] = "test data";

// Helper class to enable close tracking on construction and turns it off
// on destruction.
class ScopedCloseTracking {
 public:
  ScopedCloseTracking() { chromeos::InitCloseTracking(); }
  ~ScopedCloseTracking() { chromeos::ShutdownCloseTracking(); }
};

class CloseTrackingTest : public testing::Test {
 public:
  CloseTrackingTest() = default;
  ~CloseTrackingTest() override = default;

  // testing::Test
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  // Creates a file, writes some data, close it and verifies that the
  // underlying fd is closed by attempting to write after close.
  void TestFileClose(base::PlatformFile* out_fd) {
    base::FilePath file_path = temp_dir_.GetPath().AppendASCII("test_file");
    base::File file(file_path,
                    base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);

    ASSERT_TRUE(file.IsValid());
    const base::PlatformFile fd = file.GetPlatformFile();
    EXPECT_EQ(static_cast<ssize_t>(arraysize(kTestData)),
              write(fd, kTestData, arraysize(kTestData)));
    file.Close();

    EXPECT_EQ(-1, write(fd, kTestData, arraysize(kTestData)));
    EXPECT_EQ(errno, EBADF);

    if (out_fd)
      *out_fd = fd;
  }

 private:
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(CloseTrackingTest);
};

// Test that a file can be closed normally with no stack tracking.
TEST_F(CloseTrackingTest, CloseWithNoTracking) {
  TestFileClose(nullptr);
}

// Test that a file can be closed normally with stack tracking and a StackTrace
// is collected.
TEST_F(CloseTrackingTest, CloseWithTracking) {
  ScopedCloseTracking tracking;

  base::PlatformFile fd;
  TestFileClose(&fd);

  const base::debug::StackTrace* stack = chromeos::GetLastCloseStackForTest(fd);
  EXPECT_NE(nullptr, stack);
}

// Test that we crash on bad close with expected error.
TEST_F(CloseTrackingTest, CrashOn2ndClose) {
  ScopedCloseTracking tracking;

  base::PlatformFile fd;
  TestFileClose(&fd);

  const base::debug::StackTrace* stack = chromeos::GetLastCloseStackForTest(fd);
  EXPECT_NE(nullptr, stack);

  // Disable pid check because death test runs in a forked process and have
  // a different pid than where InitCloseTracking() is called above.
  chromeos::SetCloseTrackingIgnorePidForTest(true);

  EXPECT_DEATH_IF_SUPPORTED(close(fd), "Failed to close a fd");

  // Last good close stack on the fd is not changed.
  const base::debug::StackTrace* stack_after_bad_close =
      chromeos::GetLastCloseStackForTest(fd);
  EXPECT_EQ(stack, stack_after_bad_close);
}

}  // namespace
