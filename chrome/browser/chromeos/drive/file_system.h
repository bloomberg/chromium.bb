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
#include "chrome/browser/chromeos/drive/file_system/drive_operations.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_list.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class PrefChangeRegistrar;
class Profile;

namespace base {
struct PlatformFileInfo;
class SequencedTaskRunner;
}

namespace google_apis {
class AboutResource;
class DriveServiceInterface;
class ResourceEntry;
}

namespace drive {

class ChangeList;
class ChangeListLoader;
class DriveWebAppsRegistry;
class FileCacheEntry;
class FileSystemObserver;
class JobScheduler;

namespace internal {
class ResourceMetadata;
}  // namespace internal

// The production implementation of FileSystemInterface.
class FileSystem : public FileSystemInterface,
                   public ChangeListLoaderObserver,
                   public file_system::OperationObserver {
 public:
  FileSystem(Profile* profile,
             FileCache* cache,
             google_apis::DriveServiceInterface* drive_service,
             JobScheduler* scheduler,
             DriveWebAppsRegistry* webapps_registry,
             internal::ResourceMetadata* resource_metadata,
             base::SequencedTaskRunner* blocking_task_runner);
  virtual ~FileSystem();

  // FileSystemInterface overrides.
  virtual void Initialize() OVERRIDE;
  virtual void AddObserver(FileSystemObserver* observer) OVERRIDE;
  virtual void RemoveObserver(FileSystemObserver* observer) OVERRIDE;
  virtual void CheckForUpdates() OVERRIDE;
  virtual void GetEntryInfoByResourceId(
      const std::string& resource_id,
      const GetEntryInfoWithFilePathCallback& callback) OVERRIDE;
  virtual void Search(const std::string& search_query,
                      const GURL& next_feed,
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
  virtual void Pin(const base::FilePath& file_path,
                   const FileOperationCallback& callback) OVERRIDE;
  virtual void Unpin(const base::FilePath& file_path,
                     const FileOperationCallback& callback) OVERRIDE;
  virtual void GetFileByPath(const base::FilePath& file_path,
                             const GetFileCallback& callback) OVERRIDE;
  virtual void GetFileByResourceId(
      const std::string& resource_id,
      const DriveClientContext& context,
      const GetFileCallback& get_file_callback,
      const google_apis::GetContentCallback& get_content_callback) OVERRIDE;
  virtual void GetFileContentByPath(
      const base::FilePath& file_path,
      const GetFileContentInitializedCallback& initialized_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const FileOperationCallback& completion_callback) OVERRIDE;
  virtual void UpdateFileByResourceId(
      const std::string& resource_id,
      const DriveClientContext& context,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void GetEntryInfoByPath(
      const base::FilePath& file_path,
      const GetEntryInfoCallback& callback) OVERRIDE;
  virtual void ReadDirectoryByPath(
      const base::FilePath& directory_path,
      const ReadDirectoryWithSettingCallback& callback) OVERRIDE;
  virtual void RefreshDirectory(
      const base::FilePath& directory_path,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void GetAvailableSpace(
      const GetAvailableSpaceCallback& callback) OVERRIDE;
  virtual void AddUploadedFile(scoped_ptr<google_apis::ResourceEntry> doc_entry,
                               const base::FilePath& file_content_path,
                               const FileOperationCallback& callback) OVERRIDE;
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
  virtual void IterateCache(const CacheIterateCallback& iteration_callback,
                            const base::Closure& completion_callback) OVERRIDE;
  virtual void Reload() OVERRIDE;

  // file_system::OperationObserver overrides.
  virtual void OnDirectoryChangedByOperation(
      const base::FilePath& directory_path) OVERRIDE;

  // ChangeListLoader::Observer overrides.
  // Used to propagate events from ChangeListLoader.
  virtual void OnDirectoryChanged(
      const base::FilePath& directory_path) OVERRIDE;
  virtual void OnFeedFromServerLoaded() OVERRIDE;
  virtual void OnInitialFeedLoaded() OVERRIDE;

  // Used in tests to update the file system from |feed_list|.
  // See also the comment at ChangeListLoader::UpdateFromFeed().
  ChangeListLoader* change_list_loader() { return change_list_loader_.get(); }

 private:
  friend class DriveFileSystemTest;

  // Defines set of parameters passed to an intermediate callback
  // OnGetFileCompleteForOpen, during execution of OpenFile() method.
  struct GetFileCompleteForOpenParams;

  // Defines set of parameters for GetResolvedFileByPath().
  struct GetResolvedFileParams;

  // Struct used for AddUploadedFile.
  struct AddUploadedFileParams;

  // Used to implement Reload().
  void ReloadAfterReset(FileError error);

  // Sets up ChangeListLoader.
  void SetupChangeListLoader();

  // Called on preference change.
  void OnDisableDriveHostedFilesChanged();

  // Callback passed to ChangeListLoader from |Search| method.
  // |callback| is that should be run with data received. It must not be null.
  // |change_lists| is the document feed for content search.
  // |error| is the error code returned by ChangeListLoader.
  void OnSearch(const SearchCallback& callback,
                ScopedVector<ChangeList> change_lists,
                FileError error);

  // Callback for ResourceMetadata::RefreshEntry, from OnSearch.
  // Adds |drive_file_path| to |results|. When |entry| is not present in
  // the local file system snapshot, it is not added to |results|. Instead,
  // CheckForUpdates is called. Runs |callback| with |results| if
  // |should_run_callback| is true.
  void AddToSearchResults(std::vector<SearchResultInfo>* results,
                          bool should_run_callback,
                          const base::Closure& callback,
                          FileError error,
                          const base::FilePath& drive_file_path,
                          scoped_ptr<ResourceEntry> entry);

  // Part of CreateDirectory(). Called after ChangeListLoader::LoadIfNeeded()
  // is called and made sure that the resource metadata is loaded.
  void CreateDirectoryAfterLoad(const base::FilePath& directory_path,
                                bool is_exclusive,
                                bool is_recursive,
                                const FileOperationCallback& callback,
                                FileError load_error);

  // Used to implement Pin().
  void PinAfterGetEntryInfoByPath(const FileOperationCallback& callback,
                                  FileError error,
                                  scoped_ptr<ResourceEntry> entry);

  // Used to implement Unpin().
  void UnpinAfterGetEntryInfoByPath(const FileOperationCallback& callback,
                                    FileError error,
                                    scoped_ptr<ResourceEntry> entry);

  // Invoked upon completion of GetEntryInfoByPath initiated by
  // GetFileByPath. It then continues to invoke GetResolvedFileByPath.
  // |callback| must not be null.
  void OnGetEntryInfoCompleteForGetFileByPath(
      const base::FilePath& file_path,
      const GetFileCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> file_info);

  // Invoked upon completion of GetEntryInfoByPath initiated by OpenFile.
  // It then continues to invoke GetResolvedFileByPath and proceeds to
  // OnGetFileCompleteForOpenFile.
  void OnGetEntryInfoCompleteForOpenFile(
      const base::FilePath& file_path,
      const OpenFileCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> file_info);

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
  void CloseFileAfterGetEntryInfo(const base::FilePath& file_path,
                                  const FileOperationCallback& callback,
                                  FileError error,
                                  scoped_ptr<ResourceEntry> entry);
  void CloseFileFinalize(const base::FilePath& file_path,
                         const FileOperationCallback& callback,
                         FileError result);

  // Invoked upon completion of GetFileByPath initiated by OpenFile. If
  // GetFileByPath is successful, calls MarkDirtyInCache to mark the cache
  // file as dirty for the file identified by |file_info.resource_id| and
  // |file_info.md5|.
  void OnGetFileCompleteForOpenFile(const GetFileCompleteForOpenParams& params,
                                    FileError error,
                                    const base::FilePath& file_path,
                                    const std::string& mime_type,
                                    DriveFileType file_type);

  // Invoked upon completion of MarkDirtyInCache initiated by OpenFile.
  void OnMarkDirtyInCacheCompleteForOpenFile(
      const GetFileCompleteForOpenParams& params,
      FileError error);

  // Callback for handling about resource fetch.
  void OnGetAboutResource(
      const GetAvailableSpaceCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Adds the uploaded file to the cache.
  void AddUploadedFileToCache(const AddUploadedFileParams& params,
                              FileError error,
                              const base::FilePath& file_path);

  // Callback for handling results of ReloadFeedFromServerIfNeeded() initiated
  // from CheckForUpdates().
  void OnUpdateChecked(FileError error);

  // Changes state of hosted documents visibility, triggers directory refresh.
  void SetHideHostedDocuments(bool hide);

  // Initializes preference change observer.
  void InitializePreferenceObserver();

  // Part of GetEntryInfoByPath()
  // 1) Called when ResourceMetadata::GetEntryInfoByPath() is complete.
  //    If succeeded, GetEntryInfoByPath() returns immediately here.
  //    Otherwise, starts loading the file system.
  // 2) Called when LoadIfNeeded() is complete.
  // 3) Called when ResourceMetadata::GetEntryInfoByPath() is complete.
  void GetEntryInfoByPathAfterGetEntry1(
      const base::FilePath& file_path,
      const GetEntryInfoCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> entry);
  void GetEntryInfoByPathAfterLoad(const base::FilePath& file_path,
                                   const GetEntryInfoCallback& callback,
                                   FileError error);
  void GetEntryInfoByPathAfterGetEntry2(
      const GetEntryInfoCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> entry);

  // Part of ReadDirectoryByPath()
  // 1) Called when ResourceMetadata::GetEntryInfoByPath() is complete.
  // 2) Called when LoadIfNeeded() is complete.
  // 3) Called when ResourceMetadata::ReadDirectoryByPath() is complete.
  // |callback| must not be null.
  void ReadDirectoryByPathAfterGetEntry(
      const base::FilePath& directory_path,
      const ReadDirectoryWithSettingCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> entry);
  void ReadDirectoryByPathAfterLoad(
      const base::FilePath& directory_path,
      const ReadDirectoryWithSettingCallback& callback,
      FileError error);
  void ReadDirectoryByPathAfterRead(
      const ReadDirectoryWithSettingCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntryVector> entries);

  // Gets the file at |file_path| from the cache (if found in the cache),
  // or the server (if not found in the cache) after the file info is
  // already resolved with GetEntryInfoByPath() or GetEntryInfoByResourceId().
  void GetResolvedFileByPath(scoped_ptr<GetResolvedFileParams> params);
  void GetResolvedFileByPathAfterCreateDocumentJsonFile(
      scoped_ptr<GetResolvedFileParams> params,
      const base::FilePath* file_path,
      FileError error);
  void GetResolvedFileByPathAfterGetFileFromCache(
      scoped_ptr<GetResolvedFileParams> params,
      FileError error,
      const base::FilePath& cache_file_path);
  void GetResolvedFileByPathAfterGetResourceEntry(
      scoped_ptr<GetResolvedFileParams> params,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::ResourceEntry> entry);
  void GetResolvedFileByPathAfterRefreshEntry(
      scoped_ptr<GetResolvedFileParams> params,
      const GURL& download_url,
      FileError error,
      const base::FilePath& drive_file_path,
      scoped_ptr<ResourceEntry> entry);
  void GetResolvedFileByPathAfterFreeDiskSpace(
      scoped_ptr<GetResolvedFileParams> params,
      const GURL& download_url,
      bool has_enough_space);
  void GetResolveFileByPathAfterCreateTemporaryFile(
      scoped_ptr<GetResolvedFileParams> params,
      const GURL& download_url,
      base::FilePath* temp_file,
      bool success);
  void GetResolvedFileByPathAfterDownloadFile(
      scoped_ptr<GetResolvedFileParams> params,
      google_apis::GDataErrorCode status,
      const base::FilePath& downloaded_file_path);
  void GetResolvedFileByPathAfterGetCacheEntryForCancel(
      const std::string& resource_id,
      const std::string& md5,
      bool success,
      const FileCacheEntry& cache_entry);
  void GetResolvedFileByPathAfterStore(
      scoped_ptr<GetResolvedFileParams> params,
      const base::FilePath& downloaded_file_path,
      FileError error);
  void GetResolvedFileByPathAfterGetFile(
      scoped_ptr<GetResolvedFileParams> params,
      FileError error,
      const base::FilePath& cache_file);

  // Part of GetEntryInfoByResourceId(). Called after
  // ResourceMetadata::GetEntryInfoByResourceId() is complete.
  // |callback| must not be null.
  void GetEntryInfoByResourceIdAfterGetEntry(
      const GetEntryInfoWithFilePathCallback& callback,
      FileError error,
      const base::FilePath& file_path,
      scoped_ptr<ResourceEntry> entry);

  // Part of GetFileByResourceId(). Called after
  // ResourceMetadata::GetEntryInfoByResourceId() is complete.
  // |get_file_callback| must not be null.
  // |get_content_callback| may be null.
  void GetFileByResourceIdAfterGetEntry(
      const DriveClientContext& context,
      const GetFileCallback& get_file_callback,
      const google_apis::GetContentCallback& get_content_callback,
      FileError error,
      const base::FilePath& file_path,
      scoped_ptr<ResourceEntry> entry);

  // Part of GetFileContentByPath(). Called after
  // ResourceMetadata::GetEntryInfoByPath() is complete.
  // |initialized_callback|, |get_content_callback| and |completion_callback|
  // must not be null.
  void GetFileContentByPathAfterGetEntry(
      const base::FilePath& file_path,
      const GetFileContentInitializedCallback& initialized_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const FileOperationCallback& completion_callback,
      FileError error,
      scoped_ptr<ResourceEntry> entry);

  // Part of RefreshDirectory(). Called after
  // GetEntryInfoByPath() is complete.
  void RefreshDirectoryAfterGetEntryInfo(
      const base::FilePath& directory_path,
      const FileOperationCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> entry);

  // Part of GetEntryByResourceId and GetEntryByPath. Checks whether there is a
  // local dirty cache for the entry, and if there is, replace the
  // PlatformFileInfo part of the |entry| with the locally modified info.
  // |callback| must not be null.
  void CheckLocalModificationAndRun(scoped_ptr<ResourceEntry> entry,
                                    const GetEntryInfoCallback& callback);
  void CheckLocalModificationAndRunAfterGetCacheEntry(
      scoped_ptr<ResourceEntry> entry,
      const GetEntryInfoCallback& callback,
      bool success,
      const FileCacheEntry& cache_entry);
  void CheckLocalModificationAndRunAfterGetCacheFile(
      scoped_ptr<ResourceEntry> entry,
      const GetEntryInfoCallback& callback,
      FileError error,
      const base::FilePath& local_cache_path);
  void CheckLocalModificationAndRunAfterGetFileInfo(
      scoped_ptr<ResourceEntry> entry,
      const GetEntryInfoCallback& callback,
      base::PlatformFileInfo* file_info,
      bool get_file_info_result);

  // Part of MarkCacheFileAsMounted. Called after GetEntryInfoByPath is
  // completed. |callback| must not be null.
  void MarkCacheFileAsMountedAfterGetEntryInfo(
      const OpenFileCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> entry);

  // Cancels the job with |id| in the scheduler.
  void CancelJobInScheduler(JobID id);

  // The profile hosts the FileSystem via DriveSystemService.
  Profile* profile_;

  // Sub components owned by DriveSystemService.
  FileCache* cache_;
  google_apis::DriveServiceInterface* drive_service_;
  JobScheduler* scheduler_;
  DriveWebAppsRegistry* webapps_registry_;
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

  // The loader is used to load the change lists.
  scoped_ptr<ChangeListLoader> change_list_loader_;

  ObserverList<FileSystemObserver> observers_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  file_system::DriveOperations drive_operations_;

  // Polling interval for checking updates in seconds.
  int polling_interval_sec_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileSystem> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystem);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_H_
