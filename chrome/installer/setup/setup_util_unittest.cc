// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class SetupUtilTest : public testing::Test {
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
  ScopedTempDir test_dir_;

  // The path to input data used in tests.
  FilePath data_dir_;
};
}

// Test that we are parsing Chrome version correctly.
TEST_F(SetupUtilTest, ApplyDiffPatchTest) {
  FilePath work_dir(test_dir_.path());
  work_dir = work_dir.AppendASCII("ApplyDiffPatchTest");
  ASSERT_FALSE(file_util::PathExists(work_dir));
  EXPECT_TRUE(file_util::CreateDirectory(work_dir));
  ASSERT_TRUE(file_util::PathExists(work_dir));

  FilePath src = data_dir_.AppendASCII("archive1.7z");
  FilePath patch = data_dir_.AppendASCII("archive.diff");
  FilePath dest = work_dir.AppendASCII("archive2.7z");
  EXPECT_EQ(installer::ApplyDiffPatch(src, patch, dest, NULL), 0);
  FilePath base = data_dir_.AppendASCII("archive2.7z");
  EXPECT_TRUE(file_util::ContentsEqual(dest, base));

  EXPECT_EQ(installer::ApplyDiffPatch(FilePath(), FilePath(), FilePath(), NULL),
            6);
}

// Test that we are parsing Chrome version correctly.
TEST_F(SetupUtilTest, GetMaxVersionFromArchiveDirTest) {
  // Create a version dir
  FilePath chrome_dir = test_dir_.path().AppendASCII("1.0.0.0");
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

TEST_F(SetupUtilTest, ProgramCompare) {
  using installer::ProgramCompare;
  ScopedTempDir temp_dir;

  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  FilePath some_long_dir(temp_dir.path().Append(L"Some Long Directory Name"));
  FilePath expect(some_long_dir.Append(L"file.txt"));
  FilePath expect_upcase(some_long_dir.Append(L"FILE.txt"));
  FilePath other(some_long_dir.Append(L"otherfile.txt"));

  // Tests where the expected file doesn't exist.

  // Paths don't match.
  EXPECT_FALSE(ProgramCompare(expect).Evaluate(L"\"" + other.value() + L"\""));
  // Paths match exactly.
  EXPECT_TRUE(ProgramCompare(expect).Evaluate(L"\"" + expect.value() + L"\""));
  // Paths differ by case.
  EXPECT_TRUE(ProgramCompare(expect).Evaluate(
      L"\"" + expect_upcase.value() + L"\""));

  // Tests where the expected file exists.
  static const char data[] = "data";
  ASSERT_TRUE(file_util::CreateDirectory(some_long_dir));
  ASSERT_NE(-1, file_util::WriteFile(expect, data, arraysize(data) - 1));
  // Paths don't match.
  EXPECT_FALSE(ProgramCompare(expect).Evaluate(L"\"" + other.value() + L"\""));
  // Paths match exactly.
  EXPECT_TRUE(ProgramCompare(expect).Evaluate(L"\"" + expect.value() + L"\""));
  // Paths differ by case.
  EXPECT_TRUE(ProgramCompare(expect).Evaluate(
      L"\"" + expect_upcase.value() + L"\""));

  // Test where strings don't match, but the same file is indicated.
  std::wstring short_expect;
  DWORD short_len = GetShortPathName(expect.value().c_str(),
                                     WriteInto(&short_expect, MAX_PATH),
                                     MAX_PATH);
  ASSERT_NE(static_cast<DWORD>(0), short_len);
  ASSERT_GT(static_cast<DWORD>(MAX_PATH), short_len);
  short_expect.resize(short_len);
  ASSERT_FALSE(FilePath::CompareEqualIgnoreCase(expect.value(), short_expect));
  EXPECT_TRUE(ProgramCompare(expect).Evaluate(L"\"" + short_expect + L"\""));
}

TEST_F(SetupUtilTest, DeleteFileFromTempProcess) {
  FilePath test_file;
  file_util::CreateTemporaryFileInDir(test_dir_.path(), &test_file);
  ASSERT_TRUE(file_util::PathExists(test_file));
  file_util::WriteFile(test_file, "foo", 3);
  EXPECT_TRUE(installer::DeleteFileFromTempProcess(test_file, 0));
  base::PlatformThread::Sleep(200);
  EXPECT_FALSE(file_util::PathExists(test_file));
}
