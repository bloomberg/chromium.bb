// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/copy_operation.h"

#include "base/file_util.h"
#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_change.h"
#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "content/public/test/test_utils.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace file_system {

class CopyOperationTest : public OperationTestBase {
 protected:
  virtual void SetUp() OVERRIDE {
   OperationTestBase::SetUp();
   operation_.reset(new CopyOperation(
       blocking_task_runner(),
       observer(),
       scheduler(),
       metadata(),
       cache(),
       util::GetIdentityResourceIdCanonicalizer()));
  }

  scoped_ptr<CopyOperation> operation_;
};

TEST_F(CopyOperationTest, TransferFileFromLocalToRemote_RegularFile) {
  const base::FilePath local_src_path = temp_dir().AppendASCII("local.txt");
  const base::FilePath remote_dest_path(
      FILE_PATH_LITERAL("drive/root/remote.txt"));

  // Prepare a local file.
  ASSERT_TRUE(
      google_apis::test_util::WriteStringToFile(local_src_path, "hello"));
  // Confirm that the remote file does not exist.
  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(remote_dest_path, &entry));

  // Transfer the local file to Drive.
  FileError error = FILE_ERROR_FAILED;
  operation_->TransferFileFromLocalToRemote(
      local_src_path,
      remote_dest_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // TransferFileFromLocalToRemote stores a copy of the local file in the cache,
  // marks it dirty and requests the observer to upload the file.
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(remote_dest_path, &entry));
  EXPECT_EQ(1U, observer()->updated_local_ids().count(entry.local_id()));
  EXPECT_TRUE(entry.file_specific_info().cache_state().is_present());
  EXPECT_TRUE(entry.file_specific_info().cache_state().is_dirty());

  EXPECT_EQ(1U, observer()->get_changed_files().size());
  EXPECT_TRUE(observer()->get_changed_files().count(remote_dest_path));
}

TEST_F(CopyOperationTest, TransferFileFromLocalToRemote_Overwrite) {
  const base::FilePath local_src_path = temp_dir().AppendASCII("local.txt");
  const base::FilePath remote_dest_path(
      FILE_PATH_LITERAL("drive/root/File 1.txt"));

  // Prepare a local file.
  EXPECT_TRUE(
      google_apis::test_util::WriteStringToFile(local_src_path, "hello"));
  // Confirm that the remote file exists.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(remote_dest_path, &entry));

  // Transfer the local file to Drive.
  FileError error = FILE_ERROR_FAILED;
  operation_->TransferFileFromLocalToRemote(
      local_src_path,
      remote_dest_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // TransferFileFromLocalToRemote stores a copy of the local file in the cache,
  // marks it dirty and requests the observer to upload the file.
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(remote_dest_path, &entry));
  EXPECT_EQ(1U, observer()->updated_local_ids().count(entry.local_id()));
  EXPECT_TRUE(entry.file_specific_info().cache_state().is_present());
  EXPECT_TRUE(entry.file_specific_info().cache_state().is_dirty());

  EXPECT_EQ(1U, observer()->get_changed_files().size());
  EXPECT_TRUE(observer()->get_changed_files().count(remote_dest_path));
}

