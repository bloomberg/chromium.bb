// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_service.h"

#include <utility>

#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/google_apis/drive_uploader.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/mock_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/browser/sync_file_system/drive_file_sync_client.h"
#include "chrome/browser/sync_file_system/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/mock_remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/escape.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/syncable/sync_file_metadata.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

#define FPL(x) FILE_PATH_LITERAL(x)

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::StrictMock;
using ::testing::_;

using google_apis::ResourceEntry;
using google_apis::DriveServiceInterface;
using google_apis::DriveUploaderInterface;
using google_apis::test_util::LoadJSONFile;

namespace sync_file_system {

namespace {

const char kSyncRootDirectoryName[] = "Chrome Syncable FileSystem";
const char* kServiceName = DriveFileSyncService::kServiceName;

FilePath::StringType ASCIIToFilePathString(const std::string& path) {
  return FilePath().AppendASCII(path).value();
}

void DidInitialize(bool* done, fileapi::SyncStatusCode status, bool created) {
  EXPECT_FALSE(*done);
  *done = true;
  EXPECT_EQ(fileapi::SYNC_STATUS_OK, status);
  EXPECT_TRUE(created);
}

void DidEntryOperation(fileapi::SyncStatusCode status) {
  EXPECT_EQ(fileapi::SYNC_STATUS_OK, status);
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
  MOCK_METHOD1(OnRemoteChangeQueueUpdated,
               void(int64 pending_changes));
  MOCK_METHOD2(OnRemoteServiceStateUpdated,
               void(RemoteServiceState state,
                    const std::string& description));
};

class DriveFileSyncServiceTest : public testing::Test {
 public:
  DriveFileSyncServiceTest()
      : file_thread_(content::BrowserThread::FILE, &message_loop_),
        mock_drive_service_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(fileapi::RegisterSyncableFileSystem(kServiceName));

    mock_drive_service_ = new StrictMock<google_apis::MockDriveService>;

    EXPECT_CALL(*mock_drive_service(), Initialize(&profile_));
    EXPECT_CALL(*mock_drive_service(), AddObserver(_));

    sync_client_ = DriveFileSyncClient::CreateForTesting(
        &profile_,
        GURL(google_apis::GDataWapiUrlGenerator::kBaseUrlForProduction),
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
        base_dir_.path(), sync_client_.Pass(), metadata_store_.Pass()).Pass();
    sync_service_->AddObserver(&mock_remote_observer_);
  }

  virtual void TearDown() OVERRIDE {
    EXPECT_CALL(*mock_drive_service(), RemoveObserver(_));
    EXPECT_CALL(*mock_drive_service(), CancelAll());

    if (sync_service_) {
      sync_service_.reset();
    }

    metadata_store_.reset();
    sync_client_.reset();
    mock_drive_service_ = NULL;

    EXPECT_TRUE(fileapi::RevokeSyncableFileSystem(kServiceName));
    message_loop_.RunUntilIdle();
  }

 protected:
  DriveFileSyncService::LocalSyncOperationType ResolveLocalSyncOperationType(
      const fileapi::FileChange& local_change,
      const fileapi::FileSystemURL& url) {
    return sync_service_->ResolveLocalSyncOperationType(local_change, url);
  }

  bool IsLocalSyncOperationAdd(
      DriveFileSyncService::LocalSyncOperationType type) {
    return type == DriveFileSyncService::LOCAL_SYNC_OPERATION_ADD;
  }

  bool IsLocalSyncOperationUpdate(
      DriveFileSyncService::LocalSyncOperationType type) {
    return type == DriveFileSyncService::LOCAL_SYNC_OPERATION_UPDATE;
  }

  bool IsLocalSyncOperationDelete(
      DriveFileSyncService::LocalSyncOperationType type) {
    return type == DriveFileSyncService::LOCAL_SYNC_OPERATION_DELETE;
  }

  bool IsLocalSyncOperationNone(
      DriveFileSyncService::LocalSyncOperationType type) {
    return type == DriveFileSyncService::LOCAL_SYNC_OPERATION_NONE;
  }

  bool IsLocalSyncOperationConflict(
      DriveFileSyncService::LocalSyncOperationType type) {
    return type == DriveFileSyncService::LOCAL_SYNC_OPERATION_CONFLICT;
  }

