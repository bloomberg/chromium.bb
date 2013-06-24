// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_service.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/sync_file_system/drive/api_util.h"
#include "chrome/browser/sync_file_system/drive_file_sync_util.h"
#include "chrome/browser/sync_file_system/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/fake_remote_change_processor.h"
#include "chrome/browser/sync_file_system/local_file_sync_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/syncable/canned_syncable_file_system.h"
#include "webkit/browser/fileapi/syncable/local_file_sync_context.h"
#include "webkit/browser/fileapi/syncable/syncable_file_system_util.h"

#define FPL(path) FILE_PATH_LITERAL(path)

using content::BrowserThread;

namespace sync_file_system {

namespace {

void SyncResultCallback(bool* done,
                        SyncStatusCode* status_out,
                        fileapi::FileSystemURL* url_out,
                        SyncStatusCode status,
                        const fileapi::FileSystemURL& url) {
  EXPECT_FALSE(*done);
  *status_out = status;
  *url_out = url;
  *done = true;
}

void ResourceEntryResultCallback(
    bool* done,
    google_apis::GDataErrorCode* error_out,
    scoped_ptr<google_apis::ResourceEntry>* entry_out,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  EXPECT_FALSE(*done);
  *error_out = error;
  *entry_out = entry.Pass();
  *done = true;
}

void UploadResultCallback(bool* done,
                          google_apis::GDataErrorCode* error_out,
                          scoped_ptr<google_apis::ResourceEntry>* entry_out,
                          google_apis::GDataErrorCode error,
                          const GURL& upload_location,
                          scoped_ptr<google_apis::ResourceEntry> entry) {
  EXPECT_FALSE(*done);
  *error_out = error;
  *entry_out = entry.Pass();
  *done = true;
}

void ResourceListResultCallback(bool* done,
                                google_apis::GDataErrorCode* error_out,
                                scoped_ptr<google_apis::ResourceList>* list_out,
                                google_apis::GDataErrorCode error,
                                scoped_ptr<google_apis::ResourceList> list) {
  EXPECT_FALSE(*done);
  *error_out = error;
  *list_out = list.Pass();
  *done = true;
}

void SyncStatusResultCallback(bool* done,
                              SyncStatusCode* status_out,
                              SyncStatusCode status) {
  EXPECT_FALSE(*done);
  *status_out = status;
  *done = true;
}

void GDataErrorResultCallback(bool* done,
                              google_apis::GDataErrorCode* error_out,
                              google_apis::GDataErrorCode error) {
  EXPECT_FALSE(*done);
  *error_out = error;
  *done = true;
}

void DownloadResultCallback(bool* done,
                            google_apis::GDataErrorCode* error_out,
                            google_apis::GDataErrorCode error,
                            const base::FilePath& local_file) {
  EXPECT_FALSE(*done);
  *error_out = error;
  *done = true;
}

void DatabaseInitResultCallback(bool* done,
                                SyncStatusCode* status_out,
                                bool* created_out,
                                SyncStatusCode status,
                                bool created) {
  EXPECT_FALSE(*done);
  *status_out = status;
  *created_out = created;
  *done = true;
}

}  // namespace

class DriveFileSyncServiceSyncTest : public testing::Test {
 public:
  DriveFileSyncServiceSyncTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}

  virtual ~DriveFileSyncServiceSyncTest() {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(base_dir_.CreateUniqueTempDir());
    temp_dir_ = base_dir_.path().Append(FPL("tmp"));
    ASSERT_TRUE(file_util::CreateDirectory(temp_dir_));

    // TODO(tzik): Set up TestExtensionSystem to support simulated relaunch.

    RegisterSyncableFileSystem();
    local_sync_service_.reset(new LocalFileSyncService(&profile_));

    fake_drive_service_ = new ::drive::FakeDriveService();
    fake_drive_service_->Initialize(&profile_);
    ASSERT_TRUE(fake_drive_service_->LoadAccountMetadataForWapi(
        "chromeos/sync_file_system/account_metadata.json"));
    ASSERT_TRUE(fake_drive_service_->LoadResourceListForWapi(
        "chromeos/gdata/root_feed.json"));

    drive_uploader_ = new ::drive::DriveUploader(
        fake_drive_service_, base::MessageLoopProxy::current());

    bool done = false;
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    bool created = false;
    scoped_ptr<DriveMetadataStore> metadata_store(
        new DriveMetadataStore(base_dir_.path(),
                               base::MessageLoopProxy::current()));
    metadata_store->Initialize(
        base::Bind(&DatabaseInitResultCallback, &done, &status, &created));
    FlushMessageLoop();
    EXPECT_TRUE(done);
    EXPECT_EQ(SYNC_STATUS_OK, status);
    EXPECT_TRUE(created);

    scoped_ptr<drive::APIUtil> api_util(
        drive::APIUtil::CreateForTesting(
            &profile_,
            scoped_ptr< ::drive::DriveServiceInterface>(fake_drive_service_),
            scoped_ptr< ::drive::DriveUploaderInterface>(drive_uploader_)));

    remote_sync_service_ = DriveFileSyncService::CreateForTesting(
        &profile_, base_dir_.path(),
        api_util.PassAs<drive::APIUtilInterface>(),
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
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));

