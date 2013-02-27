// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_service.h"

#include <utility>

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/google_apis/drive_uploader.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/mock_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/browser/sync_file_system/drive_file_sync_client.h"
#include "chrome/browser/sync_file_system/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/file_status_observer.h"
#include "chrome/browser/sync_file_system/mock_remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/escape.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/syncable/sync_direction.h"
#include "webkit/fileapi/syncable/sync_file_metadata.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

#define FPL(x) FILE_PATH_LITERAL(x)

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Sequence;
using ::testing::StrictMock;
using ::testing::_;

using google_apis::ResourceEntry;
using google_apis::DriveServiceInterface;
using google_apis::DriveUploaderInterface;
using google_apis::test_util::LoadJSONFile;

using extensions::Extension;
using extensions::DictionaryBuilder;
using extensions::ListBuilder;

namespace sync_file_system {

namespace {

const char kSyncRootDirectoryName[] = "Chrome Syncable FileSystem";
const char* kServiceName = DriveFileSyncService::kServiceName;

base::FilePath::StringType ASCIIToFilePathString(const std::string& path) {
  return base::FilePath().AppendASCII(path).value();
}

void DidInitialize(bool* done, SyncStatusCode status, bool created) {
  EXPECT_FALSE(*done);
  *done = true;
  EXPECT_EQ(SYNC_STATUS_OK, status);
  EXPECT_TRUE(created);
}

void DidUpdateEntry(SyncStatusCode status) {
  EXPECT_EQ(SYNC_STATUS_OK, status);
}

void ExpectEqStatus(bool* done,
                    SyncStatusCode expected,
                    SyncStatusCode actual) {
  EXPECT_FALSE(*done);
  *done = true;
  EXPECT_EQ(expected, actual);
}

// Mocks adding an installed extension to ExtensionService.
scoped_refptr<const extensions::Extension> AddTestExtension(
    ExtensionService* extension_service,
    const base::FilePath::StringType& extension_name) {
  std::string id = Extension::GenerateIdForPath(base::FilePath(extension_name));

  scoped_refptr<const Extension> extension =
      extensions::ExtensionBuilder().SetManifest(
          DictionaryBuilder()
            .Set("name", extension_name)
            .Set("version", "1.0"))
          .SetID(id)
      .Build();
  extension_service->AddExtension(extension);
  return extension;
}

// Converts extension_name to GURL version.
GURL ExtensionNameToGURL(const base::FilePath::StringType& extension_name) {
  std::string id = Extension::GenerateIdForPath(base::FilePath(extension_name));
  return extensions::Extension::GetBaseURLFromExtensionId(id);
}

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
                 SYNC_STATUS_FILE_BUSY,
                 SyncFileMetadata(),
                 FileChangeList()));
}

ACTION(PrepareForRemoteChange_NotFound) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg2,
                 SYNC_STATUS_OK,
                 SyncFileMetadata(SYNC_FILE_TYPE_UNKNOWN, 0, base::Time()),
                 FileChangeList()));
}

ACTION(PrepareForRemoteChange_NotModified) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg2,
                 SYNC_STATUS_OK,
                 SyncFileMetadata(SYNC_FILE_TYPE_FILE, 0, base::Time()),
                 FileChangeList()));
}

ACTION(InvokeDidDownloadFile) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(arg3, google_apis::HTTP_SUCCESS, arg1));
}

ACTION(InvokeDidApplyRemoteChange) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(arg3, SYNC_STATUS_OK));
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

class MockFileStatusObserver: public FileStatusObserver {
 public:
  MockFileStatusObserver() {}
  virtual ~MockFileStatusObserver() {}

  MOCK_METHOD4(OnFileStatusChanged,
               void(const fileapi::FileSystemURL& url,
                    SyncFileStatus sync_status,
                    SyncAction action_taken,
                    SyncDirection direction));
};

