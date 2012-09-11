// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_FILE_SYSTEM_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/gdata/drive_cache.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_interface.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_feed_loader.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "content/public/browser/notification_observer.h"

class PrefChangeRegistrar;

namespace base {
struct PlatformFileInfo;
class SequencedTaskRunner;
}

namespace gdata {

class DriveFunctionRemove;
class DriveResourceMetadata;
class DriveServiceInterface;
class DriveUploaderInterface;
class DriveWebAppsRegistryInterface;
struct UploadFileInfo;

// The production implementation of DriveFileSystemInterface.
class DriveFileSystem : public DriveFileSystemInterface,
                        public GDataWapiFeedLoader::Observer,
                        public content::NotificationObserver {
 public:
  DriveFileSystem(Profile* profile,
                  DriveCache* cache,
                  DriveServiceInterface* drive_service,
                  DriveUploaderInterface* uploader,
                  DriveWebAppsRegistryInterface* webapps_registry,
                  base::SequencedTaskRunner* blocking_task_runner);
  virtual ~DriveFileSystem();

  // DriveFileSystem overrides.
  virtual void Initialize() OVERRIDE;
  virtual void AddObserver(
      DriveFileSystemInterface::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      DriveFileSystemInterface::Observer* observer) OVERRIDE;
  virtual void StartInitialFeedFetch() OVERRIDE;
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
      const GetContentCallback& get_content_callback) OVERRIDE;
  virtual void GetFileByResourceId(
      const std::string& resource_id,
      const GetFileCallback& get_file_callback,
      const GetContentCallback& get_content_callback) OVERRIDE;
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
                               const FilePath& directory_path,
                               scoped_ptr<DocumentEntry> doc_entry,
                               const FilePath& file_content_path,
                               DriveCache::FileOperationType cache_operation,
                               const base::Closure& callback) OVERRIDE;
  virtual void UpdateEntryData(const std::string& resource_id,
                               const std::string& md5,
                               scoped_ptr<DocumentEntry> entry,
                               const FilePath& file_content_path,
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

  DriveResourceMetadata* ResourceMetadata() { return resource_metadata_.get(); }

  // Used in tests to load the root feed from the cache.
  void LoadRootFeedFromCacheForTesting();

  // Used in tests to update the file system from |feed_list|.
  // See also the comment at GDataWapiFeedLoader::UpdateFromFeed().
  DriveFileError UpdateFromFeedForTesting(
      const ScopedVector<DocumentFeed>& feed_list,
      int64 start_changestamp,
      int64 root_feed_changestamp);

 private:
  friend class DriveFileSystemTest;
  FRIEND_TEST_ALL_PREFIXES(DriveFileSystemTest,
                           FindFirstMissingParentDirectory);
  // Defines possible search results of FindFirstMissingParentDirectory().
  enum FindFirstMissingParentDirectoryError {
    // Target directory found, it's not a directory.
    FIND_FIRST_FOUND_INVALID,
    // Found missing directory segment while searching for given directory.
    FIND_FIRST_FOUND_MISSING,
    // Found target directory, it already exists.
    FIND_FIRST_DIRECTORY_ALREADY_PRESENT,
  };

  // The result struct for FindFirstMissingParentDirectory().
  struct FindFirstMissingParentDirectoryResult {
    FindFirstMissingParentDirectoryResult();
    ~FindFirstMissingParentDirectoryResult();

    // Initializes the struct.
    void Init(FindFirstMissingParentDirectoryError error,
              FilePath first_missing_parent_path,
              GURL last_dir_content_url);

    FindFirstMissingParentDirectoryError error;
    // The following two fields are provided when |error| is set to
    // FIND_FIRST_FOUND_MISSING. Otherwise, the two fields are undefined.

    // Suppose "drive/foo/bar/baz/qux" is being checked, and only
    // "drive/foo/bar" is present, "drive/foo/bar/baz" is the first missing
    // parent path.
    FilePath first_missing_parent_path;

    // The content URL of the last found directory. In the above example, the
    // content URL of "drive/foo/bar". Note that this value is empty if the
    // last found directory is the root of Drive.
    GURL last_dir_content_url;
  };

  // Callback for FindFirstMissingParentDirectory().
  typedef base::Callback<void(
      const FindFirstMissingParentDirectoryResult& result)>
      FindFirstMissingParentDirectoryCallback;

  // Params for FindFirstMissingParentDirectory().
  struct FindFirstMissingParentDirectoryParams;

  // Defines set of parameters passes to intermediate callbacks during
  // execution of CreateDirectory() method.
  struct CreateDirectoryParams {
    CreateDirectoryParams(const FilePath& created_directory_path,
                          const FilePath& target_directory_path,
                          bool is_exclusive,
                          bool is_recursive,
                          const FileOperationCallback& callback);
    ~CreateDirectoryParams();

    const FilePath created_directory_path;
    const FilePath target_directory_path;
    const bool is_exclusive;
    const bool is_recursive;
    FileOperationCallback callback;
  };

  // Defines set of parameters passed to an intermediate callback
  // OnGetFileCompleteForOpen, during execution of OpenFile() method.
  struct GetFileCompleteForOpenParams;

  // Defines set of parameters passed to intermediate callbacks during
  // execution of GetFileByPath() method.
  struct GetFileFromCacheParams;

  // Struct used for StartFileUploadOnUIThread().
  struct StartFileUploadParams;

  // Struct used for AddUploadedFile.
  struct AddUploadedFileParams;

  // Struct used by UpdateEntryData.
  struct UpdateEntryParams;

  // Callback passed to |LoadFeedFromServer| from |Search| method.
  // |callback| is that should be run with data received from
  // |LoadFeedFromServer|. |callback| must not be null.
  // |params| params used for getting document feed for content search.
  // |error| error code returned by |LoadFeedFromServer|.
  void OnSearch(const SearchCallback& callback,
                scoped_ptr<LoadFeedParams> params,
                DriveFileError error);

  // Callback for DriveResourceMetadata::RefreshFile, from OnSearch.
  // Adds |drive_file_path| to |results|. When |entry_proto| is not present in
  // the local file system snapshot, it is not added to |results|. Instead,
  // CheckForUpdates is called. Runs |callback| with |results|.
  // |callback| may be null.
  void AddToSearchResults(
      std::vector<SearchResultInfo>* results,
      const base::Closure& callback,
      DriveFileError error,
      const FilePath& drive_file_path,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Part of TransferFileFromLocalToRemote(). Called after
  // GetEntryInfoByPath() is complete.
  void TransferFileFromLocalToRemoteAfterGetEntryInfo(
      const FilePath& local_src_file_path,
      const FilePath& remote_dest_file_path,
      const FileOperationCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Initiates transfer of |local_file_path| with |resource_id| to
  // |remote_dest_file_path|. |local_file_path| must be a file from the local
  // file system, |remote_dest_file_path| is the virtual destination path within
  // Drive file system. If |resource_id| is a non-empty string, the transfer is
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
  // path within Drive file system.
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
  // |callback| must not be null.
  void OnGetEntryInfoForCreateFile(const FilePath& file_path,
                                   bool is_exclusive,
                                   const FileOperationCallback& callback,
                                   DriveFileError result,
                                   scoped_ptr<DriveEntryProto> entry_proto);
  void DoUploadForCreateBrandNewFile(const FilePath& remote_path,
                                     FilePath* local_path,
                                     const FileOperationCallback& callback);
  void DidUploadForCreateBrandNewFile(const FilePath& local_path,
                                      const FileOperationCallback& callback,
                                      DriveFileError result);

  // Invoked upon completion of GetEntryInfoByPath initiated by
  // GetFileByPath. It then continues to invoke GetResolvedFileByPath.
  // |get_file_callback| must not be null.
  // |get_content_callback| may be null.
  void OnGetEntryInfoCompleteForGetFileByPath(
      const FilePath& file_path,
      const GetFileCallback& get_file_callback,
      const GetContentCallback& get_content_callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> file_info);

  // Invoked upon completion of GetEntryInfoByPath initiated by OpenFile.
  // It then continues to invoke GetResolvedFileByPath and proceeds to
  // OnGetFileCompleteForOpenFile.
  void OnGetEntryInfoCompleteForOpenFile(
      const FilePath& file_path,
      const OpenFileCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> file_info);

  // Invoked at the last step of OpenFile. It removes |file_path| from the
  // current set of opened files if |result| is an error, and then invokes the
  // |callback| function.
  void OnOpenFileFinished(const FilePath& file_path,
                          const OpenFileCallback& callback,
                          DriveFileError result,
                          const FilePath& cache_file_path);

  // Invoked during the process of CloseFile. What is done here is as follows:
  // 1) Gets resource_id and md5 of the entry at |file_path|.
  // 2) Commits the modification to the cache system.
  // 3) Removes the |file_path| from the remembered set of opened files.
  // 4) Invokes the user-supplied |callback|.
  // |callback| must not be null.
  void CloseFileOnUIThreadAfterGetEntryInfo(
      const FilePath& file_path,
      const FileOperationCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);
  void CloseFileOnUIThreadAfterCommitDirtyInCache(
      const FileOperationCallback& callback,
      DriveFileError error,
      const std::string& resource_id,
      const std::string& md5);
  void CloseFileOnUIThreadFinalize(const FilePath& file_path,
                                   const FileOperationCallback& callback,
                                   DriveFileError result);

  // Invoked upon completion of GetFileByPath initiated by Copy. If
  // GetFileByPath reports no error, calls TransferRegularFile to transfer
  // |local_file_path| to |remote_dest_file_path|.
  //
  // Can be called from UI thread. |callback| is run on the calling thread.
  void OnGetFileCompleteForCopy(const FilePath& remote_dest_file_path,
                                const FileOperationCallback& callback,
                                DriveFileError error,
                                const FilePath& local_file_path,
                                const std::string& unused_mime_type,
                                DriveFileType file_type);

  // Invoked upon completion of GetFileByPath initiated by
  // TransferFileFromRemoteToLocal. If GetFileByPath reports no error, calls
  // CopyLocalFileOnBlockingPool to copy |local_file_path| to
  // |local_dest_file_path|.
  //
  // Can be called from UI thread. |callback| is run on the calling thread.
  // |callback| must not be null.
  void OnGetFileCompleteForTransferFile(const FilePath& local_dest_file_path,
                                        const FileOperationCallback& callback,
                                        DriveFileError error,
                                        const FilePath& local_file_path,
                                        const std::string& unused_mime_type,
                                        DriveFileType file_type);

  // Invoked upon completion of GetFileByPath initiated by OpenFile. If
  // GetFileByPath is successful, calls MarkDirtyInCache to mark the cache
  // file as dirty for the file identified by |file_info.resource_id| and
  // |file_info.md5|.
  //
  // Can be called from UI thread. |callback| is run on the calling thread.
  void OnGetFileCompleteForOpenFile(
      const OpenFileCallback& callback,
      const GetFileCompleteForOpenParams& file_info,
      DriveFileError error,
      const FilePath& file_path,
      const std::string& mime_type,
      DriveFileType file_type);

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
  // |callback| must not be null.
  void RenameAfterGetEntryInfo(const FilePath& file_path,
                               const FilePath::StringType& new_name,
                               const FileMoveCallback& callback,
                               DriveFileError error,
                               scoped_ptr<DriveEntryProto> entry_proto);

  // Moves a file or directory at |file_path| in the root directory to
  // another directory at |dir_path|. This function does nothing if
  // |dir_path| points to the root directory.
  //
  // Can be called from UI thread. |callback| is run on the calling thread.
  // |callback| must not be null.
  void MoveEntryFromRootDirectory(const FilePath& dir_path,
                                  const FileOperationCallback& callback,
                                  DriveFileError error,
                                  const FilePath& file_path);

  // Part of MoveEntryFromRootDirectory(). Called after
  // GetEntryInfoPairByPaths() is complete. |callback| must not be null.
  void MoveEntryFromRootDirectoryAfterGetEntryInfoPair(
    const FileOperationCallback& callback,
    scoped_ptr<EntryInfoPairResult> result);

  // Part of RemoveEntryFromNonRootDirectory(). Called after
  // GetEntryInfoPairByPaths() is complete. |callback| must not be null.
  void RemoveEntryFromNonRootDirectoryAfterEntryInfoPair(
    const FileMoveCallback& callback,
    scoped_ptr<EntryInfoPairResult> result);

  // Removes a file or directory at |file_path| from the current directory if
  // it's not in the root directory. This essentially moves an entry to the
  // root directory on the server side.
  //
  // Can be called from UI thread. |callback| is run on the calling thread.
  // |callback| must not be null.
  void RemoveEntryFromNonRootDirectory(const FileMoveCallback& callback,
                                       DriveFileError error,
                                       const FilePath& file_path);

  // A pass-through callback used for bridging from
  // FileMoveCallback to FileOperationCallback.
  void OnFilePathUpdated(const FileOperationCallback& cllback,
                         DriveFileError error,
                         const FilePath& file_path);

  // Invoked upon completion of MarkDirtyInCache initiated by OpenFile. Invokes
  // |callback| with |cache_file_path|, which is the path of the cache file
  // ready for modification.
  //
  // Must be called on UI thread.
  void OnMarkDirtyInCacheCompleteForOpenFile(const OpenFileCallback& callback,
                                             DriveFileError error,
                                             const FilePath& cache_file_path);

  // Callback for handling document copy attempt.
  // |callback| must not be null.
  void OnCopyDocumentCompleted(const FilePath& dir_path,
                               const FileOperationCallback& callback,
                               GDataErrorCode status,
                               scoped_ptr<base::Value> data);

  // Callback for handling account metadata fetch.
  void OnGetAvailableSpace(const GetAvailableSpaceCallback& callback,
                           GDataErrorCode status,
                           scoped_ptr<base::Value> data);

  // Callback for handling Drive V2 about resource fetch.
  void OnGetAboutResource(const GetAvailableSpaceCallback& callback,
                          GDataErrorCode status,
                          scoped_ptr<base::Value> data);

  // Callback for handling directory create requests. Adds the directory
  // represented by |created_entry| to the local filesystem.
  void AddNewDirectory(const CreateDirectoryParams& params,
                       GDataErrorCode status,
                       scoped_ptr<base::Value> created_entry);

  // Callback for DriveResourceMetadata::AddEntryToDirectory. Continues the
  // recursive creation of a directory path by calling CreateDirectory again.
  void ContinueCreateDirectory(
      const CreateDirectoryParams& params,
      DriveFileError error,
      const FilePath& moved_file_path);

  // Callback for handling file downloading requests.
  void OnFileDownloaded(const GetFileFromCacheParams& params,
                        GDataErrorCode status,
                        const GURL& content_url,
                        const FilePath& downloaded_file_path);

  // Unpins file if cache entry is pinned.
  void UnpinIfPinned(const std::string& resource_id,
                     const std::string& md5,
                     bool success,
                     const DriveCacheEntry& cache_entry);

  // Similar to OnFileDownloaded() but takes |has_enough_space| so we report
  // an error in case we don't have enough disk space.
  void OnFileDownloadedAndSpaceChecked(const GetFileFromCacheParams& params,
                                       GDataErrorCode status,
                                       const GURL& content_url,
                                       const FilePath& downloaded_file_path,
                                       bool* has_enough_space);

  // Callback for handling internal StoreToCache() calls after downloading
  // file content.
  void OnDownloadStoredToCache(DriveFileError error,
                               const std::string& resource_id,
                               const std::string& md5);

  // Callback for handling resource rename attempt. Renames a file or
  // directory at |file_path| on the client side.
  // |callback| must not be null.
  void RenameEntryLocally(const FilePath& file_path,
                          const FilePath::StringType& new_name,
                          const FileMoveCallback& callback,
                          GDataErrorCode status,
                          const GURL& document_url);

  // Moves entry specified by |file_path| to the directory specified by
  // |dir_path| and calls |callback| asynchronously.
  // |callback| must not be null.
  void MoveEntryToDirectory(const FilePath& file_path,
                            const FilePath& dir_path,
                            const FileMoveCallback& callback,
                            GDataErrorCode status,
                            const GURL& document_url);

  // Callback when an entry is moved to another directory on the client side.
  // Notifies the directory change and runs |callback|.
  // |callback| must not be null.
  void NotifyAndRunFileMoveCallback(
      const FileMoveCallback& callback,
      DriveFileError error,
      const FilePath& moved_file_path);

  // Callback when an entry is moved to another directory on the client side.
  // Notifies the directory change and runs |callback|.
  // |callback| must not be null.
  void NotifyAndRunFileOperationCallback(
      const FileOperationCallback& callback,
      DriveFileError error,
      const FilePath& moved_file_path);

  // FileMoveCallback for directory changes. Notifies of directory changes,
  // and runs |callback| with |error|. |callback| may be null.
  void OnDirectoryChangeFileMoveCallback(
      const FileOperationCallback& callback,
      DriveFileError error,
      const FilePath& directory_path);

  // Continues to add an uploaded file after existing entry has been deleted.
  void ContinueAddUploadedFile(scoped_ptr<AddUploadedFileParams> params,
                               DriveFileError error,
                               const FilePath& file_path);

  // Adds the uploaded file to the cache.
  void AddUploadedFileToCache(scoped_ptr<AddUploadedFileParams> params,
                              DriveFileError error,
                              const FilePath& file_path);

  // Finds the first missing parent directory of |directory_path|.
  // |callback| must not be null.
  void FindFirstMissingParentDirectory(
      const FilePath& directory_path,
      const FindFirstMissingParentDirectoryCallback& callback);

  // Helper function for FindFirstMissingParentDirectory, for recursive search
  // for first missing parent.
  void FindFirstMissingParentDirectoryInternal(
      scoped_ptr<FindFirstMissingParentDirectoryParams> params);

  // Callback for ResourceMetadata::GetEntryInfoByPath from
  // FindFirstMissingParentDirectory.
  void ContinueFindFirstMissingParentDirectory(
      scoped_ptr<FindFirstMissingParentDirectoryParams> params,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Callback for handling results of ReloadFeedFromServerIfNeeded() initiated
  // from CheckForUpdates(). This callback checks whether feed is successfully
  // reloaded, and in case of failure, restores the content origin of the root
  // directory.
  void OnUpdateChecked(ContentOrigin initial_origin,
                       DriveFileError error);

  // Notifies that the initial load is finished and runs |callback|.
  // |callback| must not be null.
  void NotifyInitialLoadFinishedAndRun(const FileOperationCallback& callback,
                                       DriveFileError error);

  // Helper function that completes bookkeeping tasks related to
  // completed file transfer.
  void OnTransferCompleted(const FileOperationCallback& callback,
                           DriveFileError error,
                           scoped_ptr<UploadFileInfo> upload_file_info);

  // Kicks off file upload once it receives |file_size| and |content_type|.
  void StartFileUploadOnUIThread(const StartFileUploadParams& params,
                                 DriveFileError* error,
                                 int64* file_size,
                                 std::string* content_type);

  // Part of StartFileUploadOnUIThread(). Called after GetEntryInfoByPath()
  // is complete.
  void StartFileUploadOnUIThreadAfterGetEntryInfo(
      const StartFileUploadParams& params,
      int64 file_size,
      std::string content_type,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Cache intermediate callbacks, that run on calling thread, for above cache
  // tasks that were run on blocking pool.

  // Helper function for internally handling responses from
  // GetFileFromCacheByResourceIdAndMd5() calls during processing of
  // GetFileByPath() request.
  void OnGetFileFromCache(const GetFileFromCacheParams& params,
                          DriveFileError error,
                          const FilePath& cache_file_path);

  // Callback for |drive_service_->GetDocumentEntry|.
  // It is called before file download. If GetDocumentEntry was successful,
  // file download procedure is started for the file. The file is downloaded
  // from the content url extracted from the fetched metadata.
  void OnGetDocumentEntry(const GetFileFromCacheParams& params,
                          GDataErrorCode status,
                          scoped_ptr<base::Value> data);

  // Check available space using file size from the fetched metadata. Called
  // from OnGetDocumentEntry after RefreshFile is complete.
  void CheckForSpaceBeforeDownload(
      const GetFileFromCacheParams& params,
      int64 file_size,
      const GURL& content_url,
      DriveFileError error,
      const FilePath& drive_file_path,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Starts downloading a file if we have enough disk space indicated by
  // |has_enough_space|.
  void StartDownloadFileIfEnoughSpace(const GetFileFromCacheParams& params,
                                      const GURL& content_url,
                                      const FilePath& cache_file_path,
                                      bool* has_enough_space);

  // Changes state of hosted documents visibility, triggers directory refresh.
  void SetHideHostedDocuments(bool hide);

  // Initializes preference change observer.
  void InitializePreferenceObserver();

  // Part of GetEntryInfoByPathOnUIThread()
  // 1) Called when the feed is loaded.
  // 2) Called when an entry is found.
  // |callback| must not be null.
  void GetEntryInfoByPathOnUIThreadAfterLoad(
      const FilePath& file_path,
      const GetEntryInfoCallback& callback,
      DriveFileError error);
  void GetEntryInfoByPathOnUIThreadAfterGetEntry(
      const GetEntryInfoCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Part of ReadDirectoryByPathOnUIThread()
  // 1) Called when the feed is loaded.
  // 2) Called when an entry is found.
  // |callback| must not be null.
  void ReadDirectoryByPathOnUIThreadAfterLoad(
      const FilePath& file_path,
      const ReadDirectoryWithSettingCallback& callback,
      DriveFileError error);
  void ReadDirectoryByPathOnUIThreadAfterRead(
      const ReadDirectoryWithSettingCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProtoVector> entries);

  // Loads the feed from the cache or the server if not yet loaded. Runs
  // |callback| upon the completion with the error code.
  // |callback| must not be null.
  void LoadFeedIfNeeded(const FileOperationCallback& callback);

  // Gets the file at |file_path| from the cache (if found in the cache),
  // or the server (if not found in the cache) after the file info is
  // already resolved with GetEntryInfoByPath() or GetEntryInfoByResourceId().
  // |get_file_callback| must not be null.
  // |get_content_callback| may be null.
  void GetResolvedFileByPath(
      const FilePath& file_path,
      const GetFileCallback& get_file_callback,
      const GetContentCallback& get_content_callback,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Part of UpdateFileByResourceId(). Called when
  // DriveDirectory::GetEntryInfoByResourceId() is complete.
  // |callback| must not be null.
  void UpdateFileByEntryInfo(
      const FileOperationCallback& callback,
      DriveFileError error,
      const FilePath& drive_file_path,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Part of UpdateFileByResourceId().
  // Called when DriveCache::GetFileOnUIThread() is completed for
  // UpdateFileByResourceId().
  // |callback| must not be null.
  void OnGetFileCompleteForUpdateFile(const FileOperationCallback& callback,
                                      const FilePath& drive_file_path,
                                      scoped_ptr<DriveEntryProto> entry_proto,
                                      DriveFileError error,
                                      const FilePath& cache_file_path);

  // Part of UpdateFileByResourceId().
  // Callback for getting the size of the cache file in the blocking pool.
  // |callback| must not be null.
  void OnGetFileSizeCompleteForUpdateFile(
      const FileOperationCallback& callback,
      const FilePath& drive_file_path,
      scoped_ptr<DriveEntryProto> entry_proto,
      const FilePath& cache_file_path,
      DriveFileError* error,
      int64* file_size);

  // Part of UpdateFileByResourceId().
  // Called when DriveUploader::UploadUpdatedFile() is completed for
  // UpdateFileByResourceId().
  // |callback| must not be null.
  void OnUpdatedFileUploaded(const FileOperationCallback& callback,
                             DriveFileError error,
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
      const GetContentCallback& get_content_callback);
  void GetFileByResourceIdOnUIThread(
      const std::string& resource_id,
      const GetFileCallback& get_file_callback,
      const GetContentCallback& get_content_callback);
  void UpdateFileByResourceIdOnUIThread(const std::string& resource_id,
                                        const FileOperationCallback& callback);
  void GetEntryInfoByPathOnUIThread(const FilePath& file_path,
                                    const GetEntryInfoCallback& callback);
  void GetEntryInfoByResourceIdOnUIThread(
      const std::string& resource_id,
      const GetEntryInfoWithFilePathCallback& callback);
  void ReadDirectoryByPathOnUIThread(
      const FilePath& file_path,
      const ReadDirectoryWithSettingCallback& callback);
  void RequestDirectoryRefreshOnUIThread(const FilePath& file_path);
  void OnRequestDirectoryRefresh(const FilePath& directory_path,
                                 scoped_ptr<LoadFeedParams> params,
                                 DriveFileError error);
  void GetAvailableSpaceOnUIThread(const GetAvailableSpaceCallback& callback);
  void AddUploadedFileOnUIThread(UploadMode upload_mode,
                                 const FilePath& directory_path,
                                 scoped_ptr<DocumentEntry> doc_entry,
                                 const FilePath& file_content_path,
                                 DriveCache::FileOperationType cache_operation,
                                 const base::Closure& callback);
  void UpdateEntryDataOnUIThread(const UpdateEntryParams& params,
                                 scoped_ptr<DocumentEntry> entry);

  // Part of CreateDirectory(). Called after
  // FindFirstMissingParentDirectory() is complete.
  // |callback| must not be null.
  void CreateDirectoryAfterFindFirstMissingPath(
      const FilePath& directory_path,
      bool is_exclusive,
      bool is_recursive,
      const FileOperationCallback& callback,
      const FindFirstMissingParentDirectoryResult& result);

  // Part of GetEntryInfoByResourceId(). Called after
  // DriveResourceMetadata::GetEntryInfoByResourceId() is complete.
  // |callback| must not be null.
  void GetEntryInfoByResourceIdAfterGetEntry(
      const GetEntryInfoWithFilePathCallback& callback,
      DriveFileError error,
      const FilePath& file_path,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Part of GetFileByResourceIdOnUIThread(). Called after
  // DriveResourceMetadata::GetEntryInfoByResourceId() is complete.
  // |get_file_callback| must not be null.
  // |get_content_callback| may be null.
  void GetFileByResourceIdAfterGetEntry(
      const GetFileCallback& get_file_callback,
      const GetContentCallback& get_content_callback,
      DriveFileError error,
      const FilePath& file_path,
      scoped_ptr<DriveEntryProto> entry_proto);

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

  // Part of RequestDirectoryRefreshOnUIThread(). Called after
  // GetEntryInfoByPath() is complete.
  void RequestDirectoryRefreshOnUIThreadAfterGetEntryInfo(
      const FilePath& file_path,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Part of UpdateEntryDataOnUIThread(). Called after RefreshFile is complete.
  void UpdateCacheEntryOnUIThread(
      const UpdateEntryParams& params,
      DriveFileError error,
      const FilePath& drive_file_path,
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
      const FilePath& local_cache_path);
  void CheckLocalModificationAndRunAfterGetFileInfo(
      scoped_ptr<DriveEntryProto> entry_proto,
      const GetEntryInfoCallback& callback,
      base::PlatformFileInfo* file_info,
      bool* get_file_info_result);

  // All members should be accessed only on UI thread. Do not post tasks to
  // other threads with base::Unretained(this).
  scoped_ptr<DriveResourceMetadata> resource_metadata_;

  // The profile hosts the DriveFileSystem via DriveSystemService.
  Profile* profile_;

  // The cache owned by DriveSystemService.
  DriveCache* cache_;

  // The uploader owned by DriveSystemService.
  DriveUploaderInterface* uploader_;

  // The document service owned by DriveSystemService.
  DriveServiceInterface* drive_service_;

  // The webapps registry owned by DriveSystemService.
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

  ObserverList<DriveFileSystemInterface::Observer> observers_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  scoped_ptr<DriveFunctionRemove> remove_function_;

  // WeakPtrFactory and WeakPtr bound to the UI thread.
  // Note: These should remain the last member so they'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveFileSystem> ui_weak_ptr_factory_;
  // Unlike other classes, we need this as we need this to redirect a task
  // from IO thread to UI thread.
  base::WeakPtr<DriveFileSystem> ui_weak_ptr_;
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_FILE_SYSTEM_H_