TEST_F(CopyOperationTest,
       TransferFileFromLocalToRemote_ExistingHostedDocument) {
  const base::FilePath local_src_path = temp_dir().AppendASCII("local.gdoc");
  const base::FilePath remote_dest_path(FILE_PATH_LITERAL(
      "drive/root/Directory 1/copied.gdoc"));

  // Prepare a local file, which is a json file of a hosted document, which
  // matches "drive/root/Document 1 excludeDir-test".
  ASSERT_TRUE(util::CreateGDocFile(
      local_src_path,
      GURL("https://3_document_self_link/document:5_document_resource_id"),
      "document:5_document_resource_id"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(remote_dest_path, &entry));

  // Transfer the local file to Drive.
  FileError error = FILE_ERROR_FAILED;
  operation_->TransferFileFromLocalToRemote(
      local_src_path,
      remote_dest_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(remote_dest_path, &entry));

  EXPECT_EQ(1U, observer()->get_changed_files().size());
  EXPECT_TRUE(observer()->get_changed_files().count(remote_dest_path));
  // New copy is created.
  EXPECT_NE("document:5_document_resource_id", entry.resource_id());
}

TEST_F(CopyOperationTest, TransferFileFromLocalToRemote_OrphanHostedDocument) {
  const base::FilePath local_src_path = temp_dir().AppendASCII("local.gdoc");
  const base::FilePath remote_dest_path(FILE_PATH_LITERAL(
      "drive/root/Directory 1/moved.gdoc"));

  // Prepare a local file, which is a json file of a hosted document, which
  // matches "drive/other/Orphan Document".
  ASSERT_TRUE(util::CreateGDocFile(
      local_src_path,
      GURL("https://3_document_self_link/document:orphan_doc_1"),
      "document:orphan_doc_1"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(remote_dest_path, &entry));

  // Transfer the local file to Drive.
  FileError error = FILE_ERROR_FAILED;
  operation_->TransferFileFromLocalToRemote(
      local_src_path,
      remote_dest_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(remote_dest_path, &entry));
  EXPECT_EQ(ResourceEntry::DIRTY, entry.metadata_edit_state());
  EXPECT_TRUE(observer()->updated_local_ids().count(entry.local_id()));

  EXPECT_EQ(1U, observer()->get_changed_files().size());
  EXPECT_TRUE(observer()->get_changed_files().count(remote_dest_path));
  // The original document got new parent.
  EXPECT_EQ("document:orphan_doc_1", entry.resource_id());
}

TEST_F(CopyOperationTest, TransferFileFromLocalToRemote_NewHostedDocument) {
  const base::FilePath local_src_path = temp_dir().AppendASCII("local.gdoc");
  const base::FilePath remote_dest_path(FILE_PATH_LITERAL(
      "drive/root/Directory 1/moved.gdoc"));

  // Create a hosted document on the server that is not synced to local yet.
  google_apis::GDataErrorCode gdata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::FileResource> new_gdoc_entry;
  fake_service()->AddNewFile(
      "application/vnd.google-apps.document", "", "", "title", true,
      google_apis::test_util::CreateCopyResultCallback(&gdata_error,
                                                       &new_gdoc_entry));
  content::RunAllBlockingPoolTasksUntilIdle();
  ASSERT_EQ(google_apis::HTTP_CREATED, gdata_error);

  // Prepare a local file, which is a json file of the added hosted document.
  ASSERT_TRUE(util::CreateGDocFile(
      local_src_path,
      GURL("https://3_document_self_link/" + new_gdoc_entry->file_id()),
      new_gdoc_entry->file_id()));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(remote_dest_path, &entry));

  // Transfer the local file to Drive.
  FileError error = FILE_ERROR_FAILED;
  operation_->TransferFileFromLocalToRemote(
      local_src_path,
      remote_dest_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(remote_dest_path, &entry));

  EXPECT_EQ(1U, observer()->get_changed_files().size());
  EXPECT_TRUE(observer()->get_changed_files().count(remote_dest_path));
  // The original document got new parent.
  EXPECT_EQ(new_gdoc_entry->file_id(), entry.resource_id());
}

TEST_F(CopyOperationTest, CopyNotExistingFile) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/Dummy file.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL("drive/root/Test.log"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(src_path, &entry));

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   false,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);

  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(src_path, &entry));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(dest_path, &entry));
  EXPECT_TRUE(observer()->get_changed_files().empty());
}

TEST_F(CopyOperationTest, CopyFileToNonExistingDirectory) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL("drive/root/Dummy/Test.log"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(dest_path.DirName(), &entry));

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   false,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(dest_path, &entry));
  EXPECT_TRUE(observer()->get_changed_files().empty());
}