class DriveFileSyncServiceTest : public testing::Test {
 public:
  DriveFileSyncServiceTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        mock_drive_service_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile());

    // Add TestExtensionSystem with registered ExtensionIds used in tests.
    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_.get())));
    extension_system->CreateExtensionService(
        CommandLine::ForCurrentProcess(), base::FilePath(), false);
    ExtensionService* extension_service = extension_system->Get(
        profile_.get())->extension_service();
    AddTestExtension(extension_service, FPL("example1"));
    AddTestExtension(extension_service, FPL("example2"));

    ASSERT_TRUE(RegisterSyncableFileSystem(kServiceName));

    mock_drive_service_ = new StrictMock<google_apis::MockDriveService>;

    EXPECT_CALL(*mock_drive_service(), Initialize(profile_.get()));
    EXPECT_CALL(*mock_drive_service(), AddObserver(_));

    sync_client_ = DriveFileSyncClient::CreateForTesting(
        profile_.get(),
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

  void SetUpDriveSyncService(bool enabled) {
    sync_service_ = DriveFileSyncService::CreateForTesting(
        profile_.get(), base_dir_.path(),
        sync_client_.PassAs<DriveFileSyncClientInterface>(),
        metadata_store_.Pass()).Pass();
    sync_service_->SetSyncEnabled(enabled);
    sync_service_->AddServiceObserver(&mock_remote_observer_);
    sync_service_->AddFileStatusObserver(&mock_file_status_observer_);
    message_loop_.RunUntilIdle();
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

    EXPECT_TRUE(RevokeSyncableFileSystem(kServiceName));

    profile_.reset();
    message_loop_.RunUntilIdle();
  }

  void SetSyncEnabled(bool enabled) {
    sync_service_->SetSyncEnabled(enabled);
  }

 protected:
  DriveFileSyncService::LocalSyncOperationType ResolveLocalSyncOperationType(
      const FileChange& local_change,
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
                       const std::string& md5_checksum,
                       const fileapi::FileSystemURL& url,
                       const FileChange& file_change) {
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
        changestamp, resource_id, md5_checksum,
        sync_type, url, file_change,
        inserted_to_queue.first);
  }

  DriveFileSyncClientInterface* sync_client() {
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

  StrictMock<MockFileStatusObserver>* mock_file_status_observer() {
    return &mock_file_status_observer_;
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
                                   const base::FilePath::StringType& path) {
    return CreateSyncableFileSystemURL(
        origin, kServiceName, base::FilePath(path));
  }

  void ProcessRemoteChange(SyncStatusCode expected_status,
                           const fileapi::FileSystemURL& expected_url,
                           SyncFileStatus expected_sync_file_status,
                           SyncAction expected_sync_action,
                           SyncDirection expected_sync_direction) {
    SyncStatusCode actual_status = SYNC_STATUS_UNKNOWN;
    fileapi::FileSystemURL actual_url;

    if (expected_sync_file_status != SYNC_FILE_STATUS_UNKNOWN) {
      EXPECT_CALL(*mock_file_status_observer(),
                  OnFileStatusChanged(expected_url,
                                      expected_sync_file_status,
                                      expected_sync_action,
                                      expected_sync_direction))
          .Times(1);
    }

    sync_service_->ProcessRemoteChange(
        mock_remote_processor(),
        base::Bind(&DriveFileSyncServiceTest::DidProcessRemoteChange,
                   base::Unretained(this),
                   &actual_status, &actual_url));
    message_loop_.RunUntilIdle();

    EXPECT_EQ(expected_status, actual_status);
    EXPECT_EQ(expected_url, actual_url);
  }

  void DidProcessRemoteChange(SyncStatusCode* status_out,
                              fileapi::FileSystemURL* url_out,
                              SyncStatusCode status,
                              const fileapi::FileSystemURL& url) {
    *status_out = status;
    *url_out = url;
  }

  bool AppendIncrementalRemoteChangeByEntry(
      const GURL& origin,
      const google_apis::ResourceEntry& entry,
      int64 changestamp) {
    return sync_service_->AppendRemoteChange(
        origin, entry, changestamp,
        DriveFileSyncService::REMOTE_SYNC_TYPE_INCREMENTAL);
  }

  bool AppendIncrementalRemoteChange(
      const GURL& origin,
      const base::FilePath& path,
      bool is_deleted,
      const std::string& resource_id,
      int64 changestamp,
      const std::string& remote_file_md5) {
    return sync_service_->AppendRemoteChangeInternal(
        origin, path, is_deleted, resource_id,
        changestamp, remote_file_md5,
        DriveFileSyncService::REMOTE_SYNC_TYPE_INCREMENTAL);
  }

  // Mock setup helpers ------------------------------------------------------
  void SetUpDriveServiceExpectCallsForGetResourceList(
      const std::string& result_mock_json_name,
      const std::string& query,
      const std::string& search_directory) {
    scoped_ptr<Value> result_value(LoadJSONFile(
            result_mock_json_name));
    scoped_ptr<google_apis::ResourceList> result(
        google_apis::ResourceList::ExtractAndParse(*result_value));
    EXPECT_CALL(*mock_drive_service(),
                GetResourceList(GURL(), 0, query, false, search_directory, _))
        .WillOnce(InvokeGetResourceListCallback5(
                google_apis::HTTP_SUCCESS,
                base::Passed(&result)))
        .RetiresOnSaturation();
  }

  void SetUpDriveServiceExpectCallsForGetSyncRoot() {
    SetUpDriveServiceExpectCallsForGetResourceList(
        "sync_file_system/sync_root_found.json",
        FormatTitleQuery(kSyncRootDirectoryName),
        std::string());
  }

  void SetUpDriveServiceExpectCallsForGetAccountMetadata() {
    scoped_ptr<Value> account_metadata_value(LoadJSONFile(
        "gdata/account_metadata.json"));
    scoped_ptr<google_apis::AccountMetadataFeed> account_metadata(
        google_apis::AccountMetadataFeed::CreateFrom(*account_metadata_value));
    EXPECT_CALL(*mock_drive_service(),
                GetAccountMetadata(_))
        .WillOnce(InvokeGetAccountMetadataCallback0(
            google_apis::HTTP_SUCCESS,
            base::Passed(&account_metadata)))
        .RetiresOnSaturation();
  }

  void SetUpDriveServiceExpectCallsForDownloadFile(
      const std::string& file_resource_id) {
    scoped_ptr<Value> file_entry_value(
        LoadJSONFile("gdata/file_entry.json").Pass());
    scoped_ptr<google_apis::ResourceEntry> file_entry
        = google_apis::ResourceEntry::ExtractAndParse(*file_entry_value);
    EXPECT_CALL(*mock_drive_service(),
                GetResourceEntry(file_resource_id, _))
        .WillOnce(InvokeGetResourceEntryCallback1(
            google_apis::HTTP_SUCCESS,
            base::Passed(&file_entry)))
        .RetiresOnSaturation();

    EXPECT_CALL(*mock_drive_service(),
                DownloadFile(_, _, GURL("https://file_content_url"), _, _))
        .WillOnce(InvokeDidDownloadFile())
        .RetiresOnSaturation();
  }

  void SetUpDriveServiceExpectCallsForAddNewDirectory(
      const std::string& parent_directory,
      const std::string& directory_name) {
    scoped_ptr<Value> origin_directory_created_value(LoadJSONFile(
        "sync_file_system/origin_directory_created.json"));
    scoped_ptr<google_apis::ResourceEntry> origin_directory_created
        = google_apis::ResourceEntry::ExtractAndParse(
            *origin_directory_created_value);
    EXPECT_CALL(*mock_drive_service(),
                AddNewDirectory(parent_directory, directory_name, _))
        .WillOnce(InvokeGetResourceEntryCallback2(
            google_apis::HTTP_SUCCESS,
            base::Passed(&origin_directory_created)))
        .RetiresOnSaturation();
  }

  // End of mock setup helpers -----------------------------------------------

 private:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  base::ScopedTempDir base_dir_;
  scoped_ptr<TestingProfile> profile_;

  scoped_ptr<DriveFileSyncService> sync_service_;

  // Owned by |sync_client_|.
  StrictMock<google_apis::MockDriveService>* mock_drive_service_;

  StrictMock<MockRemoteServiceObserver> mock_remote_observer_;
  StrictMock<MockFileStatusObserver> mock_file_status_observer_;
  StrictMock<MockRemoteChangeProcessor> mock_remote_processor_;

  scoped_ptr<DriveFileSyncClient> sync_client_;
  scoped_ptr<DriveMetadataStore> metadata_store_;

  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncServiceTest);
};

