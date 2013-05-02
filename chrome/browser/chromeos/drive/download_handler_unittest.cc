// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/download_handler.h"

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/file_write_helper.h"
#include "chrome/browser/chromeos/drive/mock_file_system.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::SaveArg;
using ::testing::_;

namespace drive {

namespace {

// Copies |file_path| to |out_file_path|, is used as a
// SubstituteDriveDownloadPathCallback.
void CopySubstituteDriveDownloadPathResult(base::FilePath* out_file_path,
                                           const base::FilePath& file_path) {
  *out_file_path = file_path;
}

// Copies |value| to |out|, is used as a content::CheckForFileExistenceCallback.
void CopyCheckForFileExistenceResult(bool* out, bool value) {
  *out = value;
}

}  // namespace

class DownloadHandlerTest : public testing::Test {
 public:
  DownloadHandlerTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        download_manager_(new content::MockDownloadManager) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Set expectations for download item.
    EXPECT_CALL(download_item_, GetState())
        .WillRepeatedly(Return(content::DownloadItem::IN_PROGRESS));

    // Set expectations for file system to save argument callbacks.
    EXPECT_CALL(file_system_, GetEntryInfoByPath(_, _))
        .WillRepeatedly(SaveArg<1>(&get_entry_info_callback_));
    EXPECT_CALL(file_system_, CreateDirectory(_, _, _, _))
        .WillRepeatedly(SaveArg<3>(&create_directory_callback_));

    file_write_helper_.reset(new FileWriteHelper(&file_system_));
    download_handler_.reset(
        new DownloadHandler(file_write_helper_.get(), &file_system_));
    download_handler_->Initialize(download_manager_, temp_dir_.path());
  }

  virtual void TearDown() OVERRIDE {
  }

 protected:
  base::ScopedTempDir temp_dir_;
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_refptr<content::MockDownloadManager> download_manager_;
  MockFileSystem file_system_;
  scoped_ptr<FileWriteHelper> file_write_helper_;
  scoped_ptr<DownloadHandler> download_handler_;
  content::MockDownloadItem download_item_;

  // Argument callbacks passed to the file system.
  GetEntryInfoCallback get_entry_info_callback_;
  FileOperationCallback create_directory_callback_;
};

TEST_F(DownloadHandlerTest, SubstituteDriveDownloadPathNonDrivePath) {
  const base::FilePath non_drive_path(FILE_PATH_LITERAL("/foo/bar"));
  ASSERT_FALSE(util::IsUnderDriveMountPoint(non_drive_path));

  // Call SubstituteDriveDownloadPath()
  base::FilePath substituted_path;
  download_handler_->SubstituteDriveDownloadPath(
      non_drive_path,
      &download_item_,
      base::Bind(&CopySubstituteDriveDownloadPathResult, &substituted_path));
  google_apis::test_util::RunBlockingPoolTask();

  // Check the result.
  EXPECT_EQ(non_drive_path, substituted_path);
  EXPECT_FALSE(download_handler_->IsDriveDownload(&download_item_));
}

TEST_F(DownloadHandlerTest, SubstituteDriveDownloadPath) {
  const base::FilePath drive_path =
      util::GetDriveMountPointPath().AppendASCII("test.dat");

  // Call SubstituteDriveDownloadPath()
  base::FilePath substituted_path;
  download_handler_->SubstituteDriveDownloadPath(
      drive_path,
      &download_item_,
      base::Bind(&CopySubstituteDriveDownloadPathResult, &substituted_path));
  google_apis::test_util::RunBlockingPoolTask();

  // Return result of GetEntryInfoByPath(), destination directory found.
  scoped_ptr<ResourceEntry> entry(new ResourceEntry);
  ASSERT_FALSE(get_entry_info_callback_.is_null());
  get_entry_info_callback_.Run(FILE_ERROR_OK, entry.Pass());
  google_apis::test_util::RunBlockingPoolTask();

  // Check the result.
  EXPECT_TRUE(temp_dir_.path().IsParent(substituted_path));
  ASSERT_TRUE(download_handler_->IsDriveDownload(&download_item_));
  EXPECT_EQ(drive_path, download_handler_->GetTargetPath(&download_item_));
}

TEST_F(DownloadHandlerTest, SubstituteDriveDownloadPathGetEntryFailure) {
  const base::FilePath drive_path =
      util::GetDriveMountPointPath().AppendASCII("test.dat");

  // Call SubstituteDriveDownloadPath()
  base::FilePath substituted_path;
  download_handler_->SubstituteDriveDownloadPath(
      drive_path,
      &download_item_,
      base::Bind(&CopySubstituteDriveDownloadPathResult, &substituted_path));
  google_apis::test_util::RunBlockingPoolTask();

  // Return result of GetEntryInfoByPath(), failing for some reason.
  ASSERT_FALSE(get_entry_info_callback_.is_null());
  get_entry_info_callback_.Run(FILE_ERROR_FAILED,
                              scoped_ptr<ResourceEntry>());
  google_apis::test_util::RunBlockingPoolTask();

  // Check the result.
  EXPECT_TRUE(substituted_path.empty());
}

