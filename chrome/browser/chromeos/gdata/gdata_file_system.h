// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/platform_file.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/gdata/gdata_cache.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system_interface.h"
#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"
#include "chrome/browser/chromeos/gdata/gdata_files.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_feed_loader.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_feed_processor.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"

namespace base {

class SequencedTaskRunner;

}  // namespace base

namespace gdata {

class DocumentsServiceInterface;
class DriveWebAppsRegistryInterface;
class GDataWapiFeedLoader;
struct UploadFileInfo;

// The production implementation of GDataFileSystemInterface.
class GDataFileSystem : public GDataFileSystemInterface,
                        public GDataWapiFeedLoader::Observer,
                        public content::NotificationObserver {
 public:
  GDataFileSystem(Profile* profile,
                  GDataCache* cache,
                  DocumentsServiceInterface* documents_service,
                  GDataUploaderInterface* uploader,
                  DriveWebAppsRegistryInterface* webapps_registry,
                  base::SequencedTaskRunner* blocking_task_runner);
  virtual ~GDataFileSystem();

  // GDataFileSystem overrides.
  virtual void Initialize() OVERRIDE;
  virtual void AddObserver(
      GDataFileSystemInterface::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      GDataFileSystemInterface::Observer* observer) OVERRIDE;
  virtual void StartUpdates() OVERRIDE;
  virtual void StopUpdates() OVERRIDE;
  virtual void NotifyFileSystemMounted() OVERRIDE;
  virtual void NotifyFileSystemToBeUnmounted() OVERRIDE;
  virtual void CheckForUpdates() OVERRIDE;
  virtual void GetEntryInfoByResourceId(
      const std::string& resource_id,
      const GetEntryInfoWithFilePathCallback& callback) OVERRIDE;
  virtual void Search(const std::string& search_query,
                      const GURL& next_feed,
                      const SearchCallback& callback) OVERRIDE;
  virtual void TransferFileFromRemoteToLocal(
      const FilePath& remote_src_file_path,
      const FilePath& local_dest_file_path,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void TransferFileFromLocalToRemote(
      const FilePath& local_src_file_path,
      const FilePath& remote_dest_file_path,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void OpenFile(const FilePath& file_path,
                        const OpenFileCallback& callback) OVERRIDE;
  virtual void CloseFile(const FilePath& file_path,
                         const FileOperationCallback& callback) OVERRIDE;
  virtual void Copy(const FilePath& src_file_path,
                    const FilePath& dest_file_path,
                    const FileOperationCallback& callback) OVERRIDE;
  virtual void Move(const FilePath& src_file_path,
                    const FilePath& dest_file_path,
                    const FileOperationCallback& callback) OVERRIDE;
  virtual void Remove(const FilePath& file_path,
                      bool is_recursive,
                      const FileOperationCallback& callback) OVERRIDE;
  virtual void CreateDirectory(const FilePath& directory_path,
                               bool is_exclusive,
                               bool is_recursive,
                               const FileOperationCallback& callback) OVERRIDE;
  virtual void CreateFile(const FilePath& file_path,
                          bool is_exclusive,
                          const FileOperationCallback& callback) OVERRIDE;
  virtual void GetFileByPath(
      const FilePath& file_path,
      const GetFileCallback& get_file_callback,
      const GetDownloadDataCallback& get_download_data_callback) OVERRIDE;
  virtual void GetFileByResourceId(
      const std::string& resource_id,
      const GetFileCallback& get_file_callback,
      const GetDownloadDataCallback& get_download_data_callback) OVERRIDE;
  virtual void UpdateFileByResourceId(
      const std::string& resource_id,
      const FileOperationCallback& callback) OVERRIDE;
  virtual void GetEntryInfoByPath(
      const FilePath& file_path,
      const GetEntryInfoCallback& callback) OVERRIDE;
  virtual void ReadDirectoryByPath(
      const FilePath& file_path,
      const ReadDirectoryWithSettingCallback& callback) OVERRIDE;
  virtual void RequestDirectoryRefresh(
      const FilePath& file_path) OVERRIDE;
  virtual void GetAvailableSpace(
      const GetAvailableSpaceCallback& callback) OVERRIDE;
  virtual void AddUploadedFile(UploadMode upload_mode,
                               const FilePath& virtual_dir_path,
                               scoped_ptr<DocumentEntry> entry,
                               const FilePath& file_content_path,
                               GDataCache::FileOperationType cache_operation,
                               const base::Closure& callback) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // GDataWapiFeedLoader::Observer overrides.
  // Used to propagate events from GDataWapiFeedLoader.
  virtual void OnDirectoryChanged(const FilePath& directory_path) OVERRIDE;
  virtual void OnDocumentFeedFetched(int num_accumulated_entries) OVERRIDE;
  virtual void OnFeedFromServerLoaded() OVERRIDE;

  // Used in tests to load the root feed from the cache.
  void LoadRootFeedFromCacheForTesting();

  // Used in tests to update the file system from |feed_list|.
  // See also the comment at GDataWapiFeedLoader::UpdateFromFeed().
  GDataFileError UpdateFromFeedForTesting(
      const std::vector<DocumentFeed*>& feed_list,
      int64 start_changestamp,
      int64 root_feed_changestamp);

 private:
  friend class GDataFileSystemTest;
  FRIEND_TEST_ALL_PREFIXES(GDataFileSystemTest,
                           FindFirstMissingParentDirectory);

  // Defines possible search results of FindFirstMissingParentDirectory().
  enum FindMissingDirectoryResult {
    // Target directory found, it's not a directory.
    FOUND_INVALID,
    // Found missing directory segment while searching for given directory.
    FOUND_MISSING,
    // Found target directory, it already exists.
    DIRECTORY_ALREADY_PRESENT,
  };

  // Defines set of parameters passes to intermediate callbacks during
  // execution of CreateDirectory() method.
  struct CreateDirectoryParams;

  // Defines set of parameters passed to an intermediate callback
  // OnGetFileCompleteForOpen, during execution of OpenFile() method.
  struct GetFileCompleteForOpenParams;

  // Defines set of parameters passed to intermediate callbacks during
  // execution of GetFileByPath() method.
  struct GetFileFromCacheParams;


  // Struct used for StartFileUploadOnUIThread().
  struct StartFileUploadParams;

  // Callback passed to |LoadFeedFromServer| from |Search| method.
  // |callback| is that should be run with data received from
  // |LoadFeedFromServer|.
  // |params| params used for getting document feed for content search.
  // |error| error code returned by |LoadFeedFromServer|.
  void OnSearch(const SearchCallback& callback,
                GetDocumentsParams* params,
                GDataFileError error);

  // Part of TransferFileFromLocalToRemote(). Called after
  // GetEntryInfoByPath() is complete.
  void TransferFileFromLocalToRemoteAfterGetEntryInfo(
      const FilePath& local_src_file_path,
      const FilePath& remote_dest_file_path,
      const FileOperationCallback& callback,
      GDataFileError error,
      scoped_ptr<GDataEntryProto> entry_proto);

  // Initiates transfer of |local_file_path| with |resource_id| to
  // |remote_dest_file_path|. |local_file_path| must be a file from the local
  // file system, |remote_dest_file_path| is the virtual destination path within
  // gdata file system. If |resource_id| is a non-empty string, the transfer is
  // handled by CopyDocumentToDirectory. Otherwise, the transfer is handled by
  // TransferRegularFile.
  //
  // Must be called from *UI* thread. |callback| is run on the calling thread.
  // |callback| must not be null.
  void TransferFileForResourceId(const FilePath& local_file_path,
                                 const FilePath& remote_dest_file_path,
                                 const FileOperationCallback& callback,
                                 std::string* resource_id);

  // Initiates transfer of |local_file_path| to |remote_dest_file_path|.
  // |local_file_path| must be a regular file (i.e. not a hosted document) from
  // the local file system, |remote_dest_file_path| is the virtual destination
  // path within gdata file system.
  //
  // Must be called from *UI* thread. |callback| is run on the calling thread.
  void TransferRegularFile(const FilePath& local_file_path,
                           const FilePath& remote_dest_file_path,
                           const FileOperationCallback& callback);

  // Invoked during the process of CreateFile.
  // First, FindEntryByPathAsyncOnUIThread is called and the result is returned
  // to OnGetEntryInfoForCreateFile. By using the information, CreateFile deals
  // with the cases when an entry already existed at the path. If there was no
  // entry, a new empty file is uploaded, and when it finishes
  // DidUploadForCreateBrandNewFile does the final clean up.
  void OnGetEntryInfoForCreateFile(
      const FilePath& file_path,
      bool is_exclusive,
      const FileOperationCallback& callback,
      GDataFileError result,
      GDataEntry* entry);
  void DoUploadForCreateBrandNewFile(
      const FilePath& remote_path,
      FilePath* local_path,
      const FileOperationCallback& callback);
  void DidUploadForCreateBrandNewFile(
      const FilePath& local_path,
      const FileOperationCallback& callback,
      GDataFileError result);

  // Invoked upon completion of GetEntryInfoByPath initiated by
  // GetFileByPath. It then continues to invoke GetResolvedFileByPath.
  void OnGetEntryInfoCompleteForGetFileByPath(
      const FilePath& file_path,
      const GetFileCallback& get_file_callback,
      const GetDownloadDataCallback& get_download_data_callback,
      GDataFileError error,
      scoped_ptr<GDataEntryProto> file_info);

  // Invoked upon completion of GetEntryInfoByPath initiated by OpenFile.
  // It then continues to invoke GetResolvedFileByPath and proceeds to
  // OnGetFileCompleteForOpenFile.
  void OnGetEntryInfoCompleteForOpenFile(
      const FilePath& file_path,
      const OpenFileCallback& callback,
      GDataFileError error,
      scoped_ptr<GDataEntryProto> file_info);

  // Invoked at the last step of OpenFile. It removes |file_path| from the
  // current set of opened files if |result| is an error, and then invokes the
  // |callback| function.
  void OnOpenFileFinished(const FilePath& file_path,
                          const OpenFileCallback& callback,
                          GDataFileError result,
                          const FilePath& cache_file_path);

  // Invoked during the process of CloseFile. What is done here is as follows:
  // 1) Gets resource_id and md5 of the entry at |file_path|.
  // 2) Gets the local path of the cache file from resource_id and md5.
  // 3) Gets PlatformFileInfo of the modified local cache file.
  // 4) Gets GDataEntry for |file_path|.
  // 5) Modifies GDataEntry using the new PlatformFileInfo.
  // 6) Commits the modification to the cache system.
  // 7) Invokes the user-supplied |callback|.
  void OnGetEntryInfoCompleteForCloseFile(
      const FilePath& file_path,
      const FileOperationCallback& callback,
      GDataFileError error,
      scoped_ptr<GDataEntryProto> entry_proto);
  void OnGetCacheFilePathCompleteForCloseFile(
      const FilePath& file_path,
      const FileOperationCallback& callback,
      GDataFileError error,
      const std::string& resource_id,
      const std::string& md5,
      const FilePath& local_cache_path);
  void OnGetModifiedFileInfoCompleteForCloseFile(
      const FilePath& file_path,
      base::PlatformFileInfo* file_info,
      bool* get_file_info_result,
      const FileOperationCallback& callback);
  void OnGetEntryCompleteForCloseFile(
      const FilePath& file_path,
      const base::PlatformFileInfo& file_info,
      const FileOperationCallback& callback,
      GDataFileError error,
      GDataEntry* entry);
  void OnCommitDirtyInCacheCompleteForCloseFile(
      const FileOperationCallback& callback,
      GDataFileError error,
      const std::string& resource_id,
      const std::string& md5);
  void OnCloseFileFinished(const FilePath& file_path,
                           const FileOperationCallback& callback,
                           GDataFileError result);

  // Invoked upon completion of GetFileByPath initiated by Copy. If
  // GetFileByPath reports no error, calls TransferRegularFile to transfer
  // |local_file_path| to |remote_dest_file_path|.
  //
  // Can be called from UI thread. |callback| is run on the calling thread.
  void OnGetFileCompleteForCopy(const FilePath& remote_dest_file_path,
                                const FileOperationCallback& callback,
                                GDataFileError error,
                                const FilePath& local_file_path,
                                const std::string& unused_mime_type,
                                GDataFileType file_type);

  // Invoked upon completion of GetFileByPath initiated by
  // TransferFileFromRemoteToLocal. If GetFileByPath reports no error, calls
  // CopyLocalFileOnBlockingPool to copy |local_file_path| to
  // |local_dest_file_path|.
  //
  // Can be called from UI thread. |callback| is run on the calling thread.
  // |callback| must not be null.
  void OnGetFileCompleteForTransferFile(const FilePath& local_dest_file_path,
                                        const FileOperationCallback& callback,
                                        GDataFileError error,
                                        const FilePath& local_file_path,
                                        const std::string& unused_mime_type,
                                        GDataFileType file_type);

  // Invoked upon completion of GetFileByPath initiated by OpenFile. If
  // GetFileByPath is successful, calls MarkDirtyInCache to mark the cache
  // file as dirty for the file identified by |file_info.resource_id| and
  // |file_info.md5|.
  //
  // Can be called from UI thread. |callback| is run on the calling thread.
  void OnGetFileCompleteForOpenFile(
      const OpenFileCallback& callback,
      const GetFileCompleteForOpenParams& file_info,
      GDataFileError error,
      const FilePath& file_path,
      const std::string& mime_type,
      GDataFileType file_type);

  // Copies a document with |resource_id| to the directory at |dir_path|
  // and names the copied document as |new_name|.
  //
  // Can be called from UI thread. |callback| is run on the calling thread.
  // |callback| must not be null.
  void CopyDocumentToDirectory(const FilePath& dir_path,
                               const std::string& resource_id,
                               const FilePath::StringType& new_name,
                               const FileOperationCallback& callback);

  // Renames a file or directory at |file_path| to |new_name| in the same
  // directory. |callback| will receive the new file path if the operation is
  // successful. If the new name already exists in the same directory, the file
  // name is uniquified by adding a parenthesized serial number like
  // "foo (2).txt"
  //
  // Can be called from UI thread. |callback| is run on the calling thread.
  // |callback| must not be null.
  void Rename(const FilePath& file_path,
              const FilePath::StringType& new_name,
              const FileMoveCallback& callback);

  // Part of Rename(). Called after GetEntryInfoByPath() is complete.
  void RenameAfterGetEntryInfo(const FilePath& file_path,
                               const FilePath::StringType& new_name,
                               const FileMoveCallback& callback,
                               GDataFileError error,
                               scoped_ptr<GDataEntryProto> entry_proto);

  // Moves a file or directory at |file_path| in the root directory to
  // another directory at |dir_path|. This function does nothing if
  // |dir_path| points to the root directory.
  //
  // Can be called from UI thread. |callback| is run on the calling thread.
  // |callback| must not be null.
  void MoveEntryFromRootDirectory(const FilePath& dir_path,
                                  const FileOperationCallback& callback,
                                  GDataFileError error,
                                  const FilePath& file_path);

  // Part of MoveEntryFromRootDirectory(). Called after
  // GetEntryInfoPairByPaths() is complete. |callback| must not be null.
  void MoveEntryFromRootDirectoryAfterGetEntryInfoPair(
    const FileOperationCallback& callback,
    scoped_ptr<EntryInfoPairResult> result);

  // Removes a file or directory at |file_path| from the directory at
  // |dir_path| and moves it to the root directory.
  //
  // Can be called from UI thread. |callback| is run on the calling thread.
  void RemoveEntryFromDirectory(const FilePath& dir_path,
                                const FileMoveCallback& callback,
                                GDataFileError error,
                                const FilePath& file_path);

  // Removes file under |file_path| from in-memory snapshot of the file system.
  // |resource_id| contains the resource id of the removed file if it was a
  // file.
  // Return PLATFORM_FILE_OK if successful.
  GDataFileError RemoveEntryFromGData(const FilePath& file_path,
                                               std::string* resource_id);

  // A pass-through callback used for bridging from
  // FileMoveCallback to FileOperationCallback.
  void OnFilePathUpdated(const FileOperationCallback& cllback,
                         GDataFileError error,
                         const FilePath& file_path);

  // Invoked upon completion of MarkDirtyInCache initiated by OpenFile. Invokes
  // |callback| with |cache_file_path|, which is the path of the cache file
  // ready for modification.
  //
  // Must be called on UI thread.
  void OnMarkDirtyInCacheCompleteForOpenFile(
      const OpenFileCallback& callback,
      GDataFileError error,
      const std::string& resource_id,
      const std::string& md5,
      const FilePath& cache_file_path);

  // Callback for handling document copy attempt.
  // |callback| must not be null.
  void OnCopyDocumentCompleted(const FilePath& dir_path,
                               const FileOperationCallback& callback,
                               GDataErrorCode status,
                               scoped_ptr<base::Value> data);

  // Callback for handling an attempt to move a file or directory from the
  // root directory to another directory on the server side. This function
  // moves |entry| to the root directory on the client side with
  // GDataDirectoryService::MoveEntryToDirectory().
  //
  // |callback| must not be null.
  void OnMoveEntryFromRootDirectoryCompleted(
      const FileOperationCallback& callback,
      const FilePath& file_path,
      const FilePath& dir_path,
      GDataErrorCode status,
      const GURL& document_url);

  // Callback for handling account metadata fetch.
  void OnGetAvailableSpace(
      const GetAvailableSpaceCallback& callback,
      GDataErrorCode status,
      scoped_ptr<base::Value> data);

  // Callback for handling Drive V2 about resource fetch.
  void OnGetAboutResource(
      const GetAvailableSpaceCallback& callback,
      GDataErrorCode status,
      scoped_ptr<base::Value> data);

  // Callback for handling document remove attempt.
  void OnRemovedDocument(
      const FileOperationCallback& callback,
      const FilePath& file_path,
      GDataErrorCode status,
      const GURL& document_url);

  // Callback for handling directory create requests.
  void OnCreateDirectoryCompleted(
      const CreateDirectoryParams& params,
      GDataErrorCode status,
      scoped_ptr<base::Value> created_entry);

  // Callback for handling file downloading requests.
  void OnFileDownloaded(
    const GetFileFromCacheParams& params,
    GDataErrorCode status,
    const GURL& content_url,
    const FilePath& downloaded_file_path);

  // Unpins file if cache entry is pinned.
  void UnpinIfPinned(const std::string& resource_id,
                     const std::string& md5,
                     bool success,
                     const GDataCacheEntry& cache_entry);

  // Similar to OnFileDownloaded() but takes |has_enough_space| so we report
  // an error in case we don't have enough disk space.
  void OnFileDownloadedAndSpaceChecked(
      const GetFileFromCacheParams& params,
      GDataErrorCode status,
      const GURL& content_url,
      const FilePath& downloaded_file_path,
      bool* has_enough_space);

  // Callback for handling internal StoreToCache() calls after downloading
  // file content.
  void OnDownloadStoredToCache(GDataFileError error,
                               const std::string& resource_id,
                               const std::string& md5);

  // Callback for handling resource rename attempt. Renames a file or
  // directory at |file_path| on in-memory snapshot of the file system.
  void RenameFileOnFileSystem(const FilePath& file_path,
                              const FilePath::StringType& new_name,
                              const FileMoveCallback& callback,
                              GDataErrorCode status,
                              const GURL& document_url);

  // Callback for handling an attempt to remove a file or directory from
  // another directory. Removes a file or directory at |file_path| and moves it
  // to root on in-memory snapshot of the file system.
  void RemoveEntryFromDirectoryOnFileSystem(
      const FileMoveCallback& callback,
      const FilePath& file_path,
      const FilePath& dir_path,
      GDataErrorCode status,
      const GURL& document_url);

  // Removes a file or directory under |file_path| from in-memory snapshot of
  // the file system and the corresponding file from cache if it exists.
  // Return PLATFORM_FILE_OK if successful.
  GDataFileError RemoveEntryFromFileSystem(const FilePath& file_path);

  // Callback for GDataDirectoryService::MoveEntryToDirectory with
  // FileMoveCallback.
  void OnMoveEntryToDirectoryWithFileMoveCallback(
      const FileMoveCallback& callback,
      GDataFileError error,
      const FilePath& moved_file_path);

  // Callback for GDataDirectoryService::MoveEntryToDirectory with
  // FileOperationCallback.
  // |callback| must not be null.
  void OnMoveEntryToDirectoryWithFileOperationCallback(
      const FileOperationCallback& callback,
      GDataFileError error,
      const FilePath& moved_file_path);

  // Callback for GetEntryByResourceIdAsync.
  // Removes stale entry upon upload of file.
  static void RemoveStaleEntryOnUpload(const std::string& resource_id,
                                       GDataDirectory* parent_dir,
                                       GDataEntry* existing_entry);

  // Converts |entry_value| into GFileDocument instance and adds it
  // to virtual file system at |directory_path|.
  GDataFileError AddNewDirectory(const FilePath& directory_path,
                                          base::Value* entry_value);

  // Given non-existing |directory_path|, finds the first missing parent
  // directory of |directory_path|.
  FindMissingDirectoryResult FindFirstMissingParentDirectory(
      const FilePath& directory_path,
      GURL* last_dir_content_url,
      FilePath* first_missing_parent_path);

  // Callback for handling results of ReloadFeedFromServerIfNeeded() initiated
  // from CheckForUpdates(). This callback checks whether feed is successfully
  // reloaded, and in case of failure, restores the content origin of the root
  // directory.
  void OnUpdateChecked(ContentOrigin initial_origin,
                       GDataFileError error,
                       GDataEntry* entry);

  // Runs the callback and notifies that the initial load is finished.
  void RunAndNotifyInitialLoadFinished(
    const FindEntryCallback& callback,
    GDataFileError error,
    GDataEntry* entry);

  // Helper function that completes bookkeeping tasks related to
  // completed file transfer.
  void OnTransferCompleted(
      const FileOperationCallback& callback,
      GDataFileError error,
      scoped_ptr<UploadFileInfo> upload_file_info);

  // Kicks off file upload once it receives |file_size| and |content_type|.
  void StartFileUploadOnUIThread(
      const StartFileUploadParams& params,
      GDataFileError* error,
      int64* file_size,
      std::string* content_type);

  // Part of StartFileUploadOnUIThread(). Called after GetEntryInfoByPath()
  // is complete.
  void StartFileUploadOnUIThreadAfterGetEntryInfo(
      const StartFileUploadParams& params,
      int64 file_size,
      std::string content_type,
      GDataFileError error,
      scoped_ptr<GDataEntryProto> entry_proto);

  // Cache intermediate callbacks, that run on calling thread, for above cache
  // tasks that were run on blocking pool.

  // Helper function for internally handling responses from
  // GetFileFromCacheByResourceIdAndMd5() calls during processing of
  // GetFileByPath() request.
  void OnGetFileFromCache(const GetFileFromCacheParams& params,
                          GDataFileError error,
                          const std::string& resource_id,
                          const std::string& md5,
                          const FilePath& cache_file_path);

  // Callback for |documents_service_->GetDocumentEntry|.
  // It is called before file download. If GetDocumentEntry was successful,
  // file download procedure is started for the file. The file is downloaded
  // from the content url extracted from the fetched metadata to
  // |cache_file_path|. Also, available space checks are done using file size
  // from the fetched metadata.
  void OnGetDocumentEntry(const FilePath& cache_file_path,
                          const GetFileFromCacheParams& params,
                          GDataErrorCode status,
                          scoped_ptr<base::Value> data);

  // Starts downloading a file if we have enough disk space indicated by
  // |has_enough_space|.
  void StartDownloadFileIfEnoughSpace(const GetFileFromCacheParams& params,
                                      const GURL& content_url,
                                      const FilePath& cache_file_path,
                                      bool* has_enough_space);

  // Helper function used to perform synchronous file search on UI thread.
  void FindEntryByPathSyncOnUIThread(const FilePath& search_file_path,
                                     const FindEntryCallback& callback);

  // Changes state of hosted documents visibility, triggers directory refresh.
  void SetHideHostedDocuments(bool hide);

  // Initializes preference change observer.
  void InitializePreferenceObserver();

  // Called when an entry is found for GetEntryInfoByPath().
  void OnGetEntryInfo(const GetEntryInfoCallback& callback,
                      GDataFileError error,
                      GDataEntry* entry);

  // Called when an entry is found for ReadDirectoryByPath().
  void OnReadDirectory(const ReadDirectoryWithSettingCallback& callback,
                       GDataFileError error,
                       GDataEntry* entry);

  // Finds file info by using virtual |file_path|. This call will also
  // retrieve and refresh file system content from server and disk cache.
  void FindEntryByPathAsyncOnUIThread(const FilePath& search_file_path,
                                      const FindEntryCallback& callback);

  // Gets |file_path| from the file system after the file info is already
  // resolved with GetEntryInfoByPath(). This function is called by
  // OnGetEntryInfoCompleteForGetFileByPath and
  // OnGetEntryInfoCompleteForOpenFile.
  void GetResolvedFileByPath(
      const FilePath& file_path,
      const GetFileCallback& get_file_callback,
      const GetDownloadDataCallback& get_download_data_callback,
      GDataFileError error,
      const GDataEntryProto* entry_proto);

  // Called when GDataCache::GetFileOnUIThread() is completed for
  // UpdateFileByResourceId().
  void OnGetFileCompleteForUpdateFile(
      const FileOperationCallback& callback,
      GDataFileError error,
      const std::string& resource_id,
      const std::string& md5,
      const FilePath& cache_file_path);

  // Callback for getting the size of the cache file in the blocking pool.
  void OnGetFileSizeCompleteForUpdateFile(
      const FileOperationCallback& callback,
      const std::string& resource_id,
      const std::string& md5,
      const FilePath& cache_file_path,
      GDataFileError* error,
      int64* file_size);

  // Callback for GDataRootDirectory::GetEntryByResourceIdAsync.
  void OnGetFileCompleteForUpdateFileByEntry(
    const FileOperationCallback& callback,
    const std::string& md5,
    int64 file_size,
    const FilePath& cache_file_path,
    GDataEntry* entry);

  // Called when GDataUploader::UploadUpdatedFile() is completed for
  // UpdateFileByResourceId().
  void OnUpdatedFileUploaded(
    const FileOperationCallback& callback,
    GDataFileError error,
    scoped_ptr<UploadFileInfo> upload_file_info);

  // The following functions are used to forward calls to asynchronous public
  // member functions to UI thread.
  void SearchAsyncOnUIThread(const std::string& search_query,
                             const GURL& next_feed,
                             const SearchCallback& callback);
  void OpenFileOnUIThread(const FilePath& file_path,
                          const OpenFileCallback& callback);
  void CloseFileOnUIThread(const FilePath& file_path,
                           const FileOperationCallback& callback);
  void CopyOnUIThread(const FilePath& src_file_path,
                      const FilePath& dest_file_path,
                      const FileOperationCallback& callback);
  void MoveOnUIThread(const FilePath& src_file_path,
                      const FilePath& dest_file_path,
                      const FileOperationCallback& callback);
  void RemoveOnUIThread(const FilePath& file_path,
                        bool is_recursive,
                        const FileOperationCallback& callback);
  void CreateDirectoryOnUIThread(const FilePath& directory_path,
                                 bool is_exclusive,
                                 bool is_recursive,
                                 const FileOperationCallback& callback);
  void CreateFileOnUIThread(const FilePath& file_path,
                            bool is_exclusive,
                            const FileOperationCallback& callback);
  void GetFileByPathOnUIThread(
      const FilePath& file_path,
      const GetFileCallback& get_file_callback,
      const GetDownloadDataCallback& get_download_data_callback);
  void GetFileByResourceIdOnUIThread(
      const std::string& resource_id,
      const GetFileCallback& get_file_callback,
      const GetDownloadDataCallback& get_download_data_callback);
  void GetFileByEntryOnUIThread(
      const GetFileCallback& get_file_callback,
      const GetDownloadDataCallback& get_download_data_callback,
      GDataEntry* entry);
  void GetEntryInfoByEntryOnUIThread(
      const GetEntryInfoWithFilePathCallback& callback,
      GDataEntry* entry);
  void UpdateFileByResourceIdOnUIThread(
      const std::string& resource_id,
      const FileOperationCallback& callback);
  void UpdateFileByEntryOnUIThread(const FileOperationCallback& callback,
      GDataEntry* entry);
  void GetEntryInfoByPathAsyncOnUIThread(
      const FilePath& file_path,
      const GetEntryInfoCallback& callback);
  void GetEntryInfoByResourceIdOnUIThread(
      const std::string& resource_id,
      const GetEntryInfoWithFilePathCallback& callback);
  void ReadDirectoryByPathAsyncOnUIThread(
      const FilePath& file_path,
      const ReadDirectoryWithSettingCallback& callback);
  void RequestDirectoryRefreshOnUIThread(
      const FilePath& file_path);
  void OnRequestDirectoryRefresh(GetDocumentsParams* params,
                                 GDataFileError error);
  void RequestDirectoryRefreshByEntry(
      const FilePath& directory_path,
      const std::string& directory_resource_id,
      const FileResourceIdMap& file_map,
      GDataEntry* directory_entry);
  void GetAvailableSpaceOnUIThread(const GetAvailableSpaceCallback& callback);
  void AddUploadedFileOnUIThread(UploadMode upload_mode,
                                 const FilePath& virtual_dir_path,
                                 scoped_ptr<DocumentEntry> entry,
                                 const FilePath& file_content_path,
                                 GDataCache::FileOperationType cache_operation,
                                 const base::Closure& callback);

  // Part of CopyOnUIThread(). Called after GetEntryInfoPairByPaths() is
  // complete. |callback| must not be null.
  void CopyOnUIThreadAfterGetEntryInfoPair(
    const FilePath& dest_file_path,
    const FileOperationCallback& callback,
    scoped_ptr<EntryInfoPairResult> result);

  // Part of MoveOnUIThread(). Called after GetEntryInfoPairByPaths() is
  // complete. |callback| must not be null.
  void MoveOnUIThreadAfterGetEntryInfoPair(
    const FilePath& dest_file_path,
    const FileOperationCallback& callback,
    scoped_ptr<EntryInfoPairResult> result);

  // Part of RemoveOnUIThread(). Called after GetEntryInfoByPath() is
  // complete.
  void RemoveOnUIThreadAfterGetEntryInfo(
    const FilePath& file_path,
    bool is_recursive,
    const FileOperationCallback& callback,
    GDataFileError error,
    scoped_ptr<GDataEntryProto> entry_proto);

  // Part of RequestDirectoryRefreshOnUIThread(). Called after
  // GetEntryInfoByPath() is complete.
  void RequestDirectoryRefreshOnUIThreadAfterGetEntryInfo(
    const FilePath& file_path,
    GDataFileError error,
    scoped_ptr<GDataEntryProto> entry_proto);

  // Part of GetEntryByResourceId and GetEntryByPath. Checks whether there is a
  // local dirty cache for the entry, and if there is, replace the
  // PlatformFileInfo part of the |entry_proto| with the locally modified info.
  void CheckLocalModificationAndRun(scoped_ptr<GDataEntryProto> entry_proto,
                                    const GetEntryInfoCallback& callback);
  void CheckLocalModificationAndRunAfterGetCacheEntry(
      scoped_ptr<GDataEntryProto> entry_proto,
      const GetEntryInfoCallback& callback,
      bool success,
      const GDataCacheEntry& cache_entry);
  void CheckLocalModificationAndRunAfterGetCacheFile(
      scoped_ptr<GDataEntryProto> entry_proto,
      const GetEntryInfoCallback& callback,
      GDataFileError error,
      const std::string& resource_id,
      const std::string& md5,
      const FilePath& local_cache_path);
  void CheckLocalModificationAndRunAfterGetFileInfo(
      scoped_ptr<GDataEntryProto> entry_proto,
      const GetEntryInfoCallback& callback,
      base::PlatformFileInfo* file_info,
      bool* get_file_info_result);

  // All members should be accessed only on UI thread. Do not post tasks to
  // other threads with base::Unretained(this).
  scoped_ptr<GDataDirectoryService> directory_service_;

  // The profile hosts the GDataFileSystem via GDataSystemService.
  Profile* profile_;

  // The cache owned by GDataSystemService.
  GDataCache* cache_;

  // The uploader owned by GDataSystemService.
  GDataUploaderInterface* uploader_;

  // The document service owned by GDataSystemService.
  DocumentsServiceInterface* documents_service_;

  // The webapps registry owned by GDataSystemService.
  DriveWebAppsRegistryInterface* webapps_registry_;

  // Periodic timer for checking updates.
  base::Timer update_timer_;

  // True if hosted documents should be hidden.
  bool hide_hosted_docs_;

  // The set of paths opened by OpenFile but not yet closed by CloseFile.
  std::set<FilePath> open_files_;

  scoped_ptr<PrefChangeRegistrar> pref_registrar_;

  // The loader is used to load the feeds.
  scoped_ptr<GDataWapiFeedLoader> feed_loader_;

  ObserverList<GDataFileSystemInterface::Observer> observers_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // WeakPtrFactory and WeakPtr bound to the UI thread.
  // Note: These should remain the last member so they'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<GDataFileSystem> ui_weak_ptr_factory_;
  // Unlike other classes, we need this as we need this to redirect a task
  // from IO thread to UI thread.
  base::WeakPtr<GDataFileSystem> ui_weak_ptr_;
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_
