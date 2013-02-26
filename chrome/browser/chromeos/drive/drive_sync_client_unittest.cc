// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_sync_client.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_test_util.h"
#include "chrome/browser/chromeos/drive/mock_drive_file_system.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Return;
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
  scoped_ptr<DriveEntryProto> entry_proto(new DriveEntryProto);
  entry_proto->mutable_file_specific_info()->set_file_md5(md5);
  arg1.Run(error, base::FilePath(), entry_proto.Pass());
}

class MockNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  MOCK_CONST_METHOD0(GetCurrentConnectionType,
                     net::NetworkChangeNotifier::ConnectionType());
};

}  // namespace

class DriveSyncClientTest : public testing::Test {
 public:
  DriveSyncClientTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        profile_(new TestingProfile),
        mock_file_system_(new StrictMock<MockDriveFileSystem>) {
  }

  virtual void SetUp() OVERRIDE {
    mock_network_change_notifier_.reset(new MockNetworkChangeNotifier);

    // Create a temporary directory.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Initialize the cache.
    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    cache_ = new DriveCache(
        temp_dir_.path(),
        pool->GetSequencedTaskRunner(pool->GetSequenceToken()),
        NULL /* free_disk_space_getter */);
    bool cache_initialization_success = false;
    cache_->RequestInitialize(
        base::Bind(&test_util::CopyResultFromInitializeCacheCallback,
                   &cache_initialization_success));
    google_apis::test_util::RunBlockingPoolTask();
    ASSERT_TRUE(cache_initialization_success);
    SetUpCache();

    // Initialize the sync client.
    sync_client_.reset(new DriveSyncClient(profile_.get(),
                                           mock_file_system_.get(),
                                           cache_));

    EXPECT_CALL(*mock_file_system_, AddObserver(sync_client_.get())).Times(1);
    EXPECT_CALL(*mock_file_system_,
                RemoveObserver(sync_client_.get())).Times(1);

    // Disable delaying so that DoSyncLoop() starts immediately.
    sync_client_->set_delay_for_testing(base::TimeDelta::FromSeconds(0));
    sync_client_->Initialize();
  }

  virtual void TearDown() OVERRIDE {
    // The sync client should be deleted before NetworkLibrary, as the sync
    // client registers itself as observer of NetworkLibrary.
    sync_client_.reset();
    cache_->Destroy();
    google_apis::test_util::RunBlockingPoolTask();
    mock_network_change_notifier_.reset();
  }

