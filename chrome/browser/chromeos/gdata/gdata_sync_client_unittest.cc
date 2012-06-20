// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_sync_client.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/gdata/mock_gdata_file_system.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::_;

namespace gdata {

// Action used to set mock expectations for GetFileByResourceId().
ACTION_P4(MockGetFileByResourceId, error, local_path, mime_type, file_type) {
  arg1.Run(error, local_path, mime_type, file_type);
}

class GDataSyncClientTest : public testing::Test {
 public:
  GDataSyncClientTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        io_thread_(content::BrowserThread::IO),
        sequence_token_(
            content::BrowserThread::GetBlockingPool()->GetSequenceToken()),
        profile_(new TestingProfile),
        mock_file_system_(new MockGDataFileSystem),
        mock_network_library_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    chromeos::CrosLibrary::Initialize(true /* use_stub */);

    // CrosLibrary takes ownership of MockNetworkLibrary.
    mock_network_library_ = new chromeos::MockNetworkLibrary;
    chromeos::CrosLibrary::Get()->GetTestApi()->SetNetworkLibrary(
        mock_network_library_, true);

    // Create a temporary directory.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Initialize the sync client.
    cache_ = GDataCache::CreateGDataCacheOnUIThread(
        temp_dir_.path(),
        content::BrowserThread::GetBlockingPool(),
        sequence_token_);
    sync_client_.reset(new GDataSyncClient(profile_.get(),
                                           mock_file_system_.get(),
                                           cache_));

    EXPECT_CALL(*mock_network_library_, AddNetworkManagerObserver(
        sync_client_.get())).Times(1);
    EXPECT_CALL(*mock_network_library_, RemoveNetworkManagerObserver(
        sync_client_.get())).Times(1);
    EXPECT_CALL(*mock_file_system_, AddObserver(sync_client_.get())).Times(1);

    sync_client_->Initialize();
  }

  virtual void TearDown() OVERRIDE {
    // The sync client should be deleted before NetworkLibrary, as the sync
    // client registers itself as observer of NetworkLibrary.
    sync_client_.reset();
    chromeos::CrosLibrary::Shutdown();
    cache_->DestroyOnUIThread();
    RunAllPendingForIO();
  }

  // Used to wait for the result from an operation that involves file IO,
  // that runs on the blocking pool thread.
  void RunAllPendingForIO() {
    // We should first flush tasks on UI thread, as it can require some
    // tasks to be run before IO tasks start.
    message_loop_.RunAllPending();
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    // Once IO tasks are done, flush UI thread again so the results from IO
    // tasks are processed.
    message_loop_.RunAllPending();
  }

  // Sets up MockNetworkLibrary as if it's connected to wifi network.
  void ConnectToWifi() {
    active_network_.reset(
        chromeos::Network::CreateForTesting(chromeos::TYPE_WIFI));
    EXPECT_CALL(*mock_network_library_, active_network())
        .Times(AnyNumber())
        .WillRepeatedly((Return(active_network_.get())));
    chromeos::Network::TestApi(active_network_.get()).SetConnected();
    // Notify the sync client that the network is changed. This is done via
    // NetworkLibrary in production, but here, we simulate the behavior by
    // directly calling OnNetworkManagerChanged().
    sync_client_->OnNetworkManagerChanged(mock_network_library_);
  }

  // Sets up MockNetworkLibrary as if it's connected to cellular network.
  void ConnectToCellular() {
    active_network_.reset(
        chromeos::Network::CreateForTesting(chromeos::TYPE_CELLULAR));
    EXPECT_CALL(*mock_network_library_, active_network())
        .Times(AnyNumber())
        .WillRepeatedly((Return(active_network_.get())));
    chromeos::Network::TestApi(active_network_.get()).SetConnected();
    sync_client_->OnNetworkManagerChanged(mock_network_library_);
  }

  // Sets up MockNetworkLibrary as if it's connected to wimax network.
  void ConnectToWimax() {
    active_network_.reset(
        chromeos::Network::CreateForTesting(chromeos::TYPE_WIMAX));
    EXPECT_CALL(*mock_network_library_, active_network())
        .Times(AnyNumber())
        .WillRepeatedly((Return(active_network_.get())));
    chromeos::Network::TestApi(active_network_.get()).SetConnected();
    sync_client_->OnNetworkManagerChanged(mock_network_library_);
  }

  // Sets up MockNetworkLibrary as if it's disconnected.
  void ConnectToNone() {
    active_network_.reset(
        chromeos::Network::CreateForTesting(chromeos::TYPE_WIFI));
    EXPECT_CALL(*mock_network_library_, active_network())
        .Times(AnyNumber())
        .WillRepeatedly((Return(active_network_.get())));
    chromeos::Network::TestApi(active_network_.get()).SetDisconnected();
    sync_client_->OnNetworkManagerChanged(mock_network_library_);
  }

