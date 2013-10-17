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

class PrefService;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace google_apis {
class AboutResource;
class ResourceEntry;
}  // namespace google_apis

namespace drive {

class DriveServiceInterface;
class FileCacheEntry;
class FileSystemObserver;
class JobScheduler;

namespace internal {
class ChangeListLoader;
class FileCache;
class ResourceMetadata;
class SyncClient;
}  // namespace internal

namespace file_system {
class CopyOperation;
class CreateDirectoryOperation;
class CreateFileOperation;
class DownloadOperation;
class GetFileForSavingOperation;
class MoveOperation;
class OpenFileOperation;
class OperationObserver;
class RemoveOperation;
class SearchOperation;
class TouchOperation;
class TruncateOperation;
class UpdateOperation;
}  // namespace file_system

// The production implementation of FileSystemInterface.
class FileSystem : public FileSystemInterface,
                   public internal::ChangeListLoaderObserver,
                   public file_system::OperationObserver {
 public:
  FileSystem(PrefService* pref_service,
             internal::FileCache* cache,
             DriveServiceInterface* drive_service,
             JobScheduler* scheduler,
             internal::ResourceMetadata* resource_metadata,
             base::SequencedTaskRunner* blocking_task_runner,
             const base::FilePath& temporary_file_directory);
  virtual ~FileSystem();

  // FileSystemInterface overrides.
  virtual void AddObserver(FileSystemObserver* observer) OVERRIDE;
  virtual void RemoveObserver(FileSystemObserver* observer) OVERRIDE;
  virtual void CheckForUpdates() OVERRIDE;
  virtual void Search(const std::string& search_query,
                      const GURL& next_link,
                      const SearchCallback& callback) OVERRIDE;
  virtual void SearchMetadata(const std::string& query,
                              int options,
                              int at_most_num_matches,
                              const SearchMetadataCallback& callback) OVERRIDE;
  virtual void TransferFileFromLocalToRemote(
      const base::FilePath& local_src_file_path,
      const base::FilePath& remote_dest_file_path,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void OpenFile(const base::FilePath& file_path,
                        OpenMode open_mode,
                        const std::string& mime_type,
                        const OpenFileCallback& callback) OVERRIDE;
  virtual void Copy(const base::FilePath& src_file_path,
                    const base::FilePath& dest_file_path,
                    bool preserve_last_modified,
                    const FileOperationCallback& callback) OVERRIDE;
  virtual void Move(const base::FilePath& src_file_path,
                    const base::FilePath& dest_file_path,
                    bool preserve_last_modified,
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
                          const std::string& mime_type,
                          const FileOperationCallback& callback) OVERRIDE;
  virtual void TouchFile(const base::FilePath& file_path,
                         const base::Time& last_access_time,
                         const base::Time& last_modified_time,
                         const FileOperationCallback& callback) OVERRIDE;
  virtual void TruncateFile(const base::FilePath& file_path,
                            int64 length,
                            const FileOperationCallback& callback) OVERRIDE;
  virtual void Pin(const base::FilePath& file_path,
                   const FileOperationCallback& callback) OVERRIDE;
  virtual void Unpin(const base::FilePath& file_path,
                     const FileOperationCallback& callback) OVERRIDE;
  virtual void GetFileByPath(const base::FilePath& file_path,
                             const GetFileCallback& callback) OVERRIDE;
  virtual void GetFileByPathForSaving(const base::FilePath& file_path,
                                      const GetFileCallback& callback) OVERRIDE;
  virtual void GetFileContentByPath(
      const base::FilePath& file_path,
      const GetFileContentInitializedCallback& initialized_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const FileOperationCallback& completion_callback) OVERRIDE;
  virtual void GetResourceEntryByPath(
      const base::FilePath& file_path,
      const GetResourceEntryCallback& callback) OVERRIDE;
  virtual void ReadDirectoryByPath(
      const base::FilePath& directory_path,
      const ReadDirectoryCallback& callback) OVERRIDE;
  virtual void GetAvailableSpace(
      const GetAvailableSpaceCallback& callback) OVERRIDE;
  virtual void GetShareUrl(
      const base::FilePath& file_path,
      const GURL& embed_origin,
      const GetShareUrlCallback& callback) OVERRIDE;
  virtual void GetMetadata(
      const GetFilesystemMetadataCallback& callback) OVERRIDE;
  virtual void MarkCacheFileAsMounted(
      const base::FilePath& drive_file_path,
      const MarkMountedCallback& callback) OVERRIDE;
  virtual void MarkCacheFileAsUnmounted(
      const base::FilePath& cache_file_path,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void GetCacheEntryByPath(
      const base::FilePath& drive_file_path,
      const GetCacheEntryCallback& callback) OVERRIDE;
  virtual void Reload(const FileOperationCallback& callback) OVERRIDE;

  // file_system::OperationObserver overrides.
  virtual void OnDirectoryChangedByOperation(
      const base::FilePath& directory_path) OVERRIDE;
  virtual void OnCacheFileUploadNeededByOperation(
      const std::string& local_id) OVERRIDE;

  // ChangeListLoader::Observer overrides.
  // Used to propagate events from ChangeListLoader.
  virtual void OnDirectoryChanged(
      const base::FilePath& directory_path) OVERRIDE;
  virtual void OnLoadFromServerComplete() OVERRIDE;
  virtual void OnInitialLoadComplete() OVERRIDE;

  // Used by tests.
  internal::ChangeListLoader* change_list_loader_for_testing() {
    return change_list_loader_.get();
  }
  internal::SyncClient* sync_client_for_testing() { return sync_client_.get(); }

 private:
  // Part of Reload(). This is called after the cache and the resource metadata
  // is cleared, and triggers full feed fetching.
  void ReloadAfterReset(const FileOperationCallback& callback, FileError error);

  // Used for initialization and Reload(). (Re-)initializes sub components that
  // need to be recreated during the reload of resource metadata and the cache.
  void ResetComponents();

  // Part of CreateDirectory(). Called after ChangeListLoader::LoadIfNeeded()
  // is called and made sure that the resource metadata is loaded.
  void CreateDirectoryAfterLoad(const base::FilePath& directory_path,
                                bool is_exclusive,
                                bool is_recursive,
                                const FileOperationCallback& callback,
                                FileError load_error);

  void FinishPin(const FileOperationCallback& callback,
                 const std::string* local_id,
                 FileError error);

  void FinishUnpin(const FileOperationCallback& callback,
                   const std::string* local_id,
                   FileError error);

  // Callback for handling about resource fetch.
  void OnGetAboutResource(
      const GetAvailableSpaceCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Part of CheckForUpdates(). Called when
  // ChangeListLoader::CheckForUpdates() is complete.
  void OnUpdateChecked(FileError error);

  // Part of GetResourceEntryByPath()
  // 1) Called when GetLocallyStoredResourceEntry() is complete.
  // 2) Called when LoadDirectoryIfNeeded() is complete.
  void GetResourceEntryByPathAfterGetEntry(
      const base::FilePath& file_path,
      const GetResourceEntryCallback& callback,
      scoped_ptr<ResourceEntry> entry,
      FileError error);
  void GetResourceEntryByPathAfterLoad(const base::FilePath& file_path,
                                       const GetResourceEntryCallback& callback,
                                       FileError error);

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
      const ReadDirectoryCallback& callback,
      FileError error);
  void ReadDirectoryByPathAfterRead(
      const base::FilePath& directory_path,
      const ReadDirectoryCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntryVector> entries);

  // Part of GetShareUrl. Resolves the resource entry to get the resource it,
  // and then uses it to ask for the share url. |callback| must not be null.
  void GetShareUrlAfterGetResourceEntry(
      const base::FilePath& file_path,
      const GURL& embed_origin,
      const GetShareUrlCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> entry);
  void OnGetResourceEntryForGetShareUrl(
      const GetShareUrlCallback& callback,
      google_apis::GDataErrorCode status,
      const GURL& share_url);

  // Reloads the metadata for the directory to refresh stale thumbnail URLs.
  void RefreshDirectory(const base::FilePath& directory_path);
  void RefreshDirectoryAfterGetResourceEntry(
      const base::FilePath& directory_path,
      FileError error,
      scoped_ptr<ResourceEntry> entry);

  // Used to get Drive related preferences.
  PrefService* pref_service_;

  // Sub components owned by DriveIntegrationService.
  internal::FileCache* cache_;
  DriveServiceInterface* drive_service_;
  JobScheduler* scheduler_;
  internal::ResourceMetadata* resource_metadata_;

  // Time of the last update check.
  base::Time last_update_check_time_;

  // Error of the last update check.
  FileError last_update_check_error_;

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
  scoped_ptr<file_system::OpenFileOperation> open_file_operation_;
  scoped_ptr<file_system::RemoveOperation> remove_operation_;
  scoped_ptr<file_system::TouchOperation> touch_operation_;
  scoped_ptr<file_system::TruncateOperation> truncate_operation_;
  scoped_ptr<file_system::DownloadOperation> download_operation_;
  scoped_ptr<file_system::UpdateOperation> update_operation_;
  scoped_ptr<file_system::SearchOperation> search_operation_;
  scoped_ptr<file_system::GetFileForSavingOperation>
      get_file_for_saving_operation_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileSystem> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystem);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_H_
