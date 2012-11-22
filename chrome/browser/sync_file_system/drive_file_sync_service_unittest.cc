// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_service.h"

#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/google_apis/drive_uploader.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/mock_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/browser/sync_file_system/drive_file_sync_client.h"
#include "chrome/browser/sync_file_system/drive_metadata_store.h"
#include "chrome/test/base/testing_profile.h"
#include "net/base/escape.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::StrictMock;
using ::testing::_;

using google_apis::DriveServiceInterface;
using google_apis::DriveUploaderInterface;
using google_apis::test_util::LoadJSONFile;

namespace sync_file_system {

namespace {

const char kSyncRootDirectoryName[] = "Chrome Syncable FileSystem";

FilePath::StringType ASCIIToFilePathString(const std::string& path) {
  return FilePath().AppendASCII(path).value();
}

void DidInitialize(bool* done, fileapi::SyncStatusCode status, bool created) {
  EXPECT_FALSE(*done);
  *done = true;
  EXPECT_EQ(fileapi::SYNC_STATUS_OK, status);
  EXPECT_TRUE(created);
}

void ExpectEqStatus(bool* done,
                    fileapi::SyncStatusCode expected,
                    fileapi::SyncStatusCode actual) {
  EXPECT_FALSE(*done);
  *done = true;
  EXPECT_EQ(expected, actual);
}

}  // namespace

class MockRemoteServiceObserver : public RemoteFileSyncService::Observer {
 public:
  MockRemoteServiceObserver() {}
  virtual ~MockRemoteServiceObserver() {}

  // LocalChangeProcessor override.
  MOCK_METHOD1(OnRemoteChangeAvailable,
               void(int64 pending_changes));
  MOCK_METHOD2(OnRemoteServiceStateUpdated,
               void(RemoteServiceState state,
                    const std::string& description));
};

class DriveFileSyncServiceTest : public testing::Test {
 public:
  DriveFileSyncServiceTest()
      : mock_drive_service_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(fileapi::RegisterSyncableFileSystem(
        DriveFileSyncService::kServiceName));

    mock_drive_service_ = new StrictMock<google_apis::MockDriveService>;

    EXPECT_CALL(*mock_drive_service(),
                Initialize(&profile_)).Times(1);

    sync_client_ = DriveFileSyncClient::CreateForTesting(
        &profile_,
        scoped_ptr<DriveServiceInterface>(mock_drive_service_),
        scoped_ptr<DriveUploaderInterface>()).Pass();
    ASSERT_TRUE(base_dir_.CreateUniqueTempDir());
    metadata_store_.reset(new DriveMetadataStore(
        base_dir_.path(), base::MessageLoopProxy::current()));

    bool done = false;
    metadata_store_->Initialize(base::Bind(&DidInitialize, &done));
    message_loop_.RunUntilIdle();
    EXPECT_TRUE(done);
  }

  void SetUpDriveSyncService() {
    sync_service_ = DriveFileSyncService::CreateForTesting(
        sync_client_.Pass(), metadata_store_.Pass()).Pass();
    sync_service_->AddObserver(&mock_remote_observer_);
  }

  virtual void TearDown() OVERRIDE {
    if (sync_service_) {
      sync_service_.reset();
    }

    metadata_store_.reset();
    sync_client_.reset();
    mock_drive_service_ = NULL;

    EXPECT_TRUE(fileapi::RevokeSyncableFileSystem(
        DriveFileSyncService::kServiceName));
    message_loop_.RunUntilIdle();
  }

 protected:
  DriveFileSyncClient* sync_client() {
    if (sync_client_)
      return sync_client_.get();
    return sync_service_->sync_client_.get();
  }

  DriveMetadataStore* metadata_store() {
    if (metadata_store_)
      return metadata_store_.get();
    return sync_service_->metadata_store_.get();
  }

  StrictMock<google_apis::MockDriveService>* mock_drive_service() {
    return mock_drive_service_;
  }

  StrictMock<MockRemoteServiceObserver>* mock_remote_observer() {
    return &mock_remote_observer_;
  }

  MessageLoop* message_loop() { return &message_loop_; }
  DriveFileSyncService* sync_service() { return sync_service_.get(); }

