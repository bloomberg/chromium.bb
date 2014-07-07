// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_service.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/drive/test_util.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_drive_service_helper.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/api_util.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_util.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/local/canned_syncable_file_system.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_context.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "webkit/browser/fileapi/file_system_context.h"

#define FPL(path) FILE_PATH_LITERAL(path)

using content::BrowserThread;

using google_apis::GDataErrorCode;
using google_apis::ResourceEntry;

namespace sync_file_system {

namespace {
const char kTestProfileName[] = "test-profile";
}

using drive_backend::APIUtil;
using drive_backend::APIUtilInterface;
using drive_backend::FakeDriveServiceHelper;

class DriveFileSyncServiceSyncTest : public testing::Test {
 public:
  DriveFileSyncServiceSyncTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  virtual ~DriveFileSyncServiceSyncTest() {
  }

  virtual void SetUp() OVERRIDE {
    // TODO(tzik): Set up TestExtensionSystem to support simulated relaunch.
    in_memory_env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));

    ASSERT_TRUE(profile_manager_.SetUp());
    TestingProfile* profile =
        profile_manager_.CreateTestingProfile(kTestProfileName);

    RegisterSyncableFileSystem();
    local_sync_service_ = LocalFileSyncService::CreateForTesting(
        profile, in_memory_env_.get());

    fake_drive_service_ = new drive::FakeDriveService();
    fake_drive_service_->Initialize("test_user@gmail.com");
    ASSERT_TRUE(drive::test_util::SetUpTestEntries(fake_drive_service_));

    drive_uploader_ = new drive::DriveUploader(
        fake_drive_service_, base::ThreadTaskRunnerHandle::Get().get());

    fake_drive_helper_.reset(new FakeDriveServiceHelper(
        fake_drive_service_, drive_uploader_,
        APIUtil::GetSyncRootDirectoryName()));

    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    bool created = false;
    scoped_ptr<DriveMetadataStore> metadata_store(
        new DriveMetadataStore(fake_drive_helper_->base_dir_path(),
                               base::ThreadTaskRunnerHandle::Get().get()));
    metadata_store->Initialize(CreateResultReceiver(&status, &created));
    FlushMessageLoop();
    EXPECT_EQ(SYNC_STATUS_OK, status);
    EXPECT_TRUE(created);

    scoped_ptr<APIUtil> api_util(APIUtil::CreateForTesting(
        fake_drive_helper_->base_dir_path().AppendASCII("tmp"),
        scoped_ptr<drive::DriveServiceInterface>(fake_drive_service_),
        scoped_ptr<drive::DriveUploaderInterface>(drive_uploader_)));

    remote_sync_service_ = DriveFileSyncService::CreateForTesting(
        profile,
        fake_drive_helper_->base_dir_path(),
        api_util.PassAs<APIUtilInterface>(),
        metadata_store.Pass());