      bool done = false;
      SyncStatusCode status = SYNC_STATUS_UNKNOWN;
      file_system->SetUp();
      local_sync_service_->MaybeInitializeFileSystemContext(
          origin, file_system->file_system_context(),
          base::Bind(&SyncStatusResultCallback, &done, &status));
      FlushMessageLoop();
      EXPECT_TRUE(done);
      EXPECT_EQ(SYNC_STATUS_OK, status);

      file_system->file_system_context()->sync_context()->
          set_mock_notify_changes_duration_in_sec(0);

      EXPECT_EQ(base::PLATFORM_FILE_OK, file_system->OpenFileSystem());
      file_systems_[origin] = file_system;
    }

    bool done = false;
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    remote_sync_service_->RegisterOriginForTrackingChanges(
        origin, base::Bind(&SyncStatusResultCallback, &done, &status));
    FlushMessageLoop();
    EXPECT_TRUE(done);
    EXPECT_EQ(SYNC_STATUS_OK, status);
  }

  void AddLocalFolder(const GURL& origin,
                      const base::FilePath& path) {
    ASSERT_TRUE(ContainsKey(file_systems_, origin));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              file_systems_[origin]->CreateDirectory(
                  CreateSyncableFileSystemURL(origin, path)));
  }

  void AddOrUpdateLocalFile(const GURL& origin,
                            const base::FilePath& path,
                            const std::string& content) {
    fileapi::FileSystemURL url(CreateSyncableFileSystemURL(origin, path));
    ASSERT_TRUE(ContainsKey(file_systems_, origin));
    EXPECT_EQ(base::PLATFORM_FILE_OK, file_systems_[origin]->CreateFile(url));
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
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              file_systems_[origin]->Remove(
                  CreateSyncableFileSystemURL(origin, path),
                  true /* recursive */));
    FlushMessageLoop();
  }

  std::string AddRemoteFolder(const std::string& parent_folder_id,
                              const std::string& title) {
    bool done = false;
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    scoped_ptr<google_apis::ResourceEntry> folder;
    fake_drive_service_->AddNewDirectory(
        parent_folder_id, title,
        base::Bind(&ResourceEntryResultCallback, &done, &error, &folder));
    FlushMessageLoop();
    EXPECT_TRUE(done);
    EXPECT_TRUE(error == google_apis::HTTP_SUCCESS ||
                error == google_apis::HTTP_CREATED);
    return folder->resource_id();
  }

  std::string AddOrphanedRemoteFolder(const std::string& title) {
    std::string root_folder_id = fake_drive_service_->GetRootResourceId();
    std::string folder_id = AddRemoteFolder(root_folder_id, title);

    bool done = false;
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    fake_drive_service_->RemoveResourceFromDirectory(
        root_folder_id, folder_id,
        base::Bind(&GDataErrorResultCallback, &done, &error));
    FlushMessageLoop();
    EXPECT_TRUE(done);
    EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
    return folder_id;
  }

  std::string AddRemoteFile(const std::string& parent_folder_id,
                            const std::string& title,
                            const std::string& content) {
    base::FilePath temp_file = WriteToTempFile(content);

    bool done = false;
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    scoped_ptr<google_apis::ResourceEntry> file;
    drive_uploader_->UploadNewFile(
        parent_folder_id, temp_file, title,
        "application/octet-stream",
        base::Bind(&UploadResultCallback, &done, &error, &file),
        google_apis::ProgressCallback());
    FlushMessageLoop();
    EXPECT_TRUE(done);
    EXPECT_TRUE(error == google_apis::HTTP_SUCCESS ||
                error == google_apis::HTTP_CREATED );
    return file->resource_id();
  }

  void UpdateRemoteFile(const std::string& file_id,
                        const std::string& content) {
    base::FilePath temp_file = WriteToTempFile(content);

    bool done = false;
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    scoped_ptr<google_apis::ResourceEntry> file;
    drive_uploader_->UploadExistingFile(
        file_id, temp_file,
        "application/octet-stream",
        std::string(),  // etag
        base::Bind(&UploadResultCallback, &done, &error, &file),
        google_apis::ProgressCallback());

    FlushMessageLoop();
    EXPECT_TRUE(done);
    EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  }

  void RemoveRemote(const std::string& file_id) {
    bool done = false;
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    fake_drive_service_->DeleteResource(
        file_id,
        std::string(), // etag
        base::Bind(&GDataErrorResultCallback, &done, &error));
    FlushMessageLoop();
    EXPECT_TRUE(done);
    EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  }

  SyncStatusCode ProcessLocalChange() {
    bool done = false;
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    fileapi::FileSystemURL url;
    local_sync_service_->ProcessLocalChange(
        base::Bind(&SyncResultCallback, &done, &status, &url));
    FlushMessageLoop();
    EXPECT_TRUE(done);
    if (status != SYNC_STATUS_NO_CHANGE_TO_SYNC)
      local_sync_service_->ClearSyncFlagForURL(url);
    return status;
  }

  SyncStatusCode ProcessRemoteChange() {
    bool done = false;
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    fileapi::FileSystemURL url;
    remote_sync_service_->ProcessRemoteChange(
        base::Bind(&SyncResultCallback, &done, &status, &url));
    FlushMessageLoop();
    EXPECT_TRUE(done);
    if (status != SYNC_STATUS_NO_CHANGE_TO_SYNC)
      local_sync_service_->ClearSyncFlagForURL(url);
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

  void VerifyConsistency() {
    std::string sync_root_folder_id = GetSyncRootFolderID();
    if (sync_root_folder_id.empty()) {
      EXPECT_TRUE(file_systems_.empty());
      return;
    }

    ScopedVector<google_apis::ResourceEntry> remote_entries =
        ListFilesInRemoteFolder(sync_root_folder_id);
    std::map<std::string, const google_apis::ResourceEntry*>
        origin_root_by_title;
    for (ScopedVector<google_apis::ResourceEntry>::iterator itr =
             remote_entries.begin();
         itr != remote_entries.end();
         ++itr) {
      const google_apis::ResourceEntry& remote_entry = **itr;
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
    std::string sync_root_folder_id = GetSyncRootFolderID();
    ASSERT_FALSE(sync_root_folder_id.empty());

    ScopedVector<google_apis::ResourceEntry> origin_folder =
        SearchByTitle(sync_root_folder_id, origin.host());
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

    ScopedVector<google_apis::ResourceEntry> remote_entries =
        ListFilesInRemoteFolder(folder_id);
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

    fileapi::FileSystemURL url(CreateSyncableFileSystemURL(origin, path));
    CannedSyncableFileSystem::FileEntryList local_entries;
    EXPECT_EQ(base::PLATFORM_FILE_OK,
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
      const google_apis::ResourceEntry& remote_entry =
          *remote_entry_by_title[title];
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
    // TODO(tzik): Verify file content after FakeDriveService supports
    // file contents upload.

    scoped_ptr<google_apis::ResourceEntry> entry = GetRemoteFile(file_id);

    fileapi::FileSystemURL url(CreateSyncableFileSystemURL(origin, path));
    base::PlatformFileInfo info;
    base::FilePath platform_path;
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              file_system->GetMetadataAndPlatformPath(
                  url, &info, &platform_path));

    EXPECT_EQ(info.size, entry->file_size());
  }

  std::string GetSyncRootFolderID() {
    bool done = false;
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    scoped_ptr<google_apis::ResourceList> resource_list;
    fake_drive_service_->SearchByTitle(
        drive::APIUtil::GetSyncRootDirectoryName(), std::string(),
        base::Bind(&ResourceListResultCallback, &done, &error, &resource_list));
    FlushMessageLoop();
    EXPECT_TRUE(done);
    EXPECT_EQ(google_apis::HTTP_SUCCESS, error);

    const ScopedVector<google_apis::ResourceEntry>& entries =
        resource_list->entries();
    for (ScopedVector<google_apis::ResourceEntry>::const_iterator itr =
             entries.begin(); itr != entries.end(); ++itr) {
      const google_apis::ResourceEntry& entry = **itr;
      if (!entry.GetLinkByType(google_apis::Link::LINK_PARENT))
        return entry.resource_id();
    }
    return std::string();
  }

  ScopedVector<google_apis::ResourceEntry> ListFilesInRemoteFolder(
      const std::string& folder_id) {
    bool done = false;
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    scoped_ptr<google_apis::ResourceList> list;
    fake_drive_service_->GetResourceListInDirectory(
        folder_id,
        base::Bind(&ResourceListResultCallback, &done, &error, &list));
    FlushMessageLoop();
    EXPECT_TRUE(done);
    EXPECT_EQ(google_apis::HTTP_SUCCESS, error);

    return CompleteListing(list.Pass());
  }

  ScopedVector<google_apis::ResourceEntry> SearchByTitle(
      const std::string& folder_id,
      const std::string& title) {
    bool done = false;
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    scoped_ptr<google_apis::ResourceList> list;
    fake_drive_service_->SearchByTitle(
        title, folder_id,
        base::Bind(&ResourceListResultCallback, &done, &error, &list));
    FlushMessageLoop();
    EXPECT_TRUE(done);
    EXPECT_EQ(google_apis::HTTP_SUCCESS, error);

    return CompleteListing(list.Pass());
  }

  ScopedVector<google_apis::ResourceEntry> CompleteListing(
      scoped_ptr<google_apis::ResourceList> list) {
    ScopedVector<google_apis::ResourceEntry> result;
    while (true) {
      result.reserve(result.size() + list->entries().size());
      for (ScopedVector<google_apis::ResourceEntry>::iterator itr =
               list->mutable_entries()->begin();
           itr != list->mutable_entries()->end(); ++itr) {
        result.push_back(*itr);
        *itr = NULL;
      }

      GURL next_feed;
      if (!list->GetNextFeedURL(&next_feed))
        return result.Pass();

      bool done = false;
      google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
      list.reset();
      fake_drive_service_->ContinueGetResourceList(
          next_feed,
          base::Bind(&ResourceListResultCallback, &done, &error, &list));
      FlushMessageLoop();
      EXPECT_TRUE(done);
      EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
    }
  }

  scoped_ptr<google_apis::ResourceEntry> GetRemoteFile(
      const std::string& file_id) {
    bool done = false;
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

    scoped_ptr<google_apis::ResourceEntry> file;
    fake_drive_service_->GetResourceEntry(
        file_id,
        base::Bind(&ResourceEntryResultCallback, &done, &error, &file));
    FlushMessageLoop();
    return file.Pass();
  }

  base::FilePath WriteToTempFile(const std::string& content) {
    base::FilePath temp_file;
    EXPECT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_, &temp_file));
    EXPECT_EQ(static_cast<int>(content.size()),
              file_util::WriteFile(temp_file, content.data(), content.size()));
    return temp_file;
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

  content::TestBrowserThreadBundle thread_bundle_;

  TestingProfile profile_;
  base::ScopedTempDir base_dir_;
  base::FilePath temp_dir_;

  ::drive::FakeDriveService* fake_drive_service_;
  ::drive::DriveUploader* drive_uploader_;
  std::map<GURL, CannedSyncableFileSystem*> file_systems_;

  scoped_ptr<DriveFileSyncService> remote_sync_service_;
  scoped_ptr<LocalFileSyncService> local_sync_service_;

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

  std::string sync_root_folder_id =
      AddOrphanedRemoteFolder(drive::APIUtil::GetSyncRootDirectoryName());
  std::string origin_root_folder_id =
      AddRemoteFolder(sync_root_folder_id, kOrigin.host());

  RegisterOrigin(kOrigin);
  AddRemoteFile(origin_root_folder_id, "file", "abcde");

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

  std::string sync_root_folder_id =
      AddOrphanedRemoteFolder(drive::APIUtil::GetSyncRootDirectoryName());
  std::string origin_root_folder_id =
      AddRemoteFolder(sync_root_folder_id, kOrigin.host());
  std::string remote_file_id =
      AddRemoteFile(origin_root_folder_id, kTitle, "abcde");

  RegisterOrigin(kOrigin);
  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistencyForOrigin(kOrigin);

  UpdateRemoteFile(remote_file_id, "1234567890");

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

}  // namespace sync_file_system