#if !defined(OS_ANDROID)

TEST_F(DriveFileSyncServiceTest, GetSyncRoot) {
  SetUpDriveServiceExpectCallsForGetSyncRoot();

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(1);
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(0))
      .Times(AnyNumber());

  SetUpDriveSyncService(true);
  message_loop()->RunUntilIdle();

  EXPECT_EQ("folder:sync_root_resource_id",
            metadata_store()->sync_root_directory());

  EXPECT_EQ(0u, metadata_store()->batch_sync_origins().size());
  EXPECT_EQ(0u, metadata_store()->incremental_sync_origins().size());
  EXPECT_EQ(0u, pending_changes().size());
}

TEST_F(DriveFileSyncServiceTest, BatchSyncOnInitialization) {
  const GURL kOrigin1 = ExtensionNameToGURL(FPL("example1"));
  const GURL kOrigin2 = ExtensionNameToGURL(FPL("example2"));
  const std::string kDirectoryResourceId1(
      "folder:origin_directory_resource_id");
  const std::string kDirectoryResourceId2(
      "folder:origin_directory_resource_id2");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");

  metadata_store()->SetSyncRootDirectory(kSyncRootResourceId);
  metadata_store()->AddBatchSyncOrigin(kOrigin1, kDirectoryResourceId1);
  metadata_store()->AddBatchSyncOrigin(kOrigin2, kDirectoryResourceId2);
  metadata_store()->MoveBatchSyncOriginToIncremental(kOrigin2);

  Sequence change_queue_seq;
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(0))
      .InSequence(change_queue_seq);
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(3))
      .InSequence(change_queue_seq);

  InSequence sequence;

  SetUpDriveServiceExpectCallsForGetAccountMetadata();
  SetUpDriveServiceExpectCallsForGetResourceList(
      "sync_file_system/listing_files_in_directory.json",
      std::string(),
      kDirectoryResourceId1);

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(1);

  SetUpDriveSyncService(true);
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
  // The root id is in the "sync_root_entry.json" file.
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");

  metadata_store()->SetSyncRootDirectory(kSyncRootResourceId);

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(AtLeast(1));
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(0))
      .Times(AnyNumber());

  SetUpDriveServiceExpectCallsForGetResourceList(
      "sync_file_system/origin_directory_found.json",
      FormatTitleQuery(DriveFileSyncClient::OriginToDirectoryTitle(kOrigin)),
      kSyncRootResourceId);
  SetUpDriveServiceExpectCallsForGetResourceList(
      "sync_file_system/origin_directory_not_found.json",
      FormatTitleQuery(DriveFileSyncClient::OriginToDirectoryTitle(kOrigin)),
      kSyncRootResourceId);

  // If the directory for the origin is missing, DriveFileSyncService should
  // attempt to create it.
  SetUpDriveServiceExpectCallsForAddNewDirectory(
      kSyncRootResourceId,
      DriveFileSyncClient::OriginToDirectoryTitle(kOrigin));

  // Once the directory is created GetAccountMetadata should be called to get
  // the largest changestamp for the origin as a prepariation of the batch sync.
  SetUpDriveServiceExpectCallsForGetAccountMetadata();

  SetUpDriveServiceExpectCallsForGetResourceList(
      "sync_file_system/listing_files_in_empty_directory.json",
      std::string(),
      kDirectoryResourceId);

  SetUpDriveSyncService(true);
  bool done = false;
  sync_service()->RegisterOriginForTrackingChanges(
      kOrigin, base::Bind(&ExpectEqStatus, &done, SYNC_STATUS_OK));
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

  // We already have a directory for the origin.
  SetUpDriveServiceExpectCallsForGetResourceList(
      "sync_file_system/origin_directory_found.json",
      FormatTitleQuery(DriveFileSyncClient::OriginToDirectoryTitle(kOrigin)),
      kSyncRootResourceId);

  SetUpDriveServiceExpectCallsForGetAccountMetadata();

  // DriveFileSyncService should fetch the list of the directory content
  // to start the batch sync.
  SetUpDriveServiceExpectCallsForGetResourceList(
      "sync_file_system/listing_files_in_directory.json",
      std::string(),
      kDirectoryResourceId);

  SetUpDriveSyncService(true);
  bool done = false;
  sync_service()->RegisterOriginForTrackingChanges(
      kOrigin, base::Bind(&ExpectEqStatus, &done, SYNC_STATUS_OK));
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(done);

  // The origin should be registered as a batch sync origin.
  EXPECT_EQ(1u, metadata_store()->batch_sync_origins().size());
  EXPECT_TRUE(metadata_store()->incremental_sync_origins().empty());

  // |listing_files_in_directory| contains 4 items to sync.
  EXPECT_EQ(3u, pending_changes().size());
}

