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
#include "base/test/test_timeouts.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
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
using ::testing::StrictMock;
using ::testing::_;

namespace gdata {

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
  scoped_ptr<GDataFileProto> file_proto(new GDataFileProto);
  file_proto->set_file_md5(md5);
  arg1.Run(error, FilePath(), file_proto.Pass());
}

class GDataSyncClientTest : public testing::Test {
 public:
  GDataSyncClientTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        io_thread_(content::BrowserThread::IO),
        sequence_token_(
            content::BrowserThread::GetBlockingPool()->GetSequenceToken()),
        profile_(new TestingProfile),
        mock_file_system_(new StrictMock<MockGDataFileSystem>),
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
    // Create a directory in the temporary directory for pinned symlinks.
    const FilePath pinned_dir =
        cache_->GetCacheDirectoryPath(GDataCache::CACHE_TYPE_PINNED);
    ASSERT_TRUE(file_util::CreateDirectory(pinned_dir));
    // Create a directory in the temporary directory for persistent files.
    const FilePath persistent_dir =
        cache_->GetCacheDirectoryPath(GDataCache::CACHE_TYPE_PERSISTENT);
    ASSERT_TRUE(file_util::CreateDirectory(persistent_dir));
    // Create a directory in the temporary directory for outgoing symlinks.
    const FilePath outgoing_dir =
        cache_->GetCacheDirectoryPath(GDataCache::CACHE_TYPE_OUTGOING);
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

  // Sets the expectation for MockGDataFileSystem::UpdateFileByResourceId(),
  // that simulates successful uploading of a file for the given resource ID.
  void SetExpectationForUpdateFileByResourceId(
      const std::string& resource_id) {
    EXPECT_CALL(*mock_file_system_,
                UpdateFileByResourceId(resource_id, _))
        .WillOnce(MockUpdateFileByResourceId(base::PLATFORM_FILE_OK));
  }

  // Sets the expectation for MockGDataFileSystem::GetFileInfoByResourceId(),
  // that simulates successful retrieval of file info for the given resource
  // ID.
  //
  // This is used for testing StartCheckingExistingPinnedFiles(), hence we
  // are only interested in the MD5 value in GDataFileProto.
  void SetExpectationForGetFileInfoByResourceId(
      const std::string& resource_id,
      const std::string& new_md5) {
  EXPECT_CALL(*mock_file_system_,
              GetFileInfoByResourceId(resource_id, _))
      .WillOnce(MockUpdateFileByResourceId(
          base::PLATFORM_FILE_OK,
          new_md5));
  }

  // Returns the resource IDs in the queue to be fetched.
  std::vector<std::string> GetResourceIdsToBeFetched() {
    return sync_client_->GetResourceIdsForTesting(
        GDataSyncClient::FETCH);
  }

  // Returns the resource IDs in the queue to be uploaded.
  std::vector<std::string> GetResourceIdsToBeUploaded() {
    return sync_client_->GetResourceIdsForTesting(
        GDataSyncClient::UPLOAD);
  }

  // Adds a resource ID of a file to fetch.
  void AddResourceIdToFetch(const std::string& resource_id) {
    sync_client_->AddResourceIdForTesting(GDataSyncClient::FETCH, resource_id);
  }

  // Adds a resource ID of a file to upload.
  void AddResourceIdToUpload(const std::string& resource_id) {
    sync_client_->AddResourceIdForTesting(GDataSyncClient::UPLOAD,
                                          resource_id);
  }

  // This class is used to monitor if any task is posted to a message loop.
  //
  // TODO(satorux): Move this class and RunBlockingPoolTask() to a common
  // place so it can be used from with GDataFileSystemTest. crbug.com/134126.
  class TaskObserver : public MessageLoop::TaskObserver {
   public:
    TaskObserver() : posted_(false) {}
    virtual ~TaskObserver() {}

