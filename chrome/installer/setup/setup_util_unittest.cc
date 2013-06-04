// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/setup_util_unittest.h"

#include <windows.h>

#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/installer/setup/setup_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SetupUtilTestWithDir : public testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_dir_));
    data_dir_ = data_dir_.AppendASCII("installer");
    ASSERT_TRUE(file_util::PathExists(data_dir_));

    // Create a temp directory for testing.
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() {
    // Clean up test directory manually so we can fail if it leaks.
    ASSERT_TRUE(test_dir_.Delete());
  }

  // The temporary directory used to contain the test operations.
  base::ScopedTempDir test_dir_;

  // The path to input data used in tests.
  base::FilePath data_dir_;
};

// The privilege tested in ScopeTokenPrivilege tests below.
// Use SE_RESTORE_NAME as it is one of the many privileges that is available,
// but not enabled by default on processes running at high integrity.
static const wchar_t kTestedPrivilege[] = SE_RESTORE_NAME;

// Returns true if the current process' token has privilege |privilege_name|
// enabled.
bool CurrentProcessHasPrivilege(const wchar_t* privilege_name) {
  base::win::ScopedHandle token;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY,
                          token.Receive())) {
    ADD_FAILURE();
    return false;
  }

  // First get the size of the buffer needed for |privileges| below.
  DWORD size;
  EXPECT_FALSE(::GetTokenInformation(token, TokenPrivileges, NULL, 0, &size));

  scoped_ptr<BYTE[]> privileges_bytes(new BYTE[size]);
  TOKEN_PRIVILEGES* privileges =
      reinterpret_cast<TOKEN_PRIVILEGES*>(privileges_bytes.get());

  if (!::GetTokenInformation(token, TokenPrivileges, privileges, size, &size)) {
    ADD_FAILURE();
    return false;
  }

  // There is no point getting a buffer to store more than |privilege_name|\0 as
  // anything longer will obviously not be equal to |privilege_name|.
  const DWORD desired_size = wcslen(privilege_name);
  const DWORD buffer_size = desired_size + 1;
  scoped_ptr<wchar_t[]> name_buffer(new wchar_t[buffer_size]);
  for (int i = privileges->PrivilegeCount - 1; i >= 0 ; --i) {
    LUID_AND_ATTRIBUTES& luid_and_att = privileges->Privileges[i];
    DWORD size = buffer_size;
    ::LookupPrivilegeName(NULL, &luid_and_att.Luid, name_buffer.get(), &size);
    if (size == desired_size &&
        wcscmp(name_buffer.get(), privilege_name) == 0) {
      return luid_and_att.Attributes == SE_PRIVILEGE_ENABLED;
    }
  }
  return false;
}

}  // namespace

// Test that we are parsing Chrome version correctly.
TEST_F(SetupUtilTestWithDir, ApplyDiffPatchTest) {
  base::FilePath work_dir(test_dir_.path());
  work_dir = work_dir.AppendASCII("ApplyDiffPatchTest");
  ASSERT_FALSE(file_util::PathExists(work_dir));
  EXPECT_TRUE(file_util::CreateDirectory(work_dir));
  ASSERT_TRUE(file_util::PathExists(work_dir));

  base::FilePath src = data_dir_.AppendASCII("archive1.7z");
  base::FilePath patch = data_dir_.AppendASCII("archive.diff");
  base::FilePath dest = work_dir.AppendASCII("archive2.7z");
  EXPECT_EQ(installer::ApplyDiffPatch(src, patch, dest, NULL), 0);
  base::FilePath base = data_dir_.AppendASCII("archive2.7z");
  EXPECT_TRUE(file_util::ContentsEqual(dest, base));

  EXPECT_EQ(installer::ApplyDiffPatch(base::FilePath(), base::FilePath(),
                                      base::FilePath(), NULL),
            6);
}

