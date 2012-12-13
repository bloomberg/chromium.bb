// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_scheduler.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive_test_util.h"
#include "chrome/browser/chromeos/drive/file_system/drive_operations.h"
#include "chrome/browser/chromeos/drive/file_system/copy_operation.h"
#include "chrome/browser/chromeos/drive/file_system/move_operation.h"
#include "chrome/browser/chromeos/drive/file_system/remove_operation.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

namespace google_apis {

class FakeDriveService : public DriveServiceInterface {
  virtual void Initialize(Profile* profile) {
  }

  virtual void AddObserver(DriveServiceObserver* observer) {
  }

  virtual void RemoveObserver(DriveServiceObserver* observer) {
  }

  virtual bool CanStartOperation() const {
    return true;
  }

  virtual void CancelAll() {
  }

  virtual bool CancelForFilePath(const FilePath& file_path) {
    return true;
  }

  virtual OperationProgressStatusList GetProgressStatusList() const {
    return OperationProgressStatusList();
  }

  virtual bool HasAccessToken() const {
    return true;
  }

  virtual bool HasRefreshToken() const {
    return true;
  }

  virtual void GetResourceList(const GURL& feed_url,
                               int64 start_changestamp,
                               const std::string& search_query,
                               bool shared_with_me,
                               const std::string& directory_resource_id,
                               const GetResourceListCallback& callback) {
    // TODO: Make this more flexible.
    if (feed_url == GURL("http://example.com/gdata/root_feed.json")) {
      // Make some sample data.
      scoped_ptr<base::Value> feed_data = google_apis::test_util::LoadJSONFile(
          "gdata/root_feed.json");
      scoped_ptr<google_apis::ResourceList> resource_list =
          google_apis::ResourceList::ExtractAndParse(*feed_data);
      base::MessageLoopProxy::current()->PostTask(FROM_HERE,
          base::Bind(callback,
                     HTTP_SUCCESS,
                     base::Passed(&resource_list)));
    } else {
      scoped_ptr<google_apis::ResourceList> resource_list;
      base::MessageLoopProxy::current()->PostTask(FROM_HERE,
          base::Bind(callback,
                     GDATA_PARSE_ERROR,
                     base::Passed(&resource_list)));
    }
  }

  virtual void GetResourceEntry(const std::string& resource_id,
                                const GetResourceEntryCallback& callback) {
  }

  virtual void GetAccountMetadata(const GetAccountMetadataCallback& callback) {
    // Make some sample data.
    scoped_ptr<Value> data = google_apis::test_util::LoadJSONFile(
        "gdata/account_metadata.json");
    scoped_ptr<google_apis::AccountMetadataFeed> account_metadata
        = google_apis::AccountMetadataFeed::CreateFrom(*data);

    base::MessageLoopProxy::current()->PostTask(FROM_HERE,
        base::Bind(callback,
                   HTTP_SUCCESS,
                   base::Passed(&account_metadata)));
  }

  virtual void GetApplicationInfo(const GetDataCallback& callback) {
    scoped_ptr<Value> data = google_apis::test_util::LoadJSONFile(
        "gdata/account_metadata.json");

    base::MessageLoopProxy::current()->PostTask(FROM_HERE,
        base::Bind(callback,
                   HTTP_SUCCESS,
                   base::Passed(&data)));
  }

  virtual void DeleteResource(const GURL& edit_url,
                              const EntryActionCallback& callback) {
  }

  virtual void DownloadHostedDocument(const FilePath& virtual_path,
                                      const FilePath& local_cache_path,
                                      const GURL& content_url,
                                      DocumentExportFormat format,
                                      const DownloadActionCallback& callback) {
  }

  virtual void CopyHostedDocument(const std::string& resource_id,
                                  const FilePath::StringType& new_name,
                                  const GetResourceEntryCallback& callback) {
  }

  virtual void RenameResource(const GURL& edit_url,
                              const FilePath::StringType& new_name,
                              const EntryActionCallback& callback) {
  }

  virtual void AddResourceToDirectory(const GURL& parent_content_url,
                                      const GURL& edit_url,
                                      const EntryActionCallback& callback) {
  }

  virtual void RemoveResourceFromDirectory(
      const GURL& parent_content_url,
      const std::string& resource_id,
      const EntryActionCallback& callback) {
  }

