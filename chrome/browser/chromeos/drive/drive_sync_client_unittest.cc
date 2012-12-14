// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_sync_client.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_sync_client_observer.h"
#include "chrome/browser/chromeos/drive/drive_test_util.h"
#include "chrome/browser/chromeos/drive/mock_drive_file_system.h"
#include "chrome/browser/prefs/pref_service.h"
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
  arg1.Run(error, local_path, mime_type, file_type);
}

// Action used to set mock expectations for UpdateFileByResourceId().
ACTION_P(MockUpdateFileByResourceId, error) {
  arg1.Run(error);
}

// Action used to set mock expectations for GetFileInfoByResourceId().
ACTION_P2(MockUpdateFileByResourceId, error, md5) {
  scoped_ptr<DriveEntryProto> entry_proto(new DriveEntryProto);
  entry_proto->mutable_file_specific_info()->set_file_md5(md5);
  arg1.Run(error, FilePath(), entry_proto.Pass());
}

class MockNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  MOCK_CONST_METHOD0(GetCurrentConnectionType,
                     net::NetworkChangeNotifier::ConnectionType());
};

class TestObserver : public DriveSyncClientObserver {
 public:
  // Type for recording sync client observer notifications.
  enum State {
    UNINITIALIZED,
    STARTED,
    STOPPED,
    IDLE,
  };

  TestObserver() : state_(UNINITIALIZED) {}

  // DriveSyncClientObserver overrides.
  virtual void OnSyncTaskStarted() { state_ = STARTED; }
  virtual void OnSyncClientStopped() { state_ = STOPPED; }
  virtual void OnSyncClientIdle() { state_ = IDLE; }

  // Returns the last notified state.
  State state() const { return state_; }

 private:
  State state_;
};

}  // namespace

class DriveSyncClientTest : public testing::Test {
 public:
  DriveSyncClientTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        io_thread_(content::BrowserThread::IO),
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
    sync_client_->AddObserver(&observer_);
  }

  virtual void TearDown() OVERRIDE {
    // The sync client should be deleted before NetworkLibrary, as the sync
    // client registers itself as observer of NetworkLibrary.
    sync_client_->RemoveObserver(&observer_);
    sync_client_.reset();
    cache_->Destroy();
    google_apis::test_util::RunBlockingPoolTask();
    mock_network_change_notifier_.reset();
  }

  // Sets up MockNetworkChangeNotifier as if it's connected to a network with
  // the specified connection type.
  void ChangeConnectionType(net::NetworkChangeNotifier::ConnectionType type) {
    EXPECT_CALL(*mock_network_change_notifier_, GetCurrentConnectionType())
        .WillRepeatedly(Return(type));
    // Notify the sync client that the network is changed. This is done via
    // NetworkChangeNotifier in production, but here, we simulate the behavior
    // by directly calling OnConnectionTypeChanged().
    sync_client_->OnConnectionTypeChanged(type);
  }

  // Sets up MockNetworkChangeNotifier as if it's connected to wifi network.
  void ConnectToWifi() {
    ChangeConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  }

  // Sets up MockNetworkChangeNotifier as if it's connected to cellular network.
  void ConnectToCellular() {
    ChangeConnectionType(net::NetworkChangeNotifier::CONNECTION_2G);
  }

  // Sets up MockNetworkChangeNotifier as if it's connected to wimax network.
  void ConnectToWimax() {
    ChangeConnectionType(net::NetworkChangeNotifier::CONNECTION_4G);
  }

  // Sets up MockNetworkChangeNotifier as if it's disconnected.
  void ConnectToNone() {
    ChangeConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  }

  // Sets up cache for tests.
  void SetUpCache() {
    // Prepare a temp file.
    FilePath temp_file;
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
    FilePath cache_file_path;
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
                GetFileByResourceId(resource_id, _, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(this,
                              &DriveSyncClientTest::VerifyStartNotified),
            MockGetFileByResourceId(
                DRIVE_FILE_OK,
                FilePath::FromUTF8Unsafe("local_path_does_not_matter"),
                std::string("mime_type_does_not_matter"),
                REGULAR_FILE)));
  }

  // Sets the expectation for MockDriveFileSystem::GetFileByResourceId(),
  // during which it disconnects from network.
  void SetDisconnectingExpectationForGetFileByResourceId(
      const std::string& resource_id) {
    EXPECT_CALL(*mock_file_system_, GetFileByResourceId(resource_id, _, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(this, &DriveSyncClientTest::ConnectToNone),
            MockGetFileByResourceId(
                DRIVE_FILE_ERROR_NO_CONNECTION,
                FilePath::FromUTF8Unsafe("local_path_does_not_matter"),
                std::string("mime_type_does_not_matter"),
                REGULAR_FILE)));
  }

  // Sets the expectation for MockDriveFileSystem::UpdateFileByResourceId(),
  // that simulates successful uploading of a file for the given resource ID.
  void SetExpectationForUpdateFileByResourceId(
      const std::string& resource_id) {
    EXPECT_CALL(*mock_file_system_,
                UpdateFileByResourceId(resource_id, _))
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

  // Helper function for verifying that observer is correctly notified the
  // start of sync client in SetExpectationForGetFileByResourceId.
  void VerifyStartNotified() {
    EXPECT_EQ(TestObserver::STARTED, observer_.state());
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<StrictMock<MockDriveFileSystem> > mock_file_system_;
  DriveCache* cache_;
  scoped_ptr<DriveSyncClient> sync_client_;
  scoped_ptr<MockNetworkChangeNotifier> mock_network_change_notifier_;
  TestObserver observer_;
};

TEST_F(DriveSyncClientTest, StartInitialScan) {
  // Connect to no network, so the sync loop won't spin.
  ConnectToNone();

  // Start processing the files in the backlog. This will collect the
  // resource IDs of these files.
  sync_client_->StartProcessingBacklog();
  google_apis::test_util::RunBlockingPoolTask();

  // Check the contents of the queue for fetching.
  std::vector<std::string> resource_ids = GetResourceIdsToBeFetched();
  ASSERT_EQ(3U, resource_ids.size());
  // Since these are the list of file names read from the disk, the order is
  // not guaranteed, hence sort it.
  sort(resource_ids.begin(), resource_ids.end());
  EXPECT_EQ("resource_id_not_fetched_bar", resource_ids[0]);
  EXPECT_EQ("resource_id_not_fetched_baz", resource_ids[1]);
  EXPECT_EQ("resource_id_not_fetched_foo", resource_ids[2]);
  // resource_id_fetched is not collected in the queue.

  // Check the contents of the queue for uploading.
  resource_ids = GetResourceIdsToBeUploaded();
  ASSERT_EQ(1U, resource_ids.size());
  EXPECT_EQ("resource_id_dirty", resource_ids[0]);
}

TEST_F(DriveSyncClientTest, StartSyncLoop) {
  ConnectToWifi();

  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  AddResourceIdToUpload("resource_id_dirty");

  // These files will be fetched or uploaded by DriveFileSystem, once
  // StartSyncLoop() starts.
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_foo");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_bar");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_baz");
  SetExpectationForUpdateFileByResourceId("resource_id_dirty");

  sync_client_->StartSyncLoop();
}

