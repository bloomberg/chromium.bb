// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/sync_client.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/chromeos/drive/change_list_loader.h"
#include "chrome/browser/chromeos/drive/change_list_processor.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/fake_free_disk_space_getter.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

namespace {

// The content of files initially stored in the cache.
const char kLocalContent[] = "Hello!";

// The content of files stored in the service.
const char kRemoteContent[] = "World!";

// SyncClientTestDriveService will return GDATA_CANCELLED when a request is
// made with the specified resource ID.
class SyncClientTestDriveService : public ::drive::FakeDriveService {
 public:
  // FakeDriveService override:
  virtual google_apis::CancelCallback DownloadFile(
      const base::FilePath& local_cache_path,
      const std::string& resource_id,
      const google_apis::DownloadActionCallback& download_action_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const google_apis::ProgressCallback& progress_callback) OVERRIDE {
    if (resource_id == resource_id_to_be_cancelled_) {
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(download_action_callback,
                     google_apis::GDATA_CANCELLED,
                     base::FilePath()));
      return google_apis::CancelCallback();
    }
    return FakeDriveService::DownloadFile(local_cache_path,
                                          resource_id,
                                          download_action_callback,
                                          get_content_callback,
                                          progress_callback);
  }

  void set_resource_id_to_be_cancelled(const std::string& resource_id) {
    resource_id_to_be_cancelled_ = resource_id;
  }

 private:
  std::string resource_id_to_be_cancelled_;
};

class DummyOperationObserver : public file_system::OperationObserver {
  // OperationObserver override:
  virtual void OnDirectoryChangedByOperation(
      const base::FilePath& path) OVERRIDE {}
  virtual void OnCacheFileUploadNeededByOperation(
      const std::string& local_id) OVERRIDE {}
};

}  // namespace

class SyncClientTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    pref_service_.reset(new TestingPrefServiceSimple);
    test_util::RegisterDrivePrefs(pref_service_->registry());

    fake_network_change_notifier_.reset(
        new test_util::FakeNetworkChangeNotifier);

    drive_service_.reset(new SyncClientTestDriveService);
    drive_service_->LoadResourceListForWapi("gdata/empty_feed.json");
    drive_service_->LoadAccountMetadataForWapi(
        "gdata/account_metadata.json");

    scheduler_.reset(new JobScheduler(pref_service_.get(),
                                      drive_service_.get(),
                                      base::MessageLoopProxy::current().get()));

    metadata_storage_.reset(new ResourceMetadataStorage(
        temp_dir_.path(), base::MessageLoopProxy::current().get()));
    ASSERT_TRUE(metadata_storage_->Initialize());

    metadata_.reset(new internal::ResourceMetadata(
        metadata_storage_.get(), base::MessageLoopProxy::current()));
    ASSERT_EQ(FILE_ERROR_OK, metadata_->Initialize());

    cache_.reset(new FileCache(metadata_storage_.get(),
                               temp_dir_.path(),
                               base::MessageLoopProxy::current().get(),
                               NULL /* free_disk_space_getter */));
    ASSERT_TRUE(cache_->Initialize());

    ASSERT_NO_FATAL_FAILURE(SetUpTestData());

    sync_client_.reset(new SyncClient(base::MessageLoopProxy::current().get(),
                                      &observer_,
                                      scheduler_.get(),
                                      metadata_.get(),
                                      cache_.get(),
                                      temp_dir_.path()));

    // Disable delaying so that DoSyncLoop() starts immediately.
    sync_client_->set_delay_for_testing(base::TimeDelta::FromSeconds(0));
  }

  // Adds a file to the service root and |resource_ids_|.
  void AddFileEntry(const std::string& title) {
    google_apis::GDataErrorCode error = google_apis::GDATA_FILE_ERROR;
    scoped_ptr<google_apis::ResourceEntry> entry;
    drive_service_->AddNewFile(
        "text/plain",
        kRemoteContent,
        drive_service_->GetRootResourceId(),
        title,
        false,  // shared_with_me
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_CREATED, error);
    ASSERT_TRUE(entry);
    resource_ids_[title] = entry->resource_id();
  }

  // Sets up data for tests.
  void SetUpTestData() {
    // Prepare a temp file.
    base::FilePath temp_file;
    EXPECT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(),
                                                    &temp_file));
    ASSERT_TRUE(google_apis::test_util::WriteStringToFile(temp_file,
                                                          kLocalContent));

    // Add file entries to the service.
    ASSERT_NO_FATAL_FAILURE(AddFileEntry("foo"));
    ASSERT_NO_FATAL_FAILURE(AddFileEntry("bar"));
    ASSERT_NO_FATAL_FAILURE(AddFileEntry("baz"));
    ASSERT_NO_FATAL_FAILURE(AddFileEntry("fetched"));
    ASSERT_NO_FATAL_FAILURE(AddFileEntry("dirty"));

    // Load data from the service to the metadata.
    FileError error = FILE_ERROR_FAILED;
    internal::ChangeListLoader change_list_loader(
        base::MessageLoopProxy::current().get(),
        metadata_.get(),
        scheduler_.get(),
        drive_service_.get());
    change_list_loader.LoadIfNeeded(
        DirectoryFetchInfo(),
        google_apis::test_util::CreateCopyResultCallback(&error));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Prepare 3 pinned-but-not-present files.
    EXPECT_EQ(FILE_ERROR_OK, cache_->Pin(GetLocalId("foo")));
    EXPECT_EQ(FILE_ERROR_OK, cache_->Pin(GetLocalId("bar")));
    EXPECT_EQ(FILE_ERROR_OK, cache_->Pin(GetLocalId("baz")));

    // Prepare a pinned-and-fetched file.
    const std::string md5_fetched = "md5";
    EXPECT_EQ(FILE_ERROR_OK,
              cache_->Store(GetLocalId("fetched"), md5_fetched,
                            temp_file, FileCache::FILE_OPERATION_COPY));
    EXPECT_EQ(FILE_ERROR_OK, cache_->Pin(GetLocalId("fetched")));

    // Prepare a pinned-and-fetched-and-dirty file.
    const std::string md5_dirty = "";  // Don't care.
    EXPECT_EQ(FILE_ERROR_OK,
              cache_->Store(GetLocalId("dirty"), md5_dirty,
                            temp_file, FileCache::FILE_OPERATION_COPY));
    EXPECT_EQ(FILE_ERROR_OK, cache_->Pin(GetLocalId("dirty")));
    EXPECT_EQ(FILE_ERROR_OK, cache_->MarkDirty(GetLocalId("dirty")));

  }

 protected:
  std::string GetLocalId(const std::string& title) {
    EXPECT_EQ(1U, resource_ids_.count(title));
    std::string local_id;
    EXPECT_EQ(FILE_ERROR_OK,
              metadata_->GetIdByResourceId(resource_ids_[title], &local_id));
    return local_id;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<TestingPrefServiceSimple> pref_service_;
  scoped_ptr<test_util::FakeNetworkChangeNotifier>
      fake_network_change_notifier_;
  scoped_ptr<SyncClientTestDriveService> drive_service_;
  DummyOperationObserver observer_;
  scoped_ptr<JobScheduler> scheduler_;
  scoped_ptr<ResourceMetadataStorage,
             test_util::DestroyHelperForTests> metadata_storage_;
  scoped_ptr<ResourceMetadata, test_util::DestroyHelperForTests> metadata_;
  scoped_ptr<FileCache, test_util::DestroyHelperForTests> cache_;
  scoped_ptr<SyncClient> sync_client_;

  std::map<std::string, std::string> resource_ids_;  // Name-to-id map.
};

TEST_F(SyncClientTest, StartProcessingBacklog) {
  sync_client_->StartProcessingBacklog();
  base::RunLoop().RunUntilIdle();

  FileCacheEntry cache_entry;
  // Pinned files get downloaded.
  EXPECT_TRUE(cache_->GetCacheEntry(GetLocalId("foo"), &cache_entry));
  EXPECT_TRUE(cache_entry.is_present());

  EXPECT_TRUE(cache_->GetCacheEntry(GetLocalId("bar"), &cache_entry));
  EXPECT_TRUE(cache_entry.is_present());

  EXPECT_TRUE(cache_->GetCacheEntry(GetLocalId("baz"), &cache_entry));
  EXPECT_TRUE(cache_entry.is_present());

  // Dirty file gets uploaded.
  EXPECT_TRUE(cache_->GetCacheEntry(GetLocalId("dirty"), &cache_entry));
  EXPECT_FALSE(cache_entry.is_dirty());
}

TEST_F(SyncClientTest, AddFetchTask) {
  sync_client_->AddFetchTask(GetLocalId("foo"));
  base::RunLoop().RunUntilIdle();

  FileCacheEntry cache_entry;
  EXPECT_TRUE(cache_->GetCacheEntry(GetLocalId("foo"), &cache_entry));
  EXPECT_TRUE(cache_entry.is_present());
}