// Test that we are parsing Chrome version correctly.
TEST_F(SetupUtilTestWithDir, GetMaxVersionFromArchiveDirTest) {
  // Create a version dir
  base::FilePath chrome_dir = test_dir_.path().AppendASCII("1.0.0.0");
  file_util::CreateDirectory(chrome_dir);
  ASSERT_TRUE(file_util::PathExists(chrome_dir));
  scoped_ptr<Version> version(
      installer::GetMaxVersionFromArchiveDir(test_dir_.path()));
  ASSERT_EQ(version->GetString(), "1.0.0.0");

  file_util::Delete(chrome_dir, true);
  ASSERT_FALSE(file_util::PathExists(chrome_dir));
  ASSERT_TRUE(installer::GetMaxVersionFromArchiveDir(test_dir_.path()) == NULL);

  chrome_dir = test_dir_.path().AppendASCII("ABC");
  file_util::CreateDirectory(chrome_dir);
  ASSERT_TRUE(file_util::PathExists(chrome_dir));
  ASSERT_TRUE(installer::GetMaxVersionFromArchiveDir(test_dir_.path()) == NULL);

  chrome_dir = test_dir_.path().AppendASCII("2.3.4.5");
  file_util::CreateDirectory(chrome_dir);
  ASSERT_TRUE(file_util::PathExists(chrome_dir));
  version.reset(installer::GetMaxVersionFromArchiveDir(test_dir_.path()));
  ASSERT_EQ(version->GetString(), "2.3.4.5");

  // Create multiple version dirs, ensure that we select the greatest.
  chrome_dir = test_dir_.path().AppendASCII("9.9.9.9");
  file_util::CreateDirectory(chrome_dir);
  ASSERT_TRUE(file_util::PathExists(chrome_dir));
  chrome_dir = test_dir_.path().AppendASCII("1.1.1.1");
  file_util::CreateDirectory(chrome_dir);
  ASSERT_TRUE(file_util::PathExists(chrome_dir));

  version.reset(installer::GetMaxVersionFromArchiveDir(test_dir_.path()));
  ASSERT_EQ(version->GetString(), "9.9.9.9");
}

TEST_F(SetupUtilTestWithDir, DeleteFileFromTempProcess) {
  base::FilePath test_file;
  file_util::CreateTemporaryFileInDir(test_dir_.path(), &test_file);
  ASSERT_TRUE(file_util::PathExists(test_file));
  file_util::WriteFile(test_file, "foo", 3);
  EXPECT_TRUE(installer::DeleteFileFromTempProcess(test_file, 0));
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(200));
  EXPECT_FALSE(file_util::PathExists(test_file));
}

// Note: This test is only valid when run at high integrity (i.e. it will fail
// at medium integrity).
TEST(SetupUtilTest, ScopedTokenPrivilegeBasic) {
  ASSERT_FALSE(CurrentProcessHasPrivilege(kTestedPrivilege));

  {
    installer::ScopedTokenPrivilege test_scoped_privilege(kTestedPrivilege);
    ASSERT_TRUE(test_scoped_privilege.is_enabled());
    ASSERT_TRUE(CurrentProcessHasPrivilege(kTestedPrivilege));
  }

  ASSERT_FALSE(CurrentProcessHasPrivilege(kTestedPrivilege));
}

// Note: This test is only valid when run at high integrity (i.e. it will fail
// at medium integrity).
TEST(SetupUtilTest, ScopedTokenPrivilegeAlreadyEnabled) {
  ASSERT_FALSE(CurrentProcessHasPrivilege(kTestedPrivilege));

  {
    installer::ScopedTokenPrivilege test_scoped_privilege(kTestedPrivilege);
    ASSERT_TRUE(test_scoped_privilege.is_enabled());
    ASSERT_TRUE(CurrentProcessHasPrivilege(kTestedPrivilege));
    {
      installer::ScopedTokenPrivilege dup_scoped_privilege(kTestedPrivilege);
      ASSERT_TRUE(dup_scoped_privilege.is_enabled());
      ASSERT_TRUE(CurrentProcessHasPrivilege(kTestedPrivilege));
    }
    ASSERT_TRUE(CurrentProcessHasPrivilege(kTestedPrivilege));
  }

  ASSERT_FALSE(CurrentProcessHasPrivilege(kTestedPrivilege));
}