TEST_F(DriveSyncClientTest, StartSyncLoop_Offline) {
  ConnectToNone();

  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  AddResourceIdToUpload("resource_id_dirty");

  // These files will be neither fetched nor uploaded not by DriveFileSystem,
  // as network is not connected.

  sync_client_->StartSyncLoop();
}

TEST_F(DriveSyncClientTest, StartSyncLoop_ResumedConnection) {
  const std::string resource_id("resource_id_not_fetched_foo");
  const FilePath file_path(
      FilePath::FromUTF8Unsafe("local_path_does_not_matter"));
  const std::string mime_type("mime_type_does_not_matter");
  ConnectToWifi();
  AddResourceIdToFetch(resource_id);

  // Disconnect from network on fetch try.
  SetDisconnectingExpectationForGetFileByResourceId(resource_id);

  sync_client_->StartSyncLoop();

  // Expect fetch retry on network reconnection.
  EXPECT_CALL(*mock_file_system_, GetFileByResourceId(resource_id, _, _))
      .WillOnce(MockGetFileByResourceId(
          DRIVE_FILE_OK, file_path, mime_type, REGULAR_FILE));

  ConnectToWifi();
}

TEST_F(DriveSyncClientTest, StartSyncLoop_CelluarDisabled) {
  ConnectToWifi();  // First connect to Wifi.

  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  AddResourceIdToUpload("resource_id_dirty");

  // These files will be neither fetched nor uploaded not by DriveFileSystem,
  // as fetching over cellular network is disabled by default.

  // Then connect to cellular. This will kick off StartSyncLoop().
  ConnectToCellular();
}

