// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/file_write_helper.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/gdata/drive_test_util.h"
#include "chrome/browser/chromeos/gdata/mock_drive_file_system.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrictMock;
using ::testing::_;

namespace gdata {

namespace {

ACTION_P(MockCreateFile, error) {
  if (!arg2.is_null())
    arg2.Run(error);
}

ACTION_P2(MockOpenFile, error, local_path) {
  if (!arg1.is_null())
    arg1.Run(error, local_path);
}

ACTION_P(MockCloseFile, error) {
  if (!arg1.is_null())
    arg1.Run(error);
}

void RecordOpenFileCallbackArguments(DriveFileError* error,
                                     FilePath* path,
                                     DriveFileError error_arg,
                                     const FilePath& path_arg) {
  base::ThreadRestrictions::AssertIOAllowed();
  *error = error_arg;
  *path = path_arg;
}

}  // namespace

class FileWriteHelperTest : public testing::Test {
 public:
  FileWriteHelperTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        mock_file_system_(new StrictMock<MockDriveFileSystem>) {
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr< StrictMock<MockDriveFileSystem> > mock_file_system_;
};

TEST_F(FileWriteHelperTest, PrepareFileForWritingSuccess) {
  const FilePath kDrivePath("/drive/file.txt");
  const FilePath kLocalPath("/tmp/dummy.txt");

  EXPECT_CALL(*mock_file_system_, CreateFile(kDrivePath, false, _))
      .WillOnce(MockCreateFile(DRIVE_FILE_OK));
  EXPECT_CALL(*mock_file_system_, OpenFile(kDrivePath, _))
      .WillOnce(MockOpenFile(DRIVE_FILE_OK, kLocalPath));
  EXPECT_CALL(*mock_file_system_, CloseFile(kDrivePath, _))
      .WillOnce(MockCloseFile(DRIVE_FILE_OK));

  FileWriteHelper file_write_helper(mock_file_system_.get());
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath path;
  file_write_helper.PrepareWritableFileAndRun(
      kDrivePath, base::Bind(&RecordOpenFileCallbackArguments, &error, &path));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(kLocalPath, path);
}

TEST_F(FileWriteHelperTest, PrepareFileForWritingCreateFail) {
  const FilePath kDrivePath("/drive/file.txt");

  EXPECT_CALL(*mock_file_system_, CreateFile(kDrivePath, false, _))
      .WillOnce(MockCreateFile(DRIVE_FILE_ERROR_ACCESS_DENIED));
  EXPECT_CALL(*mock_file_system_, OpenFile(_, _)).Times(0);
  EXPECT_CALL(*mock_file_system_, CloseFile(_, _)).Times(0);

  FileWriteHelper file_write_helper(mock_file_system_.get());
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath path;
  file_write_helper.PrepareWritableFileAndRun(
      kDrivePath, base::Bind(&RecordOpenFileCallbackArguments, &error, &path));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_ERROR_ACCESS_DENIED, error);
  EXPECT_EQ(FilePath(), path);
}

TEST_F(FileWriteHelperTest, PrepareFileForWritingOpenFail) {
  const FilePath kDrivePath("/drive/file.txt");

  EXPECT_CALL(*mock_file_system_, CreateFile(kDrivePath, false, _))
      .WillOnce(MockCreateFile(DRIVE_FILE_OK));
  EXPECT_CALL(*mock_file_system_, OpenFile(kDrivePath, _))
      .WillOnce(MockOpenFile(DRIVE_FILE_ERROR_IN_USE, FilePath()));
  EXPECT_CALL(*mock_file_system_, CloseFile(_, _)).Times(0);

  FileWriteHelper file_write_helper(mock_file_system_.get());
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  FilePath path;
  file_write_helper.PrepareWritableFileAndRun(
      kDrivePath, base::Bind(&RecordOpenFileCallbackArguments, &error, &path));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_ERROR_IN_USE, error);
  EXPECT_EQ(FilePath(), path);
}

}   // namespace gdata