  std::string FormatTitleQuery(const std::string& title) {
    return DriveFileSyncClient::FormatTitleQuery(title);
  }

  const DriveFileSyncService::PendingChangeQueue& pending_changes() const {
    return sync_service_->pending_changes_;
  }

 private:
  MessageLoop message_loop_;

  base::ScopedTempDir base_dir_;
  TestingProfile profile_;

  scoped_ptr<DriveFileSyncService> sync_service_;

  // Owned by |sync_client_|.
  StrictMock<google_apis::MockDriveService>* mock_drive_service_;
  StrictMock<MockRemoteServiceObserver> mock_remote_observer_;

  scoped_ptr<DriveFileSyncClient> sync_client_;
  scoped_ptr<DriveMetadataStore> metadata_store_;

  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncServiceTest);
};

// Invokes |arg0| as a GetDataCallback.
ACTION_P2(InvokeGetDataCallback0, error, result) {
  scoped_ptr<base::Value> value(result.Pass());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg0, error, base::Passed(&value)));
}

// Invokes |arg0| as a GetDataCallback.
ACTION_P2(InvokeGetDataCallback1, error, result) {
  scoped_ptr<base::Value> value(result.Pass());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg1, error, base::Passed(&value)));
}

// Invokes |arg2| as a GetDataCallback.
ACTION_P2(InvokeGetDataCallback2, error, result) {
  scoped_ptr<base::Value> value(result.Pass());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg2, error, base::Passed(&value)));
}

// Invokes |arg5| as a GetDataCallback.
ACTION_P2(InvokeGetDataCallback5, error, result) {
  scoped_ptr<base::Value> value(result.Pass());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg5, error, base::Passed(&value)));
}

#if !defined(OS_ANDROID)

TEST_F(DriveFileSyncServiceTest, GetSyncRoot) {
  scoped_ptr<Value> sync_root_found(LoadJSONFile(
      "sync_file_system/sync_root_found.json"));
  std::string query = FormatTitleQuery(kSyncRootDirectoryName);
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL(), 0, query, false, std::string(), _))
      .WillOnce(InvokeGetDataCallback5(
          google_apis::HTTP_SUCCESS,
          base::Passed(&sync_root_found)));

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(1);

  SetUpDriveSyncService();
  message_loop()->RunUntilIdle();

  EXPECT_EQ("folder:sync_root_resource_id",
            metadata_store()->sync_root_directory());

  EXPECT_EQ(0u, metadata_store()->batch_sync_origins().size());
  EXPECT_EQ(0u, metadata_store()->incremental_sync_origins().size());
  EXPECT_EQ(0u, pending_changes().size());
}

TEST_F(DriveFileSyncServiceTest, BatchSyncOnInitialization) {
  const GURL kOrigin1("http://example.com");
  const GURL kOrigin2("http://hoge.example.com");
  const std::string kDirectoryResourceId1(
      "folder:origin_directory_resource_id");
  const std::string kDirectoryResourceId2(
      "folder:origin_directory_resource_id2");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");

  metadata_store()->SetSyncRootDirectory(kSyncRootResourceId);
  metadata_store()->AddBatchSyncOrigin(kOrigin1, kDirectoryResourceId1);
  metadata_store()->AddBatchSyncOrigin(kOrigin2, kDirectoryResourceId2);
  metadata_store()->MoveBatchSyncOriginToIncremental(kOrigin2);

  InSequence sequence;

  scoped_ptr<Value> account_metadata(LoadJSONFile(
      "gdata/account_metadata.json"));
  EXPECT_CALL(*mock_drive_service(),
              GetAccountMetadata(_))
      .WillOnce(InvokeGetDataCallback0(
          google_apis::HTTP_SUCCESS,
          base::Passed(&account_metadata)));

  scoped_ptr<Value> listing_files_in_directory(LoadJSONFile(
      "sync_file_system/listing_files_in_directory.json"));
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL(), 0, std::string(), false,
                           kDirectoryResourceId1, _))
      .WillOnce(InvokeGetDataCallback5(
          google_apis::HTTP_SUCCESS,
          base::Passed(&listing_files_in_directory)));

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(1);

  SetUpDriveSyncService();
  message_loop()->RunUntilIdle();

  // kOrigin1 should be a batch sync origin and kOrigin2 should be an
  // incremental sync origin.
  // 4 pending remote changes are from listing_files_in_directory as batch sync
  // changes.
  EXPECT_EQ(1u, metadata_store()->batch_sync_origins().size());
  EXPECT_EQ(1u, metadata_store()->incremental_sync_origins().size());
  EXPECT_EQ(4u, pending_changes().size());
}

