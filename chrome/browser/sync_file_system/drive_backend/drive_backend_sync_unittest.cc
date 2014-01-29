// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <stack>

#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_drive_service_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"
#include "chrome/browser/sync_file_system/local/canned_syncable_file_system.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_context.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "webkit/browser/fileapi/file_system_context.h"

#define FPL(a) FILE_PATH_LITERAL(a)

namespace sync_file_system {
namespace drive_backend {

typedef fileapi::FileSystemOperation::FileEntryList FileEntryList;

class DriveBackendSyncTest : public testing::Test {
 public:
  DriveBackendSyncTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  virtual ~DriveBackendSyncTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(base_dir_.CreateUniqueTempDir());
    in_memory_env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));

    io_task_runner_ = content::BrowserThread::GetMessageLoopProxyForThread(
        content::BrowserThread::IO);
    file_task_runner_ = content::BrowserThread::GetMessageLoopProxyForThread(
        content::BrowserThread::FILE);

    RegisterSyncableFileSystem();
    local_sync_service_ = LocalFileSyncService::CreateForTesting(
        &profile_, in_memory_env_.get());

    scoped_ptr<drive::FakeDriveService> drive_service(
        new drive::FakeDriveService());
    drive_service->Initialize("test@example.com");
    ASSERT_TRUE(drive_service->LoadAccountMetadataForWapi(
        "sync_file_system/account_metadata.json"));
    ASSERT_TRUE(drive_service->LoadResourceListForWapi(
        "gdata/root_feed.json"));

    scoped_ptr<drive::DriveUploaderInterface> uploader(
        new drive::DriveUploader(drive_service.get(),
                                 file_task_runner_.get()));

    remote_sync_service_.reset(new SyncEngine(
        base_dir_.path(),
        file_task_runner_.get(),
        drive_service.PassAs<drive::DriveServiceInterface>(),
        uploader.Pass(),
        NULL, NULL, NULL, in_memory_env_.get()));
    remote_sync_service_->Initialize();

    fake_drive_service_helper_.reset(new FakeDriveServiceHelper(
        fake_drive_service(), drive_uploader(),
        kSyncRootFolderTitle));