TEST_F(DriveFileSyncServiceTest, UnregisterOrigin) {
  const GURL kOrigin1 = ExtensionNameToGURL(FPL("example1"));
  const GURL kOrigin2 = ExtensionNameToGURL(FPL("example2"));
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

  SetUpDriveServiceExpectCallsForGetAccountMetadata();
  SetUpDriveServiceExpectCallsForGetResourceList(
      "sync_file_system/listing_files_in_directory.json",
      std::string(),
      kDirectoryResourceId1);

  SetUpDriveSyncService(true);
  message_loop()->RunUntilIdle();

  EXPECT_EQ(1u, metadata_store()->batch_sync_origins().size());
  EXPECT_EQ(1u, metadata_store()->incremental_sync_origins().size());
  EXPECT_EQ(3u, pending_changes().size());

  bool done = false;
  sync_service()->UnregisterOriginForTrackingChanges(
      kOrigin1, base::Bind(&ExpectEqStatus, &done, SYNC_STATUS_OK));
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(done);

  EXPECT_TRUE(metadata_store()->batch_sync_origins().empty());
  EXPECT_EQ(1u, metadata_store()->incremental_sync_origins().size());
  EXPECT_TRUE(pending_changes().empty());
}

TEST_F(DriveFileSyncServiceTest, ResolveLocalSyncOperationType) {
  const fileapi::FileSystemURL url = CreateSyncableFileSystemURL(
      GURL("chrome-extension://example/"),
      kServiceName,
      base::FilePath().AppendASCII("path/to/file"));
  const std::string kResourceId("123456");
  const int64 kChangestamp = 654321;

  SetUpDriveServiceExpectCallsForGetSyncRoot();

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(1);
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(_))
      .Times(AnyNumber());

  SetUpDriveSyncService(true);
  message_loop()->RunUntilIdle();

  const FileChange local_add_or_update_change(
      FileChange::FILE_CHANGE_ADD_OR_UPDATE, SYNC_FILE_TYPE_FILE);
  const FileChange local_delete_change(
      FileChange::FILE_CHANGE_DELETE, SYNC_FILE_TYPE_FILE);

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
                                base::Bind(&DidUpdateEntry));

  message_loop()->RunUntilIdle();

  // There is no pending remote change, but metadata in DriveMetadataStore.
  EXPECT_TRUE(IsLocalSyncOperationUpdate(
      ResolveLocalSyncOperationType(local_add_or_update_change, url)));
  EXPECT_TRUE(IsLocalSyncOperationDelete(
      ResolveLocalSyncOperationType(local_delete_change, url)));

  // Add an ADD_OR_UPDATE change for the file to the pending change queue.
  AddRemoteChange(
      kChangestamp, kResourceId, "hoge", url,
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE, SYNC_FILE_TYPE_FILE));

  EXPECT_TRUE(IsLocalSyncOperationConflict(
      ResolveLocalSyncOperationType(local_add_or_update_change, url)));
  EXPECT_TRUE(IsLocalSyncOperationNone(
      ResolveLocalSyncOperationType(local_delete_change, url)));

  // Add a DELETE change for the file to the pending change queue.
  AddRemoteChange(
      kChangestamp, kResourceId, "fuga", url,
      FileChange(FileChange::FILE_CHANGE_DELETE, SYNC_FILE_TYPE_FILE));

  EXPECT_TRUE(IsLocalSyncOperationAdd(
      ResolveLocalSyncOperationType(local_add_or_update_change, url)));
  EXPECT_TRUE(IsLocalSyncOperationNone(
      ResolveLocalSyncOperationType(local_delete_change, url)));

  // Mark the file as conflicted so that the conflict resolution will occur.
  metadata.set_conflicted(true);
  metadata_store()->UpdateEntry(url, metadata,
                                base::Bind(&DidUpdateEntry));

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

  SetUpDriveSyncService(true);

  ProcessRemoteChange(SYNC_STATUS_NO_CHANGE_TO_SYNC,
                      fileapi::FileSystemURL(),
                      SYNC_FILE_STATUS_UNKNOWN,
                      SYNC_ACTION_NONE,
                      SYNC_DIRECTION_NONE);
  EXPECT_TRUE(metadata_store()->batch_sync_origins().empty());
  EXPECT_TRUE(metadata_store()->incremental_sync_origins().empty());
  EXPECT_TRUE(pending_changes().empty());
}