TEST_F(DriveFileSyncServiceTest, RegisterNewOrigin) {
  const GURL kOrigin("http://example.com");
  const std::string kDirectoryResourceId("folder:origin_directory_resource_id");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");
  const GURL kSyncRootContentURL("https://sync_root_content_url/");

  metadata_store()->SetSyncRootDirectory(kSyncRootResourceId);

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(AtLeast(1));

  InSequence sequence;

  scoped_ptr<Value> origin_directory_not_found(LoadJSONFile(
      "sync_file_system/origin_directory_not_found.json"));
  std::string query = FormatTitleQuery(kOrigin.spec());

  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL(), 0, query, false, kSyncRootResourceId, _))
      .WillOnce(InvokeGetDataCallback5(
          google_apis::HTTP_SUCCESS,
          base::Passed(&origin_directory_not_found)))
      .RetiresOnSaturation();

  // If the directory for the origin is missing, DriveFileSyncService should
  // attempt to create it. And as a preparation, it should fetch the content url
  // of the parent directory.
  //
  // |sync_root_entry| contains kSyncRootContentURL which is to be added as
  // a new origin directory under the root directory.
  scoped_ptr<Value> sync_root_entry(LoadJSONFile(
      "sync_file_system/sync_root_entry.json"));
  EXPECT_CALL(*mock_drive_service(),
              GetDocumentEntry(kSyncRootResourceId, _))
      .WillOnce(InvokeGetDataCallback1(
          google_apis::HTTP_SUCCESS,
          base::Passed(&sync_root_entry)));

  scoped_ptr<Value> origin_directory_created(LoadJSONFile(
      "sync_file_system/origin_directory_created.json"));
  FilePath::StringType dirname =
      FilePath().AppendASCII(kOrigin.spec()).value();
  EXPECT_CALL(*mock_drive_service(),
              AddNewDirectory(kSyncRootContentURL, dirname, _))
      .WillOnce(InvokeGetDataCallback2(
          google_apis::HTTP_SUCCESS,
          base::Passed(&origin_directory_created)));

  // Once the directory is created GetAccountMetadata should be called to get
  // the largest changestamp for the origin as a prepariation of the batch sync.
  scoped_ptr<Value> account_metadata(LoadJSONFile(
      "gdata/account_metadata.json"));
  EXPECT_CALL(*mock_drive_service(), GetAccountMetadata(_))
      .WillOnce(InvokeGetDataCallback0(
          google_apis::HTTP_SUCCESS,
          base::Passed(&account_metadata)));

  scoped_ptr<Value> listing_files_in_empty_directory(LoadJSONFile(
      "sync_file_system/listing_files_in_empty_directory.json"));
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL(), 0, std::string(), false,
                           kDirectoryResourceId, _))
      .WillOnce(InvokeGetDataCallback5(
          google_apis::HTTP_SUCCESS,
          base::Passed(&listing_files_in_empty_directory)));

  SetUpDriveSyncService();
  bool done = false;
  sync_service()->RegisterOriginForTrackingChanges(
      kOrigin, base::Bind(&ExpectEqStatus, &done, fileapi::SYNC_STATUS_OK));
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(done);

  EXPECT_EQ(1u, metadata_store()->batch_sync_origins().size());
  EXPECT_TRUE(metadata_store()->incremental_sync_origins().empty());
  EXPECT_TRUE(pending_changes().empty());
}

