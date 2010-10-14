// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/path_service.h"
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

    // Name a subdirectory of the user temp directory.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
    test_dir_ = test_dir_.AppendASCII("SetupUtilTest");

    // Create a fresh, empty copy of this test directory.
    file_util::Delete(test_dir_, true);
    file_util::CreateDirectory(test_dir_);
    ASSERT_TRUE(file_util::PathExists(test_dir_));
  }

  virtual void TearDown() {
    // Clean up test directory
    ASSERT_TRUE(file_util::Delete(test_dir_, false));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  // the path to temporary directory used to contain the test operations
  FilePath test_dir_;

  // The path to input data used in tests.
  FilePath data_dir_;
};
};

// Test that we are parsing Chrome version correctly.
TEST_F(SetupUtilTest, ApplyDiffPatchTest) {
  FilePath work_dir(test_dir_);
  work_dir = work_dir.AppendASCII("ApplyDiffPatchTest");
  ASSERT_FALSE(file_util::PathExists(work_dir));
  EXPECT_TRUE(file_util::CreateDirectory(work_dir));
  ASSERT_TRUE(file_util::PathExists(work_dir));

  FilePath src = data_dir_.AppendASCII("archive1.7z");
  FilePath patch = data_dir_.AppendASCII("archive.diff");
  FilePath dest = work_dir.AppendASCII("archive2.7z");
  EXPECT_EQ(setup_util::ApplyDiffPatch(src, patch, dest), 0);
  FilePath base = data_dir_.AppendASCII("archive2.7z");
  EXPECT_TRUE(file_util::ContentsEqual(dest, base));

  EXPECT_EQ(setup_util::ApplyDiffPatch(FilePath(), FilePath(), FilePath()), 6);
}

// Test that we are parsing Chrome version correctly.
TEST_F(SetupUtilTest, GetVersionFromDirTest) {
  // Create a version dir
  FilePath chrome_dir = test_dir_.AppendASCII("1.0.0.0");
  file_util::CreateDirectory(chrome_dir);
  ASSERT_TRUE(file_util::PathExists(chrome_dir));
  scoped_ptr<installer::Version> version(
      setup_util::GetVersionFromDir(test_dir_));
  ASSERT_TRUE(version->GetString() == L"1.0.0.0");

  file_util::Delete(chrome_dir, true);
  ASSERT_FALSE(file_util::PathExists(chrome_dir));
  ASSERT_TRUE(setup_util::GetVersionFromDir(test_dir_) == NULL);

  chrome_dir = test_dir_.AppendASCII("ABC");
  file_util::CreateDirectory(chrome_dir);
  ASSERT_TRUE(file_util::PathExists(chrome_dir));
  ASSERT_TRUE(setup_util::GetVersionFromDir(test_dir_) == NULL);

  chrome_dir = test_dir_.AppendASCII("2.3.4.5");
  file_util::CreateDirectory(chrome_dir);
  ASSERT_TRUE(file_util::PathExists(chrome_dir));
  version.reset(setup_util::GetVersionFromDir(test_dir_));
  ASSERT_TRUE(version->GetString() == L"2.3.4.5");
}