TEST_F(DriveSyncClientTest, StartSyncLoop_CelluarEnabled) {
  ConnectToWifi();  // First connect to Wifi.

  // Enable fetching over cellular network.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableDriveOverCellular, false);

  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  AddResourceIdToUpload("resource_id_dirty");

  // These files will be fetched or uploaded by DriveFileSystem, as syncing
  // over cellular network is explicitly enabled.
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_foo");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_bar");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_baz");
  SetExpectationForUpdateFileByResourceId("resource_id_dirty");

  // Then connect to cellular. This will kick off StartSyncLoop().
  ConnectToCellular();
}

TEST_F(DriveSyncClientTest, StartSyncLoop_WimaxDisabled) {
  ConnectToWifi();  // First connect to Wifi.

  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  AddResourceIdToUpload("resource_id_dirty");

  // These files will be neither fetched nor uploaded not by DriveFileSystem,
  // as syncing over wimax network is disabled by default.

  // Then connect to wimax. This will kick off StartSyncLoop().
  ConnectToWimax();
}

TEST_F(DriveSyncClientTest, StartSyncLoop_CelluarEnabledWithWimax) {
  ConnectToWifi();  // First connect to Wifi.

  // Enable fetching over cellular network. This includes wimax.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableDriveOverCellular, false);

  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  AddResourceIdToUpload("resource_id_dirty");

  // These files will be fetched or uploaded by DriveFileSystem, as syncing
  // over cellular network, which includes wimax, is explicitly enabled.
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_foo");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_bar");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_baz");
  SetExpectationForUpdateFileByResourceId("resource_id_dirty");

  // Then connect to wimax. This will kick off StartSyncLoop().
  ConnectToWimax();
}

TEST_F(DriveSyncClientTest, StartSyncLoop_DriveDisabled) {
  ConnectToWifi();

  // Disable the Drive feature.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableDrive, true);

  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  AddResourceIdToUpload("resource_id_dirty");

  // These files will be neither fetched nor uploaded not by DriveFileSystem,
  // as the Drive feature is disabled.

  sync_client_->StartSyncLoop();
}

TEST_F(DriveSyncClientTest, OnCachePinned) {
  ConnectToWifi();

  // This file will be fetched by GetFileByResourceId() as OnCachePinned()
  // will kick off the sync loop.
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_foo");

  sync_client_->OnCachePinned("resource_id_not_fetched_foo", "md5");
}

TEST_F(DriveSyncClientTest, OnCacheUnpinned) {
  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  ASSERT_EQ(3U, GetResourceIdsToBeFetched().size());

  sync_client_->OnCacheUnpinned("resource_id_not_fetched_bar", "md5");
  // "bar" should be gone.
  std::vector<std::string> resource_ids = GetResourceIdsToBeFetched();
  ASSERT_EQ(2U, resource_ids.size());
  EXPECT_EQ("resource_id_not_fetched_foo", resource_ids[0]);
  EXPECT_EQ("resource_id_not_fetched_baz", resource_ids[1]);

  sync_client_->OnCacheUnpinned("resource_id_not_fetched_foo", "md5");
  // "foo" should be gone.
  resource_ids = GetResourceIdsToBeFetched();
  ASSERT_EQ(1U, resource_ids.size());
  EXPECT_EQ("resource_id_not_fetched_baz", resource_ids[1]);

  sync_client_->OnCacheUnpinned("resource_id_not_fetched_baz", "md5");
  // "baz" should be gone.
  resource_ids = GetResourceIdsToBeFetched();
  ASSERT_TRUE(resource_ids.empty());
}

TEST_F(DriveSyncClientTest, Deduplication) {
  ConnectToWifi();

  AddResourceIdToFetch("resource_id_not_fetched_foo");

  // Set the delay so that DoSyncLoop() is delayed.
  sync_client_->set_delay_for_testing(TestTimeouts::action_max_timeout());
  // Raise OnCachePinned() event. This shouldn't result in adding the second
  // task, as tasks are de-duplicated.
  sync_client_->OnCachePinned("resource_id_not_fetched_foo", "md5");

  ASSERT_EQ(1U, GetResourceIdsToBeFetched().size());
}

TEST_F(DriveSyncClientTest, ExistingPinnedFiles) {
  // Connect to no network, so the sync loop won't spin.
  ConnectToNone();

  // Set the expectation so that the MockDriveFileSystem returns "new_md5"
  // for "resource_id_fetched". This simulates that the file is updated on
  // the server side, and the new MD5 is obtained from the server (i.e. the
  // local cach file is stale).
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
  google_apis::test_util::RunBlockingPoolTask();

  // Check the contents of the queue for fetching.
  std::vector<std::string> resource_ids =
      GetResourceIdsToBeFetched();
  ASSERT_EQ(1U, resource_ids.size());
  EXPECT_EQ("resource_id_fetched", resource_ids[0]);
  // resource_id_dirty is not collected in the queue.

  // Check the contents of the queue for uploading.
  resource_ids = GetResourceIdsToBeUploaded();
  ASSERT_TRUE(resource_ids.empty());
}

