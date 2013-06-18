// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_write_helper.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/drive/dummy_file_system.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

namespace {

const base::FilePath::CharType kDrivePath[] =
    FILE_PATH_LITERAL("drive/root/file.txt");
const base::FilePath::CharType kInvalidPath[] =
    FILE_PATH_LITERAL("drive/invalid/path");
const base::FilePath::CharType kLocalPath[] =
    FILE_PATH_LITERAL("/tmp/local.txt");

class TestFileSystem : public DummyFileSystem {
 public:
  // Mimics CreateFile. It always succeeds unless kInvalidPath is passed.
  virtual void CreateFile(const base::FilePath& file_path,
                          bool is_exclusive,
                          const FileOperationCallback& callback) OVERRIDE {
   if (file_path == base::FilePath(kInvalidPath)) {
     callback.Run(FILE_ERROR_ACCESS_DENIED);
     return;
   }
   created.insert(file_path);
   callback.Run(FILE_ERROR_OK);
  }

  // Mimics OpenFile. It fails if the |file_path| is a path that is not
  // passed to CreateFile before. This tests that FileWriteHelper always
  // ensures file existence before trying to open. It also fails if the
  // path is already opened, to match the behavior of real FileSystem.
  virtual void OpenFile(const base::FilePath& file_path,
                        const OpenFileCallback& callback) OVERRIDE {
    // Files failed to create should never be opened.
    EXPECT_TRUE(created.count(file_path));
    created.erase(file_path);
    if (opened.count(file_path)) {
      callback.Run(FILE_ERROR_IN_USE, base::FilePath());
    } else {
      opened.insert(file_path);
      callback.Run(FILE_ERROR_OK, base::FilePath(kLocalPath));
    }
  }

  // Mimics CloseFile. It fails if it is passed a path not OpenFile'd.
  virtual void CloseFile(const base::FilePath& file_path,
                         const FileOperationCallback& callback) OVERRIDE {
    // Files failed to open should never be closed.
    EXPECT_TRUE(opened.count(file_path));
    opened.erase(file_path);
    callback.Run(FILE_ERROR_OK);
  }
  std::set<base::FilePath> created;
  std::set<base::FilePath> opened;
};

}  // namespace

class FileWriteHelperTest : public testing::Test {
 public:
  FileWriteHelperTest()
      : test_file_system_(new TestFileSystem) {
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestFileSystem> test_file_system_;
};

TEST_F(FileWriteHelperTest, PrepareFileForWritingSuccess) {
  FileWriteHelper file_write_helper(test_file_system_.get());
  FileError error = FILE_ERROR_FAILED;
  base::FilePath path;
  // The file should successfully be opened.
  file_write_helper.PrepareWritableFileAndRun(
      base::FilePath(kDrivePath),
      google_apis::test_util::CreateCopyResultCallback(&error, &path));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(kLocalPath, path.value());
}

TEST_F(FileWriteHelperTest, PrepareFileForWritingCreateFail) {
  FileWriteHelper file_write_helper(test_file_system_.get());
  FileError error = FILE_ERROR_FAILED;
  base::FilePath path;
  // Access to kInvalidPath should fail, and FileWriteHelper should not try to
  // open or close the file.
  file_write_helper.PrepareWritableFileAndRun(
      base::FilePath(kInvalidPath),
      google_apis::test_util::CreateCopyResultCallback(&error, &path));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_ACCESS_DENIED, error);
  EXPECT_TRUE(path.empty());
}

TEST_F(FileWriteHelperTest, PrepareFileForWritingOpenFail) {
  // Externally open the path beforehand.
  FileError error = FILE_ERROR_FAILED;
  base::FilePath path;
  test_file_system_->CreateFile(
      base::FilePath(kDrivePath),
      false,
      google_apis::test_util::CreateCopyResultCallback(&error));
  ASSERT_EQ(FILE_ERROR_OK, error);
  error = FILE_ERROR_FAILED;
  test_file_system_->OpenFile(
      base::FilePath(kDrivePath),
      google_apis::test_util::CreateCopyResultCallback(&error, &path));
  ASSERT_EQ(FILE_ERROR_OK, error);

  // Run FileWriteHelper on a file already opened in somewhere else.
  // It should fail to open the file, and should not try to close it.
  FileWriteHelper file_write_helper(test_file_system_.get());
  file_write_helper.PrepareWritableFileAndRun(
      base::FilePath(kDrivePath),
      google_apis::test_util::CreateCopyResultCallback(&error, &path));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_IN_USE, error);
  EXPECT_TRUE(path.empty());
}

}   // namespace drive
