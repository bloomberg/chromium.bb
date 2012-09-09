// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_sync_client.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"
#include "chrome/browser/chromeos/gdata/drive_test_util.h"
#include "chrome/browser/chromeos/gdata/mock_drive_file_system.h"
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

namespace gdata {

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

    // Initialize the sync client.
    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    cache_ = DriveCache::CreateDriveCacheOnUIThread(
        temp_dir_.path(),
        pool->GetSequencedTaskRunner(pool->GetSequenceToken()));
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
    cache_->DestroyOnUIThread();
    test_util::RunBlockingPoolTask();
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

  // Sets up test files in the temporary directory.
  void SetUpTestFiles() {
    // Create a directory in the temporary directory for pinned symlinks.
    const FilePath pinned_dir =
        cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_PINNED);
    ASSERT_TRUE(file_util::CreateDirectory(pinned_dir));
    // Create a directory in the temporary directory for persistent files.
    const FilePath persistent_dir =
        cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_PERSISTENT);
    ASSERT_TRUE(file_util::CreateDirectory(persistent_dir));
    // Create a directory in the temporary directory for outgoing symlinks.
    const FilePath outgoing_dir =
        cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_OUTGOING);
    ASSERT_TRUE(file_util::CreateDirectory(outgoing_dir));

    // Create a symlink in the pinned directory to /dev/null.
    // We'll collect this resource ID as a file to be fetched.
    ASSERT_TRUE(
        file_util::CreateSymbolicLink(
            FilePath::FromUTF8Unsafe("/dev/null"),
            pinned_dir.AppendASCII("resource_id_not_fetched_foo")));
    // Create some more.
    ASSERT_TRUE(
        file_util::CreateSymbolicLink(
            FilePath::FromUTF8Unsafe("/dev/null"),
            pinned_dir.AppendASCII("resource_id_not_fetched_bar")));
    ASSERT_TRUE(
        file_util::CreateSymbolicLink(
            FilePath::FromUTF8Unsafe("/dev/null"),
            pinned_dir.AppendASCII("resource_id_not_fetched_baz")));

    // Create a file in the persistent directory.
    const FilePath persistent_file_path =
        persistent_dir.AppendASCII("resource_id_fetched.md5");
    const std::string content = "hello";
    ASSERT_TRUE(file_util::WriteFile(
        persistent_file_path, content.data(), content.size()));
    // Create a symlink in the pinned directory to the test file.
    ASSERT_TRUE(
        file_util::CreateSymbolicLink(
            persistent_file_path,
            pinned_dir.AppendASCII("resource_id_fetched")));

    // Create a dirty file in the persistent directory.
    const FilePath dirty_file_path =
        persistent_dir.AppendASCII(std::string("resource_id_dirty.") +
                                   util::kLocallyModifiedFileExtension);
    std::string dirty_content = "dirty";
    ASSERT_TRUE(file_util::WriteFile(
        dirty_file_path, dirty_content.data(), dirty_content.size()));
    // Create a symlink in the outgoing directory to the dirty file.
    ASSERT_TRUE(
        file_util::CreateSymbolicLink(
            dirty_file_path,
            outgoing_dir.AppendASCII("resource_id_dirty")));
    // Create a symlink in the pinned directory to the dirty file.
    ASSERT_TRUE(
        file_util::CreateSymbolicLink(
            dirty_file_path,
            pinned_dir.AppendASCII("resource_id_dirty")));
  }

  // Sets the expectation for MockDriveFileSystem::GetFileByResourceId(),
  // that simulates successful retrieval of a file for the given resource ID.
  void SetExpectationForGetFileByResourceId(const std::string& resource_id) {
    EXPECT_CALL(*mock_file_system_,
                GetFileByResourceId(resource_id, _, _))
        .WillOnce(MockGetFileByResourceId(
            DRIVE_FILE_OK,
            FilePath::FromUTF8Unsafe("local_path_does_not_matter"),
            std::string("mime_type_does_not_matter"),
            REGULAR_FILE));
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

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<StrictMock<MockDriveFileSystem> > mock_file_system_;
  DriveCache* cache_;
  scoped_ptr<DriveSyncClient> sync_client_;
  scoped_ptr<MockNetworkChangeNotifier> mock_network_change_notifier_;
};

TEST_F(DriveSyncClientTest, StartInitialScan) {
  SetUpTestFiles();
  // Connect to no network, so the sync loop won't spin.
  ConnectToNone();

  // Kick off the cache initialization. This will scan the contents in the
  // test cache directory.
  cache_->RequestInitializeOnUIThread();
  // Start processing the files in the backlog. This will collect the
  // resource IDs of these files.
  sync_client_->StartProcessingBacklog();
  test_util::RunBlockingPoolTask();

  // Check the contents of the queue for fetching.
  std::vector<std::string> resource_ids =
      GetResourceIdsToBeFetched();
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
  SetUpTestFiles();
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
  SetUpTestFiles();
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
  SetUpTestFiles();
  ConnectToWifi();
  AddResourceIdToFetch(resource_id);

  // Disconnect from network on fetch try.
  EXPECT_CALL(*mock_file_system_, GetFileByResourceId(resource_id, _, _))
      .WillOnce(DoAll(
          InvokeWithoutArgs(this, &DriveSyncClientTest::ConnectToNone),
          MockGetFileByResourceId(DRIVE_FILE_ERROR_NO_CONNECTION,
                                  file_path,
                                  mime_type,
                                  REGULAR_FILE)));

  sync_client_->StartSyncLoop();

  // Expect fetch retry on network reconnection.
  EXPECT_CALL(*mock_file_system_, GetFileByResourceId(resource_id, _, _))
      .WillOnce(MockGetFileByResourceId(
          DRIVE_FILE_OK, file_path, mime_type, REGULAR_FILE));

  ConnectToWifi();
}

TEST_F(DriveSyncClientTest, StartSyncLoop_CelluarDisabled) {
  SetUpTestFiles();
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
  SetUpTestFiles();
  ConnectToWifi();  // First connect to Wifi.

  // Enable fetching over cellular network.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableGDataOverCellular, false);

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
  SetUpTestFiles();
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
  SetUpTestFiles();
  ConnectToWifi();  // First connect to Wifi.

  // Enable fetching over cellular network. This includes wimax.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableGDataOverCellular, false);

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
  SetUpTestFiles();
  ConnectToWifi();

  // Disable the Drive feature.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableGData, true);

  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  AddResourceIdToUpload("resource_id_dirty");

  // These files will be neither fetched nor uploaded not by DriveFileSystem,
  // as the Drive feature is disabled.

  sync_client_->StartSyncLoop();
}

TEST_F(DriveSyncClientTest, OnCachePinned) {
  SetUpTestFiles();
  ConnectToWifi();

  // This file will be fetched by GetFileByResourceId() as OnCachePinned()
  // will kick off the sync loop.
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_foo");

  sync_client_->OnCachePinned("resource_id_not_fetched_foo", "md5");
}

TEST_F(DriveSyncClientTest, OnCacheUnpinned) {
  SetUpTestFiles();

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
  SetUpTestFiles();
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
  SetUpTestFiles();
  // Connect to no network, so the sync loop won't spin.
  ConnectToNone();

  // Kick off the cache initialization. This will scan the contents in the
  // test cache directory.
  cache_->RequestInitializeOnUIThread();

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
  test_util::RunBlockingPoolTask();

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

}  // namespace gdata