  virtual void AddNewDirectory(const GURL& parent_content_url,
                               const FilePath::StringType& directory_name,
                               const GetDataCallback& callback) {
  }

  virtual void DownloadFile(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& content_url,
      const DownloadActionCallback& download_action_callback,
      const GetContentCallback& get_content_callback) {
  }

  virtual void InitiateUpload(const InitiateUploadParams& params,
                              const InitiateUploadCallback& callback) {
  }

  virtual void ResumeUpload(const ResumeUploadParams& params,
                            const ResumeUploadCallback& callback) {
  }

  virtual void AuthorizeApp(const GURL& edit_url,
                            const std::string& app_id,
                            const GetDataCallback& callback) {
  }

};

} // namespace google_apis

namespace drive {

namespace {

class MockNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  MOCK_CONST_METHOD0(GetCurrentConnectionType,
                     net::NetworkChangeNotifier::ConnectionType());
};

class MockCopyOperation : public file_system::CopyOperation {
 public:
  MockCopyOperation()
      : file_system::CopyOperation(NULL, NULL, NULL, NULL, NULL, NULL) {
  }

  MOCK_METHOD3(Copy, void(const FilePath& src_file_path,
                          const FilePath& dest_file_path,
                          const FileOperationCallback& callback));

  MOCK_METHOD3(TransferFileFromRemoteToLocal,
               void(const FilePath& remote_src_file_path,
                    const FilePath& local_dest_file_path,
                    const FileOperationCallback& callback));

  MOCK_METHOD3(TransferFileFromLocalToRemote,
               void(const FilePath& local_src_file_path,
                    const FilePath& remote_dest_file_path,
                    const FileOperationCallback& callback));

  MOCK_METHOD3(TransferRegularFile,
               void(const FilePath& local_src_file_path,
                    const FilePath& remote_dest_file_path,
                    const FileOperationCallback& callback));
};

class MockMoveOperation : public file_system::MoveOperation {
 public:
  MockMoveOperation()
      : file_system::MoveOperation(NULL, NULL, NULL) {
  }

  MOCK_METHOD3(Move, void(const FilePath& src_file_path,
                          const FilePath& dest_file_path,
                          const FileOperationCallback& callback));
};

class MockRemoveOperation : public file_system::RemoveOperation {
 public:
  MockRemoveOperation()
      : file_system::RemoveOperation(NULL, NULL, NULL, NULL) {
  }

  MOCK_METHOD3(Remove, void(const FilePath& file_path,
                            bool is_recursive,
                            const FileOperationCallback& callback));
};

// Action used to set mock expectations,
ACTION_P(MockOperation, status) {
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
                                              base::Bind(arg2, status));
}

}  // namespace

class DriveSchedulerTest : public testing::Test {
 public:
  DriveSchedulerTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        profile_(new TestingProfile) {
  }

  virtual void SetUp() OVERRIDE {
    mock_network_change_notifier_.reset(new MockNetworkChangeNotifier);

    fake_drive_service_.reset(new google_apis::FakeDriveService());
    mock_copy_operation_ = new StrictMock<MockCopyOperation>();
    mock_move_operation_ = new StrictMock<MockMoveOperation>();
    mock_remove_operation_ = new StrictMock<MockRemoveOperation>();
    drive_operations_.InitForTesting(mock_copy_operation_,
                                     mock_move_operation_,
                                     mock_remove_operation_,
                                     NULL);
    scheduler_.reset(new DriveScheduler(profile_.get(),
                                        fake_drive_service_.get(),
                                        &drive_operations_));

    scheduler_->Initialize();
    scheduler_->SetDisableThrottling(true);
  }