    // MessageLoop::TaskObserver overrides.
    virtual void WillProcessTask(base::TimeTicks time_posted) {}
    virtual void DidProcessTask(base::TimeTicks time_posted) {
      posted_ = true;
    }

    // Returns true if any task was posted.
    bool posted() const { return posted_; }

   private:
    bool posted_;
    DISALLOW_COPY_AND_ASSIGN(TaskObserver);
  };

  // Runs a task posted to the blocking pool, including subquent tasks posted
  // to the UI message loop and the blocking pool.
  //
  // A task is often posted to the blocking pool with PostTaskAndReply(). In
  // that case, a task is posted back to the UI message loop, which can again
  // post a task to the blocking pool. This function processes these tasks
  // repeatedly.
  void RunBlockingPoolTask() {
    while (true) {
      content::BrowserThread::GetBlockingPool()->FlushForTesting();

      TaskObserver task_observer;
      MessageLoop::current()->AddTaskObserver(&task_observer);
      MessageLoop::current()->RunAllPending();
      MessageLoop::current()->RemoveTaskObserver(&task_observer);
      if (!task_observer.posted())
        break;
    }
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  const base::SequencedWorkerPool::SequenceToken sequence_token_;
  ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<StrictMock<MockGDataFileSystem> > mock_file_system_;
  GDataCache* cache_;
  scoped_ptr<GDataSyncClient> sync_client_;
  chromeos::MockNetworkLibrary* mock_network_library_;
  scoped_ptr<chromeos::Network> active_network_;
};

TEST_F(GDataSyncClientTest, StartInitialScan) {
  SetUpTestFiles();
  // Connect to no network, so the sync loop won't spin.
  ConnectToNone();

  // Kick off the cache initialization. This will scan the contents in the
  // test cache directory.
  cache_->RequestInitializeOnUIThread();
  // Start processing the files in the backlog. This will collect the
  // resource IDs of these files.
  sync_client_->StartProcessingBacklog();
  RunBlockingPoolTask();

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

TEST_F(GDataSyncClientTest, StartSyncLoop) {
  SetUpTestFiles();
  ConnectToWifi();

  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  AddResourceIdToUpload("resource_id_dirty");

  // The three files will be fetched by GetFileByResourceId(), once
  // StartSyncLoop() starts.
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_foo");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_bar");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_baz");
  SetExpectationForUpdateFileByResourceId("resource_id_dirty");

  sync_client_->StartSyncLoop();
}

TEST_F(GDataSyncClientTest, StartSyncLoop_Offline) {
  SetUpTestFiles();
  ConnectToNone();

  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  AddResourceIdToUpload("resource_id_dirty");

  // The three files will not be fetched by GetFileByResourceId(), as
  // network is not connected.
  EXPECT_CALL(*mock_file_system_, GetFileByResourceId(_, _, _)).Times(0);

  sync_client_->StartSyncLoop();
}

TEST_F(GDataSyncClientTest, StartSyncLoop_CelluarDisabled) {
  SetUpTestFiles();
  ConnectToWifi();  // First connect to Wifi.

  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  AddResourceIdToUpload("resource_id_dirty");

  // The three files will not be fetched by GetFileByResourceId(), as
  // fetching over cellular network is disabled by default.
  EXPECT_CALL(*mock_file_system_, GetFileByResourceId(_, _, _)).Times(0);

  // Then connect to cellular. This will kick off StartSyncLoop().
  ConnectToCellular();
}

TEST_F(GDataSyncClientTest, StartSyncLoop_CelluarEnabled) {
  SetUpTestFiles();
  ConnectToWifi();  // First connect to Wifi.

  // Enable fetching over cellular network.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableGDataOverCellular, false);

  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  AddResourceIdToUpload("resource_id_dirty");

  // The three files will be fetched by GetFileByResourceId(), as fetching
  // over cellular network is explicitly enabled.
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_foo");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_bar");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_baz");
  SetExpectationForUpdateFileByResourceId("resource_id_dirty");