TEST_F(DriveSyncClientTest, ObserveEmptyQueue) {
  ConnectToCellular();
  EXPECT_EQ(TestObserver::STOPPED, observer_.state());

  // Try all possible transitions over {None, Cellular, Wifi}, and check that
  // the state change is notified to the observer at every step.
  ConnectToNone();
  EXPECT_EQ(TestObserver::STOPPED, observer_.state());
  ConnectToWifi();
  EXPECT_EQ(TestObserver::IDLE, observer_.state());
  ConnectToCellular();
  EXPECT_EQ(TestObserver::STOPPED, observer_.state());
  ConnectToWifi();
  EXPECT_EQ(TestObserver::IDLE, observer_.state());
  ConnectToNone();
  EXPECT_EQ(TestObserver::STOPPED, observer_.state());
  ConnectToCellular();
  EXPECT_EQ(TestObserver::STOPPED, observer_.state());

  // Enable fetching over cellular network.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableDriveOverCellular, false);
  EXPECT_EQ(TestObserver::IDLE, observer_.state());

  // Try all possible transitions again.
  ConnectToNone();
  EXPECT_EQ(TestObserver::STOPPED, observer_.state());
  ConnectToWifi();
  EXPECT_EQ(TestObserver::IDLE, observer_.state());
  ConnectToCellular();
  EXPECT_EQ(TestObserver::IDLE, observer_.state());
  ConnectToWifi();
  EXPECT_EQ(TestObserver::IDLE, observer_.state());
  ConnectToNone();
  EXPECT_EQ(TestObserver::STOPPED, observer_.state());
  ConnectToCellular();
  EXPECT_EQ(TestObserver::IDLE, observer_.state());

  // Disable fetching over cellular network.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableDriveOverCellular, true);
  EXPECT_EQ(TestObserver::STOPPED, observer_.state());

  ConnectToWifi();
  EXPECT_EQ(TestObserver::IDLE, observer_.state());

  // Disable Drive feature.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableDrive, true);
  EXPECT_EQ(TestObserver::STOPPED, observer_.state());
}

TEST_F(DriveSyncClientTest, ObserveRunAllTaskQueue) {
  AddResourceIdToFetch("resource_id_foo");
  AddResourceIdToFetch("resource_id_bar");

  // Starts the sync queue, and eventually notifies the idle state.
  SetExpectationForGetFileByResourceId("resource_id_foo");
  SetExpectationForGetFileByResourceId("resource_id_bar");
  ConnectToWifi();
  EXPECT_EQ(TestObserver::IDLE, observer_.state());

  AddResourceIdToFetch("resource_id_foo");
  AddResourceIdToFetch("resource_id_bar");

  // Sync queue should stop on cellular network.
  ConnectToCellular();
  EXPECT_EQ(TestObserver::STOPPED, observer_.state());

  // Change of the preference should be notified to the observer.
  SetExpectationForGetFileByResourceId("resource_id_foo");
  SetExpectationForGetFileByResourceId("resource_id_bar");
  profile_->GetPrefs()->SetBoolean(prefs::kDisableDriveOverCellular, false);
  EXPECT_EQ(TestObserver::IDLE, observer_.state());
}

TEST_F(DriveSyncClientTest, ObserveDisconnection) {
  // Start sync loop.
  AddResourceIdToFetch("resource_id_foo");
  SetExpectationForGetFileByResourceId("resource_id_foo");

  ConnectToWifi();
  EXPECT_EQ(TestObserver::IDLE, observer_.state());

  // Disconnection during the sync loop should be notified.
  AddResourceIdToFetch("resource_id_bar");
  AddResourceIdToFetch("resource_id_buz");
  SetDisconnectingExpectationForGetFileByResourceId("resource_id_bar");

  sync_client_->StartSyncLoop();
  EXPECT_EQ(TestObserver::STOPPED, observer_.state());

  // So as the resume from the disconnection.
  SetExpectationForGetFileByResourceId("resource_id_bar");
  SetExpectationForGetFileByResourceId("resource_id_buz");
  ConnectToWifi();
  EXPECT_EQ(TestObserver::IDLE, observer_.state());
}

}  // namespace drive