TEST_F(SyncClientTest, AddFetchTaskAndCancelled) {
  // Trigger fetching of a file which results in cancellation.
  drive_service_->set_resource_id_to_be_cancelled(resource_ids_["foo"]);
  sync_client_->AddFetchTask(GetLocalId("foo"));
  base::RunLoop().RunUntilIdle();

  // The file should be unpinned if the user wants the download to be cancelled.
  FileCacheEntry cache_entry;
  EXPECT_FALSE(cache_->GetCacheEntry(GetLocalId("foo"), &cache_entry));
}

TEST_F(SyncClientTest, RemoveFetchTask) {
  sync_client_->AddFetchTask(GetLocalId("foo"));
  sync_client_->AddFetchTask(GetLocalId("bar"));
  sync_client_->AddFetchTask(GetLocalId("baz"));

  sync_client_->RemoveFetchTask(GetLocalId("foo"));
  sync_client_->RemoveFetchTask(GetLocalId("baz"));
  base::RunLoop().RunUntilIdle();

  // Only "bar" should be fetched.
  FileCacheEntry cache_entry;
  EXPECT_TRUE(cache_->GetCacheEntry(GetLocalId("foo"), &cache_entry));
  EXPECT_FALSE(cache_entry.is_present());

  EXPECT_TRUE(cache_->GetCacheEntry(GetLocalId("bar"), &cache_entry));
  EXPECT_TRUE(cache_entry.is_present());

  EXPECT_TRUE(cache_->GetCacheEntry(GetLocalId("baz"), &cache_entry));
  EXPECT_FALSE(cache_entry.is_present());

}

TEST_F(SyncClientTest, ExistingPinnedFiles) {
  // Start checking the existing pinned files. This will collect the resource
  // IDs of pinned files, with stale local cache files.
  sync_client_->StartCheckingExistingPinnedFiles();
  base::RunLoop().RunUntilIdle();

  // "fetched" and "dirty" are the existing pinned files.
  // The non-dirty one should be synced, but the dirty one should not.
  base::FilePath cache_file;
  std::string content;
  EXPECT_EQ(FILE_ERROR_OK, cache_->GetFile(GetLocalId("fetched"), &cache_file));
  EXPECT_TRUE(base::ReadFileToString(cache_file, &content));
  EXPECT_EQ(kRemoteContent, content);
  content.clear();

  EXPECT_EQ(FILE_ERROR_OK, cache_->GetFile(GetLocalId("dirty"), &cache_file));
  EXPECT_TRUE(base::ReadFileToString(cache_file, &content));
  EXPECT_EQ(kLocalContent, content);
}

TEST_F(SyncClientTest, RetryOnDisconnection) {
  // Let the service go down.
  drive_service_->set_offline(true);
  // Change the network connection state after some delay, to test that
  // FILE_ERROR_NO_CONNECTION is handled by SyncClient correctly.
  // Without this delay, JobScheduler will keep the jobs unrun and SyncClient
  // will receive no error.
  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&test_util::FakeNetworkChangeNotifier::SetConnectionType,
                 base::Unretained(fake_network_change_notifier_.get()),
                 net::NetworkChangeNotifier::CONNECTION_NONE),
      TestTimeouts::tiny_timeout());

  // Try fetch and upload.
  sync_client_->AddFetchTask(GetLocalId("foo"));
  sync_client_->AddUploadTask(GetLocalId("dirty"));
  base::RunLoop().RunUntilIdle();

  // Not yet fetched nor uploaded.
  FileCacheEntry cache_entry;
  EXPECT_TRUE(cache_->GetCacheEntry(GetLocalId("foo"), &cache_entry));
  EXPECT_FALSE(cache_entry.is_present());
  EXPECT_TRUE(cache_->GetCacheEntry(GetLocalId("dirty"), &cache_entry));
  EXPECT_TRUE(cache_entry.is_dirty());

  // Switch to online.
  fake_network_change_notifier_->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  drive_service_->set_offline(false);
  base::RunLoop().RunUntilIdle();

  // Fetched and uploaded.
  EXPECT_TRUE(cache_->GetCacheEntry(GetLocalId("foo"), &cache_entry));
  EXPECT_TRUE(cache_entry.is_present());
  EXPECT_TRUE(cache_->GetCacheEntry(GetLocalId("dirty"), &cache_entry));
  EXPECT_FALSE(cache_entry.is_dirty());
}

}  // namespace internal
}  // namespace drive