  // Sets up test files in the temporary directory.
  void SetUpTestFiles() {
    // Create a directory in the temporary directory for pinned files.
    const FilePath pinned_dir =
        cache_->GetCacheDirectoryPath(GDataCache::CACHE_TYPE_PINNED);
    ASSERT_TRUE(file_util::CreateDirectory(pinned_dir));

    // Create a symlink in the temporary directory to /dev/null.
    // We'll collect this resource ID as a file to be fetched.
    ASSERT_TRUE(
        file_util::CreateSymbolicLink(
            FilePath::FromUTF8Unsafe("/dev/null"),
            pinned_dir.Append("resource_id_not_fetched_foo")));
    // Create some more.
    ASSERT_TRUE(
        file_util::CreateSymbolicLink(
            FilePath::FromUTF8Unsafe("/dev/null"),
            pinned_dir.Append("resource_id_not_fetched_bar")));
    ASSERT_TRUE(
        file_util::CreateSymbolicLink(
            FilePath::FromUTF8Unsafe("/dev/null"),
            pinned_dir.Append("resource_id_not_fetched_baz")));

    // Create a symlink in the temporary directory to a test file
    // We won't collect this resource ID, as it already exists.
    FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    FilePath test_file_path = test_data_dir.AppendASCII("chromeos")
        .AppendASCII("gdata").AppendASCII("testfile.txt");
    ASSERT_TRUE(
        file_util::CreateSymbolicLink(
            test_file_path,
            pinned_dir.Append("resource_id_fetched")));
  }

  // Called when StartInitialScan() is complete.
  void OnInitialScanComplete() {
    message_loop_.Quit();
  }

  // Sets the expectation for MockGDataFileSystem::GetFileByResourceId(),
  // that simulates successful retrieval of a file for the given resource ID.
  void SetExpectationForGetFileByResourceId(const std::string& resource_id) {
    EXPECT_CALL(*mock_file_system_,
                GetFileByResourceId(resource_id, _, _))
        .WillOnce(MockGetFileByResourceId(
            base::PLATFORM_FILE_OK,
            FilePath::FromUTF8Unsafe("local_path_does_not_matter"),
            std::string("mime_type_does_not_matter"),
            REGULAR_FILE));
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  const base::SequencedWorkerPool::SequenceToken sequence_token_;
  ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<MockGDataFileSystem> mock_file_system_;
  GDataCache* cache_;
  scoped_ptr<GDataSyncClient> sync_client_;
  chromeos::MockNetworkLibrary* mock_network_library_;
  scoped_ptr<chromeos::Network> active_network_;
};

TEST_F(GDataSyncClientTest, StartInitialScan) {
  SetUpTestFiles();

  sync_client_->StartInitialScan(
      base::Bind(&GDataSyncClientTest::OnInitialScanComplete,
                 base::Unretained(this)));
  // Wait until the initial scan is complete.
  message_loop_.Run();

  // Check the contents of the queue.
  const std::deque<std::string>& resource_ids =
      sync_client_->GetResourceIdsForTesting();
  ASSERT_EQ(3U, resource_ids.size());
  // Since these are the list of file names read from the disk, the order is
  // not guaranteed
  EXPECT_TRUE(std::find(resource_ids.begin(), resource_ids.end(),
                        "resource_id_not_fetched_foo") != resource_ids.end());
  EXPECT_TRUE(std::find(resource_ids.begin(), resource_ids.end(),
                        "resource_id_not_fetched_bar") != resource_ids.end());
  EXPECT_TRUE(std::find(resource_ids.begin(), resource_ids.end(),
                        "resource_id_not_fetched_baz") != resource_ids.end());
  // resource_id_fetched is not collected in the queue.
}

TEST_F(GDataSyncClientTest, StartFetchLoop) {
  SetUpTestFiles();
  ConnectToWifi();

  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_foo");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_bar");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_baz");

  // The three files will be fetched by GetFileByResourceId(), once
  // StartFetchLoop() starts.
  EXPECT_CALL(*mock_file_system_,
              GetFileByResourceId("resource_id_not_fetched_foo", _, _))
      .WillOnce(MockGetFileByResourceId(
          base::PLATFORM_FILE_OK,
          FilePath::FromUTF8Unsafe("local_path_does_not_matter"),
          std::string("mime_type_does_not_matter"),
          REGULAR_FILE));
  EXPECT_CALL(*mock_file_system_,
              GetFileByResourceId("resource_id_not_fetched_bar", _, _))
      .WillOnce(MockGetFileByResourceId(
          base::PLATFORM_FILE_OK,
          FilePath::FromUTF8Unsafe("local_path_does_not_matter"),
          std::string("mime_type_does_not_matter"),
          REGULAR_FILE));
  EXPECT_CALL(*mock_file_system_,
              GetFileByResourceId("resource_id_not_fetched_baz", _, _))
      .WillOnce(MockGetFileByResourceId(
          base::PLATFORM_FILE_OK,
          FilePath::FromUTF8Unsafe("local_path_does_not_matter"),
          std::string("mime_type_does_not_matter"),
          REGULAR_FILE));

  sync_client_->StartFetchLoop();
}