    local_sync_service_->SetLocalChangeProcessor(remote_sync_service_.get());
    remote_sync_service_->SetRemoteChangeProcessor(local_sync_service_.get());
  }

  virtual void TearDown() OVERRIDE {
    drive_uploader_ = NULL;
    fake_drive_service_ = NULL;
    remote_sync_service_.reset();
    local_sync_service_.reset();
    FlushMessageLoop();

    typedef std::map<GURL, CannedSyncableFileSystem*>::iterator iterator;
    for (iterator itr = file_systems_.begin();
         itr != file_systems_.end(); ++itr) {
      itr->second->TearDown();
      delete itr->second;
    }
    file_systems_.clear();

    FlushMessageLoop();
    RevokeSyncableFileSystem();
  }

 protected:
  void RegisterOrigin(const GURL& origin) {
    if (!ContainsKey(file_systems_, origin)) {
      CannedSyncableFileSystem* file_system = new CannedSyncableFileSystem(
          origin,
          in_memory_env_.get(),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO).get(),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)
              .get());

      SyncStatusCode status = SYNC_STATUS_UNKNOWN;
      file_system->SetUp(CannedSyncableFileSystem::QUOTA_ENABLED);
      local_sync_service_->MaybeInitializeFileSystemContext(
          origin, file_system->file_system_context(),
          CreateResultReceiver(&status));
      FlushMessageLoop();
      EXPECT_EQ(SYNC_STATUS_OK, status);

      file_system->backend()->sync_context()->
          set_mock_notify_changes_duration_in_sec(0);

      EXPECT_EQ(base::File::FILE_OK, file_system->OpenFileSystem());
      file_systems_[origin] = file_system;
    }

    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    remote_sync_service_->RegisterOrigin(
        origin, CreateResultReceiver(&status));
    FlushMessageLoop();
    EXPECT_EQ(SYNC_STATUS_OK, status);
  }

  void AddOrUpdateLocalFile(const GURL& origin,
                            const base::FilePath& path,
                            const std::string& content) {
    fileapi::FileSystemURL url(CreateSyncableFileSystemURL(origin, path));
    ASSERT_TRUE(ContainsKey(file_systems_, origin));
    EXPECT_EQ(base::File::FILE_OK, file_systems_[origin]->CreateFile(url));
    int64 bytes_written = file_systems_[origin]->WriteString(url, content);
    EXPECT_EQ(static_cast<int64>(content.size()), bytes_written);
    FlushMessageLoop();
  }

  void UpdateLocalFile(const GURL& origin,
                       const base::FilePath& path,
                       const std::string& content) {
    ASSERT_TRUE(ContainsKey(file_systems_, origin));
    int64 bytes_written = file_systems_[origin]->WriteString(
        CreateSyncableFileSystemURL(origin, path), content);
    EXPECT_EQ(static_cast<int64>(content.size()), bytes_written);
    FlushMessageLoop();
  }

  void RemoveLocal(const GURL& origin, const base::FilePath& path) {
    ASSERT_TRUE(ContainsKey(file_systems_, origin));
    EXPECT_EQ(base::File::FILE_OK,
              file_systems_[origin]->Remove(
                  CreateSyncableFileSystemURL(origin, path),
                  true /* recursive */));
    FlushMessageLoop();
  }

  SyncStatusCode ProcessLocalChange() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    fileapi::FileSystemURL url;
    local_sync_service_->ProcessLocalChange(
        CreateResultReceiver(&status, &url));
    FlushMessageLoop();
    return status;
  }

  SyncStatusCode ProcessRemoteChange() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    fileapi::FileSystemURL url;
    remote_sync_service_->ProcessRemoteChange(
        CreateResultReceiver(&status, &url));
    FlushMessageLoop();
    return status;
  }

  SyncStatusCode ProcessChangesUntilDone() {
    remote_sync_service_->OnNotificationReceived();
    FlushMessageLoop();

    SyncStatusCode local_sync_status;
    SyncStatusCode remote_sync_status;
    do {
      local_sync_status = ProcessLocalChange();
      if (local_sync_status != SYNC_STATUS_OK &&
          local_sync_status != SYNC_STATUS_NO_CHANGE_TO_SYNC)
        return local_sync_status;

      remote_sync_status = ProcessRemoteChange();
      if (remote_sync_status != SYNC_STATUS_OK &&
          remote_sync_status != SYNC_STATUS_NO_CHANGE_TO_SYNC)
        return remote_sync_status;
    } while (local_sync_status != SYNC_STATUS_NO_CHANGE_TO_SYNC &&
             remote_sync_status != SYNC_STATUS_NO_CHANGE_TO_SYNC);
    return SYNC_STATUS_OK;
  }

  // Verifies local and remote files/folders are consistent.
  // This function checks:
  //  - Each registered origin has corresponding remote folder.
  //  - Each local file/folder has corresponding remote one.
  //  - Each remote file/folder has corresponding local one.
  // TODO(tzik): Handle conflict case. i.e. allow remote file has different
  // file content if the corresponding local file conflicts to it.
  void VerifyConsistency() {
    std::string sync_root_folder_id;
    GDataErrorCode error =
        fake_drive_helper_->GetSyncRootFolderID(&sync_root_folder_id);
    if (sync_root_folder_id.empty()) {
      EXPECT_EQ(google_apis::HTTP_NOT_FOUND, error);
      EXPECT_TRUE(file_systems_.empty());
      return;
    }
    EXPECT_EQ(google_apis::HTTP_SUCCESS, error);

    ScopedVector<ResourceEntry> remote_entries;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->ListFilesInFolder(
                  sync_root_folder_id, &remote_entries));
    std::map<std::string, const ResourceEntry*> origin_root_by_title;
    for (ScopedVector<ResourceEntry>::iterator itr = remote_entries.begin();
         itr != remote_entries.end();
         ++itr) {
      const ResourceEntry& remote_entry = **itr;
      EXPECT_FALSE(ContainsKey(origin_root_by_title, remote_entry.title()));
      origin_root_by_title[remote_entry.title()] = *itr;
    }

    for (std::map<GURL, CannedSyncableFileSystem*>::const_iterator itr =
             file_systems_.begin();
         itr != file_systems_.end(); ++itr) {
      const GURL& origin = itr->first;
      SCOPED_TRACE(testing::Message() << "Verifying origin: " << origin);
      CannedSyncableFileSystem* file_system = itr->second;
      ASSERT_TRUE(ContainsKey(origin_root_by_title, origin.host()));
      VerifyConsistencyForFolder(
          origin, base::FilePath(),
          origin_root_by_title[origin.host()]->resource_id(),
          file_system);
    }
  }

  void VerifyConsistencyForOrigin(const GURL& origin) {
    std::string sync_root_folder_id;
    ASSERT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->GetSyncRootFolderID(&sync_root_folder_id));
    ASSERT_FALSE(sync_root_folder_id.empty());

    ScopedVector<ResourceEntry> origin_folder;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->SearchByTitle(
                  sync_root_folder_id, origin.host(), &origin_folder));
    ASSERT_EQ(1u, origin_folder.size());

    ASSERT_TRUE(ContainsKey(file_systems_, origin));
    VerifyConsistencyForFolder(
        origin, base::FilePath(),
        origin_folder[0]->resource_id(),
        file_systems_[origin]);
  }

  void VerifyConsistencyForFolder(const GURL& origin,
                                  const base::FilePath& path,
                                  const std::string& folder_id,
                                  CannedSyncableFileSystem* file_system) {
    SCOPED_TRACE(testing::Message() << "Verifying path: " << path.value());

    ScopedVector<ResourceEntry> remote_entries;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->ListFilesInFolder(
                  folder_id, &remote_entries));
    std::map<std::string, const ResourceEntry*> remote_entry_by_title;
    for (ScopedVector<ResourceEntry>::iterator itr = remote_entries.begin();
         itr != remote_entries.end();
         ++itr) {
      const ResourceEntry& remote_entry = **itr;
      EXPECT_FALSE(ContainsKey(remote_entry_by_title, remote_entry.title()));
      remote_entry_by_title[remote_entry.title()] = *itr;
    }

    fileapi::FileSystemURL url(CreateSyncableFileSystemURL(origin, path));
    CannedSyncableFileSystem::FileEntryList local_entries;
    EXPECT_EQ(base::File::FILE_OK,
              file_system->ReadDirectory(url, &local_entries));
    for (CannedSyncableFileSystem::FileEntryList::iterator itr =
             local_entries.begin();
         itr != local_entries.end();
         ++itr) {
      const fileapi::DirectoryEntry& local_entry = *itr;
      fileapi::FileSystemURL entry_url(
          CreateSyncableFileSystemURL(origin, path.Append(local_entry.name)));
      std::string title = DriveFileSyncService::PathToTitle(entry_url.path());
      ASSERT_TRUE(ContainsKey(remote_entry_by_title, title));
      const ResourceEntry& remote_entry = *remote_entry_by_title[title];
      if (local_entry.is_directory) {
        ASSERT_TRUE(remote_entry.is_folder());
        VerifyConsistencyForFolder(origin, entry_url.path(),
                                   remote_entry.resource_id(),
                                   file_system);
      } else {
        ASSERT_TRUE(remote_entry.is_file());
        VerifyConsistencyForFile(origin, entry_url.path(),
                                 remote_entry.resource_id(),
                                 file_system);
      }
      remote_entry_by_title.erase(title);
    }

    EXPECT_TRUE(remote_entry_by_title.empty());
  }

  void VerifyConsistencyForFile(const GURL& origin,
                                const base::FilePath& path,
                                const std::string& file_id,
                                CannedSyncableFileSystem* file_system) {
    fileapi::FileSystemURL url(CreateSyncableFileSystemURL(origin, path));
    std::string file_content;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->ReadFile(file_id, &file_content));
    EXPECT_EQ(base::File::FILE_OK,
              file_system->VerifyFile(url, file_content));
  }

  void FlushMessageLoop() {
    base::MessageLoop::current()->RunUntilIdle();
    BrowserThread::GetBlockingPool()->FlushForTesting();
    base::MessageLoop::current()->RunUntilIdle();
  }

  void TestInitialization();
  void TestLocalToRemoteBasic();
  void TestRemoteToLocalBasic();
  void TestLocalFileUpdate();
  void TestRemoteFileUpdate();
  void TestLocalFileDeletion();
  void TestRemoteFileDeletion();

  ScopedDisableSyncFSV2 scoped_disable_v2_;
  content::TestBrowserThreadBundle thread_bundle_;

  scoped_ptr<leveldb::Env> in_memory_env_;
  TestingProfileManager profile_manager_;

  drive::FakeDriveService* fake_drive_service_;
  drive::DriveUploader* drive_uploader_;
  scoped_ptr<FakeDriveServiceHelper> fake_drive_helper_;
  std::map<GURL, CannedSyncableFileSystem*> file_systems_;

  scoped_ptr<DriveFileSyncService> remote_sync_service_;
  scoped_ptr<LocalFileSyncService> local_sync_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncServiceSyncTest);
};