TEST_F(DriveFileSyncServiceTest, RemoteChange_Busy) {
  const GURL kOrigin("chrome-extension://example");
  const std::string kDirectoryResourceId("folder:origin_directory_resource_id");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");
  const base::FilePath::StringType kFileName(FPL("File 1.mp3"));

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

  SetUpDriveSyncService(true);

  scoped_ptr<ResourceEntry> entry(ResourceEntry::ExtractAndParse(
      *LoadJSONFile("gdata/file_entry.json")));
  AppendIncrementalRemoteChangeByEntry(kOrigin, *entry, 12345);

  ProcessRemoteChange(SYNC_STATUS_FILE_BUSY,
                      CreateURL(kOrigin, kFileName),
                      SYNC_FILE_STATUS_UNKNOWN,
                      SYNC_ACTION_NONE,
                      SYNC_DIRECTION_NONE);
}

TEST_F(DriveFileSyncServiceTest, RemoteChange_NewFile) {
  const GURL kOrigin = ExtensionNameToGURL(FPL("example1"));
  const std::string kDirectoryResourceId("folder:origin_directory_resource_id");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");
  const base::FilePath::StringType kFileName(FPL("File 1.mp3"));
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

  SetUpDriveServiceExpectCallsForDownloadFile(kFileResourceId);

  EXPECT_CALL(*mock_remote_processor(),
              ApplyRemoteChange(_, _, CreateURL(kOrigin, kFileName), _))
      .WillOnce(InvokeDidApplyRemoteChange());

  SetUpDriveSyncService(true);

  scoped_ptr<ResourceEntry> entry(ResourceEntry::ExtractAndParse(
      *LoadJSONFile("gdata/file_entry.json")));
  AppendIncrementalRemoteChangeByEntry(kOrigin, *entry, 12345);

  ProcessRemoteChange(SYNC_STATUS_OK,
                      CreateURL(kOrigin, kFileName),
                      SYNC_FILE_STATUS_SYNCED,
                      SYNC_ACTION_ADDED,
                      SYNC_DIRECTION_REMOTE_TO_LOCAL);
}