    local_sync_service_->SetLocalChangeProcessor(remote_sync_service_.get());
    remote_sync_service_->SetRemoteChangeProcessor(local_sync_service_.get());
  }

  virtual void TearDown() OVERRIDE {
    typedef std::map<std::string, CannedSyncableFileSystem*>::iterator iterator;
    for (iterator itr = file_systems_.begin();
         itr != file_systems_.end(); ++itr) {
      itr->second->TearDown();
      delete itr->second;
    }
    file_systems_.clear();

    fake_drive_service_helper_.reset();
    remote_sync_service_.reset();

    base::RunLoop().RunUntilIdle();
    RevokeSyncableFileSystem();
  }

 protected:
  fileapi::FileSystemURL CreateURL(const std::string& app_id,
                                   const base::FilePath::StringType& path) {
    return CreateURL(app_id, base::FilePath(path));
  }

  fileapi::FileSystemURL CreateURL(const std::string& app_id,
                                   const base::FilePath& path) {
    GURL origin = extensions::Extension::GetBaseURLFromExtensionId(app_id);
    return CreateSyncableFileSystemURL(origin, path);
  }

  bool GetAppRootFolderID(const std::string& app_id,
                          std::string* folder_id) {
    FileTracker tracker;
    if (!metadata_database()->FindAppRootTracker(app_id, &tracker))
      return false;
    *folder_id = tracker.file_id();
    return true;
  }

  bool GetFileIDByPath(const std::string& app_id,
                       const base::FilePath::StringType& path,
                       std::string* file_id) {
    return GetFileIDByPath(app_id, base::FilePath(path), file_id);
  }

  bool GetFileIDByPath(const std::string& app_id,
                       const base::FilePath& path,
                       std::string* file_id) {
    FileTracker tracker;
    base::FilePath result_path;
    if (!metadata_database()->FindNearestActiveAncestor(
            app_id, path, &tracker, &result_path) ||
        path != result_path)
      return false;
    *file_id = tracker.file_id();
    return true;
  }

  SyncStatusCode RegisterApp(const std::string& app_id) {
    GURL origin = extensions::Extension::GetBaseURLFromExtensionId(app_id);
    if (!ContainsKey(file_systems_, app_id)) {
      CannedSyncableFileSystem* file_system = new CannedSyncableFileSystem(
          origin, in_memory_env_.get(),
          io_task_runner_.get(), file_task_runner_.get());
      file_system->SetUp();

      SyncStatusCode status = SYNC_STATUS_UNKNOWN;
      local_sync_service_->MaybeInitializeFileSystemContext(
          origin, file_system->file_system_context(),
          CreateResultReceiver(&status));
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(SYNC_STATUS_OK, status);

      file_system->backend()->sync_context()->
          set_mock_notify_changes_duration_in_sec(0);

      EXPECT_EQ(base::File::FILE_OK, file_system->OpenFileSystem());
      file_systems_[app_id] = file_system;
    }

    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    remote_sync_service_->RegisterOrigin(origin, CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  void AddLocalFolder(const std::string& app_id,
                      const base::FilePath::StringType& path) {
    ASSERT_TRUE(ContainsKey(file_systems_, app_id));
    EXPECT_EQ(base::File::FILE_OK,
              file_systems_[app_id]->CreateDirectory(
                  CreateURL(app_id, path)));
  }

  void AddOrUpdateLocalFile(const std::string& app_id,
                            const base::FilePath::StringType& path,
                            const std::string& content) {
    fileapi::FileSystemURL url(CreateURL(app_id, path));
    ASSERT_TRUE(ContainsKey(file_systems_, app_id));
    EXPECT_EQ(base::File::FILE_OK, file_systems_[app_id]->CreateFile(url));
    int64 bytes_written = file_systems_[app_id]->WriteString(url, content);
    EXPECT_EQ(static_cast<int64>(content.size()), bytes_written);
    base::RunLoop().RunUntilIdle();
  }

  void UpdateLocalFile(const std::string& app_id,
                       const base::FilePath::StringType& path,
                       const std::string& content) {
    ASSERT_TRUE(ContainsKey(file_systems_, app_id));
    int64 bytes_written = file_systems_[app_id]->WriteString(
        CreateURL(app_id, path), content);
    EXPECT_EQ(static_cast<int64>(content.size()), bytes_written);
    base::RunLoop().RunUntilIdle();
  }

  void RemoveLocal(const std::string& app_id,
                   const base::FilePath::StringType& path) {
    ASSERT_TRUE(ContainsKey(file_systems_, app_id));
    EXPECT_EQ(base::File::FILE_OK,
              file_systems_[app_id]->Remove(
                  CreateURL(app_id, path),
                  true /* recursive */));
    base::RunLoop().RunUntilIdle();
  }

  SyncStatusCode ProcessLocalChange() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    fileapi::FileSystemURL url;
    local_sync_service_->ProcessLocalChange(
        CreateResultReceiver(&status, &url));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  SyncStatusCode ProcessRemoteChange() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    fileapi::FileSystemURL url;
    remote_sync_service_->ProcessRemoteChange(
        CreateResultReceiver(&status, &url));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  void FetchRemoteChanges() {
    remote_sync_service_->OnNotificationReceived();
    base::RunLoop().RunUntilIdle();
  }

  SyncStatusCode ProcessChangesUntilDone() {
    SyncStatusCode local_sync_status;
    SyncStatusCode remote_sync_status;
    do {
      FetchRemoteChanges();

      local_sync_status = ProcessLocalChange();
      if (local_sync_status != SYNC_STATUS_OK &&
          local_sync_status != SYNC_STATUS_NO_CHANGE_TO_SYNC)
        return local_sync_status;

      remote_sync_status = ProcessRemoteChange();
      if (remote_sync_status != SYNC_STATUS_OK &&
          remote_sync_status != SYNC_STATUS_NO_CHANGE_TO_SYNC)
        return remote_sync_status;
    } while (local_sync_status != SYNC_STATUS_NO_CHANGE_TO_SYNC ||
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
    google_apis::GDataErrorCode error =
        fake_drive_service_helper_->GetSyncRootFolderID(&sync_root_folder_id);
    if (sync_root_folder_id.empty()) {
      EXPECT_EQ(google_apis::HTTP_NOT_FOUND, error);
      EXPECT_TRUE(file_systems_.empty());
      return;
    }
    EXPECT_EQ(google_apis::HTTP_SUCCESS, error);

    ScopedVector<google_apis::ResourceEntry> remote_entries;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper_->ListFilesInFolder(
                  sync_root_folder_id, &remote_entries));
    std::map<std::string, const google_apis::ResourceEntry*> app_root_by_title;
    for (ScopedVector<google_apis::ResourceEntry>::iterator itr =
             remote_entries.begin();
         itr != remote_entries.end();
         ++itr) {
      const google_apis::ResourceEntry& remote_entry = **itr;
      EXPECT_FALSE(ContainsKey(app_root_by_title, remote_entry.title()));
      app_root_by_title[remote_entry.title()] = *itr;
    }

    for (std::map<std::string, CannedSyncableFileSystem*>::const_iterator itr =
             file_systems_.begin();
         itr != file_systems_.end(); ++itr) {
      const std::string& app_id = itr->first;
      SCOPED_TRACE(testing::Message() << "Verifying app: " << app_id);
      CannedSyncableFileSystem* file_system = itr->second;
      ASSERT_TRUE(ContainsKey(app_root_by_title, app_id));
      VerifyConsistencyForFolder(
          app_id, base::FilePath(),
          app_root_by_title[app_id]->resource_id(),
          file_system);
    }
  }

  void VerifyConsistencyForFolder(const std::string& app_id,
                                  const base::FilePath& path,
                                  const std::string& folder_id,
                                  CannedSyncableFileSystem* file_system) {
    SCOPED_TRACE(testing::Message() << "Verifying folder: " << path.value());

    ScopedVector<google_apis::ResourceEntry> remote_entries;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper_->ListFilesInFolder(
                  folder_id, &remote_entries));
    std::map<std::string, const google_apis::ResourceEntry*>
        remote_entry_by_title;
    for (ScopedVector<google_apis::ResourceEntry>::iterator itr =
             remote_entries.begin();
         itr != remote_entries.end();
         ++itr) {
      const google_apis::ResourceEntry& remote_entry = **itr;
      EXPECT_FALSE(ContainsKey(remote_entry_by_title, remote_entry.title()));
      remote_entry_by_title[remote_entry.title()] = *itr;
    }

    fileapi::FileSystemURL url(CreateURL(app_id, path));
    FileEntryList local_entries;
    EXPECT_EQ(base::File::FILE_OK,
              file_system->ReadDirectory(url, &local_entries));
    for (FileEntryList::iterator itr = local_entries.begin();
         itr != local_entries.end();
         ++itr) {
      const fileapi::DirectoryEntry& local_entry = *itr;
      fileapi::FileSystemURL entry_url(
          CreateURL(app_id, path.Append(local_entry.name)));
      std::string title = entry_url.path().AsUTF8Unsafe();
      ASSERT_TRUE(ContainsKey(remote_entry_by_title, title));
      const google_apis::ResourceEntry& remote_entry =
          *remote_entry_by_title[title];
      if (local_entry.is_directory) {
        ASSERT_TRUE(remote_entry.is_folder());
        VerifyConsistencyForFolder(app_id, entry_url.path(),
                                   remote_entry.resource_id(),
                                   file_system);
      } else {
        ASSERT_TRUE(remote_entry.is_file());
        VerifyConsistencyForFile(app_id, entry_url.path(),
                                 remote_entry.resource_id(),
                                 file_system);
      }
      remote_entry_by_title.erase(title);
    }

    EXPECT_TRUE(remote_entry_by_title.empty());
  }

  void VerifyConsistencyForFile(const std::string& app_id,
                                const base::FilePath& path,
                                const std::string& file_id,
                                CannedSyncableFileSystem* file_system) {
    fileapi::FileSystemURL url(CreateURL(app_id, path));
    std::string file_content;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper_->ReadFile(file_id, &file_content));
    EXPECT_EQ(base::File::FILE_OK,
              file_system->VerifyFile(url, file_content));
  }

  size_t CountApp() {
    return file_systems_.size();
  }

  size_t CountLocalFile(const std::string& app_id) {
    if (!ContainsKey(file_systems_, app_id))
      return 0;

    CannedSyncableFileSystem* file_system = file_systems_[app_id];
    std::stack<base::FilePath> folders;
    folders.push(base::FilePath()); // root folder

    size_t result = 1;
    while (!folders.empty()) {
      fileapi::FileSystemURL url(CreateURL(app_id, folders.top()));
      folders.pop();

      FileEntryList entries;
      EXPECT_EQ(base::File::FILE_OK,
                file_system->ReadDirectory(url, &entries));
      for (FileEntryList::iterator itr = entries.begin();
           itr != entries.end(); ++itr) {
        ++result;
        if (itr->is_directory)
          folders.push(url.path().Append(itr->name));
      }
    }

    return result;
  }

  void VerifyLocalFile(const std::string& app_id,
                       const base::FilePath& path,
                       const std::string& content) {
    ASSERT_TRUE(ContainsKey(file_systems_, app_id));
    EXPECT_EQ(base::File::FILE_OK,
              file_systems_[app_id]->VerifyFile(
                  CreateURL(app_id, path), content));
  }

  size_t CountMetadata() {
    return metadata_database()->file_by_id_.size();
  }

  size_t CountTracker() {
    return metadata_database()->tracker_by_id_.size();
  }

  drive::FakeDriveService* fake_drive_service() {
    return static_cast<drive::FakeDriveService*>(
        remote_sync_service_->GetDriveService());
  }

  drive::DriveUploaderInterface* drive_uploader() {
    return remote_sync_service_->GetDriveUploader();
  }

  FakeDriveServiceHelper* fake_drive_service_helper() {
    return fake_drive_service_helper_.get();
  }

  MetadataDatabase* metadata_database() {
    return remote_sync_service_->GetMetadataDatabase();
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  base::ScopedTempDir base_dir_;
  scoped_ptr<leveldb::Env> in_memory_env_;
  TestingProfile profile_;

  scoped_ptr<SyncEngine> remote_sync_service_;
  scoped_ptr<LocalFileSyncService> local_sync_service_;

  scoped_ptr<FakeDriveServiceHelper> fake_drive_service_helper_;
  std::map<std::string, CannedSyncableFileSystem*> file_systems_;


  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DriveBackendSyncTest);
};