  bool IsLocalSyncOperationResolveToRemote(
      DriveFileSyncService::LocalSyncOperationType type) {
    return type == DriveFileSyncService::LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE;
  }

  void AddRemoteChange(int64 changestamp,
                       const std::string& resource_id,
                       const fileapi::FileSystemURL& url,
                       const fileapi::FileChange& file_change) {
    typedef DriveFileSyncService::PendingChangeQueue::iterator iterator;
    typedef DriveFileSyncService::ChangeQueueItem ChangeQueueItem;
    typedef DriveFileSyncService::RemoteSyncType RemoteSyncType;
    sync_service_->pending_changes_.clear();

    RemoteSyncType sync_type = DriveFileSyncService::REMOTE_SYNC_TYPE_BATCH;
    std::pair<iterator, bool> inserted_to_queue =
        sync_service_->pending_changes_.insert(
            ChangeQueueItem(changestamp, sync_type, url));
    DCHECK(inserted_to_queue.second);

    DriveFileSyncService::PathToChangeMap* path_to_change =
        &sync_service_->origin_to_changes_map_[url.origin()];
    (*path_to_change)[url.path()] = DriveFileSyncService::RemoteChange(
        changestamp, resource_id, sync_type, url, file_change,
        inserted_to_queue.first);
  }

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

  StrictMock<MockRemoteChangeProcessor>* mock_remote_processor() {
    return &mock_remote_processor_;
  }

  MessageLoop* message_loop() { return &message_loop_; }
  DriveFileSyncService* sync_service() { return sync_service_.get(); }

  std::string FormatTitleQuery(const std::string& title) {
    return DriveFileSyncClient::FormatTitleQuery(title);
  }

  const DriveFileSyncService::PendingChangeQueue& pending_changes() const {
    return sync_service_->pending_changes_;
  }

  fileapi::FileSystemURL CreateURL(const GURL& origin,
                                   const FilePath::StringType& path) {
    return fileapi::CreateSyncableFileSystemURL(
        origin, kServiceName, FilePath(path));
  }

  void ProcessRemoteChange(fileapi::SyncStatusCode expected_status,
                           const fileapi::FileSystemURL& expected_url,
                           fileapi::SyncOperationResult expected_result) {
    fileapi::SyncStatusCode actual_status = fileapi::SYNC_STATUS_UNKNOWN;
    fileapi::FileSystemURL actual_url;
    fileapi::SyncOperationResult actual_result = fileapi::SYNC_OPERATION_NONE;

    sync_service_->ProcessRemoteChange(
        mock_remote_processor(),
        base::Bind(&DriveFileSyncServiceTest::DidProcessRemoteChange,
                   base::Unretained(this),
                   &actual_status, &actual_url, &actual_result));
    message_loop_.RunUntilIdle();

    EXPECT_EQ(expected_status, actual_status);
    EXPECT_EQ(expected_url, actual_url);
    EXPECT_EQ(expected_result, actual_result);
  }

  void DidProcessRemoteChange(fileapi::SyncStatusCode* status_out,
                              fileapi::FileSystemURL* url_out,
                              fileapi::SyncOperationResult* result_out,
                              fileapi::SyncStatusCode status,
                              const fileapi::FileSystemURL& url,
                              fileapi::SyncOperationResult result) {
    *status_out = status;
    *url_out = url;
    *result_out = result;
  }

  void AppendIncrementalRemoteChange(const GURL& origin,
                                     const google_apis::ResourceEntry& entry,
                                     int64 changestamp) {
    sync_service_->AppendRemoteChange(
        origin, entry, changestamp,
        DriveFileSyncService::REMOTE_SYNC_TYPE_INCREMENTAL);
  }

 private:
  MessageLoop message_loop_;
  content::TestBrowserThread file_thread_;

  base::ScopedTempDir base_dir_;
  TestingProfile profile_;

  scoped_ptr<DriveFileSyncService> sync_service_;

  // Owned by |sync_client_|.
  StrictMock<google_apis::MockDriveService>* mock_drive_service_;
  StrictMock<MockRemoteServiceObserver> mock_remote_observer_;
  StrictMock<MockRemoteChangeProcessor> mock_remote_processor_;