TEST_F(DriveFileSyncServiceTest, RemoteChange_UpdateFile) {
  const GURL kOrigin = ExtensionNameToGURL(FPL("example1"));
  const std::string kDirectoryResourceId("folder:origin_directory_resource_id");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");
  const base::FilePath::StringType kFileName(FPL("File 1.mp3"));
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

  SetUpDriveServiceExpectCallsForDownloadFile(kFileResourceId);

  EXPECT_CALL(*mock_remote_processor(),
              ApplyRemoteChange(_, _, CreateURL(kOrigin, kFileName), _))
      .WillOnce(InvokeDidApplyRemoteChange());

  SetUpDriveSyncService(true);

  scoped_ptr<ResourceEntry> entry(ResourceEntry::ExtractAndParse(
      *LoadJSONFile("gdata/file_entry.json")));
  AppendIncrementalRemoteChangeByEntry(kOrigin, *entry, 12345);
  ProcessRemoteChange(SYNC_STATUS_OK,
                      CreateURL(kOrigin, kFileName),
                      SYNC_FILE_STATUS_SYNCED,
                      SYNC_ACTION_UPDATED,
                      SYNC_DIRECTION_REMOTE_TO_LOCAL);
}

TEST_F(DriveFileSyncServiceTest, RegisterOriginWithSyncDisabled) {
  const GURL kOrigin("chrome-extension://example");
  const std::string kDirectoryResourceId("folder:origin_directory_resource_id");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");

  metadata_store()->SetSyncRootDirectory(kSyncRootResourceId);

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_DISABLED, _))
      .Times(AtLeast(1));
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(0))
      .Times(AnyNumber());

  InSequence sequence;

  SetUpDriveServiceExpectCallsForGetResourceList(
      "sync_file_system/origin_directory_found.json",
      FormatTitleQuery(DriveFileSyncClient::OriginToDirectoryTitle(kOrigin)),
      kSyncRootResourceId);

  // Usually the sync service starts batch sync here, but since we're
  // setting up a drive service with sync disabled batch sync doesn't
  // start (while register origin should still return OK).

  SetUpDriveSyncService(false);
  bool done = false;
  sync_service()->RegisterOriginForTrackingChanges(
      kOrigin, base::Bind(&ExpectEqStatus, &done, SYNC_STATUS_OK));
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(done);

  // We must not have started batch sync for the newly registered origin,
  // so it should still be in the batch_sync_origins.
  EXPECT_EQ(1u, metadata_store()->batch_sync_origins().size());
  EXPECT_TRUE(metadata_store()->incremental_sync_origins().empty());
  EXPECT_TRUE(pending_changes().empty());
}