TEST_F(DriveBackendSyncTest, LocalToRemoteBasicTest) {
  std::string app_id = "example";

  RegisterApp(app_id);
  AddOrUpdateLocalFile(app_id, FPL("file"), "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(2u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, base::FilePath(FPL("file")), "abcde");

  EXPECT_EQ(3u, CountMetadata());
  EXPECT_EQ(3u, CountTracker());
}

TEST_F(DriveBackendSyncTest, RemoteToLocalBasicTest) {
  std::string app_id = "example";
  RegisterApp(app_id);

  std::string app_root_folder_id;
  EXPECT_TRUE(GetAppRootFolderID(app_id, &app_root_folder_id));

  std::string file_id;
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->AddFile(
                app_root_folder_id, "file", "abcde", &file_id));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(2u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, base::FilePath(FPL("file")), "abcde");

  EXPECT_EQ(3u, CountMetadata());
  EXPECT_EQ(3u, CountTracker());
}

TEST_F(DriveBackendSyncTest, LocalFileUpdateTest) {
  std::string app_id = "example";
  const base::FilePath::StringType kPath(FPL("file"));

  RegisterApp(app_id);
  AddOrUpdateLocalFile(app_id, kPath, "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  UpdateLocalFile(app_id, kPath, "1234567890");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(2u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, base::FilePath(FPL("file")), "1234567890");

  EXPECT_EQ(3u, CountMetadata());
  EXPECT_EQ(3u, CountTracker());
}

TEST_F(DriveBackendSyncTest, RemoteFileUpdateTest) {
  std::string app_id = "example";

  RegisterApp(app_id);
  std::string remote_file_id;
  std::string app_root_folder_id;
  EXPECT_TRUE(GetAppRootFolderID(app_id, &app_root_folder_id));
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->AddFile(
                app_root_folder_id, "file", "abcde", &remote_file_id));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->UpdateFile(
                remote_file_id, "1234567890"));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(2u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, base::FilePath(FPL("file")), "1234567890");

  EXPECT_EQ(3u, CountMetadata());
  EXPECT_EQ(3u, CountTracker());
}

