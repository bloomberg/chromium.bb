// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/drive/change_list_loader.h"
#include "chrome/browser/chromeos/drive/change_list_loader_observer.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/file_system/drive_operations.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
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
class DriveUploaderInterface;
class ResourceEntry;
class ResourceList;
}

namespace drive {

class DriveCacheEntry;
class DriveFileSystemObserver;
class DriveFunctionRemove;
class DriveResourceMetadata;
class DriveScheduler;
class DriveWebAppsRegistry;
class ChangeListLoader;

namespace file_system {
class CopyOperation;
class MoveOperation;
class RemoveOperation;
}

// The production implementation of DriveFileSystemInterface.
class DriveFileSystem : public DriveFileSystemInterface,
                        public ChangeListLoaderObserver,
                        public file_system::OperationObserver {
 public:
  DriveFileSystem(Profile* profile,
                  DriveCache* cache,
                  google_apis::DriveServiceInterface* drive_service,
                  google_apis::DriveUploaderInterface* uploader,
                  DriveWebAppsRegistry* webapps_registry,
                  DriveResourceMetadata* resource_metadata,
                  base::SequencedTaskRunner* blocking_task_runner);
  virtual ~DriveFileSystem();

  // DriveFileSystem overrides.
  virtual void Initialize() OVERRIDE;
  virtual void AddObserver(DriveFileSystemObserver* observer) OVERRIDE;
  virtual void RemoveObserver(DriveFileSystemObserver* observer) OVERRIDE;
  virtual void StartInitialFeedFetch() OVERRIDE;
  virtual void StartPolling() OVERRIDE;
  virtual void StopPolling() OVERRIDE;
  virtual void SetPushNotificationEnabled(bool enabled) OVERRIDE;
  virtual void NotifyFileSystemMounted() OVERRIDE;
  virtual void NotifyFileSystemToBeUnmounted() OVERRIDE;
  virtual void CheckForUpdates() OVERRIDE;
  virtual void GetEntryInfoByResourceId(
      const std::string& resource_id,
      const GetEntryInfoWithFilePathCallback& callback) OVERRIDE;
  virtual void Search(const std::string& search_query,
                      bool shared_with_me,
                      const GURL& next_feed,
                      const SearchCallback& callback) OVERRIDE;
  virtual void SearchMetadata(const std::string& query,
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
  virtual void GetFileByPath(const base::FilePath& file_path,
                             const GetFileCallback& callback) OVERRIDE;
  virtual void GetFileByResourceId(
      const std::string& resource_id,
      const DriveClientContext& context,
      const GetFileCallback& get_file_callback,
      const google_apis::GetContentCallback& get_content_callback) OVERRIDE;
  virtual void CancelGetFile(const base::FilePath& drive_file_path) OVERRIDE;
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
  virtual void Reload() OVERRIDE;

  // file_system::OperationObserver overrides.
  virtual void OnDirectoryChangedByOperation(
      const base::FilePath& directory_path) OVERRIDE;

  // ChangeListLoader::Observer overrides.
  // Used to propagate events from ChangeListLoader.
  virtual void OnDirectoryChanged(
      const base::FilePath& directory_path) OVERRIDE;
  virtual void OnResourceListFetched(int num_accumulated_entries) OVERRIDE;
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

  // Defines set of parameters passed to intermediate callbacks during
  // execution of GetFileByPath() method.
  struct GetFileFromCacheParams;

  // Struct used for AddUploadedFile.
  struct AddUploadedFileParams;

  // Used to implement Reload().
  void ReloadAfterReset();

  // Sets up ChangeListLoader.
  void SetupChangeListLoader();

  // Called on preference change.
  void OnDisableDriveHostedFilesChanged();

  // Callback passed to ChangeListLoader from |Search| method.
  // |callback| is that should be run with data received. It must not be null.
  // |feed_list| is the document feed for content search.
  // |error| is the error code returned by ChangeListLoader.
  void OnSearch(bool shared_with_me,
                const SearchCallback& callback,
                const ScopedVector<google_apis::ResourceList>& feed_list,
                DriveFileError error);

  // Callback for DriveResourceMetadata::RefreshEntry, from OnSearch.
  // Adds |drive_file_path| to |results|. When |entry_proto| is not present in
  // the local file system snapshot, it is not added to |results|. Instead,
  // CheckForUpdates is called. Runs |callback| with |results| if
  // |should_run_callback| is true.
  void AddToSearchResults(std::vector<SearchResultInfo>* results,
                          bool should_run_callback,
                          const base::Closure& callback,
                          DriveFileError error,
                          const base::FilePath& drive_file_path,
                          scoped_ptr<DriveEntryProto> entry_proto);

  // Invoked during the process of CreateFile.
  // First, FindEntryByPathAsync is called and the result is returned
  // to OnGetEntryInfoForCreateFile. By using the information, CreateFile deals
  // with the cases when an entry already existed at the path. If there was no
  // entry, a new empty file is uploaded, and when it finishes
  // DidUploadForCreateBrandNewFile does the final clean up.
  // |callback| must not be null.
  void OnGetEntryInfoForCreateFile(const base::FilePath& file_path,
                                   bool is_exclusive,
                                   const FileOperationCallback& callback,
                                   DriveFileError result,
                                   scoped_ptr<DriveEntryProto> entry_proto);
  void DoUploadForCreateBrandNewFile(const base::FilePath& remote_path,
                                     base::FilePath* local_path,
                                     const FileOperationCallback& callback);
  void DidUploadForCreateBrandNewFile(const base::FilePath& local_path,
                                      const FileOperationCallback& callback,
                                      DriveFileError result);

  // Invoked upon completion of GetEntryInfoByPath initiated by
  // GetFileByPath. It then continues to invoke GetResolvedFileByPath.
  // |callback| must not be null.
  void OnGetEntryInfoCompleteForGetFileByPath(
      const base::FilePath& file_path,
      const GetFileCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> file_info);

  // Invoked upon completion of GetEntryInfoByPath initiated by OpenFile.
  // It then continues to invoke GetResolvedFileByPath and proceeds to
  // OnGetFileCompleteForOpenFile.
  void OnGetEntryInfoCompleteForOpenFile(
      const base::FilePath& file_path,
      const OpenFileCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> file_info);

  // Invoked at the last step of OpenFile. It removes |file_path| from the
  // current set of opened files if |result| is an error, and then invokes the
  // |callback| function.
  void OnOpenFileFinished(const base::FilePath& file_path,
                          const OpenFileCallback& callback,
                          DriveFileError result,
                          const base::FilePath& cache_file_path);

  // Invoked during the process of CloseFile. What is done here is as follows:
  // 1) Gets resource_id and md5 of the entry at |file_path|.
  // 2) Commits the modification to the cache system.
  // 3) Removes the |file_path| from the remembered set of opened files.
  // 4) Invokes the user-supplied |callback|.
  // |callback| must not be null.
  void CloseFileAfterGetEntryInfo(const base::FilePath& file_path,
                                  const FileOperationCallback& callback,
                                  DriveFileError error,
                                  scoped_ptr<DriveEntryProto> entry_proto);
  void CloseFileFinalize(const base::FilePath& file_path,
                         const FileOperationCallback& callback,
                         DriveFileError result);

  // Invoked upon completion of GetFileByPath initiated by OpenFile. If
  // GetFileByPath is successful, calls MarkDirtyInCache to mark the cache
  // file as dirty for the file identified by |file_info.resource_id| and
  // |file_info.md5|.
  void OnGetFileCompleteForOpenFile(const GetFileCompleteForOpenParams& params,
                                    DriveFileError error,
                                    const base::FilePath& file_path,
                                    const std::string& mime_type,
                                    DriveFileType file_type);

  // Invoked upon completion of MarkDirtyInCache initiated by OpenFile.
  void OnMarkDirtyInCacheCompleteForOpenFile(
      const GetFileCompleteForOpenParams& params,
      DriveFileError error);

  // Callback for handling about resource fetch.
  void OnGetAboutResource(
      const GetAvailableSpaceCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Callback for handling file downloading requests.
  void OnFileDownloaded(const GetFileFromCacheParams& params,
                        google_apis::GDataErrorCode status,
                        const base::FilePath& downloaded_file_path);

  // Unpins file if cache entry is pinned.
  void UnpinIfPinned(const std::string& resource_id,
                     const std::string& md5,
                     bool success,
                     const DriveCacheEntry& cache_entry);

  // Similar to OnFileDownloaded() but takes |has_enough_space| so we report
  // an error in case we don't have enough disk space.
  void OnFileDownloadedAndSpaceChecked(
      const GetFileFromCacheParams& params,
      google_apis::GDataErrorCode status,
      const base::FilePath& downloaded_file_path,
      bool has_enough_space);

  // Adds the uploaded file to the cache.
  void AddUploadedFileToCache(const AddUploadedFileParams& params,
                              DriveFileError error,
                              const base::FilePath& file_path);

  // Callback for handling results of ReloadFeedFromServerIfNeeded() initiated
  // from CheckForUpdates().
  void OnUpdateChecked(DriveFileError error);

  // Helper function for internally handling responses from
  // GetFileFromCacheByResourceIdAndMd5() calls during processing of
  // GetFileByPath() request.
  void OnGetFileFromCache(const GetFileFromCacheParams& params,
                          DriveFileError error,
                          const base::FilePath& cache_file_path);

  // Callback for |drive_service_->GetResourceEntry|.
  // It is called before file download. If GetResourceEntry was successful,
  // file download procedure is started for the file. The file is downloaded
  // from the content url extracted from the fetched metadata.
  void OnGetResourceEntry(const GetFileFromCacheParams& params,
                          google_apis::GDataErrorCode status,
                          scoped_ptr<google_apis::ResourceEntry> entry);

  // Check available space using file size from the fetched metadata. Called
  // from OnGetResourceEntry after RefreshEntry is complete.
  void CheckForSpaceBeforeDownload(
      const GetFileFromCacheParams& params,
      int64 file_size,
      const GURL& download_url,
      DriveFileError error,
      const base::FilePath& drive_file_path,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Starts downloading a file if we have enough disk space indicated by
  // |has_enough_space|.
  void StartDownloadFileIfEnoughSpace(const GetFileFromCacheParams& params,
                                      const GURL& download_url,
                                      const DriveClientContext& context,
                                      const base::FilePath& cache_file_path,
                                      bool has_enough_space);

  // Changes state of hosted documents visibility, triggers directory refresh.
  void SetHideHostedDocuments(bool hide);

  // Initializes preference change observer.
  void InitializePreferenceObserver();

  // Part of GetEntryInfoByPath()
  // 1) Called when DriveResourceMetadata::GetEntryInfoByPath() is complete.
  //    If succeeded, GetEntryInfoByPath() returns immediately here.
  //    Otherwise, starts loading the file system.
  // 2) Called when LoadIfNeeded() is complete.
  // 3) Called when DriveResourceMetadata::GetEntryInfoByPath() is complete.
  void GetEntryInfoByPathAfterGetEntry1(
      const base::FilePath& file_path,
      const GetEntryInfoCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);
  void GetEntryInfoByPathAfterLoad(const base::FilePath& file_path,
                                   const GetEntryInfoCallback& callback,
                                   DriveFileError error);
  void GetEntryInfoByPathAfterGetEntry2(
      const GetEntryInfoCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Part of ReadDirectoryByPath()
  // 1) Called when DriveResourceMetadata::GetEntryInfoByPath() is complete.
  // 2) Called when LoadIfNeeded() is complete.
  // 3) Called when DriveResourceMetadata::ReadDirectoryByPath() is complete.
  // |callback| must not be null.
  void ReadDirectoryByPathAfterGetEntry(
      const base::FilePath& directory_path,
      const ReadDirectoryWithSettingCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);
  void ReadDirectoryByPathAfterLoad(
      const base::FilePath& directory_path,
      const ReadDirectoryWithSettingCallback& callback,
      DriveFileError error);
  void ReadDirectoryByPathAfterRead(
      const ReadDirectoryWithSettingCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProtoVector> entries);

  // Loads the file system from the cache or the server via change lists if
  // the file system is not yet loaded. Runs |callback| upon the completion
  // with the error code.  |callback| must not be null.
  void LoadIfNeeded(const DirectoryFetchInfo& directory_fetch_info,
                    const FileOperationCallback& callback);

  // Gets the file at |file_path| from the cache (if found in the cache),
  // or the server (if not found in the cache) after the file info is
  // already resolved with GetEntryInfoByPath() or GetEntryInfoByResourceId().
  // |get_file_callback| must not be null.
  // |get_content_callback| may be null.
  void GetResolvedFileByPath(
      const base::FilePath& file_path,
      const DriveClientContext& context,
      const GetFileCallback& get_file_callback,
      const google_apis::GetContentCallback& get_content_callback,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Part of GetEntryInfoByResourceId(). Called after
  // DriveResourceMetadata::GetEntryInfoByResourceId() is complete.
  // |callback| must not be null.
  void GetEntryInfoByResourceIdAfterGetEntry(
      const GetEntryInfoWithFilePathCallback& callback,
      DriveFileError error,
      const base::FilePath& file_path,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Part of GetFileByResourceId(). Called after
  // DriveResourceMetadata::GetEntryInfoByResourceId() is complete.
  // |get_file_callback| must not be null.
  // |get_content_callback| may be null.
  void GetFileByResourceIdAfterGetEntry(
      const DriveClientContext& context,
      const GetFileCallback& get_file_callback,
      const google_apis::GetContentCallback& get_content_callback,
      DriveFileError error,
      const base::FilePath& file_path,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Part of RefreshDirectory(). Called after
  // GetEntryInfoByPath() is complete.
  void RefreshDirectoryAfterGetEntryInfo(
      const base::FilePath& directory_path,
      const FileOperationCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Part of GetEntryByResourceId and GetEntryByPath. Checks whether there is a
  // local dirty cache for the entry, and if there is, replace the
  // PlatformFileInfo part of the |entry_proto| with the locally modified info.
  // |callback| must not be null.
  void CheckLocalModificationAndRun(scoped_ptr<DriveEntryProto> entry_proto,
                                    const GetEntryInfoCallback& callback);
  void CheckLocalModificationAndRunAfterGetCacheEntry(
      scoped_ptr<DriveEntryProto> entry_proto,
      const GetEntryInfoCallback& callback,
      bool success,
      const DriveCacheEntry& cache_entry);
  void CheckLocalModificationAndRunAfterGetCacheFile(
      scoped_ptr<DriveEntryProto> entry_proto,
      const GetEntryInfoCallback& callback,
      DriveFileError error,
      const base::FilePath& local_cache_path);
  void CheckLocalModificationAndRunAfterGetFileInfo(
      scoped_ptr<DriveEntryProto> entry_proto,
      const GetEntryInfoCallback& callback,
      base::PlatformFileInfo* file_info,
      bool get_file_info_result);

  // The profile hosts the DriveFileSystem via DriveSystemService.
  Profile* profile_;

  // The cache owned by DriveSystemService.
  DriveCache* cache_;

  // The uploader owned by DriveSystemService.
  google_apis::DriveUploaderInterface* uploader_;

  // The document service owned by DriveSystemService.
  google_apis::DriveServiceInterface* drive_service_;

  // The webapps registry owned by DriveSystemService.
  DriveWebAppsRegistry* webapps_registry_;

  DriveResourceMetadata* resource_metadata_;

  // Periodic timer for checking updates.
  base::Timer update_timer_;

  // Time of the last update check.
  base::Time last_update_check_time_;

  // Error of the last update check.
  DriveFileError last_update_check_error_;

  // True if hosted documents should be hidden.
  bool hide_hosted_docs_;

  // The set of paths opened by OpenFile but not yet closed by CloseFile.
  std::set<base::FilePath> open_files_;

  scoped_ptr<PrefChangeRegistrar> pref_registrar_;

  // The loader is used to load the change lists.
  scoped_ptr<ChangeListLoader> change_list_loader_;

  ObserverList<DriveFileSystemObserver> observers_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  file_system::DriveOperations drive_operations_;

  scoped_ptr<DriveScheduler> scheduler_;

  // Polling interval for checking updates in seconds.
  int polling_interval_sec_;

  // True if push notification is enabled.
  bool push_notification_enabled_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveFileSystem> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveFileSystem);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_H_