TEST_F(DriveFileSyncServiceTest, RemoteChange_Override) {
  const GURL kOrigin = ExtensionNameToGURL(FPL("example1"));
  const std::string kDirectoryResourceId("folder:origin_directory_resource_id");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");
  const base::FilePath kFilePath(FPL("File 1.mp3"));
  const std::string kFileResourceId("file:2_file_resource_id");
  const std::string kFileResourceId2("file:2_file_resource_id_2");
  const fileapi::FileSystemURL kURL(CreateURL(kOrigin, kFilePath.value()));

  metadata_store()->SetSyncRootDirectory(kSyncRootResourceId);
  metadata_store()->AddBatchSyncOrigin(kOrigin, kDirectoryResourceId);
  metadata_store()->MoveBatchSyncOriginToIncremental(kOrigin);

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(_))
      .Times(AnyNumber());

  SetUpDriveSyncService(true);

  EXPECT_TRUE(AppendIncrementalRemoteChange(
      kOrigin, kFilePath, false /* is_deleted */,
      kFileResourceId, 2, "remote_file_md5"));

  // Expect to drop this change since there is another newer change on the
  // queue.
  EXPECT_FALSE(AppendIncrementalRemoteChange(
      kOrigin, kFilePath, false /* is_deleted */,
      kFileResourceId, 1, "remote_file_md5_2"));

  // Expect to drop this change since it has the same md5 with the previous one.
  EXPECT_FALSE(AppendIncrementalRemoteChange(
      kOrigin, kFilePath, false /* is_deleted */,
      kFileResourceId, 4, "remote_file_md5"));

  // This should not cause browser crash.
  EXPECT_FALSE(AppendIncrementalRemoteChange(
      kOrigin, kFilePath, false /* is_deleted */,
      kFileResourceId, 4, "remote_file_md5"));

  // Expect to drop these changes since they have different resource IDs with
  // the previous ones.
  EXPECT_FALSE(AppendIncrementalRemoteChange(
      kOrigin, kFilePath, false /* is_deleted */,
      kFileResourceId2, 5, "updated_file_md5"));
  EXPECT_FALSE(AppendIncrementalRemoteChange(
      kOrigin, kFilePath, true /* is_deleted */,
      kFileResourceId2, 5, "deleted_file_md5"));

  // Push delete change.
  EXPECT_TRUE(AppendIncrementalRemoteChange(
      kOrigin, kFilePath, true /* is_deleted */,
      kFileResourceId, 6, "deleted_file_md5"));

  // Expect not to drop these changes even if they have different resource IDs.
  EXPECT_TRUE(AppendIncrementalRemoteChange(
      kOrigin, kFilePath, true /* is_deleted */,
      kFileResourceId2, 7, "deleted_file_md5"));
  EXPECT_TRUE(AppendIncrementalRemoteChange(
      kOrigin, kFilePath, false /* is_deleted */,
      kFileResourceId, 8, "updated_file_md5"));
}

TEST_F(DriveFileSyncServiceTest, RemoteChange_Folder) {
  const GURL kOrigin = ExtensionNameToGURL(FPL("example1"));
  const std::string kDirectoryResourceId("folder:origin_directory_resource_id");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");

  metadata_store()->SetSyncRootDirectory(kSyncRootResourceId);
  metadata_store()->AddBatchSyncOrigin(kOrigin, kDirectoryResourceId);
  metadata_store()->MoveBatchSyncOriginToIncremental(kOrigin);

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(_))
      .Times(AnyNumber());

  SetUpDriveSyncService(true);

  scoped_ptr<ResourceEntry> entry(ResourceEntry::ExtractAndParse(
      *LoadJSONFile("gdata/file_entry.json")));
  entry->set_kind(google_apis::ENTRY_KIND_FOLDER);

  // Expect to drop this change for file.
  EXPECT_FALSE(AppendIncrementalRemoteChangeByEntry(
      kOrigin, *entry, 1));
}

#endif  // !defined(OS_ANDROID)

}  // namespace sync_file_system
