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
#include "chrome/browser/google_apis/drive_api_parser.h"
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
#include "extensions/common/id_util.h"
#include "net/base/escape.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/syncable/sync_direction.h"
#include "webkit/fileapi/syncable/sync_file_metadata.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

#define FPL(x) FILE_PATH_LITERAL(x)

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::NiceMock;
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

const char kRootResourceId[] = "folder:root";
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

void DidGetSyncRoot(bool* done,
                    SyncStatusCode status,
                    const std::string& resource_id) {
  EXPECT_FALSE(*done);
  *done = true;
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
  std::string id = extensions::id_util::GenerateIdForPath(
      base::FilePath(extension_name));

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
  std::string id = extensions::id_util::GenerateIdForPath(
      base::FilePath(extension_name));
  return extensions::Extension::GetBaseURLFromExtensionId(id);
}

ACTION(InvokeCompletionCallback) {
  base::MessageLoopProxy::current()->PostTask(FROM_HERE, arg1);
}

// Invoke |arg2| as a EntryActionCallback.
ACTION_P(InvokeEntryActionCallback, error) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(arg2, error));
}

// Invokes |arg0| as a GetDataCallback.
ACTION_P2(InvokeGetAboutResourceCallback0, error, result) {
  scoped_ptr<google_apis::AboutResource> about_resource(result.Pass());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg0, error, base::Passed(&about_resource)));
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

// Invokes |arg1| as a GetResourceListCallback.
ACTION_P2(InvokeGetResourceListCallback1, error, result) {
  scoped_ptr<google_apis::ResourceList> resource_list(result.Pass());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg1, error, base::Passed(&resource_list)));
}

// Invokes |arg2| as a GetResourceListCallback.
ACTION_P2(InvokeGetResourceListCallback2, error, result) {
  scoped_ptr<google_apis::ResourceList> resource_list(result.Pass());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg2, error, base::Passed(&resource_list)));
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

class DriveFileSyncServiceMockTest : public testing::Test {
 public:
  DriveFileSyncServiceMockTest()
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
    extension_service_ = extension_system->Get(
        profile_.get())->extension_service();

    AddTestExtension(extension_service_, FPL("example1"));
    AddTestExtension(extension_service_, FPL("example2"));

    ASSERT_TRUE(RegisterSyncableFileSystem(kServiceName));

    mock_drive_service_ = new NiceMock<google_apis::MockDriveService>;

    EXPECT_CALL(*mock_drive_service(), Initialize(profile_.get()));
    EXPECT_CALL(*mock_drive_service(), AddObserver(_));

    // Expect to call GetRootResourceId and RemoveResourceFromDirectory to
    // ensure the sync root directory is not in 'My Drive' directory.
    EXPECT_CALL(*mock_drive_service(), GetRootResourceId())
        .WillRepeatedly(Return(kRootResourceId));
    EXPECT_CALL(*mock_drive_service(),
                RemoveResourceFromDirectory(kRootResourceId, _, _))
        .Times(AnyNumber());

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
    sync_service_->SetRemoteChangeProcessor(mock_remote_processor());
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

