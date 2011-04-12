// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/memory/scoped_temp_dir.h"
#include "build/build_config.h"
#include "chrome/browser/importer/firefox_profile_lock.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/file_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

class FirefoxProfileLockTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  ScopedTempDir temp_dir_;
};

TEST_F(FirefoxProfileLockTest, LockTest) {
  FirefoxProfileLock lock1(temp_dir_.path());
  ASSERT_TRUE(lock1.HasAcquired());
  lock1.Unlock();
  ASSERT_FALSE(lock1.HasAcquired());
  lock1.Lock();
  ASSERT_TRUE(lock1.HasAcquired());
}

// Tests basic functionality and verifies that the lock file is deleted after
// use.
TEST_F(FirefoxProfileLockTest, ProfileLock) {
  FilePath test_path;
  ASSERT_TRUE(file_util::CreateNewTempDirectory(
      FILE_PATH_LITERAL("firefox_profile"), &test_path));
  FilePath lock_file_path = test_path;
  FileAutoDeleter deleter(lock_file_path);
  lock_file_path = lock_file_path.Append(FirefoxProfileLock::kLockFileName);

  scoped_ptr<FirefoxProfileLock> lock;
  EXPECT_EQ(static_cast<FirefoxProfileLock*>(NULL), lock.get());
  EXPECT_FALSE(file_util::PathExists(lock_file_path));
  lock.reset(new FirefoxProfileLock(test_path));
  EXPECT_TRUE(lock->HasAcquired());
  EXPECT_TRUE(file_util::PathExists(lock_file_path));
  lock->Unlock();
  EXPECT_FALSE(lock->HasAcquired());

  // In the posix code, we don't delete the file when releasing the lock.
#if !defined(OS_POSIX)
  EXPECT_FALSE(file_util::PathExists(lock_file_path));
#endif  // !defined(OS_POSIX)
  lock->Lock();
  EXPECT_TRUE(lock->HasAcquired());
  EXPECT_TRUE(file_util::PathExists(lock_file_path));
  lock->Lock();
  EXPECT_TRUE(lock->HasAcquired());
  lock->Unlock();
  EXPECT_FALSE(lock->HasAcquired());
  // In the posix code, we don't delete the file when releasing the lock.
#if !defined(OS_POSIX)
  EXPECT_FALSE(file_util::PathExists(lock_file_path));
#endif  // !defined(OS_POSIX)
}

// If for some reason the lock file is left behind by the previous owner, we
// should still be able to lock it, at least in the Windows implementation.
TEST_F(FirefoxProfileLockTest, ProfileLockOrphaned) {
  FilePath test_path;
  ASSERT_TRUE(file_util::CreateNewTempDirectory(
      FILE_PATH_LITERAL("firefox_profile"), &test_path));
  FilePath lock_file_path = test_path;
  FileAutoDeleter deleter(lock_file_path);
  lock_file_path = lock_file_path.Append(FirefoxProfileLock::kLockFileName);

  // Create the orphaned lock file.
  FILE* lock_file = file_util::OpenFile(lock_file_path, "w");
  ASSERT_TRUE(lock_file);
  file_util::CloseFile(lock_file);
  EXPECT_TRUE(file_util::PathExists(lock_file_path));

  scoped_ptr<FirefoxProfileLock> lock;
  EXPECT_EQ(static_cast<FirefoxProfileLock*>(NULL), lock.get());
  lock.reset(new FirefoxProfileLock(test_path));
  EXPECT_TRUE(lock->HasAcquired());
  lock->Unlock();
  EXPECT_FALSE(lock->HasAcquired());
}

// This is broken on POSIX since the same process is allowed to reacquire a
// lock.
#if !defined(OS_POSIX)
// Tests two locks contending for the same lock file.
TEST_F(FirefoxProfileLockTest, ProfileLockContention) {
  FilePath test_path;
  ASSERT_TRUE(file_util::CreateNewTempDirectory(
      FILE_PATH_LITERAL("firefox_profile"), &test_path));
  FileAutoDeleter deleter(test_path);

  scoped_ptr<FirefoxProfileLock> lock1;
  EXPECT_EQ(static_cast<FirefoxProfileLock*>(NULL), lock1.get());
  lock1.reset(new FirefoxProfileLock(test_path));
  EXPECT_TRUE(lock1->HasAcquired());

  scoped_ptr<FirefoxProfileLock> lock2;
  EXPECT_EQ(static_cast<FirefoxProfileLock*>(NULL), lock2.get());
  lock2.reset(new FirefoxProfileLock(test_path));
  EXPECT_FALSE(lock2->HasAcquired());

  lock1->Unlock();
  EXPECT_FALSE(lock1->HasAcquired());

  lock2->Lock();
  EXPECT_TRUE(lock2->HasAcquired());
  lock2->Unlock();
  EXPECT_FALSE(lock2->HasAcquired());
}
#endif