TEST_F(DriveFileSyncServiceTest, RegisterExistingOrigin) {
  const GURL kOrigin("http://example.com");
  const std::string kDirectoryResourceId("folder:origin_directory_resource_id");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");

  metadata_store()->SetSyncRootDirectory(kSyncRootResourceId);

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(AtLeast(1));

  InSequence sequence;

  scoped_ptr<Value> origin_directory_found(LoadJSONFile(
      "sync_file_system/origin_directory_found.json"));
  std::string query = FormatTitleQuery("http://example.com/");
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL(), 0, query, false, kSyncRootResourceId, _))
      .WillOnce(InvokeGetDataCallback5(
          google_apis::HTTP_SUCCESS,
          base::Passed(&origin_directory_found)))
      .RetiresOnSaturation();

  scoped_ptr<Value> account_metadata(LoadJSONFile(
      "gdata/account_metadata.json"));
  EXPECT_CALL(*mock_drive_service(), GetAccountMetadata(_))
      .WillOnce(InvokeGetDataCallback0(
          google_apis::HTTP_SUCCESS,
          base::Passed(&account_metadata)));

  // DriveFileSyncService should fetch the list of the directory content
  // to start the batch sync.
  scoped_ptr<Value> listing_files_in_directory(LoadJSONFile(
      "sync_file_system/listing_files_in_directory.json"));
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL(), 0, std::string(),
                           false, kDirectoryResourceId, _))
      .WillOnce(InvokeGetDataCallback5(
          google_apis::HTTP_SUCCESS,
          base::Passed(&listing_files_in_directory)));

  SetUpDriveSyncService();
  bool done = false;
  sync_service()->RegisterOriginForTrackingChanges(
      kOrigin, base::Bind(&ExpectEqStatus, &done, fileapi::SYNC_STATUS_OK));
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(done);

  // The origin should be registered as a batch sync origin.
  EXPECT_EQ(1u, metadata_store()->batch_sync_origins().size());
  EXPECT_TRUE(metadata_store()->incremental_sync_origins().empty());

  // |listing_files_in_directory| contains 4 items to sync.
  EXPECT_EQ(4u, pending_changes().size());
}

TEST_F(DriveFileSyncServiceTest, UnregisterOrigin) {
  const GURL kOrigin1("chrome-extension://example1");
  const GURL kOrigin2("chrome-extension://example2");
  const std::string kDirectoryResourceId1(
      "folder:origin_directory_resource_id");
  const std::string kDirectoryResourceId2(
      "folder:origin_directory_resource_id2");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");

  metadata_store()->SetSyncRootDirectory(kSyncRootResourceId);
  metadata_store()->AddBatchSyncOrigin(kOrigin1, kDirectoryResourceId1);
  metadata_store()->AddBatchSyncOrigin(kOrigin2, kDirectoryResourceId2);
  metadata_store()->MoveBatchSyncOriginToIncremental(kOrigin2);

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(AtLeast(1));

  InSequence sequence;

  scoped_ptr<Value> account_metadata(LoadJSONFile(
      "gdata/account_metadata.json"));
  EXPECT_CALL(*mock_drive_service(),
              GetAccountMetadata(_))
      .WillOnce(InvokeGetDataCallback0(
          google_apis::HTTP_SUCCESS,
          base::Passed(&account_metadata)));

  scoped_ptr<Value> listing_files_in_directory(LoadJSONFile(
      "sync_file_system/listing_files_in_directory.json"));
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL(), 0, std::string(), false,
                           kDirectoryResourceId1, _))
      .WillOnce(InvokeGetDataCallback5(
          google_apis::HTTP_SUCCESS,
          base::Passed(&listing_files_in_directory)));

  SetUpDriveSyncService();
  message_loop()->RunUntilIdle();

  EXPECT_EQ(1u, metadata_store()->batch_sync_origins().size());
  EXPECT_EQ(1u, metadata_store()->incremental_sync_origins().size());
  EXPECT_EQ(4u, pending_changes().size());

  bool done = false;
  sync_service()->UnregisterOriginForTrackingChanges(
      kOrigin1, base::Bind(&ExpectEqStatus, &done, fileapi::SYNC_STATUS_OK));
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(done);

  EXPECT_TRUE(metadata_store()->batch_sync_origins().empty());
  EXPECT_EQ(1u, metadata_store()->incremental_sync_origins().size());
  EXPECT_TRUE(pending_changes().empty());
}

#endif  // !defined(OS_ANDROID)

}  // namespace sync_file_system