    extension_service_ = NULL;
    profile_.reset();
    message_loop_.RunUntilIdle();
  }

  void SetSyncEnabled(bool enabled) {
    sync_service_->SetSyncEnabled(enabled);
  }

 protected:
  void EnableExtension(const std::string& extension_id) {
    extension_service_->EnableExtension(extension_id);
  }

  void DisableExtension(const std::string& extension_id) {
    extension_service_->DisableExtension(
        extension_id, extensions::Extension::DISABLE_NONE);
  }

  void UninstallExtension(const std::string& extension_id) {
    // Call UnloadExtension instead of UninstallExtension since it does
    // unnecessary cleanup (e.g. deleting extension data) and emits warnings.
    extension_service_->UnloadExtension(
        extension_id, extension_misc::UNLOAD_REASON_UNINSTALL);
  }

  void UpdateRegisteredOrigins() {
    sync_service_->UpdateRegisteredOrigins();
    // Wait for completion of uninstalling origin.
    message_loop()->RunUntilIdle();
  }

  void VerifySizeOfRegisteredOrigins(
      DriveMetadataStore::ResourceIdByOrigin::size_type b_size,
      DriveMetadataStore::ResourceIdByOrigin::size_type i_size,
      DriveMetadataStore::ResourceIdByOrigin::size_type d_size) {
    EXPECT_EQ(b_size, metadata_store()->batch_sync_origins().size());
    EXPECT_EQ(i_size, metadata_store()->incremental_sync_origins().size());
    EXPECT_EQ(d_size, metadata_store()->disabled_origins().size());
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

  NiceMock<google_apis::MockDriveService>* mock_drive_service() {
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
        base::Bind(&DriveFileSyncServiceMockTest::DidProcessRemoteChange,
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
        changestamp, remote_file_md5, base::Time(),
        SYNC_FILE_TYPE_FILE,
        DriveFileSyncService::REMOTE_SYNC_TYPE_INCREMENTAL);
  }

  // Mock setup helpers ------------------------------------------------------
  void SetUpDriveServiceExpectCallsForSearchByTitle(
      const std::string& result_mock_json_name,
      const std::string& title,
      const std::string& search_directory) {
    scoped_ptr<Value> result_value(LoadJSONFile(
        result_mock_json_name));
    scoped_ptr<google_apis::ResourceList> result(
        google_apis::ResourceList::ExtractAndParse(*result_value));
    EXPECT_CALL(*mock_drive_service(),
                SearchByTitle(title, search_directory, _))
        .WillOnce(InvokeGetResourceListCallback2(
            google_apis::HTTP_SUCCESS,
            base::Passed(&result)))
        .RetiresOnSaturation();
  }

  void SetUpDriveServiceExpectCallsForGetResourceListInDirectory(
      const std::string& result_mock_json_name,
      const std::string& search_directory) {
    scoped_ptr<Value> result_value(LoadJSONFile(
        result_mock_json_name));
    scoped_ptr<google_apis::ResourceList> result(
        google_apis::ResourceList::ExtractAndParse(*result_value));
    EXPECT_CALL(*mock_drive_service(),
                GetResourceListInDirectory(search_directory, _))
        .WillOnce(InvokeGetResourceListCallback1(
            google_apis::HTTP_SUCCESS,
            base::Passed(&result)))
        .RetiresOnSaturation();
  }

  void SetUpDriveServiceExpectCallsForIncrementalSync() {
    scoped_ptr<Value> result_value(LoadJSONFile(
        "chromeos/sync_file_system/origin_directory_not_found.json"));
    scoped_ptr<google_apis::ResourceList> result(
        google_apis::ResourceList::ExtractAndParse(*result_value));
    EXPECT_CALL(*mock_drive_service(), GetChangeList(1, _))
        .WillOnce(InvokeGetResourceListCallback1(
            google_apis::HTTP_SUCCESS,
            base::Passed(&result)))
        .RetiresOnSaturation();
  }

  void SetUpDriveServiceExpectCallsForGetSyncRoot() {
    scoped_ptr<Value> result_value(LoadJSONFile(
        "chromeos/sync_file_system/sync_root_found.json"));
    scoped_ptr<google_apis::ResourceList> result(
        google_apis::ResourceList::ExtractAndParse(*result_value));
    EXPECT_CALL(*mock_drive_service(),
                SearchByTitle(DriveFileSyncClient::GetSyncRootDirectoryName(),
                              std::string(), _))
        .Times(AtMost(1))
        .WillOnce(InvokeGetResourceListCallback2(
            google_apis::HTTP_SUCCESS,
            base::Passed(&result)))
        .RetiresOnSaturation();
  }

  void SetUpDriveServiceExpectCallsForGetAboutResource() {
    scoped_ptr<Value> account_metadata_value(LoadJSONFile(
        "chromeos/gdata/account_metadata.json"));
    scoped_ptr<google_apis::AboutResource> about_resource(
        google_apis::AboutResource::CreateFromAccountMetadata(
            *google_apis::AccountMetadata::CreateFrom(*account_metadata_value),
            kRootResourceId));
    EXPECT_CALL(*mock_drive_service(), GetAboutResource(_))
        .WillOnce(InvokeGetAboutResourceCallback0(
            google_apis::HTTP_SUCCESS,
            base::Passed(&about_resource)))
        .RetiresOnSaturation();
  }

  void SetUpDriveServiceExpectCallsForDownloadFile(
      const std::string& file_resource_id) {
    scoped_ptr<Value> file_entry_value(
        LoadJSONFile("chromeos/gdata/file_entry.json").Pass());
    scoped_ptr<google_apis::ResourceEntry> file_entry
        = google_apis::ResourceEntry::ExtractAndParse(*file_entry_value);
    EXPECT_CALL(*mock_drive_service(),
                GetResourceEntry(file_resource_id, _))
        .WillOnce(InvokeGetResourceEntryCallback1(
            google_apis::HTTP_SUCCESS,
            base::Passed(&file_entry)))
        .RetiresOnSaturation();

    EXPECT_CALL(*mock_drive_service(),
                DownloadFile(_, _, GURL("https://file_content_url"), _, _, _))
        .WillOnce(InvokeDidDownloadFile())
        .RetiresOnSaturation();
  }

  void SetUpDriveServiceExpectCallsForAddNewDirectory(
      const std::string& parent_directory,
      const std::string& directory_name) {
    scoped_ptr<Value> origin_directory_created_value(LoadJSONFile(
        "chromeos/sync_file_system/origin_directory_created.json"));
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

#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif

  scoped_ptr<DriveFileSyncService> sync_service_;

  // Not owned.
  ExtensionService* extension_service_;

  // Owned by |sync_client_|.
  NiceMock<google_apis::MockDriveService>* mock_drive_service_;

  StrictMock<MockRemoteServiceObserver> mock_remote_observer_;
  StrictMock<MockFileStatusObserver> mock_file_status_observer_;
  StrictMock<MockRemoteChangeProcessor> mock_remote_processor_;

  scoped_ptr<DriveFileSyncClient> sync_client_;
  scoped_ptr<DriveMetadataStore> metadata_store_;

  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncServiceMockTest);
};