void DriveFileSyncServiceSyncTest::TestInitialization() {
  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();
}

void DriveFileSyncServiceSyncTest::TestLocalToRemoteBasic() {
  const GURL kOrigin("chrome-extension://example");

  RegisterOrigin(kOrigin);
  AddOrUpdateLocalFile(kOrigin, base::FilePath(FPL("file")), "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();
}

void DriveFileSyncServiceSyncTest::TestRemoteToLocalBasic() {
  const GURL kOrigin("chrome-extension://example");

  std::string sync_root_folder_id;
  EXPECT_EQ(google_apis::HTTP_CREATED,
            fake_drive_helper_->AddOrphanedFolder(
                APIUtil::GetSyncRootDirectoryName(),
                &sync_root_folder_id));

  std::string origin_root_folder_id;
  EXPECT_EQ(google_apis::HTTP_CREATED,
            fake_drive_helper_->AddFolder(
                sync_root_folder_id, kOrigin.host(), &origin_root_folder_id));

  RegisterOrigin(kOrigin);

  std::string file_id;
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_helper_->AddFile(
                origin_root_folder_id, "file", "abcde", &file_id));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();
}

void DriveFileSyncServiceSyncTest::TestLocalFileUpdate() {
  const GURL kOrigin("chrome-extension://example");
  const base::FilePath kPath(FPL("file"));

  RegisterOrigin(kOrigin);
  AddOrUpdateLocalFile(kOrigin, kPath, "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistencyForOrigin(kOrigin);

  UpdateLocalFile(kOrigin, kPath, "1234567890");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();
}

void DriveFileSyncServiceSyncTest::TestRemoteFileUpdate() {
  const GURL kOrigin("chrome-extension://example");
  const base::FilePath kPath(FPL("file"));
  const std::string kTitle(DriveFileSyncService::PathToTitle(kPath));

  std::string sync_root_folder_id;
  EXPECT_EQ(google_apis::HTTP_CREATED,
            fake_drive_helper_->AddOrphanedFolder(
                APIUtil::GetSyncRootDirectoryName(),
                &sync_root_folder_id));

  std::string origin_root_folder_id;
  EXPECT_EQ(google_apis::HTTP_CREATED,
            fake_drive_helper_->AddFolder(
                sync_root_folder_id, kOrigin.host(), &origin_root_folder_id));

  std::string remote_file_id;
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_helper_->AddFile(
                origin_root_folder_id, kTitle, "abcde", &remote_file_id));

  RegisterOrigin(kOrigin);
  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistencyForOrigin(kOrigin);

  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_helper_->UpdateFile(remote_file_id, "1234567890"));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();
}