  scoped_ptr<DriveFileSyncClient> sync_client_;
  scoped_ptr<DriveMetadataStore> metadata_store_;

  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncServiceTest);
};

ACTION(InvokeCompletionCallback) {
  base::MessageLoopProxy::current()->PostTask(FROM_HERE, arg1);
}

// Invokes |arg0| as a GetDataCallback.
ACTION_P2(InvokeGetAccountMetadataCallback0, error, result) {
  scoped_ptr<google_apis::AccountMetadataFeed> account_metadata(result.Pass());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg0, error, base::Passed(&account_metadata)));
}

// Invokes |arg1| as a GetResourceEntryCallback.
ACTION_P2(InvokeGetResourceEntryCallback1, error, result) {
  scoped_ptr<google_apis::ResourceEntry> entry(result.Pass());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg1, error, base::Passed(&entry)));
}

// Invokes |arg2| as a GetResourceEntryCallback.
ACTION_P2(InvokeGetResourceEntryCallback2, error, result) {
  scoped_ptr<google_apis::ResourceEntry> entry(result.Pass());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg2, error, base::Passed(&entry)));
}

// Invokes |arg5| as a GetResourceListCallback.
ACTION_P2(InvokeGetResourceListCallback5, error, result) {
  scoped_ptr<google_apis::ResourceList> resource_list(result.Pass());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg5, error, base::Passed(&resource_list)));
}

ACTION(PrepareForRemoteChange_Busy) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg2,
                 fileapi::SYNC_STATUS_FILE_BUSY,
                 fileapi::SyncFileMetadata(),
                 fileapi::FileChangeList()));
}

ACTION(PrepareForRemoteChange_NotFound) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg2,
                 fileapi::SYNC_STATUS_OK,
                 fileapi::SyncFileMetadata(fileapi::SYNC_FILE_TYPE_UNKNOWN, 0,
                                           base::Time()),
                 fileapi::FileChangeList()));
}

ACTION(PrepareForRemoteChange_NotModified) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg2,
                 fileapi::SYNC_STATUS_OK,
                 fileapi::SyncFileMetadata(fileapi::SYNC_FILE_TYPE_FILE, 0,
                                           base::Time()),
                 fileapi::FileChangeList()));
}

ACTION(InvokeDidDownloadFile) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(arg3, google_apis::HTTP_SUCCESS, arg1));
}

ACTION(InvokeDidApplyRemoteChange) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(arg3, fileapi::SYNC_STATUS_OK));
}

#if !defined(OS_ANDROID)

TEST_F(DriveFileSyncServiceTest, GetSyncRoot) {
  scoped_ptr<Value> sync_root_found_value(LoadJSONFile(
      "sync_file_system/sync_root_found.json"));
  scoped_ptr<google_apis::ResourceList> sync_root_found(
      google_apis::ResourceList::ExtractAndParse(*sync_root_found_value));
  std::string query = FormatTitleQuery(kSyncRootDirectoryName);
  EXPECT_CALL(*mock_drive_service(),
              GetResourceList(GURL(), 0, query, false, std::string(), _))
      .WillOnce(InvokeGetResourceListCallback5(
          google_apis::HTTP_SUCCESS,
          base::Passed(&sync_root_found)));

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(1);
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(0))
      .Times(AnyNumber());

  SetUpDriveSyncService();
  message_loop()->RunUntilIdle();

  EXPECT_EQ("folder:sync_root_resource_id",
            metadata_store()->sync_root_directory());

  EXPECT_EQ(0u, metadata_store()->batch_sync_origins().size());
  EXPECT_EQ(0u, metadata_store()->incremental_sync_origins().size());
  EXPECT_EQ(0u, pending_changes().size());
}

