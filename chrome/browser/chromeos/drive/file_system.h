// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/drive/change_list_loader_observer.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class PrefChangeRegistrar;
class Profile;

namespace base {
struct PlatformFileInfo;
class SequencedTaskRunner;
}

namespace google_apis {
class AboutResource;
class ResourceEntry;
}

namespace drive {

class DriveServiceInterface;
class FileCacheEntry;
class FileSystemObserver;
class JobScheduler;

namespace internal {
class ChangeListLoader;
class ResourceMetadata;
class SyncClient;
}  // namespace internal

namespace file_system {
class CopyOperation;
class CreateDirectoryOperation;
class CreateFileOperation;
class DownloadOperation;
class MoveOperation;
class OperationObserver;
class RemoveOperation;
class SearchOperation;
class TouchOperation;
class UpdateOperation;
}  // namespace file_system

// The production implementation of FileSystemInterface.
class FileSystem : public FileSystemInterface,
                   public internal::ChangeListLoaderObserver,
                   public file_system::OperationObserver {
 public:
  FileSystem(Profile* profile,
             internal::FileCache* cache,
             DriveServiceInterface* drive_service,
             JobScheduler* scheduler,
             internal::ResourceMetadata* resource_metadata,
             base::SequencedTaskRunner* blocking_task_runner,
             const base::FilePath& temporary_file_directory);
  virtual ~FileSystem();

  // FileSystemInterface overrides.
  virtual void Initialize() OVERRIDE;
  virtual void AddObserver(FileSystemObserver* observer) OVERRIDE;
  virtual void RemoveObserver(FileSystemObserver* observer) OVERRIDE;
  virtual void CheckForUpdates() OVERRIDE;
  virtual void GetResourceEntryById(
      const std::string& resource_id,
      const GetResourceEntryCallback& callback) OVERRIDE;
  virtual void Search(const std::string& search_query,
                      const GURL& next_url,
                      const SearchCallback& callback) OVERRIDE;
  virtual void SearchMetadata(const std::string& query,
                              int options,
                              int at_most_num_matches,
                              const SearchMetadataCallback& callback) OVERRIDE;
  virtual void TransferFileFromRemoteToLocal(
      const base::FilePath& remote_src_file_path,
      const base::FilePath& local_dest_file_path,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void TransferFileFromLocalToRemote(
      const base::FilePath& local_src_file_path,
      const base::FilePath& remote_dest_file_path,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void OpenFile(const base::FilePath& file_path,
                        const OpenFileCallback& callback) OVERRIDE;
  virtual void CloseFile(const base::FilePath& file_path,
                         const FileOperationCallback& callback) OVERRIDE;
  virtual void Copy(const base::FilePath& src_file_path,
                    const base::FilePath& dest_file_path,
                    const FileOperationCallback& callback) OVERRIDE;
  virtual void Move(const base::FilePath& src_file_path,
                    const base::FilePath& dest_file_path,
                    const FileOperationCallback& callback) OVERRIDE;
  virtual void Remove(const base::FilePath& file_path,
                      bool is_recursive,
                      const FileOperationCallback& callback) OVERRIDE;
  virtual void CreateDirectory(const base::FilePath& directory_path,
                               bool is_exclusive,
                               bool is_recursive,
                               const FileOperationCallback& callback) OVERRIDE;
  virtual void CreateFile(const base::FilePath& file_path,
                          bool is_exclusive,
                          const FileOperationCallback& callback) OVERRIDE;
  virtual void TouchFile(const base::FilePath& file_path,
                         const base::Time& last_access_time,
                         const base::Time& last_modified_time,
                         const FileOperationCallback& callback) OVERRIDE;
  virtual void Pin(const base::FilePath& file_path,
                   const FileOperationCallback& callback) OVERRIDE;
  virtual void Unpin(const base::FilePath& file_path,
                     const FileOperationCallback& callback) OVERRIDE;
  virtual void GetFileByPath(const base::FilePath& file_path,
                             const GetFileCallback& callback) OVERRIDE;
  virtual void GetFileByResourceId(
      const std::string& resource_id,
      const ClientContext& context,
      const GetFileCallback& get_file_callback,
      const google_apis::GetContentCallback& get_content_callback) OVERRIDE;
  virtual void GetFileContentByPath(
      const base::FilePath& file_path,
      const GetFileContentInitializedCallback& initialized_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const FileOperationCallback& completion_callback) OVERRIDE;
  virtual void UpdateFileByResourceId(
      const std::string& resource_id,
      const ClientContext& context,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void GetResourceEntryByPath(
      const base::FilePath& file_path,
      const GetResourceEntryCallback& callback) OVERRIDE;
  virtual void ReadDirectoryByPath(
      const base::FilePath& directory_path,
      const ReadDirectoryWithSettingCallback& callback) OVERRIDE;
  virtual void RefreshDirectory(
      const base::FilePath& directory_path,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void GetAvailableSpace(
      const GetAvailableSpaceCallback& callback) OVERRIDE;
  virtual void GetMetadata(
      const GetFilesystemMetadataCallback& callback) OVERRIDE;
  virtual void MarkCacheFileAsMounted(
      const base::FilePath& drive_file_path,
      const OpenFileCallback& callback) OVERRIDE;
  virtual void MarkCacheFileAsUnmounted(
      const base::FilePath& cache_file_path,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void GetCacheEntryByResourceId(
      const std::string& resource_id,
      const std::string& md5,
      const GetCacheEntryCallback& callback) OVERRIDE;
  virtual void Reload() OVERRIDE;

  // file_system::OperationObserver overrides.
  virtual void OnDirectoryChangedByOperation(
      const base::FilePath& directory_path) OVERRIDE;
  virtual void OnCacheFileUploadNeededByOperation(
      const std::string& resource_id) OVERRIDE;

  // ChangeListLoader::Observer overrides.
  // Used to propagate events from ChangeListLoader.
  virtual void OnDirectoryChanged(
      const base::FilePath& directory_path) OVERRIDE;
  virtual void OnLoadFromServerComplete() OVERRIDE;
  virtual void OnInitialLoadComplete() OVERRIDE;

  // Used in tests to update the file system using the change list loader.
  internal::ChangeListLoader* change_list_loader() {
    return change_list_loader_.get();
  }

  // Used by tests.
  internal::SyncClient* sync_client_for_testing() { return sync_client_.get(); }

 private:
  // Used to implement Reload().
  void ReloadAfterReset(FileError error);

  // Sets up ChangeListLoader.
  void SetupChangeListLoader();

  // Called on preference change.
  void OnDisableDriveHostedFilesChanged();

  // Part of CreateDirectory(). Called after ChangeListLoader::LoadIfNeeded()
  // is called and made sure that the resource metadata is loaded.
  void CreateDirectoryAfterLoad(const base::FilePath& directory_path,
                                bool is_exclusive,
                                bool is_recursive,
                                const FileOperationCallback& callback,
                                FileError load_error);

  // Used to implement Pin().
  void PinAfterGetResourceEntryByPath(const FileOperationCallback& callback,
                                      FileError error,
                                      scoped_ptr<ResourceEntry> entry);
  void FinishPin(const FileOperationCallback& callback,
                 const std::string& resource_id,
                 FileError error);

  // Used to implement Unpin().
  void UnpinAfterGetResourceEntryByPath(const FileOperationCallback& callback,
                                        FileError error,
                                        scoped_ptr<ResourceEntry> entry);
  void FinishUnpin(const FileOperationCallback& callback,
                   const std::string& resource_id,
                   FileError error);

  // Part of OpenFile(). Called after the file downloading is completed.
  void OpenFileAfterFileDownloaded(
      const base::FilePath& file_path,
      const OpenFileCallback& callback,
      FileError error,
      const base::FilePath& local_file_path,
      scoped_ptr<ResourceEntry> entry);

  // Part of OpenFile(). Called after the cache file is marked dirty.
  void OpenFileAfterMarkDirty(
      const std::string& resource_id,
      const std::string& md5,
      const OpenFileCallback& callback,
      FileError error);

  // Invoked at the last step of OpenFile. It removes |file_path| from the
  // current set of opened files if |result| is an error, and then invokes the
  // |callback| function.
  void OnOpenFileFinished(const base::FilePath& file_path,
                          const OpenFileCallback& callback,
                          FileError result,
                          const base::FilePath& cache_file_path);

  // Invoked during the process of CloseFile. What is done here is as follows:
  // 1) Gets resource_id and md5 of the entry at |file_path|.
  // 2) Commits the modification to the cache system.
  // 3) Removes the |file_path| from the remembered set of opened files.
  // 4) Invokes the user-supplied |callback|.
  // |callback| must not be null.
  void CloseFileAfterGetResourceEntry(const base::FilePath& file_path,
                                      const FileOperationCallback& callback,
                                      FileError error,
                                      scoped_ptr<ResourceEntry> entry);

  // Callback for handling about resource fetch.
  void OnGetAboutResource(
      const GetAvailableSpaceCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Part of CheckForUpdates(). Called when
  // ChangeListLoader::CheckForUpdates() is complete.
  void OnUpdateChecked(FileError error);

  // Changes state of hosted documents visibility, triggers directory refresh.
  void SetHideHostedDocuments(bool hide);

  // Initializes preference change observer.
  void InitializePreferenceObserver();

  // Part of GetResourceEntryByPath()
  // 1) Called when ResourceMetadata::GetResourceEntryByPath() is complete.
  //    If succeeded, GetResourceEntryByPath() returns immediately here.
  //    Otherwise, starts loading the file system.
  // 2) Called when LoadIfNeeded() is complete.
  // 3) Called when ResourceMetadata::GetResourceEntryByPath() is complete.
  void GetResourceEntryByPathAfterGetEntry1(
      const base::FilePath& file_path,
      const GetResourceEntryCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> entry);
  void GetResourceEntryByPathAfterLoad(const base::FilePath& file_path,
                                       const GetResourceEntryCallback& callback,
                                       FileError error);
  void GetResourceEntryByPathAfterGetEntry2(
      const GetResourceEntryCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> entry);

  // Loads the entry info of the children of |directory_path| to resource
  // metadata. |callback| must not be null.
  void LoadDirectoryIfNeeded(const base::FilePath& directory_path,
                             const FileOperationCallback& callback);
  void LoadDirectoryIfNeededAfterGetEntry(
      const base::FilePath& directory_path,
      const FileOperationCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> entry);

  // Part of ReadDirectoryByPath()
  // 1) Called when LoadDirectoryIfNeeded() is complete.
  // 2) Called when ResourceMetadata::ReadDirectoryByPath() is complete.
  // |callback| must not be null.
  void ReadDirectoryByPathAfterLoad(
      const base::FilePath& directory_path,
      const ReadDirectoryWithSettingCallback& callback,
      FileError error);
  void ReadDirectoryByPathAfterRead(
      const ReadDirectoryWithSettingCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntryVector> entries);

  // Part of GetResourceEntryById(). Called after
  // ResourceMetadata::GetResourceEntryById() is complete.
  // |callback| must not be null.
  void GetResourceEntryByIdAfterGetEntry(
      const GetResourceEntryCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> entry);

  // Part of RefreshDirectory(). Called after
  // GetResourceEntryByPath() is complete.
  void RefreshDirectoryAfterGetResourceEntry(
      const base::FilePath& directory_path,
      const FileOperationCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> entry);

  // Part of GetEntryByResourceId and GetEntryByPath. Checks whether there is a
  // local dirty cache for the entry, and if there is, replace the
  // PlatformFileInfo part of the |entry| with the locally modified info.
  // |callback| must not be null.
  void CheckLocalModificationAndRun(scoped_ptr<ResourceEntry> entry,
                                    const GetResourceEntryCallback& callback);
  void CheckLocalModificationAndRunAfterGetCacheEntry(
      scoped_ptr<ResourceEntry> entry,
      const GetResourceEntryCallback& callback,
      bool success,
      const FileCacheEntry& cache_entry);
  void CheckLocalModificationAndRunAfterGetCacheFile(
      scoped_ptr<ResourceEntry> entry,
      const GetResourceEntryCallback& callback,
      FileError error,
      const base::FilePath& local_cache_path);
  void CheckLocalModificationAndRunAfterGetFileInfo(
      scoped_ptr<ResourceEntry> entry,
      const GetResourceEntryCallback& callback,
      base::PlatformFileInfo* file_info,
      bool get_file_info_result);

  // Part of MarkCacheFileAsMounted. Called after GetResourceEntryByPath is
  // completed. |callback| must not be null.
  void MarkCacheFileAsMountedAfterGetResourceEntry(
      const OpenFileCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> entry);

  // The profile hosts the FileSystem via DriveIntegrationService.
  Profile* profile_;

  // Sub components owned by DriveIntegrationService.
  internal::FileCache* cache_;
  DriveServiceInterface* drive_service_;
  JobScheduler* scheduler_;
  internal::ResourceMetadata* resource_metadata_;

  // Time of the last update check.
  base::Time last_update_check_time_;

  // Error of the last update check.
  FileError last_update_check_error_;

  // True if hosted documents should be hidden.
  bool hide_hosted_docs_;

  // The set of paths opened by OpenFile but not yet closed by CloseFile.
  std::set<base::FilePath> open_files_;

  scoped_ptr<PrefChangeRegistrar> pref_registrar_;

  scoped_ptr<internal::SyncClient> sync_client_;

  // The loader is used to load the change lists.
  scoped_ptr<internal::ChangeListLoader> change_list_loader_;

  ObserverList<FileSystemObserver> observers_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  base::FilePath temporary_file_directory_;

  // Implementation of each file system operation.
  scoped_ptr<file_system::CopyOperation> copy_operation_;
  scoped_ptr<file_system::CreateDirectoryOperation> create_directory_operation_;
  scoped_ptr<file_system::CreateFileOperation> create_file_operation_;
  scoped_ptr<file_system::MoveOperation> move_operation_;
  scoped_ptr<file_system::RemoveOperation> remove_operation_;
  scoped_ptr<file_system::TouchOperation> touch_operation_;
  scoped_ptr<file_system::DownloadOperation> download_operation_;
  scoped_ptr<file_system::UpdateOperation> update_operation_;
  scoped_ptr<file_system::SearchOperation> search_operation_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileSystem> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystem);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_H_
