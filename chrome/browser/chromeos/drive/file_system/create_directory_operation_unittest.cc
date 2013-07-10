// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/create_directory_operation.h"

#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/google_apis/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace file_system {

class CreateDirectoryOperationTest : public OperationTestBase {
 protected:
  // Returns FILE_ERROR_OK if a directory is found at |path|.
  FileError FindDirectory(const base::FilePath& path) {
    ResourceEntry entry;
    FileError error = GetLocalResourceEntry(path, &entry);
    if (error == FILE_ERROR_OK && !entry.file_info().is_directory())
      error = FILE_ERROR_NOT_A_DIRECTORY;
    return error;
  }
};

TEST_F(CreateDirectoryOperationTest, CreateDirectory) {
  CreateDirectoryOperation operation(blocking_task_runner(),
                                     observer(),
                                     scheduler(),
                                     metadata());

  const base::FilePath kExistingFile(
      FILE_PATH_LITERAL("drive/root/File 1.txt"));
  const base::FilePath kExistingDirectory(
      FILE_PATH_LITERAL("drive/root/Directory 1"));
  const base::FilePath kNewDirectory1(
      FILE_PATH_LITERAL("drive/root/New Directory"));
  const base::FilePath kNewDirectory2 =
      kNewDirectory1.AppendASCII("New Directory 2/a/b/c");

  // Create a new directory, not recursively.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, FindDirectory(kNewDirectory1));

  FileError error = FILE_ERROR_FAILED;
  operation.CreateDirectory(
      kNewDirectory1,
      true, // is_exclusive
      false,  // is_recursive
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(FILE_ERROR_OK, FindDirectory(kNewDirectory1));
  EXPECT_EQ(1U, observer()->get_changed_paths().size());
  EXPECT_EQ(1U,
            observer()->get_changed_paths().count(kNewDirectory1.DirName()));

  // Create a new directory recursively.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, FindDirectory(kNewDirectory2));
  operation.CreateDirectory(
      kNewDirectory2,
      true, // is_exclusive
      false,  // is_recursive
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, FindDirectory(kNewDirectory2));

  operation.CreateDirectory(
      kNewDirectory2,
      true, // is_exclusive
      true,  // is_recursive
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(FILE_ERROR_OK, FindDirectory(kNewDirectory2));

  // Try to create an existing directory.
  operation.CreateDirectory(
      kExistingDirectory,
      true, // is_exclusive
      false,  // is_recursive
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_EXISTS, error);

  operation.CreateDirectory(
      kExistingDirectory,
      false, // is_exclusive
      false,  // is_recursive
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Try to create a directory with a path for an existing file.
  operation.CreateDirectory(
      kExistingFile,
      false, // is_exclusive
      true,  // is_recursive
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);

  // Try to create a directory under a file.
  operation.CreateDirectory(
      kExistingFile.AppendASCII("New Directory"),
      false, // is_exclusive
      true,  // is_recursive
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
}

}  // namespace file_system
}  // namespace drive
