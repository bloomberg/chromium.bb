// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>

#include "chrome/browser/process_singleton.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/platform_test.h"

namespace {

class ProcessSingletonMacTest : public PlatformTest {
 public:
  virtual void SetUp() {
    PlatformTest::SetUp();

    // Put the lock in a temporary directory.  Doesn't need to be a
    // full profile to test this code.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    lock_path_ = temp_dir_.path().Append(chrome::kSingletonLockFilename);
  }

  virtual void TearDown() {
    PlatformTest::TearDown();

    // Verify that the lock was released.
    EXPECT_FALSE(IsLocked());
  }

  // Return |true| if the file exists and is locked.  Forces a failure
  // in the containing test in case of error condition.
  bool IsLocked() {
    int fd = HANDLE_EINTR(open(lock_path_.value().c_str(), O_RDONLY));
    if (fd == -1) {
      EXPECT_EQ(ENOENT, errno) << "Unexpected error opening lockfile.";
      return false;
    }

    file_util::ScopedFD auto_close(&fd);

    int rc = HANDLE_EINTR(flock(fd, LOCK_EX|LOCK_NB));

    // Got the lock, so it wasn't already locked.  Close releases.
    if (rc != -1)
      return false;

    // Someone else has the lock.
    if (errno == EWOULDBLOCK)
      return true;

    EXPECT_EQ(EWOULDBLOCK, errno) << "Unexpected error acquiring lock.";
    return false;
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath lock_path_;
};

// Test that the base case doesn't blow up.
TEST_F(ProcessSingletonMacTest, Basic) {
  ProcessSingleton ps(temp_dir_.path(),
                      ProcessSingleton::NotificationCallback());
  EXPECT_FALSE(IsLocked());
  EXPECT_TRUE(ps.Create());
  EXPECT_TRUE(IsLocked());
  ps.Cleanup();
  EXPECT_FALSE(IsLocked());
}

// The destructor should release the lock.
TEST_F(ProcessSingletonMacTest, DestructorReleases) {
  EXPECT_FALSE(IsLocked());
  {
    ProcessSingleton ps(temp_dir_.path(),
                        ProcessSingleton::NotificationCallback());
    EXPECT_TRUE(ps.Create());
    EXPECT_TRUE(IsLocked());
  }
  EXPECT_FALSE(IsLocked());
}

// Multiple singletons should interlock appropriately.
TEST_F(ProcessSingletonMacTest, Interlock) {
  ProcessSingleton ps1(temp_dir_.path(),
                       ProcessSingleton::NotificationCallback());
  ProcessSingleton ps2(temp_dir_.path(),
                       ProcessSingleton::NotificationCallback());

  // Windows and Linux use a command-line flag to suppress this, but
  // it is on a sub-process so the scope is contained.  Rather than
  // add additional API to process_singleton.h in an #ifdef, just tell
  // the reader what to expect and move on.
  LOG(ERROR) << "Expect two failures to obtain the lock.";

  // When |ps1| has the lock, |ps2| cannot get it.
  EXPECT_FALSE(IsLocked());
  EXPECT_TRUE(ps1.Create());
  EXPECT_TRUE(IsLocked());
  EXPECT_FALSE(ps2.Create());
  ps1.Cleanup();

  // And when |ps2| has the lock, |ps1| cannot get it.
  EXPECT_FALSE(IsLocked());
  EXPECT_TRUE(ps2.Create());
  EXPECT_TRUE(IsLocked());
  EXPECT_FALSE(ps1.Create());
  ps2.Cleanup();
  EXPECT_FALSE(IsLocked());
}

// Like |Interlock| test, but via |NotifyOtherProcessOrCreate()|.
TEST_F(ProcessSingletonMacTest, NotifyOtherProcessOrCreate) {
  ProcessSingleton ps1(temp_dir_.path(),
                       ProcessSingleton::NotificationCallback());
  ProcessSingleton ps2(temp_dir_.path(),
                       ProcessSingleton::NotificationCallback());

  // Windows and Linux use a command-line flag to suppress this, but
  // it is on a sub-process so the scope is contained.  Rather than
  // add additional API to process_singleton.h in an #ifdef, just tell
  // the reader what to expect and move on.
  LOG(ERROR) << "Expect two failures to obtain the lock.";

  // When |ps1| has the lock, |ps2| cannot get it.
  EXPECT_FALSE(IsLocked());
  EXPECT_EQ(
      ProcessSingleton::PROCESS_NONE,
      ps1.NotifyOtherProcessOrCreate());
  EXPECT_TRUE(IsLocked());
  EXPECT_EQ(
      ProcessSingleton::PROFILE_IN_USE,
      ps2.NotifyOtherProcessOrCreate());
  ps1.Cleanup();

  // And when |ps2| has the lock, |ps1| cannot get it.
  EXPECT_FALSE(IsLocked());
  EXPECT_EQ(
      ProcessSingleton::PROCESS_NONE,
      ps2.NotifyOtherProcessOrCreate());
  EXPECT_TRUE(IsLocked());
  EXPECT_EQ(
      ProcessSingleton::PROFILE_IN_USE,
      ps1.NotifyOtherProcessOrCreate());
  ps2.Cleanup();
  EXPECT_FALSE(IsLocked());
}

// TODO(shess): Test that the lock is released when the process dies.
// DEATH_TEST?  I don't know.  If the code to communicate between
// browser processes is ever written, this all would need to be tested
// more like the other platforms, in which case it would be easy.

}  // namespace