  // Sets up cache for tests.
  void SetUpCache() {
    // Prepare a temp file.
    base::FilePath temp_file;
    EXPECT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(),
                                                    &temp_file));
    const std::string content = "hello";
    EXPECT_EQ(static_cast<int>(content.size()),
              file_util::WriteFile(temp_file, content.data(), content.size()));

    // Prepare 3 pinned-but-not-present files.
    DriveFileError error = DRIVE_FILE_OK;
    cache_->Pin("resource_id_not_fetched_foo", "",
                base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback,
                           &error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(DRIVE_FILE_OK, error);

    cache_->Pin("resource_id_not_fetched_bar", "",
                base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback,
                           &error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(DRIVE_FILE_OK, error);

    cache_->Pin("resource_id_not_fetched_baz", "",
                base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback,
                           &error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(DRIVE_FILE_OK, error);

    // Prepare a pinned-and-fetched file.
    const std::string resource_id_fetched = "resource_id_fetched";
    const std::string md5_fetched = "md5";
    base::FilePath cache_file_path;
    cache_->Store(resource_id_fetched, md5_fetched, temp_file,
                  DriveCache::FILE_OPERATION_COPY,
                  base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback,
                             &error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(DRIVE_FILE_OK, error);
    cache_->Pin(resource_id_fetched, md5_fetched,
                base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback,
                           &error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(DRIVE_FILE_OK, error);

    // Prepare a pinned-and-fetched-and-dirty file.
    const std::string resource_id_dirty = "resource_id_dirty";
    const std::string md5_dirty = "";  // Don't care.
    cache_->Store(resource_id_dirty, md5_dirty, temp_file,
                  DriveCache::FILE_OPERATION_COPY,
                  base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback,
                             &error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(DRIVE_FILE_OK, error);
    cache_->Pin(resource_id_dirty, md5_dirty,
                base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback,
                           &error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(DRIVE_FILE_OK, error);
    cache_->MarkDirty(
        resource_id_dirty, md5_dirty,
        base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback, &error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(DRIVE_FILE_OK, error);
    cache_->CommitDirty(
        resource_id_dirty, md5_dirty,
        base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback,
                   &error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(DRIVE_FILE_OK, error);
  }

  // Sets the expectation for MockDriveFileSystem::GetFileByResourceId(),
  // that simulates successful retrieval of a file for the given resource ID.
  void SetExpectationForGetFileByResourceId(const std::string& resource_id) {
    EXPECT_CALL(*mock_file_system_,
                GetFileByResourceId(resource_id, _, _, _))
        .WillOnce(
            MockGetFileByResourceId(
                DRIVE_FILE_OK,
                base::FilePath::FromUTF8Unsafe("local_path_does_not_matter"),
                std::string("mime_type_does_not_matter"),
                REGULAR_FILE));
  }

  // Sets the expectation for MockDriveFileSystem::UpdateFileByResourceId(),
  // that simulates successful uploading of a file for the given resource ID.
  void SetExpectationForUpdateFileByResourceId(
      const std::string& resource_id) {
    EXPECT_CALL(*mock_file_system_,
                UpdateFileByResourceId(resource_id, _, _))
        .WillOnce(MockUpdateFileByResourceId(DRIVE_FILE_OK));
  }

  // Sets the expectation for MockDriveFileSystem::GetFileInfoByResourceId(),
  // that simulates successful retrieval of file info for the given resource
  // ID.
  //
  // This is used for testing StartCheckingExistingPinnedFiles(), hence we
  // are only interested in the MD5 value in DriveEntryProto.
  void SetExpectationForGetFileInfoByResourceId(
      const std::string& resource_id,
      const std::string& new_md5) {
    EXPECT_CALL(*mock_file_system_,
                GetEntryInfoByResourceId(resource_id, _))
        .WillOnce(MockUpdateFileByResourceId(
            DRIVE_FILE_OK,
            new_md5));
  }

  // Returns the resource IDs in the queue to be fetched.
  std::vector<std::string> GetResourceIdsToBeFetched() {
    return sync_client_->GetResourceIdsForTesting(
        DriveSyncClient::FETCH);
  }

  // Returns the resource IDs in the queue to be uploaded.
  std::vector<std::string> GetResourceIdsToBeUploaded() {
    return sync_client_->GetResourceIdsForTesting(
        DriveSyncClient::UPLOAD);
  }

  // Adds a resource ID of a file to fetch.
  void AddResourceIdToFetch(const std::string& resource_id) {
    sync_client_->AddResourceIdForTesting(DriveSyncClient::FETCH, resource_id);
  }

  // Adds a resource ID of a file to upload.
  void AddResourceIdToUpload(const std::string& resource_id) {
    sync_client_->AddResourceIdForTesting(DriveSyncClient::UPLOAD,
                                          resource_id);
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<StrictMock<MockDriveFileSystem> > mock_file_system_;
  DriveCache* cache_;
  scoped_ptr<DriveSyncClient> sync_client_;
  scoped_ptr<MockNetworkChangeNotifier> mock_network_change_notifier_;
};

TEST_F(DriveSyncClientTest, StartInitialScan) {
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

TEST_F(DriveSyncClientTest, OnCachePinned) {
  // This file will be fetched by GetFileByResourceId() as OnCachePinned()
  // will kick off the sync loop.
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_foo");

  sync_client_->OnCachePinned("resource_id_not_fetched_foo", "md5");

  google_apis::test_util::RunBlockingPoolTask();
}

TEST_F(DriveSyncClientTest, OnCacheUnpinned) {
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

TEST_F(DriveSyncClientTest, Deduplication) {
  AddResourceIdToFetch("resource_id_not_fetched_foo");

  // Set the delay so that DoSyncLoop() is delayed.
  sync_client_->set_delay_for_testing(TestTimeouts::action_max_timeout());
  // Raise OnCachePinned() event. This shouldn't result in adding the second
  // task, as tasks are de-duplicated.
  sync_client_->OnCachePinned("resource_id_not_fetched_foo", "md5");

  ASSERT_EQ(1U, GetResourceIdsToBeFetched().size());
}

TEST_F(DriveSyncClientTest, ExistingPinnedFiles) {
  // Set the expectation so that the MockDriveFileSystem returns "new_md5"
  // for "resource_id_fetched". This simulates that the file is updated on
  // the server side, and the new MD5 is obtained from the server (i.e. the
  // local cache file is stale).
  SetExpectationForGetFileInfoByResourceId("resource_id_fetched",
                                           "new_md5");
  // Set the expectation so that the MockDriveFileSystem returns "some_md5"
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