TEST_F(DriveBackendSyncTest, LocalFileDeletionTest) {
  std::string app_id = "example";
  const base::FilePath::StringType path(FPL("file"));

  RegisterApp(app_id);
  AddOrUpdateLocalFile(app_id, path, "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  RemoveLocal(app_id, path);

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(1u, CountLocalFile(app_id));

  EXPECT_EQ(2u, CountMetadata());
  EXPECT_EQ(2u, CountTracker());
}

TEST_F(DriveBackendSyncTest, RemoteFileDeletionTest) {
  std::string app_id = "example";
  const base::FilePath::StringType path(FPL("file"));

  RegisterApp(app_id);
  AddOrUpdateLocalFile(app_id, path, "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  std::string file_id;
  EXPECT_TRUE(GetFileIDByPath(app_id, base::FilePath(path), &file_id));
  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(file_id));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(1u, CountLocalFile(app_id));

  EXPECT_EQ(2u, CountMetadata());
  EXPECT_EQ(2u, CountTracker());
}

TEST_F(DriveBackendSyncTest, RemoteRenameTest) {
  std::string app_id = "example";
  const base::FilePath::StringType path(FPL("file"));

  RegisterApp(app_id);
  AddOrUpdateLocalFile(app_id, path, "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  std::string file_id;
  EXPECT_TRUE(GetFileIDByPath(app_id, base::FilePath(path), &file_id));
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->RenameResource(
                file_id, "renamed_file"));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(2u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, base::FilePath(FPL("renamed_file")), "abcde");

  EXPECT_EQ(3u, CountMetadata());
  EXPECT_EQ(3u, CountTracker());
}

TEST_F(DriveBackendSyncTest, RemoteRenameAndRevertTest) {
  std::string app_id = "example";
  const base::FilePath::StringType path(FPL("file"));

  RegisterApp(app_id);
  AddOrUpdateLocalFile(app_id, path, "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  std::string file_id;
  EXPECT_TRUE(GetFileIDByPath(app_id, base::FilePath(path), &file_id));
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->RenameResource(
                file_id, "renamed_file"));

  FetchRemoteChanges();

  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->RenameResource(
                file_id, base::FilePath(path).AsUTF8Unsafe()));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(2u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, base::FilePath(FPL("file")), "abcde");

  EXPECT_EQ(3u, CountMetadata());
  EXPECT_EQ(3u, CountTracker());
}

}  // namespace drive_backend
}  // namespace sync_file_system