#if !defined(OS_ANDROID)

TEST_F(DriveFileSyncServiceMockTest, BatchSyncOnInitialization) {
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
  EXPECT_CALL(*mock_remote_observer(), OnRemoteChangeQueueUpdated(1))
      .Times(AnyNumber())
      .InSequence(change_queue_seq);

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(AnyNumber());

  SetUpDriveServiceExpectCallsForGetAboutResource();
  SetUpDriveServiceExpectCallsForGetResourceListInDirectory(
      "chromeos/sync_file_system/listing_files_in_directory.json",
      kDirectoryResourceId1);

  EXPECT_CALL(*mock_remote_observer(),
              OnRemoteServiceStateUpdated(REMOTE_SERVICE_OK, _))
      .Times(AnyNumber());

  // The service will get called for incremental sync at the end after
  // batch sync's done.
  SetUpDriveServiceExpectCallsForIncrementalSync();

  SetUpDriveSyncService(true);
  message_loop()->RunUntilIdle();

  // kOrigin1 should be a batch sync origin and kOrigin2 should be an
  // incremental sync origin.
  // 4 pending remote changes are from listing_files_in_directory as batch sync
  // changes.
  VerifySizeOfRegisteredOrigins(1u, 1u, 0u);
  EXPECT_EQ(1u, pending_changes().size());
}