  // Then connect to cellular. This will kick off StartSyncLoop().
  ConnectToCellular();
}

TEST_F(GDataSyncClientTest, StartSyncLoop_WimaxDisabled) {
  SetUpTestFiles();
  ConnectToWifi();  // First connect to Wifi.

  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  AddResourceIdToUpload("resource_id_dirty");

  // The three files will not be fetched by GetFileByResourceId(), as
  // fetching over wimax network is disabled by default.
  EXPECT_CALL(*mock_file_system_, GetFileByResourceId(_, _, _)).Times(0);

  // Then connect to wimax. This will kick off StartSyncLoop().
  ConnectToWimax();
}

TEST_F(GDataSyncClientTest, StartSyncLoop_CelluarEnabledWithWimax) {
  SetUpTestFiles();
  ConnectToWifi();  // First connect to Wifi.

  // Enable fetching over cellular network. This includes wimax.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableGDataOverCellular, false);

  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  AddResourceIdToUpload("resource_id_dirty");

  // The three files will be fetched by GetFileByResourceId(), as fetching
  // over cellular network, which includes wimax, is explicitly enabled.
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_foo");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_bar");
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_baz");
  SetExpectationForUpdateFileByResourceId("resource_id_dirty");

  // Then connect to wimax. This will kick off StartSyncLoop().
  ConnectToWimax();
}

TEST_F(GDataSyncClientTest, StartSyncLoop_GDataDisabled) {
  SetUpTestFiles();
  ConnectToWifi();

  // Disable the GData feature.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableGData, true);

  AddResourceIdToFetch("resource_id_not_fetched_foo");
  AddResourceIdToFetch("resource_id_not_fetched_bar");
  AddResourceIdToFetch("resource_id_not_fetched_baz");
  AddResourceIdToUpload("resource_id_dirty");

  // The three files will not be fetched by GetFileByResourceId(), as the
  // GData feature is disabled.
  EXPECT_CALL(*mock_file_system_, GetFileByResourceId(_, _, _)).Times(0);

  sync_client_->StartSyncLoop();
}

TEST_F(GDataSyncClientTest, OnCachePinned) {
  SetUpTestFiles();
  ConnectToWifi();

  // This file will be fetched by GetFileByResourceId() as OnCachePinned()
  // will kick off the sync loop.
  SetExpectationForGetFileByResourceId("resource_id_not_fetched_foo");

  sync_client_->OnCachePinned("resource_id_not_fetched_foo", "md5");
}

TEST_F(GDataSyncClientTest, OnCacheUnpinned) {
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

TEST_F(GDataSyncClientTest, Deduplication) {
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

TEST_F(GDataSyncClientTest, ExistingPinnedFiles) {
  SetUpTestFiles();
  // Connect to no network, so the sync loop won't spin.
  ConnectToNone();

  // Kick off the cache initialization. This will scan the contents in the
  // test cache directory.
  cache_->RequestInitializeOnUIThread();

  // Set the expectation so that the MockGDataFileSystem returns "new_md5"
  // for "resource_id_fetched". This simulates that the file is updated on
  // the server side, and the new MD5 is obtained from the server (i.e. the
  // local cach file is stale).
  SetExpectationForGetFileInfoByResourceId("resource_id_fetched",
                                           "new_md5");
  // Set the expectation so that the MockGDataFileSystem returns "some_md5"
  // for "resource_id_dirty". The MD5 on the server is always different from
  // the MD5 of a dirty file, which is set to "local". We should not collect
  // this by StartCheckingExistingPinnedFiles().
  SetExpectationForGetFileInfoByResourceId("resource_id_dirty",
                                           "some_md5");

  // Start checking the existing pinned files. This will collect the resource
  // IDs of pinned files, with stale local cache files.
  sync_client_->StartCheckingExistingPinnedFiles();
  RunBlockingPoolTask();

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