  virtual void TearDown() OVERRIDE {
    // The scheduler should be deleted before NetworkLibrary, as it
    // registers itself as observer of NetworkLibrary.
    scheduler_.reset();
    google_apis::test_util::RunBlockingPoolTask();
    fake_drive_service_.reset();
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
    scheduler_->OnConnectionTypeChanged(type);
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

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<DriveScheduler> scheduler_;
  scoped_ptr<MockNetworkChangeNotifier> mock_network_change_notifier_;
  scoped_ptr<google_apis::FakeDriveService> fake_drive_service_;

  file_system::DriveOperations drive_operations_;
  StrictMock<MockCopyOperation>* mock_copy_operation_;
  StrictMock<MockMoveOperation>* mock_move_operation_;
  StrictMock<MockRemoveOperation>* mock_remove_operation_;
};

TEST_F(DriveSchedulerTest, CopyFile) {
  ConnectToWifi();

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_file(FILE_PATH_LITERAL("drive/File 1.txt"));
  EXPECT_CALL(*mock_copy_operation_, Copy(file_in_root, dest_file, _))
      .WillOnce(MockOperation(DRIVE_FILE_OK));

  DriveFileError error(DRIVE_FILE_ERROR_FAILED);
  scheduler_->Copy(
      file_in_root, dest_file,
      base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback, &error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(DRIVE_FILE_OK, error);
}

TEST_F(DriveSchedulerTest, TransferFileFromRemoteToLocalFile) {
  ConnectToWifi();

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_file(FILE_PATH_LITERAL("drive/File 1.txt"));
  EXPECT_CALL(*mock_copy_operation_,
              TransferFileFromRemoteToLocal(file_in_root, dest_file, _))
      .WillOnce(MockOperation(DRIVE_FILE_OK));

  DriveFileError error(DRIVE_FILE_ERROR_FAILED);
  scheduler_->TransferFileFromRemoteToLocal(
      file_in_root, dest_file,
      base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback, &error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(DRIVE_FILE_OK, error);
}

TEST_F(DriveSchedulerTest, TransferFileFromLocalToRemoteFile) {
  ConnectToWifi();

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_file(FILE_PATH_LITERAL("drive/File 1.txt"));
  EXPECT_CALL(*mock_copy_operation_,
              TransferFileFromLocalToRemote(file_in_root, dest_file, _))
      .WillOnce(MockOperation(DRIVE_FILE_OK));

  DriveFileError error(DRIVE_FILE_ERROR_FAILED);
  scheduler_->TransferFileFromLocalToRemote(
      file_in_root, dest_file,
      base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback, &error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(DRIVE_FILE_OK, error);
}

TEST_F(DriveSchedulerTest, TransferRegularFileFile) {
  ConnectToWifi();

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_file(FILE_PATH_LITERAL("drive/File 1.txt"));
  EXPECT_CALL(*mock_copy_operation_,
              TransferRegularFile(file_in_root, dest_file, _))
      .WillOnce(MockOperation(DRIVE_FILE_OK));

  DriveFileError error(DRIVE_FILE_ERROR_FAILED);
  scheduler_->TransferRegularFile(
      file_in_root, dest_file,
      base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback, &error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(DRIVE_FILE_OK, error);
}

TEST_F(DriveSchedulerTest, GetApplicationInfo) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> value;

  scheduler_->GetApplicationInfo(
      base::Bind(&google_apis::test_util::CopyResultsFromGetDataCallback,
                 &error,
                 &value));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(value);
}

TEST_F(DriveSchedulerTest, GetAccountMetadata) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::AccountMetadataFeed> account_metadata;

  scheduler_->GetAccountMetadata(
      base::Bind(
          &google_apis::test_util::CopyResultsFromGetAccountMetadataCallback,
          &error,
          &account_metadata));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(account_metadata);
}


TEST_F(DriveSchedulerTest, GetResourceList) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceList> resource_list;

  scheduler_->GetResourceList(
      GURL("http://example.com/gdata/root_feed.json"),
      0,
      std::string(),
      true,
      std::string(),
      base::Bind(
          &google_apis::test_util::CopyResultsFromGetResourceListCallback,
          &error,
          &resource_list));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
}

TEST_F(DriveSchedulerTest, MoveFile) {
  ConnectToWifi();

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_file(FILE_PATH_LITERAL("drive/File 1.txt"));
  EXPECT_CALL(*mock_move_operation_, Move(file_in_root, dest_file, _))
      .WillOnce(MockOperation(DRIVE_FILE_OK));

  DriveFileError error(DRIVE_FILE_ERROR_FAILED);
  scheduler_->Move(
      file_in_root, dest_file,
      base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback, &error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(DRIVE_FILE_OK, error);
}

TEST_F(DriveSchedulerTest, MoveFileRetry) {
  ConnectToWifi();

  // This will fail once with a throttled message.  It tests that the scheduler
  // will retry in this case.
  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_file(FILE_PATH_LITERAL("drive/File 1.txt"));
  EXPECT_CALL(*mock_move_operation_, Move(file_in_root, dest_file, _))
      .WillOnce(MockOperation(DRIVE_FILE_ERROR_THROTTLED))
      .WillOnce(MockOperation(DRIVE_FILE_OK));

  DriveFileError error(DRIVE_FILE_ERROR_FAILED);
  scheduler_->Move(
      file_in_root, dest_file,
      base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback, &error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(DRIVE_FILE_OK, error);
}

TEST_F(DriveSchedulerTest, RemoveFile) {
  ConnectToWifi();

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  EXPECT_CALL(*mock_remove_operation_, Remove(file_in_root, _, _))
      .WillOnce(MockOperation(DRIVE_FILE_OK));

  DriveFileError error;
  scheduler_->Remove(
      file_in_root, false,
      base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback, &error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(DRIVE_FILE_OK, error);
}

TEST_F(DriveSchedulerTest, RemoveFileRetry) {
  ConnectToWifi();

  // This will fail once with a throttled message.  It tests that the scheduler
  // will retry in this case.
  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  EXPECT_CALL(*mock_remove_operation_, Remove(file_in_root, _, _))
      .WillOnce(MockOperation(DRIVE_FILE_ERROR_THROTTLED))
      .WillOnce(MockOperation(DRIVE_FILE_OK));

  DriveFileError error;
  scheduler_->Remove(
      file_in_root, false,
      base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback, &error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(DRIVE_FILE_OK, error);
}

TEST_F(DriveSchedulerTest, QueueOperation_Offline) {
  ConnectToNone();

  // This file will not be removed, as network is not connected.
  EXPECT_CALL(*mock_remove_operation_, Remove(_, _, _)).Times(0);

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  DriveFileError error;
  scheduler_->Remove(
      file_in_root, false,
      base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback, &error));
  google_apis::test_util::RunBlockingPoolTask();
}

TEST_F(DriveSchedulerTest, QueueOperation_CelluarDisabled) {
  ConnectToCellular();

  // This file will not be removed, as fetching over cellular network is
  // disabled by default.
  EXPECT_CALL(*mock_remove_operation_, Remove(_, _, _)).Times(0);

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  DriveFileError error;
  scheduler_->Remove(
      file_in_root, false,
      base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback, &error));
  google_apis::test_util::RunBlockingPoolTask();
}

TEST_F(DriveSchedulerTest, QueueOperation_CelluarEnabled) {
  // Enable fetching over cellular network.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableDriveOverCellular, false);

  ConnectToCellular();

  // This file will be removed, as syncing over cellular network is explicitly
  // enabled.
  EXPECT_CALL(*mock_remove_operation_, Remove(_, _, _)).Times(1);

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  DriveFileError error;
  scheduler_->Remove(
      file_in_root, false,
      base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback, &error));
  google_apis::test_util::RunBlockingPoolTask();
}

TEST_F(DriveSchedulerTest, QueueOperation_WimaxDisabled) {
  // Then connect to wimax. This will kick off StartSyncLoop().
  ConnectToWimax();

  // This file will not be removed, as syncing over wimax network is disabled
  // by default.
  EXPECT_CALL(*mock_remove_operation_, Remove(_, _, _)).Times(0);

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  DriveFileError error;
  scheduler_->Remove(
      file_in_root, false,
      base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback, &error));
  google_apis::test_util::RunBlockingPoolTask();
}

TEST_F(DriveSchedulerTest, QueueOperation_CelluarEnabledWithWimax) {
  // Enable fetching over cellular network.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableDriveOverCellular, false);

  ConnectToWimax();

  // This file will be removed, as syncing over cellular network is explicitly
  // enabled.
  EXPECT_CALL(*mock_remove_operation_, Remove(_, _, _)).Times(1);

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  DriveFileError error;
  scheduler_->Remove(
      file_in_root, false,
      base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback, &error));
  google_apis::test_util::RunBlockingPoolTask();
}

TEST_F(DriveSchedulerTest, QueueOperation_DriveDisabled) {
  // Disable the Drive feature.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableDrive, true);

  // This file will not be removed, as the Drive feature is disabled.
  EXPECT_CALL(*mock_remove_operation_, Remove(_, _, _)).Times(0);

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  DriveFileError error;
  scheduler_->Remove(
      file_in_root, false,
      base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback, &error));
  google_apis::test_util::RunBlockingPoolTask();
}

}  // namespace drive