TEST_F(DriveFileSyncServiceTest, BatchSyncOnInitialization) {
  const GURL kOrigin1("chrome-extension://example");
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

  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(3))
      .Times(AnyNumber());

  InSequence sequence;

  scoped_ptr<Value> account_metadata_value(LoadJSONFile(
      "gdata/account_metadata.json"));
  scoped_ptr<google_apis::AccountMetadataFeed> account_metadata(
      google_apis::AccountMetadataFeed::CreateFrom(*account_metadata_value));
  EXPECT_CALL(*mock_drive_service(),
              GetAccountMetadata(_))
      .WillOnce(InvokeGetAccountMetadataCallback0(
          google_apis::HTTP_SUCCESS,
          base::Passed(&account_metadata)));

  scoped_ptr<Value> listing_files_in_directory_value(LoadJSONFile(
      "sync_file_system/listing_files_in_directory.json"));
  scoped_ptr<google_apis::ResourceList> listing_files_in_directory(
      google_apis::ResourceList::ExtractAndParse(
          *listing_files_in_directory_value));
  EXPECT_CALL(*mock_drive_service(),
              GetResourceList(GURL(), 0, std::string(), false,
                              kDirectoryResourceId1, _))
      .WillOnce(InvokeGetResourceListCallback5(
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
  EXPECT_EQ(3u, pending_changes().size());
}

TEST_F(DriveFileSyncServiceTest, RegisterNewOrigin) {
  const GURL kOrigin("chrome-extension://example");
  const std::string kDirectoryResourceId("folder:origin_directory_resource_id");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");
  const GURL kSyncRootContentURL("https://sync_root_content_url/");

  metadata_store()->SetSyncRootDirectory(kSyncRootResourceId);

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(AtLeast(1));
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(0))
      .Times(AnyNumber());

  InSequence sequence;

  scoped_ptr<Value> origin_directory_not_found_value(LoadJSONFile(
      "sync_file_system/origin_directory_not_found.json"));
  scoped_ptr<google_apis::ResourceList> origin_directory_not_found(
      google_apis::ResourceList::ExtractAndParse(
          *origin_directory_not_found_value));

  std::string query = FormatTitleQuery(
      DriveFileSyncClient::OriginToDirectoryTitle(kOrigin));

  EXPECT_CALL(*mock_drive_service(),
              GetResourceList(GURL(), 0, query, false, kSyncRootResourceId, _))
      .WillOnce(InvokeGetResourceListCallback5(
          google_apis::HTTP_SUCCESS,
          base::Passed(&origin_directory_not_found)))
      .RetiresOnSaturation();

  // If the directory for the origin is missing, DriveFileSyncService should
  // attempt to create it. And as a preparation, it should fetch the content url
  // of the parent directory.
  //
  // |sync_root_entry| contains kSyncRootContentURL which is to be added as
  // a new origin directory under the root directory.
  scoped_ptr<Value> sync_root_entry_value(LoadJSONFile(
      "sync_file_system/sync_root_entry.json"));
  scoped_ptr<google_apis::ResourceEntry> sync_root_entry
      = google_apis::ResourceEntry::ExtractAndParse(*sync_root_entry_value);
  EXPECT_CALL(*mock_drive_service(),
              GetResourceEntry(kSyncRootResourceId, _))
      .WillOnce(InvokeGetResourceEntryCallback1(
          google_apis::HTTP_SUCCESS,
          base::Passed(&sync_root_entry)));

  scoped_ptr<Value> origin_directory_created_value(LoadJSONFile(
      "sync_file_system/origin_directory_created.json"));
  scoped_ptr<google_apis::ResourceEntry> origin_directory_created
      = google_apis::ResourceEntry::ExtractAndParse(
          *origin_directory_created_value);
  FilePath::StringType dirname = FilePath().AppendASCII(
      DriveFileSyncClient::OriginToDirectoryTitle(kOrigin)).value();
  EXPECT_CALL(*mock_drive_service(),
              AddNewDirectory(kSyncRootContentURL, dirname, _))
      .WillOnce(InvokeGetResourceEntryCallback2(
          google_apis::HTTP_SUCCESS,
          base::Passed(&origin_directory_created)));

  // Once the directory is created GetAccountMetadata should be called to get
  // the largest changestamp for the origin as a prepariation of the batch sync.
  scoped_ptr<Value> account_metadata_value(LoadJSONFile(
      "gdata/account_metadata.json"));
  scoped_ptr<google_apis::AccountMetadataFeed> account_metadata(
      google_apis::AccountMetadataFeed::CreateFrom(*account_metadata_value));
  EXPECT_CALL(*mock_drive_service(), GetAccountMetadata(_))
      .WillOnce(InvokeGetAccountMetadataCallback0(
          google_apis::HTTP_SUCCESS,
          base::Passed(&account_metadata)));

  scoped_ptr<Value> listing_files_in_empty_directory_value(LoadJSONFile(
      "sync_file_system/listing_files_in_empty_directory.json"));
  scoped_ptr<google_apis::ResourceList> listing_files_in_empty_directory(
      google_apis::ResourceList::ExtractAndParse(
          *listing_files_in_empty_directory_value));

  EXPECT_CALL(*mock_drive_service(),
              GetResourceList(GURL(), 0, std::string(), false,
                              kDirectoryResourceId, _))
      .WillOnce(InvokeGetResourceListCallback5(
          google_apis::HTTP_SUCCESS,
          base::Passed(&listing_files_in_empty_directory)));

  SetUpDriveSyncService();
  bool done = false;
  sync_service()->RegisterOriginForTrackingChanges(
      kOrigin, base::Bind(&ExpectEqStatus, &done, fileapi::SYNC_STATUS_OK));
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(done);

  EXPECT_TRUE(metadata_store()->batch_sync_origins().empty());
  EXPECT_EQ(1u, metadata_store()->incremental_sync_origins().size());
  EXPECT_TRUE(pending_changes().empty());
}