TEST_F(DriveFileSyncServiceMockTest, RegisterNewOrigin) {
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

  // Expect to call GetResourceList for the sync root from
  // RegisterOriginForTrackingChanges.
  SetUpDriveServiceExpectCallsForGetSyncRoot();

  SetUpDriveServiceExpectCallsForSearchByTitle(
      "chromeos/sync_file_system/origin_directory_found.json",
      DriveFileSyncClient::OriginToDirectoryTitle(kOrigin),
      kSyncRootResourceId);
  SetUpDriveServiceExpectCallsForSearchByTitle(
      "chromeos/sync_file_system/origin_directory_not_found.json",
      DriveFileSyncClient::OriginToDirectoryTitle(kOrigin),
      kSyncRootResourceId);

  // If the directory for the origin is missing, DriveFileSyncService should
  // attempt to create it.
  SetUpDriveServiceExpectCallsForAddNewDirectory(
      kSyncRootResourceId,
      DriveFileSyncClient::OriginToDirectoryTitle(kOrigin));

  // Once the directory is created GetAboutResource should be called to get
  // the largest changestamp for the origin as a prepariation of the batch sync.
  SetUpDriveServiceExpectCallsForGetAboutResource();

  SetUpDriveServiceExpectCallsForGetResourceListInDirectory(
      "chromeos/sync_file_system/listing_files_in_empty_directory.json",
      kDirectoryResourceId);

  SetUpDriveSyncService(true);
  bool done = false;
  sync_service()->RegisterOriginForTrackingChanges(
      kOrigin, base::Bind(&ExpectEqStatus, &done, SYNC_STATUS_OK));
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(done);

  VerifySizeOfRegisteredOrigins(0u, 1u, 0u);
  EXPECT_TRUE(pending_changes().empty());
}

TEST_F(DriveFileSyncServiceMockTest, RegisterExistingOrigin) {
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

  // Expect to call GetResourceList for the sync root from
  // RegisterOriginForTrackingChanges.
  SetUpDriveServiceExpectCallsForGetSyncRoot();

  // We already have a directory for the origin.
  SetUpDriveServiceExpectCallsForSearchByTitle(
      "chromeos/sync_file_system/origin_directory_found.json",
      DriveFileSyncClient::OriginToDirectoryTitle(kOrigin),
      kSyncRootResourceId);

  SetUpDriveServiceExpectCallsForGetAboutResource();

  // DriveFileSyncService should fetch the list of the directory content
  // to start the batch sync.
  SetUpDriveServiceExpectCallsForGetResourceListInDirectory(
      "chromeos/sync_file_system/listing_files_in_directory.json",
      kDirectoryResourceId);

  SetUpDriveSyncService(true);
  bool done = false;
  sync_service()->RegisterOriginForTrackingChanges(
      kOrigin, base::Bind(&ExpectEqStatus, &done, SYNC_STATUS_OK));
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(done);

  // The origin should be registered as a batch sync origin.
  VerifySizeOfRegisteredOrigins(1u, 0u, 0u);

  // |listing_files_in_directory| contains 4 items to sync.
  EXPECT_EQ(1u, pending_changes().size());
}

TEST_F(DriveFileSyncServiceMockTest, UnregisterOrigin) {
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

  SetUpDriveServiceExpectCallsForGetAboutResource();
  SetUpDriveServiceExpectCallsForGetResourceListInDirectory(
      "chromeos/sync_file_system/listing_files_in_directory.json",
      kDirectoryResourceId1);

  SetUpDriveServiceExpectCallsForIncrementalSync();

  SetUpDriveSyncService(true);
  message_loop()->RunUntilIdle();

  VerifySizeOfRegisteredOrigins(1u, 1u, 0u);
  EXPECT_EQ(1u, pending_changes().size());

  bool done = false;
  sync_service()->UnregisterOriginForTrackingChanges(
      kOrigin1, base::Bind(&ExpectEqStatus, &done, SYNC_STATUS_OK));
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(done);

  VerifySizeOfRegisteredOrigins(0u, 1u, 0u);
  EXPECT_TRUE(pending_changes().empty());
}

