// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/copy_operation.h"

#include "base/file_util.h"
#include "chrome/browser/chromeos/drive/file_cache_observer.h"
#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/google_apis/fake_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace file_system {

namespace {
class CacheCommitObserver : public internal::FileCacheObserver {
 public:
  virtual void OnCacheCommitted(const std::string& resource_id) OVERRIDE {
   last_committed_resource_id_ = resource_id;
  }
  const std::string last_committed_resource_id() const {
    return last_committed_resource_id_;
  }

 private:
  std::string last_committed_resource_id_;
};

}  // namespace

class CopyOperationTest : public OperationTestBase {
 protected:
  virtual void SetUp() OVERRIDE {
   OperationTestBase::SetUp();
   operation_.reset(new CopyOperation(blocking_task_runner(),
                                      observer(),
                                      scheduler(),
                                      metadata(),
                                      cache(),
                                      fake_service()));
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

  CacheCommitObserver cache_observer;
  cache()->AddObserver(&cache_observer);

  // Transfer the local file to Drive.
  FileError error = FILE_ERROR_FAILED;
  operation_->TransferFileFromLocalToRemote(
      local_src_path,
      remote_dest_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // What TransferFileFromLocalToRemote does is to store the local file in
  // the Drive file cache, and mark it as dirty+committed. Here we test that
  // the "cache committed" event is indeed fired as a result of this test case.
  //
  // In the production environment, SyncClient listens this event and uploads
  // the file to the remote server in background. This part should be tested in
  // sync_client_unittest.cc
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(remote_dest_path, &entry));
  EXPECT_FALSE(cache_observer.last_committed_resource_id().empty());
  EXPECT_EQ(entry.resource_id(), cache_observer.last_committed_resource_id());

  EXPECT_EQ(1U, observer()->get_changed_paths().size());
  EXPECT_TRUE(observer()->get_changed_paths().count(
      remote_dest_path.DirName()));
}

TEST_F(CopyOperationTest, TransferFileFromLocalToRemote_HostedDocument) {
  const base::FilePath local_src_path = temp_dir().AppendASCII("local.gdoc");
  const base::FilePath remote_dest_path(FILE_PATH_LITERAL(
      "drive/root/Directory 1/Document 1 excludeDir-test.gdoc"));

  // Prepare a local file, which is a json file of a hosted document, which
  // matches "Document 1" in root_feed.json.
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
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(remote_dest_path, &entry));

  // We added a file to the Drive root and then moved to "Directory 1".
  EXPECT_EQ(2U, observer()->get_changed_paths().size());
  EXPECT_TRUE(observer()->get_changed_paths().count(
      base::FilePath(FILE_PATH_LITERAL("drive/root"))));
  EXPECT_TRUE(observer()->get_changed_paths().count(
      remote_dest_path.DirName()));
}

TEST_F(CopyOperationTest, TransferFileFromRemoteToLocal_RegularFile) {
  base::FilePath remote_src_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath local_dest_path = temp_dir().AppendASCII("local_copy.txt");

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(remote_src_path, &entry));

  FileError error = FILE_ERROR_FAILED;
  operation_->TransferFileFromRemoteToLocal(
      remote_src_path,
      local_dest_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // The content is "x"s of the file size.
  base::FilePath cache_path;
  cache()->GetFileOnUIThread(entry.resource_id(),
                             entry.file_specific_info().file_md5(),
                             google_apis::test_util::CreateCopyResultCallback(
                                 &error, &cache_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  const std::string kExpectedContent = "This is some test content.";
  std::string cache_file_data;
  EXPECT_TRUE(file_util::ReadFileToString(cache_path, &cache_file_data));
  EXPECT_EQ(kExpectedContent, cache_file_data);

  std::string local_dest_file_data;
  EXPECT_TRUE(file_util::ReadFileToString(local_dest_path,
                                          &local_dest_file_data));
  EXPECT_EQ(kExpectedContent, local_dest_file_data);

  // The transfered file is cached and the change of "offline available"
  // attribute is notified.
  EXPECT_EQ(1U, observer()->get_changed_paths().size());
  EXPECT_TRUE(observer()->get_changed_paths().count(remote_src_path.DirName()));
}

TEST_F(CopyOperationTest, TransferFileFromRemoteToLocal_HostedDocument) {
  base::FilePath local_dest_path = temp_dir().AppendASCII("local_copy.txt");
  base::FilePath remote_src_path(
      FILE_PATH_LITERAL("drive/root/Document 1 excludeDir-test.gdoc"));

  FileError error = FILE_ERROR_FAILED;
  operation_->TransferFileFromRemoteToLocal(
      remote_src_path,
      local_dest_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(remote_src_path, &entry));
  EXPECT_EQ(GURL(entry.file_specific_info().alternate_url()),
            util::ReadUrlFromGDocFile(local_dest_path));
  EXPECT_EQ(entry.resource_id(),
            util::ReadResourceIdFromGDocFile(local_dest_path));
  EXPECT_TRUE(observer()->get_changed_paths().empty());
}

TEST_F(CopyOperationTest, CopyNotExistingFile) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/Dummy file.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL("drive/root/Test.log"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(src_path, &entry));

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);

  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(src_path, &entry));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(dest_path, &entry));
  EXPECT_TRUE(observer()->get_changed_paths().empty());
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
                   google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(dest_path, &entry));
  EXPECT_TRUE(observer()->get_changed_paths().empty());
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
                   google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_A_DIRECTORY, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(dest_path, &entry));
  EXPECT_TRUE(observer()->get_changed_paths().empty());
}

}  // namespace file_system
}  // namespace drive