TEST_F(DriveFileSyncServiceTest, RegisterExistingOrigin) {
  const GURL kOrigin("chrome-extension://example");
  const std::string kDirectoryResourceId("folder:origin_directory_resource_id");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");

  metadata_store()->SetSyncRootDirectory(kSyncRootResourceId);

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(AtLeast(1));
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(_))
      .Times(AnyNumber());

  InSequence sequence;

  scoped_ptr<Value> origin_directory_found_value(LoadJSONFile(
      "sync_file_system/origin_directory_found.json"));
  scoped_ptr<google_apis::ResourceList> origin_directory_found(
      google_apis::ResourceList::ExtractAndParse(
          *origin_directory_found_value));

  std::string query = FormatTitleQuery(
      DriveFileSyncClient::OriginToDirectoryTitle(kOrigin));
  EXPECT_CALL(*mock_drive_service(),
              GetResourceList(GURL(), 0, query, false, kSyncRootResourceId, _))
      .WillOnce(InvokeGetResourceListCallback5(
          google_apis::HTTP_SUCCESS,
          base::Passed(&origin_directory_found)))
      .RetiresOnSaturation();

  scoped_ptr<Value> account_metadata_value(LoadJSONFile(
      "gdata/account_metadata.json"));
  scoped_ptr<google_apis::AccountMetadataFeed> account_metadata(
      google_apis::AccountMetadataFeed::CreateFrom(*account_metadata_value));
  EXPECT_CALL(*mock_drive_service(), GetAccountMetadata(_))
      .WillOnce(InvokeGetAccountMetadataCallback0(
          google_apis::HTTP_SUCCESS,
          base::Passed(&account_metadata)));

  // DriveFileSyncService should fetch the list of the directory content
  // to start the batch sync.
  scoped_ptr<Value> listing_files_in_directory_value(LoadJSONFile(
      "sync_file_system/listing_files_in_directory.json"));
  scoped_ptr<google_apis::ResourceList> listing_files_in_directory(
      google_apis::ResourceList::ExtractAndParse(
          *listing_files_in_directory_value));
  EXPECT_CALL(*mock_drive_service(),
              GetResourceList(GURL(), 0, std::string(),
                              false, kDirectoryResourceId, _))
      .WillOnce(InvokeGetResourceListCallback5(
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
  EXPECT_EQ(3u, pending_changes().size());
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
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(_))
      .Times(AnyNumber());

  InSequence sequence;

  scoped_ptr<Value> account_metadata_value(LoadJSONFile(
      "gdata/account_metadata.json"));
  scoped_ptr<google_apis::AccountMetadataFeed> account_metadata(
      google_apis::AccountMetadataFeed::CreateFrom(*account_metadata_value));

  EXPECT_CALL(*mock_drive_service(),
              GetAccountMetadata(_))
      .WillOnce(InvokeGetAccountMetadataCallback0(
          google_apis::HTTP_SUCCESS,
          base::Passed(&account_metadata)));

  scoped_ptr<Value> value(LoadJSONFile(
      "sync_file_system/listing_files_in_directory.json"));
  scoped_ptr<google_apis::ResourceList> listing_files_in_directory(
      google_apis::ResourceList::ExtractAndParse(*value));

  EXPECT_CALL(*mock_drive_service(),
              GetResourceList(GURL(), 0, std::string(), false,
                              kDirectoryResourceId1, _))
      .WillOnce(InvokeGetResourceListCallback5(
          google_apis::HTTP_SUCCESS,
          base::Passed(&listing_files_in_directory)));

  SetUpDriveSyncService();
  message_loop()->RunUntilIdle();

  EXPECT_EQ(1u, metadata_store()->batch_sync_origins().size());
  EXPECT_EQ(1u, metadata_store()->incremental_sync_origins().size());
  EXPECT_EQ(3u, pending_changes().size());

  bool done = false;
  sync_service()->UnregisterOriginForTrackingChanges(
      kOrigin1, base::Bind(&ExpectEqStatus, &done, fileapi::SYNC_STATUS_OK));
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(done);

  EXPECT_TRUE(metadata_store()->batch_sync_origins().empty());
  EXPECT_EQ(1u, metadata_store()->incremental_sync_origins().size());
  EXPECT_TRUE(pending_changes().empty());
}