TEST_F(DownloadHandlerTest, SubstituteDriveDownloadPathCreateDirectory) {
  const base::FilePath drive_path =
      util::GetDriveMountPointPath().AppendASCII("test.dat");

  // Call SubstituteDriveDownloadPath()
  base::FilePath substituted_path;
  download_handler_->SubstituteDriveDownloadPath(
      drive_path,
      &download_item_,
      base::Bind(&CopySubstituteDriveDownloadPathResult, &substituted_path));
  google_apis::test_util::RunBlockingPoolTask();

  // Return result of GetEntryInfoByPath(), destination directory not found.
  ASSERT_FALSE(get_entry_info_callback_.is_null());
  get_entry_info_callback_.Run(FILE_ERROR_NOT_FOUND,
                              scoped_ptr<ResourceEntry>());
  google_apis::test_util::RunBlockingPoolTask();

  // Return result of CreateDirecotry().
  ASSERT_FALSE(create_directory_callback_.is_null());
  create_directory_callback_.Run(FILE_ERROR_OK);
  google_apis::test_util::RunBlockingPoolTask();

  // Check the result.
  EXPECT_TRUE(temp_dir_.path().IsParent(substituted_path));
  ASSERT_TRUE(download_handler_->IsDriveDownload(&download_item_));
  EXPECT_EQ(drive_path, download_handler_->GetTargetPath(&download_item_));
}

TEST_F(DownloadHandlerTest,
       SubstituteDriveDownloadPathCreateDirectoryFailure) {
  const base::FilePath drive_path =
      util::GetDriveMountPointPath().AppendASCII("test.dat");

  // Call SubstituteDriveDownloadPath()
  base::FilePath substituted_path;
  download_handler_->SubstituteDriveDownloadPath(
      drive_path,
      &download_item_,
      base::Bind(&CopySubstituteDriveDownloadPathResult, &substituted_path));
  google_apis::test_util::RunBlockingPoolTask();

  // Return result of GetEntryInfoByPath(), destination directory not found.
  ASSERT_FALSE(get_entry_info_callback_.is_null());
  get_entry_info_callback_.Run(FILE_ERROR_NOT_FOUND,
                              scoped_ptr<ResourceEntry>());
  google_apis::test_util::RunBlockingPoolTask();

  // Return result of CreateDirecotry().
  ASSERT_FALSE(create_directory_callback_.is_null());
  create_directory_callback_.Run(FILE_ERROR_FAILED);
  google_apis::test_util::RunBlockingPoolTask();

  // Check the result.
  EXPECT_TRUE(substituted_path.empty());
}

// content::SavePackage calls SubstituteDriveDownloadPath before creating
// DownloadItem.
TEST_F(DownloadHandlerTest, SubstituteDriveDownloadPathForSavePackage) {
  const base::FilePath drive_path =
      util::GetDriveMountPointPath().AppendASCII("test.dat");

  // Call SubstituteDriveDownloadPath()
  base::FilePath substituted_path;
  download_handler_->SubstituteDriveDownloadPath(
      drive_path,
      NULL,  // DownloadItem is not available at this moment.
      base::Bind(&CopySubstituteDriveDownloadPathResult, &substituted_path));
  google_apis::test_util::RunBlockingPoolTask();

  // Return result of GetEntryInfoByPath(), destination directory found.
  scoped_ptr<ResourceEntry> entry(new ResourceEntry);
  ASSERT_FALSE(get_entry_info_callback_.is_null());
  get_entry_info_callback_.Run(FILE_ERROR_OK, entry.Pass());
  google_apis::test_util::RunBlockingPoolTask();

  // Check the result of SubstituteDriveDownloadPath().
  EXPECT_TRUE(temp_dir_.path().IsParent(substituted_path));

  // |download_item_| is not a drive download yet.
  EXPECT_FALSE(download_handler_->IsDriveDownload(&download_item_));

  // Call SetDownloadParams().
  download_handler_->SetDownloadParams(drive_path, &download_item_);

  // |download_item_| is a drive download now.
  ASSERT_TRUE(download_handler_->IsDriveDownload(&download_item_));
  EXPECT_EQ(drive_path, download_handler_->GetTargetPath(&download_item_));
}

TEST_F(DownloadHandlerTest, CheckForFileExistence) {
  const base::FilePath drive_path =
      util::GetDriveMountPointPath().AppendASCII("test.dat");

  // Make |download_item_| a drive download.
  download_handler_->SetDownloadParams(drive_path, &download_item_);
  ASSERT_TRUE(download_handler_->IsDriveDownload(&download_item_));
  EXPECT_EQ(drive_path, download_handler_->GetTargetPath(&download_item_));

  // Call CheckForFileExistence.
  bool file_exists = false;
  download_handler_->CheckForFileExistence(
      &download_item_,
      base::Bind(&CopyCheckForFileExistenceResult, &file_exists));
  google_apis::test_util::RunBlockingPoolTask();

  // Return result of GetEntryInfoByPath(), file exists.
  {
    scoped_ptr<ResourceEntry> entry(new ResourceEntry);
    ASSERT_FALSE(get_entry_info_callback_.is_null());
    get_entry_info_callback_.Run(FILE_ERROR_OK, entry.Pass());
  }
  google_apis::test_util::RunBlockingPoolTask();

  // Check the result.
  EXPECT_TRUE(file_exists);

  // Reset callback to call CheckForFileExistence again.
  get_entry_info_callback_.Reset();

  // Call CheckForFileExistence again.
  download_handler_->CheckForFileExistence(
      &download_item_,
      base::Bind(&CopyCheckForFileExistenceResult, &file_exists));
  google_apis::test_util::RunBlockingPoolTask();

  // Return result of GetEntryInfoByPath(), file does not exist.
  ASSERT_FALSE(get_entry_info_callback_.is_null());
  get_entry_info_callback_.Run(FILE_ERROR_NOT_FOUND,
                               scoped_ptr<ResourceEntry>());
  google_apis::test_util::RunBlockingPoolTask();

  // Check the result.
  EXPECT_FALSE(file_exists);
}

}  // namespace drive
