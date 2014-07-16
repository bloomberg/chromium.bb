// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/download_handler.h"

#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chromeos/drive/dummy_file_system.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

namespace {

// Test file system for verifying the behavior of DownloadHandler, by simulating
// various responses from FileSystem.
class DownloadHandlerTestFileSystem : public DummyFileSystem {
 public:
  DownloadHandlerTestFileSystem() : error_(FILE_ERROR_FAILED) {}

  void set_error(FileError error) { error_ = error; }

  // FileSystemInterface overrides.
  virtual void GetResourceEntry(
      const base::FilePath& file_path,
      const GetResourceEntryCallback& callback) OVERRIDE {
    callback.Run(error_, scoped_ptr<ResourceEntry>(
        error_ == FILE_ERROR_OK ? new ResourceEntry : NULL));
  }

  virtual void CreateDirectory(
      const base::FilePath& directory_path,
      bool is_exclusive,
      bool is_recursive,
      const FileOperationCallback& callback) OVERRIDE {
    callback.Run(error_);
  }

 private:
  FileError error_;
};

}  // namespace

class DownloadHandlerTest : public testing::Test {
 public:
  DownloadHandlerTest()
      : download_manager_(new content::MockDownloadManager) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Set expectations for download item.
    EXPECT_CALL(download_item_, GetState())
        .WillRepeatedly(testing::Return(content::DownloadItem::IN_PROGRESS));

    download_handler_.reset(new DownloadHandler(&test_file_system_));
    download_handler_->Initialize(download_manager_.get(), temp_dir_.path());
  }

 protected:
  base::ScopedTempDir temp_dir_;
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  scoped_ptr<content::MockDownloadManager> download_manager_;
  DownloadHandlerTestFileSystem test_file_system_;
  scoped_ptr<DownloadHandler> download_handler_;
  content::MockDownloadItem download_item_;
};

TEST_F(DownloadHandlerTest, SubstituteDriveDownloadPathNonDrivePath) {
  const base::FilePath non_drive_path(FILE_PATH_LITERAL("/foo/bar"));
  ASSERT_FALSE(util::IsUnderDriveMountPoint(non_drive_path));

  // Call SubstituteDriveDownloadPath()
  base::FilePath substituted_path;
  download_handler_->SubstituteDriveDownloadPath(
      non_drive_path,
      &download_item_,
      google_apis::test_util::CreateCopyResultCallback(&substituted_path));
  content::RunAllBlockingPoolTasksUntilIdle();

  // Check the result.
  EXPECT_EQ(non_drive_path, substituted_path);
  EXPECT_FALSE(download_handler_->IsDriveDownload(&download_item_));
}

TEST_F(DownloadHandlerTest, SubstituteDriveDownloadPath) {
  const base::FilePath drive_path =
      util::GetDriveMountPointPath(&profile_).AppendASCII("test.dat");

  // Test the case that the download target directory already exists.
  test_file_system_.set_error(FILE_ERROR_OK);

  // Call SubstituteDriveDownloadPath()
  base::FilePath substituted_path;
  download_handler_->SubstituteDriveDownloadPath(
      drive_path,
      &download_item_,
      google_apis::test_util::CreateCopyResultCallback(&substituted_path));
  content::RunAllBlockingPoolTasksUntilIdle();

  // Check the result.
  EXPECT_TRUE(temp_dir_.path().IsParent(substituted_path));
  ASSERT_TRUE(download_handler_->IsDriveDownload(&download_item_));
  EXPECT_EQ(drive_path, download_handler_->GetTargetPath(&download_item_));
}

TEST_F(DownloadHandlerTest, SubstituteDriveDownloadPathGetEntryFailure) {
  const base::FilePath drive_path =
      util::GetDriveMountPointPath(&profile_).AppendASCII("test.dat");

  // Test the case that access to the download target directory failed for some
  // reason.
  test_file_system_.set_error(FILE_ERROR_FAILED);

  // Call SubstituteDriveDownloadPath()
  base::FilePath substituted_path;
  download_handler_->SubstituteDriveDownloadPath(
      drive_path,
      &download_item_,
      google_apis::test_util::CreateCopyResultCallback(&substituted_path));
  content::RunAllBlockingPoolTasksUntilIdle();

  // Check the result.
  EXPECT_TRUE(substituted_path.empty());
}

// content::SavePackage calls SubstituteDriveDownloadPath before creating
// DownloadItem.
TEST_F(DownloadHandlerTest, SubstituteDriveDownloadPathForSavePackage) {
  const base::FilePath drive_path =
      util::GetDriveMountPointPath(&profile_).AppendASCII("test.dat");
  test_file_system_.set_error(FILE_ERROR_OK);

  // Call SubstituteDriveDownloadPath()
  base::FilePath substituted_path;
  download_handler_->SubstituteDriveDownloadPath(
      drive_path,
      NULL,  // DownloadItem is not available at this moment.
      google_apis::test_util::CreateCopyResultCallback(&substituted_path));
  content::RunAllBlockingPoolTasksUntilIdle();

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
      util::GetDriveMountPointPath(&profile_).AppendASCII("test.dat");

  // Make |download_item_| a drive download.
  download_handler_->SetDownloadParams(drive_path, &download_item_);
  ASSERT_TRUE(download_handler_->IsDriveDownload(&download_item_));
  EXPECT_EQ(drive_path, download_handler_->GetTargetPath(&download_item_));

  // Test for the case when the path exists.
  test_file_system_.set_error(FILE_ERROR_OK);

  // Call CheckForFileExistence.
  bool file_exists = false;
  download_handler_->CheckForFileExistence(
      &download_item_,
      google_apis::test_util::CreateCopyResultCallback(&file_exists));
  content::RunAllBlockingPoolTasksUntilIdle();

  // Check the result.
  EXPECT_TRUE(file_exists);

  // Test for the case when the path does not exist.
  test_file_system_.set_error(FILE_ERROR_NOT_FOUND);

  // Call CheckForFileExistence again.
  download_handler_->CheckForFileExistence(
      &download_item_,
      google_apis::test_util::CreateCopyResultCallback(&file_exists));
  content::RunAllBlockingPoolTasksUntilIdle();

  // Check the result.
  EXPECT_FALSE(file_exists);
}

}  // namespace drive
