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
  // Mimics OpenFile. It fails if the |file_path| points to a hosted document.
  virtual void OpenFile(const base::FilePath& file_path,
                        OpenMode open_mode,
                        const OpenFileCallback& callback) OVERRIDE {
    EXPECT_EQ(OPEN_OR_CREATE_FILE, open_mode);

    // Emulate a case of opening a hosted document.
    if (file_path == base::FilePath(kInvalidPath)) {
      callback.Run(FILE_ERROR_INVALID_OPERATION, base::FilePath());
      return;
    }

    callback.Run(FILE_ERROR_OK, base::FilePath(kLocalPath));
  }

  virtual void CloseFile(const base::FilePath& file_path,
                         const FileOperationCallback& callback) OVERRIDE {
    callback.Run(FILE_ERROR_OK);
  }
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
  test_util::RunBlockingPoolTask();

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
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_INVALID_OPERATION, error);
  EXPECT_TRUE(path.empty());
}

}   // namespace drive