TEST_F(DriveFileSyncServiceMockTest, UpdateRegisteredOrigins) {
  const GURL kOrigin1 = ExtensionNameToGURL(FPL("example1"));
  const GURL kOrigin2 = ExtensionNameToGURL(FPL("example2"));
  const std::string kDirectoryResourceId1(
      "folder:origin_directory_resource_id");
  const std::string kDirectoryResourceId2(
      "folder:origin_directory_resource_id2");
  const std::string kSyncRootResourceId("folder:sync_root_resource_id");
  const std::string extension_id1 =
      extensions::id_util::GenerateIdForPath(base::FilePath(FPL("example1")));
  const std::string extension_id2 =
      extensions::id_util::GenerateIdForPath(base::FilePath(FPL("example2")));

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

  SetUpDriveServiceExpectCallsForGetAboutResource();
  SetUpDriveServiceExpectCallsForGetResourceListInDirectory(
      "chromeos/sync_file_system/listing_files_in_directory.json",
      kDirectoryResourceId1);

  SetUpDriveServiceExpectCallsForIncrementalSync();

  SetUpDriveSyncService(true);
  message_loop()->RunUntilIdle();

  // [1] Both extensions and origins are enabled. Nothing to do.
  VerifySizeOfRegisteredOrigins(1u, 1u, 0u);
  UpdateRegisteredOrigins();
  VerifySizeOfRegisteredOrigins(1u, 1u, 0u);

  DisableExtension(extension_id1);
  DisableExtension(extension_id2);

  // [2] Origins should be disabled since extensions were disabled.
  VerifySizeOfRegisteredOrigins(1u, 1u, 0u);
  UpdateRegisteredOrigins();
  VerifySizeOfRegisteredOrigins(0u, 0u, 2u);

  // [3] Both extensions and origins are disabled. Nothing to do.
  VerifySizeOfRegisteredOrigins(0u, 0u, 2u);
  UpdateRegisteredOrigins();
  VerifySizeOfRegisteredOrigins(0u, 0u, 2u);

  EnableExtension(extension_id1);
  EnableExtension(extension_id2);

  // [4] Origins should be re-enabled since extensions were re-enabled.
  VerifySizeOfRegisteredOrigins(0u, 0u, 2u);
  UpdateRegisteredOrigins();
  VerifySizeOfRegisteredOrigins(2u, 0u, 0u);

  UninstallExtension(extension_id2);

  // Prepare for UninstallOrigin called by UpdateRegisteredOrigins.
  EXPECT_CALL(*mock_drive_service(),
              DeleteResource(kDirectoryResourceId2, _, _))
      .WillOnce(InvokeEntryActionCallback(google_apis::HTTP_SUCCESS));
  SetUpDriveServiceExpectCallsForGetAboutResource();
  SetUpDriveServiceExpectCallsForGetResourceListInDirectory(
      "chromeos/sync_file_system/listing_files_in_directory.json",
      kDirectoryResourceId1);

  // [5] |kOrigin2| should be unregistered since its extension was uninstalled.
  VerifySizeOfRegisteredOrigins(2u, 0u, 0u);
  UpdateRegisteredOrigins();
  VerifySizeOfRegisteredOrigins(1u, 0u, 0u);
}

TEST_F(DriveFileSyncServiceMockTest, RemoteChange_NoChange) {
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
  VerifySizeOfRegisteredOrigins(0u, 0u, 0u);
  EXPECT_TRUE(pending_changes().empty());
}