const char kAdjustProcessPriority[] = "adjust-process-priority";

PriorityClassChangeResult DoProcessPriorityAdjustment() {
  return installer::AdjustProcessPriority() ? PCCR_CHANGED : PCCR_UNCHANGED;
}

namespace {

// A scoper that sets/resets the current process's priority class.
class ScopedPriorityClass {
 public:
  // Applies |priority_class|, returning an instance if a change was made.
  // Otherwise, returns an empty scoped_ptr.
  static scoped_ptr<ScopedPriorityClass> Create(DWORD priority_class);
  ~ScopedPriorityClass();

 private:
  explicit ScopedPriorityClass(DWORD original_priority_class);
  DWORD original_priority_class_;
  DISALLOW_COPY_AND_ASSIGN(ScopedPriorityClass);
};

scoped_ptr<ScopedPriorityClass> ScopedPriorityClass::Create(
    DWORD priority_class) {
  HANDLE this_process = ::GetCurrentProcess();
  DWORD original_priority_class = ::GetPriorityClass(this_process);
  EXPECT_NE(0U, original_priority_class);
  if (original_priority_class && original_priority_class != priority_class) {
    BOOL result = ::SetPriorityClass(this_process, priority_class);
    EXPECT_NE(FALSE, result);
    if (result) {
      return scoped_ptr<ScopedPriorityClass>(
          new ScopedPriorityClass(original_priority_class));
    }
  }
  return scoped_ptr<ScopedPriorityClass>();
}

ScopedPriorityClass::ScopedPriorityClass(DWORD original_priority_class)
    : original_priority_class_(original_priority_class) {}

ScopedPriorityClass::~ScopedPriorityClass() {
  BOOL result = ::SetPriorityClass(::GetCurrentProcess(),
                                   original_priority_class_);
  EXPECT_NE(FALSE, result);
}

PriorityClassChangeResult RelaunchAndDoProcessPriorityAdjustment() {
  CommandLine cmd_line(*CommandLine::ForCurrentProcess());
  cmd_line.AppendSwitch(kAdjustProcessPriority);
  base::ProcessHandle process_handle = NULL;
  int exit_code = 0;
  if (!base::LaunchProcess(cmd_line, base::LaunchOptions(),
                           &process_handle)) {
    ADD_FAILURE() << " to launch subprocess.";
  } else if (!base::WaitForExitCode(process_handle, &exit_code)) {
    ADD_FAILURE() << " to wait for subprocess to exit.";
  } else {
    return static_cast<PriorityClassChangeResult>(exit_code);
  }
  return PCCR_UNKNOWN;
}

}  // namespace

// Launching a subprocess at normal priority class is a noop.
TEST(SetupUtilTest, AdjustFromNormalPriority) {
  ASSERT_EQ(NORMAL_PRIORITY_CLASS, ::GetPriorityClass(::GetCurrentProcess()));
  EXPECT_EQ(PCCR_UNCHANGED, RelaunchAndDoProcessPriorityAdjustment());
}

// Launching a subprocess below normal priority class drops it to bg mode for
// sufficiently recent operating systems.
TEST(SetupUtilTest, AdjustFromBelowNormalPriority) {
  scoped_ptr<ScopedPriorityClass> below_normal =
      ScopedPriorityClass::Create(BELOW_NORMAL_PRIORITY_CLASS);
  ASSERT_TRUE(below_normal);
  if (base::win::GetVersion() > base::win::VERSION_SERVER_2003)
    EXPECT_EQ(PCCR_CHANGED, RelaunchAndDoProcessPriorityAdjustment());
  else
    EXPECT_EQ(PCCR_UNCHANGED, RelaunchAndDoProcessPriorityAdjustment());
}