TEST_F(DriveFileSyncServiceTest, ResolveLocalSyncOperationType) {
  const fileapi::FileSystemURL url = fileapi::CreateSyncableFileSystemURL(
      GURL("chrome-extension://example/"),
      kServiceName,
      FilePath().AppendASCII("path/to/file"));
  const std::string kResourceId("123456");
  const int64 kChangestamp = 654321;

  scoped_ptr<Value> value(LoadJSONFile(
      "sync_file_system/sync_root_found.json"));
  scoped_ptr<google_apis::ResourceList> sync_root_found(
      google_apis::ResourceList::ExtractAndParse(*value));

  std::string query = FormatTitleQuery(kSyncRootDirectoryName);
  EXPECT_CALL(*mock_drive_service(),
              GetResourceList(GURL(), 0, query, false, std::string(), _))
      .WillOnce(InvokeGetResourceListCallback5(
          google_apis::HTTP_SUCCESS,
          base::Passed(&sync_root_found)));

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(1);
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(_))
      .Times(AnyNumber());

  SetUpDriveSyncService();
  message_loop()->RunUntilIdle();

  const fileapi::FileChange local_add_or_update_change(
      fileapi::FileChange::FILE_CHANGE_ADD_OR_UPDATE,
      fileapi::SYNC_FILE_TYPE_FILE);
  const fileapi::FileChange local_delete_change(
      fileapi::FileChange::FILE_CHANGE_DELETE,
      fileapi::SYNC_FILE_TYPE_FILE);

  // There is no pending remote change and no metadata in DriveMetadataStore.
  EXPECT_TRUE(IsLocalSyncOperationAdd(
      ResolveLocalSyncOperationType(local_add_or_update_change, url)));
  EXPECT_TRUE(IsLocalSyncOperationNone(
      ResolveLocalSyncOperationType(local_delete_change, url)));

  // Add metadata for the file identified by |url|.
  DriveMetadata metadata;
  metadata.set_resource_id(kResourceId);
  metadata.set_md5_checksum("654321");
  metadata.set_conflicted(false);
  metadata.set_to_be_fetched(false);
  metadata_store()->UpdateEntry(url, metadata,
                                base::Bind(&DidEntryOperation));

  message_loop()->RunUntilIdle();

  // There is no pending remote change, but metadata in DriveMetadataStore.
  EXPECT_TRUE(IsLocalSyncOperationUpdate(
      ResolveLocalSyncOperationType(local_add_or_update_change, url)));
  EXPECT_TRUE(IsLocalSyncOperationDelete(
      ResolveLocalSyncOperationType(local_delete_change, url)));

  // Add an ADD_OR_UPDATE change for the file to the pending change queue.
  AddRemoteChange(
      kChangestamp, kResourceId, url,
      fileapi::FileChange(fileapi::FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                          fileapi::SYNC_FILE_TYPE_FILE));

  EXPECT_TRUE(IsLocalSyncOperationConflict(
      ResolveLocalSyncOperationType(local_add_or_update_change, url)));
  EXPECT_TRUE(IsLocalSyncOperationNone(
      ResolveLocalSyncOperationType(local_delete_change, url)));

  // Add a DELETE change for the file to the pending change queue.
  AddRemoteChange(
      kChangestamp, kResourceId, url,
      fileapi::FileChange(fileapi::FileChange::FILE_CHANGE_DELETE,
                          fileapi::SYNC_FILE_TYPE_FILE));

  EXPECT_TRUE(IsLocalSyncOperationAdd(
      ResolveLocalSyncOperationType(local_add_or_update_change, url)));
  EXPECT_TRUE(IsLocalSyncOperationNone(
      ResolveLocalSyncOperationType(local_delete_change, url)));

  // Mark the file as conflicted so that the conflict resolution will occur.
  metadata.set_conflicted(true);
  metadata_store()->UpdateEntry(url, metadata,
                                base::Bind(&DidEntryOperation));

  EXPECT_TRUE(IsLocalSyncOperationNone(
      ResolveLocalSyncOperationType(local_add_or_update_change, url)));
  EXPECT_TRUE(IsLocalSyncOperationResolveToRemote(
      ResolveLocalSyncOperationType(local_delete_change, url)));
}