void DriveFileSyncServiceSyncTest::TestLocalFileDeletion() {
  const GURL kOrigin("chrome-extension://example");
  const base::FilePath kPath(FPL("file"));

  RegisterOrigin(kOrigin);
  AddOrUpdateLocalFile(kOrigin, kPath, "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistencyForOrigin(kOrigin);

  RemoveLocal(kOrigin, kPath);

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();
}

void DriveFileSyncServiceSyncTest::TestRemoteFileDeletion() {
  const GURL kOrigin("chrome-extension://example");
  const base::FilePath kPath(FPL("file"));
  const std::string kTitle(DriveFileSyncService::PathToTitle(kPath));

  std::string sync_root_folder_id;
  EXPECT_EQ(google_apis::HTTP_CREATED,
            fake_drive_helper_->AddOrphanedFolder(
                APIUtil::GetSyncRootDirectoryName(),
                &sync_root_folder_id));

  std::string origin_root_folder_id;
  EXPECT_EQ(google_apis::HTTP_CREATED,
            fake_drive_helper_->AddFolder(
                sync_root_folder_id, kOrigin.host(), &origin_root_folder_id));

  std::string remote_file_id;
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_helper_->AddFile(
                origin_root_folder_id, kTitle, "abcde", &remote_file_id));

  RegisterOrigin(kOrigin);
  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistencyForOrigin(kOrigin);

  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_helper_->TrashResource(remote_file_id));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();
}