TEST_F(DriveFileSyncServiceMockTest, RemoteChange_Busy) {
  const GURL kOrigin = ExtensionNameToGURL(FPL("example1"));
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

  SetUpDriveServiceExpectCallsForIncrementalSync();

  SetUpDriveSyncService(true);

  scoped_ptr<ResourceEntry> entry(ResourceEntry::ExtractAndParse(
      *LoadJSONFile("chromeos/gdata/file_entry.json")));
  AppendIncrementalRemoteChangeByEntry(kOrigin, *entry, 12345);

  ProcessRemoteChange(SYNC_STATUS_FILE_BUSY,
                      CreateURL(kOrigin, kFileName),
                      SYNC_FILE_STATUS_UNKNOWN,
                      SYNC_ACTION_NONE,
                      SYNC_DIRECTION_NONE);
}

TEST_F(DriveFileSyncServiceMockTest, RemoteChange_NewFile) {
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

  SetUpDriveServiceExpectCallsForIncrementalSync();

  SetUpDriveSyncService(true);

  scoped_ptr<ResourceEntry> entry(ResourceEntry::ExtractAndParse(
      *LoadJSONFile("chromeos/gdata/file_entry.json")));
  AppendIncrementalRemoteChangeByEntry(kOrigin, *entry, 12345);

  ProcessRemoteChange(SYNC_STATUS_OK,
                      CreateURL(kOrigin, kFileName),
                      SYNC_FILE_STATUS_SYNCED,
                      SYNC_ACTION_ADDED,
                      SYNC_DIRECTION_REMOTE_TO_LOCAL);
}

TEST_F(DriveFileSyncServiceMockTest, RemoteChange_UpdateFile) {
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

  SetUpDriveServiceExpectCallsForIncrementalSync();

  SetUpDriveSyncService(true);

  scoped_ptr<ResourceEntry> entry(ResourceEntry::ExtractAndParse(
      *LoadJSONFile("chromeos/gdata/file_entry.json")));
  AppendIncrementalRemoteChangeByEntry(kOrigin, *entry, 12345);
  ProcessRemoteChange(SYNC_STATUS_OK,
                      CreateURL(kOrigin, kFileName),
                      SYNC_FILE_STATUS_SYNCED,
                      SYNC_ACTION_UPDATED,
                      SYNC_DIRECTION_REMOTE_TO_LOCAL);
}

TEST_F(DriveFileSyncServiceMockTest, RegisterOriginWithSyncDisabled) {
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

  // Expect to call GetResourceList for the sync root from
  // RegisterOriginForTrackingChanges.
  SetUpDriveServiceExpectCallsForGetSyncRoot();

  SetUpDriveServiceExpectCallsForSearchByTitle(
      "chromeos/sync_file_system/origin_directory_found.json",
      DriveFileSyncClient::OriginToDirectoryTitle(kOrigin),
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
  VerifySizeOfRegisteredOrigins(1u, 0u, 0u);
  EXPECT_TRUE(pending_changes().empty());
}

TEST_F(DriveFileSyncServiceMockTest, RemoteChange_Override) {
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

  // Expect to drop this delete change since it has a different resource ID with
  // the previous one.
  EXPECT_FALSE(AppendIncrementalRemoteChange(
      kOrigin, kFilePath, true /* is_deleted */,
      kFileResourceId2, 7, "deleted_file_md5"));

  // Expect not to drop this change even if it has a different resource ID with
  // the previous one.
  EXPECT_TRUE(AppendIncrementalRemoteChange(
      kOrigin, kFilePath, false /* is_deleted */,
      kFileResourceId2, 8, "updated_file_md5"));
}

TEST_F(DriveFileSyncServiceMockTest, RemoteChange_Folder) {
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
      *LoadJSONFile("chromeos/gdata/file_entry.json")));
  entry->set_kind(google_apis::ENTRY_KIND_FOLDER);

  // Expect to drop this change for file.
  EXPECT_FALSE(AppendIncrementalRemoteChangeByEntry(
      kOrigin, *entry, 1));
}

#endif  // !defined(OS_ANDROID)

}  // namespace sync_file_system