TEST_F(DriveFileSyncServiceTest, RemoteChange_NoChange) {
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");

  metadata_store()->SetSyncRootDirectory(kSyncRootResourceId);

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(_))
      .Times(AnyNumber());

  SetUpDriveSyncService();

  ProcessRemoteChange(fileapi::SYNC_STATUS_NO_CHANGE_TO_SYNC,
                      fileapi::FileSystemURL(),
                      fileapi::SYNC_OPERATION_NONE);
  EXPECT_TRUE(metadata_store()->batch_sync_origins().empty());
  EXPECT_TRUE(metadata_store()->incremental_sync_origins().empty());
  EXPECT_TRUE(pending_changes().empty());
}

TEST_F(DriveFileSyncServiceTest, RemoteChange_Busy) {
  const GURL kOrigin("chrome-extension://example");
  const std::string kDirectoryResourceId("folder:origin_directory_resource_id");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");
  const FilePath::StringType kFileName(FPL("File 1.mp3"));

  metadata_store()->SetSyncRootDirectory(kSyncRootResourceId);
  metadata_store()->AddBatchSyncOrigin(kOrigin, kDirectoryResourceId);
  metadata_store()->MoveBatchSyncOriginToIncremental(kOrigin);

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(_))
      .Times(AnyNumber());

  EXPECT_CALL(*mock_remote_processor(),
              PrepareForProcessRemoteChange(CreateURL(kOrigin, kFileName),
                                            kServiceName, _))
      .WillOnce(PrepareForRemoteChange_Busy());
  EXPECT_CALL(*mock_remote_processor(),
              ClearLocalChanges(CreateURL(kOrigin, kFileName), _))
      .WillOnce(InvokeCompletionCallback());

  SetUpDriveSyncService();

  scoped_ptr<ResourceEntry> entry(ResourceEntry::ExtractAndParse(
      *LoadJSONFile("gdata/file_entry.json")));
  AppendIncrementalRemoteChange(kOrigin, *entry, 12345);

  ProcessRemoteChange(fileapi::SYNC_STATUS_FILE_BUSY,
                      CreateURL(kOrigin, kFileName),
                      fileapi::SYNC_OPERATION_NONE);
}

