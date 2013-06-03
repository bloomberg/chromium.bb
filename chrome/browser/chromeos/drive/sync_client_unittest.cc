// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/sync_client.h"

#include <set>
#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/dummy_file_system.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

namespace {

// Test file system for checking the behavior of SyncClient.
// It just records the arguments GetFileByResourceId and UpdateFileByResourceId.
class SyncClientTestFileSystem : public DummyFileSystem {
 public:
  // FileSystemInterface overrides.
  virtual void GetFileByResourceId(
      const std::string& resource_id,
      const ClientContext& context,
      const GetFileCallback& get_file_callback,
      const google_apis::GetContentCallback& get_content_callback) OVERRIDE {
    downloaded_.insert(resource_id);
    get_file_callback.Run(
        FILE_ERROR_OK,
        base::FilePath::FromUTF8Unsafe("local_path_does_not_matter"),
        scoped_ptr<ResourceEntry>(new ResourceEntry));
  }

  virtual void UpdateFileByResourceId(
      const std::string& resource_id,
      const ClientContext& context,
      const FileOperationCallback& callback) OVERRIDE {
    uploaded_.insert(resource_id);
    callback.Run(FILE_ERROR_OK);
  }

  virtual void GetResourceEntryById(
      const std::string& resource_id,
      const GetResourceEntryCallback& callback) OVERRIDE {
    // Only md5 field is cared in SyncClient, so fill only the field. To test
    // the case that there is a server-side update, this test implementation
    // returns a new md5 value different from values set in SetUpCache().
    scoped_ptr<ResourceEntry> entry(new ResourceEntry);
    entry->mutable_file_specific_info()->set_file_md5("new_md5");
    callback.Run(FILE_ERROR_OK, entry.Pass());
  }

  // Returns the multiset of IDs that the SyncClient tried to download/upload.
  const std::multiset<std::string>& downloaded() const { return downloaded_; }
  const std::multiset<std::string>& uploaded() const { return uploaded_; }

 private:
  std::multiset<std::string> downloaded_;
  std::multiset<std::string> uploaded_;
};

}  // namespace

class SyncClientTest : public testing::Test {
 public:
  SyncClientTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        test_file_system_(new SyncClientTestFileSystem) {
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
    sync_client_.reset(new SyncClient(test_file_system_.get(), cache_.get()));

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
    cache_->PinOnUIThread(
        "resource_id_not_fetched_foo", "",
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);

    cache_->PinOnUIThread(
        "resource_id_not_fetched_bar", "",
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);

    cache_->PinOnUIThread(
        "resource_id_not_fetched_baz", "",
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Prepare a pinned-and-fetched file.
    const std::string resource_id_fetched = "resource_id_fetched";
    const std::string md5_fetched = "md5";
    cache_->StoreOnUIThread(
        resource_id_fetched, md5_fetched, temp_file,
        FileCache::FILE_OPERATION_COPY,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);
    cache_->PinOnUIThread(
        resource_id_fetched, md5_fetched,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Prepare a pinned-and-fetched-and-dirty file.
    const std::string resource_id_dirty = "resource_id_dirty";
    const std::string md5_dirty = "";  // Don't care.
    cache_->StoreOnUIThread(
        resource_id_dirty, md5_dirty, temp_file,
        FileCache::FILE_OPERATION_COPY,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);
    cache_->PinOnUIThread(
        resource_id_dirty, md5_dirty,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);
    cache_->MarkDirtyOnUIThread(
        resource_id_dirty, md5_dirty,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);
    cache_->CommitDirtyOnUIThread(
        resource_id_dirty, md5_dirty,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);
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
  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<SyncClientTestFileSystem> test_file_system_;
  scoped_ptr<FileCache, test_util::DestroyHelperForTests> cache_;
  scoped_ptr<SyncClient> sync_client_;
};

TEST_F(SyncClientTest, StartInitialScan) {
  // Start processing the files in the backlog. This will collect the
  // resource IDs of these files.
  sync_client_->StartProcessingBacklog();
  google_apis::test_util::RunBlockingPoolTask();

  // Check the contents of the queue for fetching.
  EXPECT_EQ(3U, test_file_system_->downloaded().size());
  EXPECT_EQ(1U, test_file_system_->downloaded().count(
      "resource_id_not_fetched_bar"));
  EXPECT_EQ(1U, test_file_system_->downloaded().count(
      "resource_id_not_fetched_baz"));
  EXPECT_EQ(1U, test_file_system_->downloaded().count(
      "resource_id_not_fetched_foo"));

  // Check the contents of the queue for uploading.
  EXPECT_EQ(1U, test_file_system_->uploaded().size());
  EXPECT_EQ(1U, test_file_system_->uploaded().count(
      "resource_id_dirty"));
}

TEST_F(SyncClientTest, OnCachePinned) {
  // This file will be fetched by GetFileByResourceId() as OnCachePinned()
  // will kick off the sync loop.
  sync_client_->OnCachePinned("resource_id_not_fetched_foo", "md5");
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(1U, test_file_system_->downloaded().size());
  EXPECT_EQ(1U, test_file_system_->downloaded().count(
      "resource_id_not_fetched_foo"));
}

TEST_F(SyncClientTest, OnCacheUnpinned) {
  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  ASSERT_EQ(3U, GetResourceIdsToBeFetched().size());

  sync_client_->OnCacheUnpinned("resource_id_not_fetched_foo", "md5");
  sync_client_->OnCacheUnpinned("resource_id_not_fetched_baz", "md5");
  google_apis::test_util::RunBlockingPoolTask();

  // Only resource_id_not_fetched_foo should be fetched.
  EXPECT_EQ(1U, test_file_system_->downloaded().size());
  EXPECT_EQ(1U, test_file_system_->downloaded().count(
      "resource_id_not_fetched_bar"));
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
  // Start checking the existing pinned files. This will collect the resource
  // IDs of pinned files, with stale local cache files.
  sync_client_->StartCheckingExistingPinnedFiles();
  google_apis::test_util::RunBlockingPoolTask();

  // "resource_id_fetched" and "resource_id_dirty" are the existing pinned
  // files. Test file system returns new MD5 values for both of them. Then,
  // the non-dirty cache should be synced, but dirty cache should not.
  EXPECT_EQ(1U, test_file_system_->downloaded().size());
  EXPECT_EQ(1U, test_file_system_->downloaded().count("resource_id_fetched"));
}

}  // namespace internal
}  // namespace drive
