// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/sync_client.h"

#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/mock_file_system.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrictMock;
using ::testing::_;

namespace drive {

namespace {

// Action used to set mock expectations for GetFileByResourceId().
ACTION_P4(MockGetFileByResourceId, error, local_path, mime_type, file_type) {
  arg2.Run(error, local_path, mime_type, file_type);
}

// Action used to set mock expectations for UpdateFileByResourceId().
ACTION_P(MockUpdateFileByResourceId, error) {
  arg2.Run(error);
}

// Action used to set mock expectations for GetFileInfoByResourceId().
ACTION_P2(MockUpdateFileByResourceId, error, md5) {
  scoped_ptr<ResourceEntry> entry(new ResourceEntry);
  entry->mutable_file_specific_info()->set_file_md5(md5);
  arg1.Run(error, base::FilePath(), entry.Pass());
}

}  // namespace

class SyncClientTest : public testing::Test {
 public:
  SyncClientTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        mock_file_system_(new StrictMock<MockFileSystem>) {
  }

  virtual void SetUp() OVERRIDE {
    // Create a temporary directory.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Initialize the cache.
    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    cache_.reset(new FileCache(
        temp_dir_.path(),
        pool->GetSequencedTaskRunner(pool->GetSequenceToken()),
        NULL /* free_disk_space_getter */));
    bool success = false;
    cache_->RequestInitialize(
        google_apis::test_util::CreateCopyResultCallback(&success));
    google_apis::test_util::RunBlockingPoolTask();
    ASSERT_TRUE(success);
    SetUpCache();

    // Initialize the sync client.
    EXPECT_CALL(*mock_file_system_, AddObserver(_)).Times(1);
    EXPECT_CALL(*mock_file_system_, RemoveObserver(_)).Times(1);
    sync_client_.reset(new SyncClient(mock_file_system_.get(), cache_.get()));

    // Disable delaying so that DoSyncLoop() starts immediately.
    sync_client_->set_delay_for_testing(base::TimeDelta::FromSeconds(0));
  }

  virtual void TearDown() OVERRIDE {
    sync_client_.reset();
    cache_.reset();
  }

  // Sets up cache for tests.
  void SetUpCache() {
    // Prepare a temp file.
    base::FilePath temp_file;
    EXPECT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(),
                                                    &temp_file));
    ASSERT_TRUE(google_apis::test_util::WriteStringToFile(temp_file, "hello"));

    // Prepare 3 pinned-but-not-present files.
    FileError error = FILE_ERROR_OK;
    cache_->Pin("resource_id_not_fetched_foo", "",
                google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);

    cache_->Pin("resource_id_not_fetched_bar", "",
                google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);