TEST_F(DriveFileSyncServiceTest, RemoteChange_NewFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string kDirectoryResourceId("folder:origin_directory_resource_id");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");
  const FilePath::StringType kFileName(FPL("File 1.mp3"));
  const std::string kFileResourceId("file:2_file_resource_id");

  metadata_store()->SetSyncRootDirectory(kSyncRootResourceId);
  metadata_store()->AddBatchSyncOrigin(kOrigin, kDirectoryResourceId);
  metadata_store()->MoveBatchSyncOriginToIncremental(kOrigin);

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(_))
      .Times(AnyNumber());

  EXPECT_CALL(*mock_remote_processor(),
              PrepareForProcessRemoteChange(CreateURL(kOrigin, kFileName),
                                            kServiceName, _))
      .WillOnce(PrepareForRemoteChange_NotFound());
  EXPECT_CALL(*mock_remote_processor(),
              ClearLocalChanges(CreateURL(kOrigin, kFileName), _))
      .WillOnce(InvokeCompletionCallback());

  scoped_ptr<Value> file_entry_value(
      LoadJSONFile("gdata/file_entry.json").Pass());
  scoped_ptr<google_apis::ResourceEntry> file_entry
      = google_apis::ResourceEntry::ExtractAndParse(*file_entry_value);
  EXPECT_CALL(*mock_drive_service(),
              GetResourceEntry(kFileResourceId, _))
      .WillOnce(InvokeGetResourceEntryCallback1(
          google_apis::HTTP_SUCCESS,
          base::Passed(&file_entry)));

  EXPECT_CALL(*mock_drive_service(),
              DownloadFile(_, _, GURL("https://file_content_url"), _, _))
      .WillOnce(InvokeDidDownloadFile());

  EXPECT_CALL(*mock_remote_processor(),
              ApplyRemoteChange(_, _, CreateURL(kOrigin, kFileName), _))
      .WillOnce(InvokeDidApplyRemoteChange());

  SetUpDriveSyncService();

  scoped_ptr<ResourceEntry> entry(ResourceEntry::ExtractAndParse(
      *LoadJSONFile("gdata/file_entry.json")));
  AppendIncrementalRemoteChange(kOrigin, *entry, 12345);

  ProcessRemoteChange(fileapi::SYNC_STATUS_OK,
                      CreateURL(kOrigin, kFileName),
                      fileapi::SYNC_OPERATION_ADDED);
}

TEST_F(DriveFileSyncServiceTest, RemoteChange_UpdateFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string kDirectoryResourceId("folder:origin_directory_resource_id");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");
  const FilePath::StringType kFileName(FPL("File 1.mp3"));
  const std::string kFileResourceId("file:2_file_resource_id");

  metadata_store()->SetSyncRootDirectory(kSyncRootResourceId);
  metadata_store()->AddBatchSyncOrigin(kOrigin, kDirectoryResourceId);
  metadata_store()->MoveBatchSyncOriginToIncremental(kOrigin);

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(_))
      .Times(AnyNumber());

  EXPECT_CALL(*mock_remote_processor(),
              PrepareForProcessRemoteChange(CreateURL(kOrigin, kFileName),
                                            kServiceName, _))
      .WillOnce(PrepareForRemoteChange_NotModified());
  EXPECT_CALL(*mock_remote_processor(),
              ClearLocalChanges(CreateURL(kOrigin, kFileName), _))
      .WillOnce(InvokeCompletionCallback());

  scoped_ptr<Value> file_entry_value(
      LoadJSONFile("gdata/file_entry.json").Pass());
  scoped_ptr<google_apis::ResourceEntry> file_entry
      = google_apis::ResourceEntry::ExtractAndParse(*file_entry_value);
  EXPECT_CALL(*mock_drive_service(),
              GetResourceEntry(kFileResourceId, _))
      .WillOnce(InvokeGetResourceEntryCallback1(
          google_apis::HTTP_SUCCESS,
          base::Passed(&file_entry)));

  EXPECT_CALL(*mock_drive_service(),
              DownloadFile(_, _, GURL("https://file_content_url"), _, _))
      .WillOnce(InvokeDidDownloadFile());

  EXPECT_CALL(*mock_remote_processor(),
              ApplyRemoteChange(_, _, CreateURL(kOrigin, kFileName), _))
      .WillOnce(InvokeDidApplyRemoteChange());

  SetUpDriveSyncService();

  scoped_ptr<ResourceEntry> entry(ResourceEntry::ExtractAndParse(
      *LoadJSONFile("gdata/file_entry.json")));
  AppendIncrementalRemoteChange(kOrigin, *entry, 12345);
  ProcessRemoteChange(fileapi::SYNC_STATUS_OK,
                      CreateURL(kOrigin, kFileName),
                      fileapi::SYNC_OPERATION_UPDATED);
}

#endif  // !defined(OS_ANDROID)

}  // namespace sync_file_system