TEST_F(GDataSyncClientTest, StartFetchLoop_Offline) {
  SetUpTestFiles();
  ConnectToNone();

  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_foo");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_bar");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_baz");

  // The three files will not be fetched by GetFileByResourceId(), as
  // network is not connected.
  EXPECT_CALL(*mock_file_system_, GetFileByResourceId(_, _, _)).Times(0);

  sync_client_->StartFetchLoop();
}

TEST_F(GDataSyncClientTest, StartFetchLoop_CelluarDisabled) {
  SetUpTestFiles();
  ConnectToWifi();  // First connect to Wifi.

  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_foo");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_bar");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_baz");

  // The three files will not be fetched by GetFileByResourceId(), as
  // fetching over cellular network is disabled by default.
  EXPECT_CALL(*mock_file_system_, GetFileByResourceId(_, _, _)).Times(0);

  // Then connect to cellular. This will kick off StartFetchLoop().
  ConnectToCellular();
}

TEST_F(GDataSyncClientTest, StartFetchLoop_CelluarEnabled) {
  SetUpTestFiles();
  ConnectToWifi();  // First connect to Wifi.

  // Enable fetching over cellular network.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableGDataOverCellular, false);

  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_foo");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_bar");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_baz");

  // The three files will be fetched by GetFileByResourceId(), as fetching
  // over cellular network is explicitly enabled.
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_foo");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_bar");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_baz");

  // Then connect to cellular. This will kick off StartFetchLoop().
  ConnectToCellular();
}

TEST_F(GDataSyncClientTest, StartFetchLoop_WimaxDisabled) {
  SetUpTestFiles();
  ConnectToWifi();  // First connect to Wifi.

  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_foo");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_bar");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_baz");

  // The three files will not be fetched by GetFileByResourceId(), as
  // fetching over wimax network is disabled by default.
  EXPECT_CALL(*mock_file_system_, GetFileByResourceId(_, _, _)).Times(0);

  // Then connect to wimax. This will kick off StartFetchLoop().
  ConnectToWimax();
}

TEST_F(GDataSyncClientTest, StartFetchLoop_CelluarEnabledWithWimax) {
  SetUpTestFiles();
  ConnectToWifi();  // First connect to Wifi.

  // Enable fetching over cellular network. This includes wimax.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableGDataOverCellular, false);

  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_foo");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_bar");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_baz");

  // The three files will be fetched by GetFileByResourceId(), as fetching
  // over cellular network, which includes wimax, is explicitly enabled.
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_foo");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_bar");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_baz");

  // Then connect to wimax. This will kick off StartFetchLoop().
  ConnectToWimax();
}

TEST_F(GDataSyncClientTest, StartFetchLoop_GDataDisabled) {
  SetUpTestFiles();
  ConnectToWifi();

  // Disable the GData feature.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableGData, true);

  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_foo");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_bar");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_baz");

  // The three files will not be fetched by GetFileByResourceId(), as the
  // GData feature is disabled.
  EXPECT_CALL(*mock_file_system_, GetFileByResourceId(_, _, _)).Times(0);

  sync_client_->StartFetchLoop();
}

TEST_F(GDataSyncClientTest, OnCachePinned) {
  SetUpTestFiles();
  ConnectToWifi();

  // This file will be fetched by GetFileByResourceId() as OnFilePinned()
  // will kick off the fetch loop.
  EXPECT_CALL(*mock_file_system_,
              GetFileByResourceId("resource_id_not_fetched_foo", _, _))
      .WillOnce(MockGetFileByResourceId(
          base::PLATFORM_FILE_OK,
          FilePath::FromUTF8Unsafe("local_path_does_not_matter"),
          std::string("mime_type_does_not_matter"),
          REGULAR_FILE));

  sync_client_->OnCachePinned("resource_id_not_fetched_foo", "md5");
}

TEST_F(GDataSyncClientTest, OnCacheUnpinned) {
  SetUpTestFiles();

  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_foo");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_bar");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_baz");
  ASSERT_EQ(3U, sync_client_->GetResourceIdsForTesting().size());

  sync_client_->OnCacheUnpinned("resource_id_not_fetched_bar", "md5");
  // "bar" should be gone.
  std::deque<std::string> resource_ids =
      sync_client_->GetResourceIdsForTesting();
  ASSERT_EQ(2U, resource_ids.size());
  EXPECT_EQ("resource_id_not_fetched_foo", resource_ids[0]);
  EXPECT_EQ("resource_id_not_fetched_baz", resource_ids[1]);

  sync_client_->OnCacheUnpinned("resource_id_not_fetched_foo", "md5");
  // "foo" should be gone.
  resource_ids = sync_client_->GetResourceIdsForTesting();
  ASSERT_EQ(1U, resource_ids.size());
  EXPECT_EQ("resource_id_not_fetched_baz", resource_ids[1]);

  sync_client_->OnCacheUnpinned("resource_id_not_fetched_baz", "md5");
  // "baz" should be gone.
  resource_ids = sync_client_->GetResourceIdsForTesting();
  ASSERT_TRUE(resource_ids.empty());
}

}  // namespace gdata