TEST_F(DriveFileSyncServiceSyncTest, InitializationTest) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestInitialization();
}

TEST_F(DriveFileSyncServiceSyncTest, InitializationTest_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestInitialization();
}

TEST_F(DriveFileSyncServiceSyncTest, LocalToRemoteBasicTest) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestLocalToRemoteBasic();
}

TEST_F(DriveFileSyncServiceSyncTest, LocalToRemoteBasicTest_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestLocalToRemoteBasic();
}

TEST_F(DriveFileSyncServiceSyncTest, RemoteToLocalBasicTest) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestRemoteToLocalBasic();
}

TEST_F(DriveFileSyncServiceSyncTest, RemoteToLocalBasicTest_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestRemoteToLocalBasic();
}

TEST_F(DriveFileSyncServiceSyncTest, LocalFileUpdateTest) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestLocalFileUpdate();
}

TEST_F(DriveFileSyncServiceSyncTest, LocalFileUpdateTest_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestLocalFileUpdate();
}

TEST_F(DriveFileSyncServiceSyncTest, RemoteFileUpdateTest) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestRemoteFileUpdate();
}

TEST_F(DriveFileSyncServiceSyncTest, RemoteFileUpdateTest_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestRemoteFileUpdate();
}

TEST_F(DriveFileSyncServiceSyncTest, LocalFileDeletionTest) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestLocalFileDeletion();
}

TEST_F(DriveFileSyncServiceSyncTest, LocalFileDeletionTest_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestLocalFileDeletion();
}

TEST_F(DriveFileSyncServiceSyncTest, RemoteFileDeletionTest) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestRemoteFileDeletion();
}

TEST_F(DriveFileSyncServiceSyncTest, RemoteFileDeletionTest_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestRemoteFileDeletion();
}

}  // namespace sync_file_system
