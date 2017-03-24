// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/jumplist_file_util.h"

#include <Shlwapi.h>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Random text to write into a file.
constexpr char kFileContent[] = "I'm random context.";

// Maximum files allowed to delete and maximum attempt failures allowed.
// For unit tests purpose only.
const int kFileDeleteLimitForTest = 1;

// Simple function to dump some text into a new file.
void CreateTextFile(const base::FilePath& file_name,
                    const std::string& contents) {
  // Since |contents|'s length is small here, static_cast won't cause overflow.
  ASSERT_EQ(static_cast<int>(contents.length()),
            base::WriteFile(file_name, contents.data(), contents.length()));
  ASSERT_TRUE(base::PathExists(file_name));
}

}  // namespace

class JumpListFileUtilTest : public testing::Test {
 protected:
  // A temporary directory where all file IO operations take place .
  base::ScopedTempDir temp_dir_;

  // Get the path to the temporary directory.
  const base::FilePath& temp_dir_path() { return temp_dir_.GetPath(); }

  // Create a unique temporary directory.
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }
};

TEST_F(JumpListFileUtilTest, DeleteDirectoryContent) {
  base::FilePath dir_path = temp_dir_path();

  // Create a file.
  base::FilePath file_name =
      dir_path.Append(FILE_PATH_LITERAL("TestDeleteFile.txt"));
  ASSERT_NO_FATAL_FAILURE(CreateTextFile(file_name, kFileContent));

  // Delete the directory content using DeleteDirectoryContent(). The file
  // should be deleted and the directory remains.
  ASSERT_EQ(DeleteDirectoryContent(dir_path, kFileDeleteLimit), SUCCEED);
  EXPECT_FALSE(PathExists(file_name));
  EXPECT_TRUE(DirectoryExists(dir_path));
}

TEST_F(JumpListFileUtilTest, DeleteSubDirectory) {
  base::FilePath dir_path = temp_dir_path();

  // Create a subdirectory.
  base::FilePath test_subdir =
      dir_path.Append(FILE_PATH_LITERAL("TestSubDirectory"));
  ASSERT_NO_FATAL_FAILURE(CreateDirectory(test_subdir));

  // Delete the directory using DeleteDirectory(), which should fail because
  // a subdirectory exists.
  ASSERT_EQ(DeleteDirectory(dir_path, kFileDeleteLimit),
            FAIL_SUBDIRECTORY_EXISTS);
  EXPECT_TRUE(DirectoryExists(dir_path));
  EXPECT_TRUE(DirectoryExists(test_subdir));

  // Delete the subdirectory alone should be working.
  ASSERT_EQ(DeleteDirectory(test_subdir, kFileDeleteLimit), SUCCEED);
  EXPECT_TRUE(DirectoryExists(dir_path));
  EXPECT_FALSE(DirectoryExists(test_subdir));
}

TEST_F(JumpListFileUtilTest, DeleteMaxFilesAllowed) {
  base::FilePath dir_path = temp_dir_path();

  // Create 2 files.
  base::FilePath file_name =
      dir_path.Append(FILE_PATH_LITERAL("TestDeleteFile1.txt"));
  ASSERT_NO_FATAL_FAILURE(CreateTextFile(file_name, kFileContent));

  file_name = dir_path.Append(FILE_PATH_LITERAL("TestDeleteFile2.txt"));
  ASSERT_NO_FATAL_FAILURE(CreateTextFile(file_name, kFileContent));

  // Delete the directory content using DeleteDirectoryContent().
  // Sine the maximum files allowed to delete is 1, only 1 out of the 2
  // files is deleted. Therefore, the directory is not empty yet.
  ASSERT_EQ(DeleteDirectoryContent(dir_path, kFileDeleteLimitForTest), SUCCEED);
  EXPECT_FALSE(::PathIsDirectoryEmpty(dir_path.value().c_str()));

  // Delete another file, and now the directory is empty.
  ASSERT_EQ(DeleteDirectoryContent(dir_path, kFileDeleteLimitForTest), SUCCEED);
  EXPECT_TRUE(::PathIsDirectoryEmpty(dir_path.value().c_str()));
  EXPECT_TRUE(DirectoryExists(dir_path));
}