    cache_->Pin("resource_id_not_fetched_baz", "",
                google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Prepare a pinned-and-fetched file.
    const std::string resource_id_fetched = "resource_id_fetched";
    const std::string md5_fetched = "md5";
    cache_->Store(resource_id_fetched, md5_fetched, temp_file,
                  FileCache::FILE_OPERATION_COPY,
                  google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);
    cache_->Pin(resource_id_fetched, md5_fetched,
                google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Prepare a pinned-and-fetched-and-dirty file.
    const std::string resource_id_dirty = "resource_id_dirty";
    const std::string md5_dirty = "";  // Don't care.
    cache_->Store(resource_id_dirty, md5_dirty, temp_file,
                  FileCache::FILE_OPERATION_COPY,
                  google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);
    cache_->Pin(resource_id_dirty, md5_dirty,
                google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);
    cache_->MarkDirty(
        resource_id_dirty, md5_dirty,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);
    cache_->CommitDirty(
        resource_id_dirty, md5_dirty,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);
  }

  // Sets the expectation for MockFileSystem::GetFileByResourceId(),
  // that simulates successful retrieval of a file for the given resource ID.
  void SetExpectationForGetFileByResourceId(const std::string& resource_id) {
    EXPECT_CALL(*mock_file_system_,
                GetFileByResourceId(resource_id, _, _, _))
        .WillOnce(
            MockGetFileByResourceId(
                FILE_ERROR_OK,
                base::FilePath::FromUTF8Unsafe("local_path_does_not_matter"),
                std::string("mime_type_does_not_matter"),
                REGULAR_FILE));
  }

  // Sets the expectation for MockFileSystem::UpdateFileByResourceId(),
  // that simulates successful uploading of a file for the given resource ID.
  void SetExpectationForUpdateFileByResourceId(
      const std::string& resource_id) {
    EXPECT_CALL(*mock_file_system_,
                UpdateFileByResourceId(resource_id, _, _))
        .WillOnce(MockUpdateFileByResourceId(FILE_ERROR_OK));
  }

  // Sets the expectation for MockFileSystem::GetFileInfoByResourceId(),
  // that simulates successful retrieval of file info for the given resource
  // ID.
  //
  // This is used for testing StartCheckingExistingPinnedFiles(), hence we
  // are only interested in the MD5 value in ResourceEntry.
  void SetExpectationForGetFileInfoByResourceId(
      const std::string& resource_id,
      const std::string& new_md5) {
    EXPECT_CALL(*mock_file_system_,
                GetEntryInfoByResourceId(resource_id, _))
        .WillOnce(MockUpdateFileByResourceId(
            FILE_ERROR_OK,
            new_md5));
  }

  // Returns the resource IDs in the queue to be fetched.
  std::vector<std::string> GetResourceIdsToBeFetched() {
    return sync_client_->GetResourceIdsForTesting(SyncClient::FETCH);
  }

  // Returns the resource IDs in the queue to be uploaded.
  std::vector<std::string> GetResourceIdsToBeUploaded() {
    return sync_client_->GetResourceIdsForTesting(SyncClient::UPLOAD);
  }

  // Adds a resource ID of a file to fetch.
  void AddResourceIdToFetch(const std::string& resource_id) {
    sync_client_->AddResourceIdForTesting(SyncClient::FETCH, resource_id);
  }

  // Adds a resource ID of a file to upload.
  void AddResourceIdToUpload(const std::string& resource_id) {
    sync_client_->AddResourceIdForTesting(SyncClient::UPLOAD, resource_id);
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<StrictMock<MockFileSystem> > mock_file_system_;
  scoped_ptr<FileCache, test_util::DestroyHelperForTests> cache_;
  scoped_ptr<SyncClient> sync_client_;
};

TEST_F(SyncClientTest, StartInitialScan) {
  // Start processing the files in the backlog. This will collect the
  // resource IDs of these files.
  sync_client_->StartProcessingBacklog();

  // Check the contents of the queue for fetching.
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_bar");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_baz");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_foo");

  // Check the contents of the queue for uploading.
  SetExpectationForUpdateFileByResourceId("resource_id_dirty");

  google_apis::test_util::RunBlockingPoolTask();
}

TEST_F(SyncClientTest, OnCachePinned) {
  // This file will be fetched by GetFileByResourceId() as OnCachePinned()
  // will kick off the sync loop.
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_foo");

  sync_client_->OnCachePinned("resource_id_not_fetched_foo", "md5");

  google_apis::test_util::RunBlockingPoolTask();
}

TEST_F(SyncClientTest, OnCacheUnpinned) {
  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  ASSERT_EQ(3U, GetResourceIdsToBeFetched().size());

  sync_client_->OnCacheUnpinned("resource_id_not_fetched_foo", "md5");
  sync_client_->OnCacheUnpinned("resource_id_not_fetched_baz", "md5");

  // Only resource_id_not_fetched_foo should be fetched.
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_bar");

  google_apis::test_util::RunBlockingPoolTask();
}

TEST_F(SyncClientTest, Deduplication) {
  AddResourceIdToFetch("resource_id_not_fetched_foo");

  // Set the delay so that DoSyncLoop() is delayed.
  sync_client_->set_delay_for_testing(TestTimeouts::action_max_timeout());
  // Raise OnCachePinned() event. This shouldn't result in adding the second
  // task, as tasks are de-duplicated.
  sync_client_->OnCachePinned("resource_id_not_fetched_foo", "md5");

  ASSERT_EQ(1U, GetResourceIdsToBeFetched().size());
}

TEST_F(SyncClientTest, ExistingPinnedFiles) {
  // Set the expectation so that the MockFileSystem returns "new_md5"
  // for "resource_id_fetched". This simulates that the file is updated on
  // the server side, and the new MD5 is obtained from the server (i.e. the
  // local cache file is stale).
  SetExpectationForGetFileInfoByResourceId("resource_id_fetched",
                                           "new_md5");
  // Set the expectation so that the MockFileSystem returns "some_md5"
  // for "resource_id_dirty". The MD5 on the server is always different from
  // the MD5 of a dirty file, which is set to "local". We should not collect
  // this by StartCheckingExistingPinnedFiles().
  SetExpectationForGetFileInfoByResourceId("resource_id_dirty",
                                           "some_md5");

  // Start checking the existing pinned files. This will collect the resource
  // IDs of pinned files, with stale local cache files.
  sync_client_->StartCheckingExistingPinnedFiles();

  SetExpectationForGetFileByResourceId("resource_id_fetched");

  google_apis::test_util::RunBlockingPoolTask();
}

}  // namespace drive
