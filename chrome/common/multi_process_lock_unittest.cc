// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/kill.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/multiprocess_test.h"
#include "base/time/time.h"
#include "chrome/common/multi_process_lock.h"
#include "testing/multiprocess_func_list.h"

class MultiProcessLockTest : public base::MultiProcessTest {
 public:
  static const char kLockEnviromentVarName[];

  class ScopedEnvironmentVariable {
   public:
    ScopedEnvironmentVariable(const std::string &name,
                              const std::string &value)
        : name_(name), environment_(base::Environment::Create()) {
      environment_->SetVar(name_.c_str(), value);
    }
    ~ScopedEnvironmentVariable() {
      environment_->UnSetVar(name_.c_str());
    }

   private:
    std::string name_;
    scoped_ptr<base::Environment> environment_;
    DISALLOW_COPY_AND_ASSIGN(ScopedEnvironmentVariable);
  };

  std::string GenerateLockName();
  void ExpectLockIsLocked(const std::string &name);
  void ExpectLockIsUnlocked(const std::string &name);
};

const char MultiProcessLockTest::kLockEnviromentVarName[]
    = "MULTI_PROCESS_TEST_LOCK_NAME";

std::string MultiProcessLockTest::GenerateLockName() {
  base::Time now = base::Time::NowFromSystemTime();
  return base::StringPrintf("multi_process_test_lock %lf%lf",
                            now.ToDoubleT(), base::RandDouble());
}

void MultiProcessLockTest::ExpectLockIsLocked(const std::string &name) {
  ScopedEnvironmentVariable var(kLockEnviromentVarName, name);
  base::ProcessHandle handle = SpawnChild("MultiProcessLockTryFailMain");
  ASSERT_TRUE(handle);
  int exit_code = 0;
  EXPECT_TRUE(base::WaitForExitCode(handle, &exit_code));
  EXPECT_EQ(exit_code, 0);
}

void MultiProcessLockTest::ExpectLockIsUnlocked(
    const std::string &name) {
  ScopedEnvironmentVariable var(kLockEnviromentVarName, name);
  base::ProcessHandle handle = SpawnChild("MultiProcessLockTrySucceedMain");
  ASSERT_TRUE(handle);
  int exit_code = 0;
  EXPECT_TRUE(base::WaitForExitCode(handle, &exit_code));
  EXPECT_EQ(exit_code, 0);
}

TEST_F(MultiProcessLockTest, BasicCreationTest) {
  // Test basic creation/destruction with no lock taken
  std::string name = GenerateLockName();
  scoped_ptr<MultiProcessLock> scoped(MultiProcessLock::Create(name));
  ExpectLockIsUnlocked(name);
  scoped.reset(NULL);
}

TEST_F(MultiProcessLockTest, LongNameTest) {
  // Every platform has has it's own max path name size,
  // so different checks are needed for them.
  // POSIX: sizeof(address.sun_path) - 2
  // Mac OS X: BOOTSTRAP_MAX_NAME_LEN
  // Windows: MAX_PATH
  LOG(INFO) << "Following error log due to long name is expected";
#if defined(OS_MACOSX)
  std::string name("This is a name that is longer than one hundred and "
      "twenty-eight characters to make sure that we fail appropriately on "
      "Mac OS X when we have a path that is too long for Mac OS X to handle");
#elif defined(OS_POSIX)
  std::string name("This is a name that is longer than one hundred and eight "
      "characters to make sure that we fail appropriately on POSIX systems "
      "when we have a path that is too long for the system to handle");
#elif defined(OS_WIN)
  std::string name("This is a name that is longer than two hundred and sixty "
      "characters to make sure that we fail appropriately on Windows when we "
      "have a path that is too long for Windows to handle "
      "This limitation comes from the MAX_PATH definition which is obviously "
      "defined to be a maximum of two hundred and sixty characters ");
#endif
  scoped_ptr<MultiProcessLock> test_lock(MultiProcessLock::Create(name));
  EXPECT_FALSE(test_lock->TryLock());
}

TEST_F(MultiProcessLockTest, SimpleLock) {
  std::string name = GenerateLockName();
  scoped_ptr<MultiProcessLock> test_lock(MultiProcessLock::Create(name));
  EXPECT_TRUE(test_lock->TryLock());
  ExpectLockIsLocked(name);
  test_lock->Unlock();
  ExpectLockIsUnlocked(name);
}

TEST_F(MultiProcessLockTest, RecursiveLock) {
  std::string name = GenerateLockName();
  scoped_ptr<MultiProcessLock> test_lock(MultiProcessLock::Create(name));
  EXPECT_TRUE(test_lock->TryLock());
  ExpectLockIsLocked(name);
  LOG(INFO) << "Following error log "
            << "'MultiProcessLock is already locked' is expected";
  EXPECT_TRUE(test_lock->TryLock());
  ExpectLockIsLocked(name);
  test_lock->Unlock();
  ExpectLockIsUnlocked(name);
  LOG(INFO) << "Following error log "
            << "'Over-unlocked MultiProcessLock' is expected";
  test_lock->Unlock();
  ExpectLockIsUnlocked(name);
  test_lock.reset();
}

TEST_F(MultiProcessLockTest, LockScope) {
  // Check to see that lock is released when it goes out of scope.
  std::string name = GenerateLockName();
  {
    scoped_ptr<MultiProcessLock> test_lock(MultiProcessLock::Create(name));
    EXPECT_TRUE(test_lock->TryLock());
    ExpectLockIsLocked(name);
  }
  ExpectLockIsUnlocked(name);
}

MULTIPROCESS_TEST_MAIN(MultiProcessLockTryFailMain) {
  std::string name;
  scoped_ptr<base::Environment> environment(base::Environment::Create());
  EXPECT_TRUE(environment->GetVar(MultiProcessLockTest::kLockEnviromentVarName,
                                  &name));
#if defined(OS_MACOSX)
  // OS X sends out a log if a lock fails.
  // Hopefully this will keep people from panicking about it when they
  // are perusing the build logs.
  LOG(INFO) << "Following error log "
            << "\"CFMessagePort: bootstrap_register(): failed 1100 (0x44c) "
            << "'Permission denied'\" is expected";
#endif  // defined(OS_MACOSX)
  scoped_ptr<MultiProcessLock> test_lock(
      MultiProcessLock::Create(name));
  EXPECT_FALSE(test_lock->TryLock());
  return 0;
}

MULTIPROCESS_TEST_MAIN(MultiProcessLockTrySucceedMain) {
  std::string name;
  scoped_ptr<base::Environment> environment(base::Environment::Create());
  EXPECT_TRUE(environment->GetVar(MultiProcessLockTest::kLockEnviromentVarName,
                                  &name));
  scoped_ptr<MultiProcessLock> test_lock(
      MultiProcessLock::Create(name));
  EXPECT_TRUE(test_lock->TryLock());
  return 0;
}