// Test the case where the parent of the destination path is an existing file,
// not a directory.
TEST_F(CopyOperationTest, CopyFileToInvalidPath) {
  base::FilePath src_path(FILE_PATH_LITERAL(
      "drive/root/Document 1 excludeDir-test.gdoc"));
  base::FilePath dest_path(FILE_PATH_LITERAL(
      "drive/root/Duplicate Name.txt/Document 1 excludeDir-test.gdoc"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path.DirName(), &entry));
  ASSERT_FALSE(entry.file_info().is_directory());

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   false,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_NOT_A_DIRECTORY, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(dest_path, &entry));
  EXPECT_TRUE(observer()->get_changed_files().empty());
}

TEST_F(CopyOperationTest, CopyDirtyFile) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL(
      "drive/root/Directory 1/New File.txt"));

  ResourceEntry src_entry;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &src_entry));

  // Store a dirty cache file.
  base::FilePath temp_file;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir(), &temp_file));
  std::string contents = "test content";
  EXPECT_TRUE(google_apis::test_util::WriteStringToFile(temp_file, contents));
  FileError error = FILE_ERROR_FAILED;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&internal::FileCache::Store,
                 base::Unretained(cache()),
                 src_entry.local_id(),
                 std::string(),
                 temp_file,
                 internal::FileCache::FILE_OPERATION_MOVE),
      google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Copy.
  operation_->Copy(src_path,
                   dest_path,
                   false,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  ResourceEntry dest_entry;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path, &dest_entry));
  EXPECT_EQ(ResourceEntry::DIRTY, dest_entry.metadata_edit_state());

  EXPECT_EQ(1u, observer()->updated_local_ids().size());
  EXPECT_TRUE(observer()->updated_local_ids().count(dest_entry.local_id()));
  EXPECT_EQ(1u, observer()->get_changed_files().size());
  EXPECT_TRUE(observer()->get_changed_files().count(dest_path));

  // Copied cache file should be dirty.
  EXPECT_TRUE(dest_entry.file_specific_info().cache_state().is_dirty());

  // File contents should match.
  base::FilePath cache_file_path;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&internal::FileCache::GetFile,
                 base::Unretained(cache()),
                 dest_entry.local_id(),
                 &cache_file_path),
      google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  std::string copied_contents;
  EXPECT_TRUE(base::ReadFileToString(cache_file_path, &copied_contents));
  EXPECT_EQ(contents, copied_contents);
}

TEST_F(CopyOperationTest, CopyFileOverwriteFile) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL(
      "drive/root/Directory 1/SubDirectory File 1.txt"));

  ResourceEntry old_dest_entry;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path, &old_dest_entry));

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   false,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  ResourceEntry new_dest_entry;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path, &new_dest_entry));

  EXPECT_EQ(1u, observer()->updated_local_ids().size());
  EXPECT_TRUE(observer()->updated_local_ids().count(old_dest_entry.local_id()));
  EXPECT_EQ(1u, observer()->get_changed_files().size());
  EXPECT_TRUE(observer()->get_changed_files().count(dest_path));
}

TEST_F(CopyOperationTest, CopyFileOverwriteDirectory) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL("drive/root/Directory 1"));

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   false,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_INVALID_OPERATION, error);
}

TEST_F(CopyOperationTest, CopyDirectory) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/Directory 1"));
  base::FilePath dest_path(FILE_PATH_LITERAL("drive/root/New Directory"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  ASSERT_TRUE(entry.file_info().is_directory());
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path.DirName(), &entry));
  ASSERT_TRUE(entry.file_info().is_directory());

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   false,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_NOT_A_FILE, error);
}

TEST_F(CopyOperationTest, PreserveLastModified) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL("drive/root/File 2.txt"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  ASSERT_EQ(FILE_ERROR_OK,
            GetLocalResourceEntry(dest_path.DirName(), &entry));

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   true,  // Preserve last modified.
                   google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  ResourceEntry entry2;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path, &entry2));
  EXPECT_EQ(entry.file_info().last_modified(),
            entry2.file_info().last_modified());
}

}  // namespace file_system
}  // namespace drive
