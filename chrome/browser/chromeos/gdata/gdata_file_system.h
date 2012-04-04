// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_
#pragma once

#include <sys/stat.h>

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/gdata/gdata_files.h"
#include "chrome/browser/chromeos/gdata/gdata_operation_registry.h"
#include "chrome/browser/chromeos/gdata/gdata_params.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/browser/chromeos/gdata/gdata_uploader.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/notification_observer.h"

namespace base {
class WaitableEvent;
}

namespace gdata {

class DocumentsServiceInterface;
struct UploadFileInfo;

// Used for file operations like removing files.
typedef base::Callback<void(base::PlatformFileError error)>
    FileOperationCallback;

// Callback for completion of cache operation.
typedef base::Callback<void(base::PlatformFileError error,
                            const std::string& resource_id,
                            const std::string& md5)> CacheOperationCallback;

// Callback for GetFromCache.
typedef base::Callback<void(base::PlatformFileError error,
                            const std::string& resource_id,
                            const std::string& md5,
                            const FilePath& gdata_file_path,
                            const FilePath& cache_file_path)>
    GetFromCacheCallback;

// Used to get result of file search. Please note that |file| is a live
// object provided to this callback under lock. It must not be used outside
// of the callback method. This callback can be invoked on different thread
// than one that started FileFileByPath() request.
typedef base::Callback<void(base::PlatformFileError error,
                            const FilePath& directory_path,
                            GDataFileBase* file)>
    FindFileCallback;

// Used to get files from the file system.
typedef base::Callback<void(base::PlatformFileError error,
                            const FilePath& file_path,
                            const std::string& mime_type,
                            GDataFileType file_type)>
    GetFileCallback;

// Used for file operations like removing files.
typedef base::Callback<void(base::PlatformFileError error,
                            base::ListValue* feed_list)>
    GetJsonDocumentCallback;

// Used to get available space for the account from GData.
typedef base::Callback<void(base::PlatformFileError error,
                            int bytes_total,
                            int bytes_used)>
    GetAvailableSpaceCallback;

// Callback type for DocumentServiceInterface::ResumeUpload.
typedef base::Callback<void(const ResumeUploadResponse& response,
                            scoped_ptr<DocumentEntry> entry)>
    ResumeFileUploadCallback;

// Used by GDataFileSystem::GetDocumentResourceIdOnIOThreadPool to return
// the resource ID read from a document JSON file on the local file system.
typedef base::Callback<void(const std::string& resource_id)>
    GetDocumentResourceIdCallback;

// Delegate class used to deal with results synchronous read-only search
// over virtual file system.
class FindFileDelegate {
 public:
  virtual ~FindFileDelegate();

  // Called when FindFileByPathSync() completes search.
  virtual void OnDone(base::PlatformFileError error,
                      const FilePath& directory_path,
                      GDataFileBase* file) = 0;
};

// Delegate used to find a directory element for file system updates.
class ReadOnlyFindFileDelegate : public FindFileDelegate {
 public:
  ReadOnlyFindFileDelegate();

  // Returns found file.
  GDataFileBase* file() { return file_; }

 private:
  // FindFileDelegate overrides.
  virtual void OnDone(base::PlatformFileError error,
                      const FilePath& directory_path,
                      GDataFileBase* file) OVERRIDE;

  // File entry that was found.
  GDataFileBase* file_;
};

// Helper structure used for extracting key properties from GDataFile object.
struct GDataFileProperties {
  GDataFileProperties();
  ~GDataFileProperties();

  base::PlatformFileInfo file_info;
  std::string resource_id;
  std::string file_md5;
  std::string mime_type;
  GURL content_url;
  GURL edit_url;
  bool is_hosted_document;
};

// GData file system abstraction layer.
// The interface is defined to make GDataFileSystem mockable.
class GDataFileSystemInterface {
 public:
  virtual ~GDataFileSystemInterface() {}

  // Used to notify events on the file system.
  // All events are notified on UI thread.
  class Observer {
   public:
    // Triggered when the cache has been initialized.
    virtual void OnCacheInitialized() {}

    // Triggered when a file has been pinned successfully, after the cache
    // state is updated, and the callback to Pin() is run.
    virtual void OnFilePinned(const std::string& resource_id,
                              const std::string& md5) {}

    // Triggered when a file has been unpinned successfully, after the cache
    // state is updated, and the callback to Unpin() is run.
    virtual void OnFileUnpinned(const std::string& resource_id,
                                const std::string& md5) {}

    // Triggered when a content of a directory has been changed.
    // |directory_path| is a virtual directory path (/gdata/...) representing
    // changed directory.
    virtual void OnDirectoryChanged(const FilePath& directory_path) {}

    // Triggered when the file system is initially loaded.
    virtual void OnInitialLoadFinished() {}

   protected:
    virtual ~Observer() {}
  };

  // Initializes the object. This function should be called before any
  // other functions.
  virtual void Initialize() = 0;

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Enum defining origin of a cached file.
  enum CachedFileOrigin {
    CACHED_FILE_FROM_SERVER = 0,
    CACHED_FILE_LOCALLY_MODIFIED,
  };

  // Enum defining type of file operation e.g. copy or move, etc.
  // For now, it's used for StoreToCache.
  enum FileOperationType {
    FILE_OPERATION_MOVE = 0,
    FILE_OPERATION_COPY,
  };

  // Authenticates the user by fetching the auth token as
  // needed. |callback| will be run with the error code and the auth
  // token, on the thread this function is run.
  //
  // Must be called on UI thread.
  virtual void Authenticate(const AuthStatusCallback& callback) = 0;

  // Finds file info by using virtual |file_path|. This call will also
  // retrieve and refresh file system content from server and disk cache.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  virtual void FindFileByPathAsync(const FilePath& file_path,
                                   const FindFileCallback& callback) = 0;

  // Finds file info by using virtual |file_path|. This call does not initiate
  // content refreshing and will invoke one of |delegate| methods directly as
  // it executes.
  //
  // Can be called from UI/IO thread. |delegate| is run on the calling thread
  // synchronously.
  virtual void FindFileByPathSync(const FilePath& file_path,
                                  FindFileDelegate* delegate) = 0;

  // Finds file info by using |resource_id|. This call does not initiate
  // content refreshing and will invoke one of |delegate| methods directly as
  // it executes.
  //
  // Can be called from UI/IO thread. |delegate| is run on the calling thread
  // synchronously.
  virtual void FindFileByResourceIdSync(const std::string& resource_id,
                                        FindFileDelegate* delegate) = 0;

  // Initiates transfer of |local_file_path| to |remote_dest_file_path|.
  // |local_file_path| must be a file from the local file system,
  // |remote_dest_file_path| is the virtual destination path within gdata file
  // system.
  //
  // Must be called from *UI* thread. |callback| is run on the calling thread.
  virtual void TransferFile(const FilePath& local_file_path,
                            const FilePath& remote_dest_file_path,
                            const FileOperationCallback& callback) = 0;

  // Copies |src_file_path| to |dest_file_path| on the file system.
  // |src_file_path| can be a hosted document (see limitations below).
  // |dest_file_path| is expected to be of the same type of |src_file_path|
  // (i.e. if |src_file_path| is a file, |dest_file_path| will be created as
  // a file).
  //
  // This method also has the following assumptions/limitations that may be
  // relaxed or addressed later:
  // - |src_file_path| cannot be a regular file (i.e. non-hosted document)
  //   or a directory.
  // - |dest_file_path| must not exist.
  // - The parent of |dest_file_path| must already exist.
  //
  // The file entries represented by |src_file_path| and the parent directory
  // of |dest_file_path| need to be present in the in-memory representation
  // of the file system.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  virtual void Copy(const FilePath& src_file_path,
                    const FilePath& dest_file_path,
                    const FileOperationCallback& callback) = 0;

  // Moves |src_file_path| to |dest_file_path| on the file system.
  // |src_file_path| can be a file (regular or hosted document) or a directory.
  // |dest_file_path| is expected to be of the same type of |src_file_path|
  // (i.e. if |src_file_path| is a file, |dest_file_path| will be created as
  // a file).
  //
  // This method also has the following assumptions/limitations that may be
  // relaxed or addressed later:
  // - |dest_file_path| must not exist.
  // - The parent of |dest_file_path| must already exist.
  //
  // The file entries represented by |src_file_path| and the parent directory
  // of |dest_file_path| need to be present in the in-memory representation
  // of the file system.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  virtual void Move(const FilePath& src_file_path,
                    const FilePath& dest_file_path,
                    const FileOperationCallback& callback) = 0;

  // Removes |file_path| from the file system.  If |is_recursive| is set and
  // |file_path| represents a directory, we will also delete all of its
  // contained children elements. The file entry represented by |file_path|
  // needs to be present in in-memory representation of the file system that
  // in order to be removed.
  //
  // TODO(zelidrag): Wire |is_recursive| through gdata api
  // (find appropriate calls for it).
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  virtual void Remove(const FilePath& file_path,
                      bool is_recursive,
                      const FileOperationCallback& callback) = 0;

  // Creates new directory under |directory_path|. If |is_exclusive| is true,
  // an error is raised in case a directory is already present at the
  // |directory_path|. If |is_recursive| is true, the call creates parent
  // directories as needed just like mkdir -p does.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  virtual void CreateDirectory(const FilePath& directory_path,
                               bool is_exclusive,
                               bool is_recursive,
                               const FileOperationCallback& callback) = 0;

  // Gets |file_path| from the file system. The file entry represented by
  // |file_path| needs to be present in in-memory representation of the file
  // system in order to be retrieved. If the file is not cached, the file
  // will be downloaded through gdata api.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  virtual void GetFile(const FilePath& file_path,
                       const GetFileCallback& callback) = 0;

  // Gets a file for the given |resource_id| from the gdata server. Used for
  // fetching pinned-but-not-fetched files.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  virtual void GetFileForResourceId(
      const std::string& resource_id,
      const GetFileCallback& callback) = 0;

  // Gets absolute path of cache file corresponding to |gdata_file_path|.
  // Upon completion, |callback| is invoked on the same thread where this method
  // was called, with path if it exists and is accessible or empty FilePath
  // otherwise.
  virtual void GetFromCacheForPath(const FilePath& gdata_file_path,
                                   const GetFromCacheCallback& callback) = 0;

  // Obtains the list of currently active operations.
  virtual std::vector<GDataOperationRegistry::ProgressStatus>
  GetProgressStatusList() = 0;

  // Cancels ongoing operation for a given |file_path|. Returns true if
  // the operation was found and canceled.
  virtual bool CancelOperation(const FilePath& file_path) = 0;

  // Add operation observer.
  virtual void AddOperationObserver(
      GDataOperationRegistry::Observer* observer) = 0;

  // Remove operation observer.
  virtual void RemoveOperationObserver(
      GDataOperationRegistry::Observer* observer) = 0;

  // Gets the cache state of file corresponding to |resource_id| and |md5| if it
  // exists on disk.
  // Initializes cache if it has not been initialized.
  // Upon completion, |callback| is invoked on the thread where this method was
  // called with the cache state if file exists in cache or CACHE_STATE_NONE
  // otherwise.
  virtual void GetCacheState(const std::string& resource_id,
                             const std::string& md5,
                             const GetCacheStateCallback& callback) = 0;

  // Finds file object by |file_path| and returns its key |properties|.
  // Returns true if file was found.
  virtual bool GetFileInfoFromPath(const FilePath& gdata_file_path,
                                   GDataFileProperties* properties) = 0;

  // Returns the tmp sub-directory under gdata cache directory, i.e.
  // <user_profile_dir>/GCache/v1/tmp
  virtual FilePath GetGDataCacheTmpDirectory() const = 0;

  // Returns the tmp downloads sub-directory under gdata cache directory, i.e.
  // <user_profile_dir>/GCache/v1/tmp/downloads/
  virtual FilePath GetGDataTempDownloadFolderPath() const = 0;

  // Returns the tmp documents sub-directory under gdata cache directory, i.e.
  // <user_profile_dir>/GCache/v1/tmp/documents/
  virtual FilePath GetGDataTempDocumentFolderPath() const = 0;

  // Returns the pinned sub-directory under gdata cache directory, i.e.
  // <user_profile_dir>/GCache/v1/pinned
  virtual FilePath GetGDataCachePinnedDirectory() const = 0;

  // Returns the pinned sub-directory under gdata cache directory, i.e.
  // <user_profile_dir>/GCache/v1/pinned
  virtual FilePath GetGDataCachePersistentDirectory() const = 0;

  // Returns absolute path of the file if it were cached or to be cached.
  virtual FilePath GetCacheFilePath(
      const std::string& resource_id,
      const std::string& md5,
      GDataRootDirectory::CacheSubDirectoryType sub_dir_type,
      CachedFileOrigin file_orign) const = 0;

  // Fetches the user's Account Metadata to find out current quota information
  // and returns it to the callback.
  virtual void GetAvailableSpace(const GetAvailableSpaceCallback& callback) = 0;

  // Pin or unpin file.
  virtual void SetPinState(const FilePath& file_path, bool to_pin,
                           const FileOperationCallback& callback) = 0;

  // Creates a new file from |entry| under |virtual_dir_path|. Stored its
  // content from |file_content_path| into the cache.
  virtual void AddUploadedFile(const FilePath& virtual_dir_path,
                               DocumentEntry* entry,
                               const FilePath& file_content_path,
                               FileOperationType cache_operation) = 0;

  // Returns true if hosted documents should be hidden.
  virtual bool hide_hosted_documents() = 0;
};

// The production implementation of GDataFileSystemInterface.
class GDataFileSystem : public GDataFileSystemInterface,
                        public content::NotificationObserver {
 public:
  GDataFileSystem(Profile* profile,
                  DocumentsServiceInterface* documents_service);
  virtual ~GDataFileSystem();

  // Shuts down the file system on UI thread. All pending operations are
  // canceled. Most parts of shutdown happens here. The destructor is only
  // used to release objects on the IO thread.
  void ShutdownOnUIThread();

  // GDataFileSystem overrides.
  virtual void Initialize() OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual void Authenticate(const AuthStatusCallback& callback) OVERRIDE;
  virtual void FindFileByPathAsync(const FilePath& file_path,
                                   const FindFileCallback& callback) OVERRIDE;
  virtual void FindFileByPathSync(const FilePath& file_path,
                                  FindFileDelegate* delegate) OVERRIDE;
  virtual void FindFileByResourceIdSync(const std::string& resource_id,
                                        FindFileDelegate* delegate) OVERRIDE;
  virtual void TransferFile(const FilePath& local_file_path,
                            const FilePath& remote_dest_file_path,
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
  virtual void GetFile(const FilePath& file_path,
                       const GetFileCallback& callback) OVERRIDE;
  virtual void GetFileForResourceId(
      const std::string& resource_id,
      const GetFileCallback& callback) OVERRIDE;
  virtual void GetFromCacheForPath(
      const FilePath& gdata_file_path,
      const GetFromCacheCallback& callback) OVERRIDE;
  virtual std::vector<GDataOperationRegistry::ProgressStatus>
  GetProgressStatusList() OVERRIDE;
  virtual bool CancelOperation(const FilePath& file_path) OVERRIDE;
  virtual void AddOperationObserver(
      GDataOperationRegistry::Observer* observer) OVERRIDE;
  virtual void RemoveOperationObserver(
      GDataOperationRegistry::Observer* observer) OVERRIDE;
  virtual void GetCacheState(const std::string& resource_id,
                             const std::string& md5,
                             const GetCacheStateCallback& callback) OVERRIDE;
  virtual bool GetFileInfoFromPath(const FilePath& gdata_file_path,
                                   GDataFileProperties* properties) OVERRIDE;
  virtual FilePath GetGDataCacheTmpDirectory() const OVERRIDE;
  virtual FilePath GetGDataTempDownloadFolderPath() const OVERRIDE;
  virtual FilePath GetGDataTempDocumentFolderPath() const OVERRIDE;
  virtual FilePath GetGDataCachePinnedDirectory() const OVERRIDE;
  virtual FilePath GetGDataCachePersistentDirectory() const OVERRIDE;
  virtual FilePath GetCacheFilePath(
      const std::string& resource_id,
      const std::string& md5,
      GDataRootDirectory::CacheSubDirectoryType sub_dir_type,
      CachedFileOrigin file_orign) const OVERRIDE;
  virtual void GetAvailableSpace(
      const GetAvailableSpaceCallback& callback) OVERRIDE;
  // Calls private Pin or Unpin methods with |callback|.
  virtual void SetPinState(const FilePath& file_path, bool pin,
                           const FileOperationCallback& callback) OVERRIDE;
  virtual void AddUploadedFile(const FilePath& virtual_dir_path,
                               DocumentEntry* entry,
                               const FilePath& file_content_path,
                               FileOperationType cache_operation) OVERRIDE;
  virtual bool hide_hosted_documents() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class GDataUploader;
  friend class GDataFileSystemTest;
  FRIEND_TEST_ALL_PREFIXES(GDataFileSystemTest,
                           FindFirstMissingParentDirectory);
  FRIEND_TEST_ALL_PREFIXES(GDataFileSystemTest,
                           GetGDataFileInfoFromPath);
  FRIEND_TEST_ALL_PREFIXES(GDataFileSystemTest,
                           GetFromCacheForPath);
  FRIEND_TEST_ALL_PREFIXES(GDataFileSystemTest,
                           GetAvailableSpace);

  // Defines possible search results of FindFirstMissingParentDirectory().
  enum FindMissingDirectoryResult {
    // Target directory found, it's not a directory.
    FOUND_INVALID,
    // Found missing directory segment while searching for given directory.
    FOUND_MISSING,
    // Found target directory, it already exists.
    DIRECTORY_ALREADY_PRESENT,
  };

  // Document feed chunk type.
  enum FeedChunkType {
    // The very first part of content we fetch (i.e. first 200 items),
    FEED_CHUNK_INITIAL,
    // The rest of the feed that excludes FEED_CHUNK_INITIAL,
    FEED_CHUNK_REST
  };

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

  // Defines set of parameters passed to intermediate callbacks during
  // execution of GetFile() method.
  struct GetFileFromCacheParams {
    GetFileFromCacheParams(const FilePath& virtual_file_path,
        const FilePath& local_tmp_path,
        const GURL& content_url,
        const std::string& resource_id,
        const std::string& md5,
        const std::string& mime_type,
        scoped_refptr<base::MessageLoopProxy> proxy,
        const GetFileCallback& callback);
    ~GetFileFromCacheParams();

    FilePath virtual_file_path;
    FilePath local_tmp_path;
    GURL content_url;
    std::string resource_id;
    std::string md5;
    std::string mime_type;
    scoped_refptr<base::MessageLoopProxy> proxy;
    const GetFileCallback callback;
  };

  // Defines set of parameters sent to callback OnGetDocuments().
  struct GetDocumentsParams {
    GetDocumentsParams(const FilePath& search_file_path,
                       FeedChunkType chunk_type,
                       int largest_changestamp,
                       base::ListValue* feed_list,
                       const FindFileCallback& callback);
    ~GetDocumentsParams();

    FilePath search_file_path;
    GDataFileSystem::FeedChunkType chunk_type;
    int largest_changestamp;
    scoped_ptr<base::ListValue> feed_list;
    FindFileCallback callback;
  };

  // Defines set of parameters sent to callback OnGetDocuments().
  struct LoadRootFeedParams {
    LoadRootFeedParams(FilePath search_file_path,
                       FeedChunkType chunk_type,
                       int largest_changestamp,
                       bool should_load_from_server,
                       const FindFileCallback& callback);
    ~LoadRootFeedParams();

    FilePath search_file_path;
    GDataFileSystem::FeedChunkType chunk_type;
    int largest_changestamp;
    bool should_load_from_server;
    const FindFileCallback callback;
  };

  // Callback similar to FileOperationCallback but with a given |file_path|.
  typedef base::Callback<void(base::PlatformFileError error,
                              const FilePath& file_path)>
      FilePathUpdateCallback;

  // Returns a WeakPtr for the current thread.
  base::WeakPtr<GDataFileSystem> GetWeakPtrForCurrentThread();

  // Finds file object by |file_path| and returns the file info.
  // Returns NULL if it does not find the file.
  GDataFileBase* GetGDataFileInfoFromPath(const FilePath& file_path);

  // Initiates upload operation of file defined with |file_name|,
  // |content_type| and |content_length|. The operation will place the newly
  // created file entity into |destination_directory|.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  void InitiateUpload(const std::string& file_name,
                      const std::string& content_type,
                      int64 content_length,
                      const FilePath& destination_directory,
                      const FilePath& virtual_path,
                      const InitiateUploadCallback& callback);

  // Resumes upload operation for chunk of file defined in |params..
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  void ResumeUpload(const ResumeUploadParams& params,
                    const ResumeFileUploadCallback& callback);

  // Unsafe (unlocked) version of FindFileByPathSync method.
  void UnsafeFindFileByPath(const FilePath& file_path,
                            FindFileDelegate* delegate);

  // Converts document feed from gdata service into DirectoryInfo. On failure,
  // returns NULL and fills in |error| with an appropriate value.
  GDataDirectory* ParseGDataFeed(GDataErrorCode status,
                                 base::Value* data,
                                 base::PlatformFileError *error);

  // Checks if a local file at |local_file_path| is a JSON file referencing a
  // hosted document on IO thread poll, and if so, gets the resource ID of the
  // document.
  static void GetDocumentResourceIdOnIOThreadPool(
      const FilePath& local_file_path,
      std::string* resource_id);

  // Creates a temporary JSON file representing a document with |edit_url|
  // and |resource_id| under |document_dir| on IO thread pool.
  static void CreateDocumentJsonFileOnIOThreadPool(
      const FilePath& document_dir,
      const GURL& edit_url,
      const std::string& resource_id,
      base::PlatformFileError* error,
      FilePath* temp_file_path,
      std::string* mime_type,
      GDataFileType* file_type);

  // Initiates transfer of |local_file_path| with |resource_id| to
  // |remote_dest_file_path|. |local_file_path| must be a file from the local
  // file system, |remote_dest_file_path| is the virtual destination path within
  // gdata file system. If |resource_id| is a non-empty string, the transfer is
  // handled by CopyDocumentToDirectory. Otherwise, the transfer is handled by
  // TransferRegularFile.
  //
  // Must be called from *UI* thread. |callback| is run on the calling thread.
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

  // Invoked upon completion of GetFile initiated by Copy. If GetFile
  // reports no error, calls TransferRegularFile to transfer |local_file_path|
  // to |remote_dest_file_path|.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  void OnGetFileCompleteForCopy(const FilePath& remote_dest_file_path,
                                const FileOperationCallback& callback,
                                base::PlatformFileError error,
                                const FilePath& local_file_path,
                                const std::string& unused_mime_type,
                                GDataFileType file_type);

  // Copies a document with |resource_id| to the directory at |dir_path|
  // and names the copied document as |new_name|.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  void CopyDocumentToDirectory(const FilePath& dir_path,
                               const std::string& resource_id,
                               const FilePath::StringType& new_name,
                               const FileOperationCallback& callback);

  // Renames a file or directory at |file_path| to |new_name|.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  void Rename(const FilePath& file_path,
              const FilePath::StringType& new_name,
              const FilePathUpdateCallback& callback);

  // Adds a file or directory at |file_path| to the directory at |dir_path|.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  void AddFileToDirectory(const FilePath& dir_path,
                          const FileOperationCallback& callback,
                          base::PlatformFileError error,
                          const FilePath& file_path);

  // Removes a file or directory at |file_path| from the directory at
  // |dir_path| and moves it to the root directory.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  void RemoveFileFromDirectory(const FilePath& dir_path,
                               const FilePathUpdateCallback& callback,
                               base::PlatformFileError error,
                               const FilePath& file_path);

  // Removes file under |file_path| from in-memory snapshot of the file system.
  // |resource_id| contains the resource id of the removed file if it was a
  // file.
  // Return PLATFORM_FILE_OK if successful.
  base::PlatformFileError RemoveFileFromGData(const FilePath& file_path,
                                              std::string* resource_id);

  // Callback for handling feed content fetching while searching for file info.
  // This callback is invoked after async feed fetch operation that was
  // invoked by StartDirectoryRefresh() completes. This callback will update
  // the content of the refreshed directory object and continue initially
  // started FindFileByPath() request.
  void OnGetDocuments(GetDocumentsParams* params,
                      GDataErrorCode status,
                      scoped_ptr<base::Value> data);

  // A pass-through callback used for bridging from
  // FilePathUpdateCallback to FileOperationCallback.
  void OnFilePathUpdated(const FileOperationCallback& cllback,
                         base::PlatformFileError error,
                         const FilePath& file_path);

  // Callback for handling resource rename attempt.
  void OnRenameResourceCompleted(const FilePath& file_path,
                                 const FilePath::StringType& new_name,
                                 const FilePathUpdateCallback& callback,
                                 GDataErrorCode status,
                                 const GURL& document_url);

  // Callback for handling document copy attempt.
  void OnCopyDocumentCompleted(const FilePathUpdateCallback& callback,
                               GDataErrorCode status,
                               scoped_ptr<base::Value> data);

  // Callback for handling an attempt to add a file or directory to another
  // directory.
  void OnAddFileToDirectoryCompleted(const FileOperationCallback& callback,
                                     const FilePath& file_path,
                                     const FilePath& dir_path,
                                     GDataErrorCode status,
                                     const GURL& document_url);

  // Callback for handling an attempt to remove a file or directory from
  // another directory.
  void OnRemoveFileFromDirectoryCompleted(
      const FilePathUpdateCallback& callback,
      const FilePath& file_path,
      const FilePath& dir_path,
      GDataErrorCode status,
      const GURL& document_url);

  // Callback for handling account metadata fetch.
  void OnGetAvailableSpace(
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

  // Callback for handling internal StoreToCache() calls after downloading
  // file content.
  void OnDownloadStoredToCache(base::PlatformFileError error,
                               const std::string& resource_id,
                               const std::string& md5);

  // Callback for handling file upload initialization requests.
  void OnUploadLocationReceived(
      const InitiateUploadCallback& callback,
      scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
      GDataErrorCode code,
      const GURL& upload_location);

  // Callback for handling file upload resume requests.
  void OnResumeUpload(
      scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
      const ResumeFileUploadCallback& callback,
      const ResumeUploadResponse& response,
      scoped_ptr<DocumentEntry> new_entry);

  // Renames a file or directory at |file_path| on in-memory snapshot
  // of the file system. Returns PLATFORM_FILE_OK if successful.
  base::PlatformFileError RenameFileOnFilesystem(
      const FilePath& file_path, const FilePath::StringType& new_name,
      FilePath* updated_file_path);

  // Adds a file or directory at |file_path| to another directory at
  // |dir_path| on in-memory snapshot of the file system.
  // Returns PLATFORM_FILE_OK if successful.
  base::PlatformFileError AddFileToDirectoryOnFilesystem(
      const FilePath& file_path, const FilePath& dir_path);

  // Restores account metadata and root feed from cache.
  void RestoreRootFeedFromCache(const FilePath& search_file_path,
                                const FindFileCallback& callback);

  // Helper function for loading account metadata during cache restore.
  void OnLoadCachedMetadata(const FilePath& search_file_path,
                            const FindFileCallback& callback,
                            base::PlatformFileError* error,
                            base::Value* metadata_value);

  // Removes a file or directory at |file_path| from another directory at
  // |dir_path| on in-memory snapshot of the file system.
  // Returns PLATFORM_FILE_OK if successful.
  base::PlatformFileError RemoveFileFromDirectoryOnFilesystem(
      const FilePath& file_path, const FilePath& dir_path,
      FilePath* updated_file_path);

  // Removes file under |file_path| from in-memory snapshot of the file system
  // and the corresponding file from cache if it exists.
  // Return PLATFORM_FILE_OK if successful.
  base::PlatformFileError RemoveFileFromFileSystem(const FilePath& file_path);

  // Parses the content of |feed_data| and returns DocumentFeed instance
  // represeting it.
  DocumentFeed* ParseDocumentFeed(base::Value* feed_data);

  // Updates whole directory structure feeds collected in |feed_list|.
  // On success, returns PLATFORM_FILE_OK. Record file statistics as UMA
  // histograms if |should_record_statistics| is true.
  base::PlatformFileError UpdateDirectoryWithDocumentFeed(
      base::ListValue* feed_list,
      ContentOrigin origin,
      bool should_record_statistics,
      int largest_changestamp);

  // Converts |entry_value| into GFileDocument instance and adds it
  // to virtual file system at |directory_path|.
  base::PlatformFileError AddNewDirectory(const FilePath& directory_path,
                                          base::Value* entry_value);

  // Given non-existing |directory_path|, finds the first missing parent
  // directory of |directory_path|.
  FindMissingDirectoryResult FindFirstMissingParentDirectory(
      const FilePath& directory_path,
      GURL* last_dir_content_url,
      FilePath* first_missing_parent_path);

  // Retreives account metadata and determines from the last change timestamp
  // if the feed content loading from the server needs to be initiated.
  void ReloadFeedFromServerIfNeeded(const FilePath& search_file_path,
                                    ContentOrigin initial_origin,
                                    const FindFileCallback& callback);

  // Helper callback for handling results of metadata retrieval initiated from
  // ReloadFeedFromServerIfNeeded(). This method makes a decision about fetching
  // the content of the root feed during the root directory refresh process.
  void OnGetAccountMetadata(const FilePath& search_file_path,
                            ContentOrigin initial_origin,
                            const FindFileCallback& callback,
                            GDataErrorCode error,
                            scoped_ptr<base::Value> feed_data);

  // Starts root feed load from the server. If successful, it will try to find
  // the file upon retrieval completion.
  void LoadFeedFromServer(const FilePath& search_file_path,
                          int largest_changestamp,
                          const FindFileCallback& callback);

  // Returns file name for cached feed given feed |chunk_type|.
  FilePath GetCachedFeedFileName(FeedChunkType chunk_type) const;

  // Starts root feed load from the cache. If successful, it will try to find
  // the file upon retrieval completion. In addition to that, it will
  // initate retrieval of the root feed from the server if
  // |should_load_from_server| is set.
  void LoadRootFeedFromCache(FeedChunkType chunk_type,
                             int largest_changestamp,
                             const FilePath& search_file_path,
                             bool should_load_from_server,
                             const FindFileCallback& callback);

  // Loads json file content content from |file_path| on IO thread pool.
  static void LoadJsonFileOnIOThreadPool(const FilePath& meta_cache_path,
                                         base::PlatformFileError* error,
                                         base::Value* result);

  // Callback for handling root directory refresh from the cache.
  void OnLoadRootFeed(LoadRootFeedParams* params,
                      base::PlatformFileError* error,
                      base::Value* feed_list_value);

  // Saves a collected feed in GCache directory under
  // <user_profile_dir>/GCache/v1/meta/|name| for later reloading when offline.
  void SaveFeed(scoped_ptr<base::Value> feed_vector,
                const FilePath& name);
  static void SaveFeedOnIOThreadPool(
      const FilePath& meta_cache_path,
      scoped_ptr<base::Value> feed_vector,
      const FilePath& name);

  // Finds and returns upload url of a given directory. Returns empty url
  // if directory can't be found.
  GURL GetUploadUrlForDirectory(const FilePath& destination_directory);

  // Notifies events to observers on UI thread.
  void NotifyCacheInitialized();
  void NotifyFilePinned(const std::string& resource_id,
                        const std::string& md5);
  void NotifyFileUnpinned(const std::string& resource_id,
                          const std::string& md5);
  void NotifyDirectoryChanged(const FilePath& directory_path);
  void NotifyInitialLoadFinished();

  // Helper function that completes bookkeeping tasks related to
  // completed file transfer.
  void OnTransferCompleted(
      const FileOperationCallback& callback,
      base::PlatformFileError error,
      UploadFileInfo* upload_file_info);

  // Kicks off file upload once it receives |upload_file_info|.
  void StartFileUploadOnUIThread(
      const FileOperationCallback& callback,
      base::PlatformFileError* error,
      UploadFileInfo* upload_file_info);

  // Reads properties of |local_file| and fills in values of UploadFileInfo.
  static void CreateUploadFileInfoOnIOThreadPool(
      const FilePath& local_file,
      const FilePath& remote_dest_file,
      base::PlatformFileError* error,
      UploadFileInfo* upload_file_info);

  // Cache entry points from within GDataFileSystem.
  // The functionalities of GData blob cache include:
  // - stores downloaded gdata files on disk, indexed by the resource_id and md5
  //   of the gdata file.
  // - provides absolute path for files to be cached or cached.
  // - updates the cached file on disk after user has edited it locally
  // - handles eviction when disk runs out of space
  // - uploads dirty files to gdata server.
  // - etc.

  // Checks if file corresponding to |resource_id| and |md5| exists in cache.
  // Initializes cache if it has not been initialized.
  // Upon completion, |callback| is invoked on the thread where this method was
  // called, with the cache file path if it exists or empty otherwise.
  // otherwise.
  void GetFromCache(const std::string& resource_id,
                    const std::string& md5,
                    const GetFromCacheCallback& callback);

  // Stores |source_path| corresponding to |resource_id| and |md5| to cache.
  // |file_operation_type| specifies if |source_path| is to be moved or copied.
  // Initializes cache if it has not been initialized.
  // If file was previously pinned, it is stored in persistent directory, with
  // the symlink in pinned dir updated to point to this new file (refer to
  // comments for Pin for explanation of symlinks for pinned files).
  // Otherwise, the file is stored in tmp dir.
  // Upon completion, |callback| is invoked on the thread where this method was
  // called.
  void StoreToCache(const std::string& resource_id,
                    const std::string& md5,
                    const FilePath& source_path,
                    FileOperationType file_operation_type,
                    const CacheOperationCallback& callback);

  // Pin file corresponding to |resource_id| and |md5|.
  // Initializes cache if it has not been initialized.
  // Pinned files have symlinks in pinned dir, that reference actual blob files
  // downloaded from server or locally modified in persistent dir.
  // If the file to be pinned does not exist in cache, a special symlink (with
  // target /dev/null) is created in pinned dir, and base::PLATFORM_FILE_OK is
  // be returned in |callback|.
  // So unless there's an error with file operations involving pinning, no
  // error, i.e. base::PLATFORM_FILE_OK, will be returned in |callback|.
  // GDataSyncClient will pick up these special symlinks during low time and
  // download pinned non-existent files.
  // We'll try not to evict pinned cache files unless there's not enough space
  // on disk and pinned files are the only ones left.
  // Upon completion, |callback| is invoked on the thread where this method was
  // called.
  void Pin(const std::string& resource_id,
           const std::string& md5,
           const CacheOperationCallback& callback);

  // Unpin file corresponding to |resource_id| and |md5|, opposite of Pin.
  // Initializes cache if it has not been initialized.
  // If the file was pinned, delete the symlink in pinned dir, and if file blob
  // exists, move it to tmp directory.
  // If the file is not known to cache i.e. wasn't pinned or cached,
  // base::PLATFORM_FILE_ERROR_NOT_FOUND is returned to |callback|.
  // Unpinned files would be evicted when space on disk runs out.
  // Upon completion, |callback| is invoked on the thread where this method was
  // called.
  void Unpin(const std::string& resource_id,
             const std::string& md5,
             const CacheOperationCallback& callback);

  // Mark file corresponding to |resource_id| and |md5| as dirty, so that it
  // can modified locally.
  // Initializes cache if it has not been initialized.
  // Dirty files are actual blob files in persistent dir with .local extension.
  // If the file to be marked dirty does not exist in cache,
  // base::PLATFORM_FILE_ERROR_NOT_FOUND is returned in |callback|.
  // If a file is already dirty (i.e. MarkDirtyInCache was called before), and
  // if outgoing symlink was already created (i.e CommitDirtyInCache was also
  // called before, refer to comments for CommitDirtyInCache), outgoing symlink
  // is deleted. Otherwise, it's a no-operation.
  // We'll not evict dirty files.
  // Upon completion, |callback| is invoked on the thread where this method was
  // called, with the absolute path of the dirty file.
  void MarkDirtyInCache(const std::string& resource_id,
                        const std::string& md5,
                        const GetFromCacheCallback& callback);

  // Commit dirty the file corresponding to |resource_id| and |md5|.
  // Must be called after MarkDirtyInCache to indicate that file modification
  // has completed and file is ready for uploading.
  // Initializes cache if it has not been initialized.
  // Committed dirty files have symlinks in outgoing dir, that reference actual
  // modified blob files in persistent dir.
  // If the file to be committed dirty does not exist in cache,
  // base::PLATFORM_FILE_ERROR_NOT_FOUND is returned in |callback|.
  // If the file is not marked dirty (via MarkDirtyInCache),
  // base::PLATFORM_FILE_ERROR_INVALID_OPERATION is returned in |callback|.
  // An uploader will pick up symlinks in outgoing dir and upload the dirty
  // files they reference.
  // Upon completion, |callback| is invoked on the thread where this method was
  // called.
  void CommitDirtyInCache(const std::string& resource_id,
                          const std::string& md5,
                          const CacheOperationCallback& callback);

  // Clear a dirty file corresponding to |resource_id| and |md5|.
  // |md5| is also the new extension for the file in cache.
  // Must be called after MarkDirtyInCache and CommitDirtyInCache to clear
  // dirty state of file, i.e. after dirty file has been uploaded and new md5
  // received from server.
  // Initializes cache if it has not been initialized.
  // If the file was dirty, delete the symlink in outgoing dir, move file to
  // tmp if it's unpinned, and rename filename extension to .<md5>.
  // If the file to be cleared does not exist in cache,
  // base::PLATFORM_FILE_ERROR_NOT_FOUND is returned in |callback|.
  // If the file is not marked dirty (via MarkDirtyInCache),
  // base::PLATFORM_FILE_ERROR_INVALID_OPERATION is returned in |callback|.
  // Files that are not dirty would be evicted when space on disk runs out.
  // Upon completion, |callback| is invoked on the thread where this method was
  // called.
  void ClearDirtyInCache(const std::string& resource_id,
                         const std::string& md5,
                         const CacheOperationCallback& callback);

  // Removes all files corresponding to |resource_id| from cache persistent,
  // tmp and pinned directories and in-memory cache map.
  // Initializes cache if it has not been initialized.
  // Upon completion, |callback| is invoked on the thread where this method was
  // called.
  void RemoveFromCache(const std::string& resource_id,
                       const CacheOperationCallback& callback);

  // Initializes cache if it hasn't been initialized by posting
  // InitializeCacheOnIOThreadPool task to IO thread pool.
  void InitializeCacheIfNecessary();

  // Cache tasks that run on IO thread pool, posted from above cache entry
  // points.

  // Task posted from InitializeCacheIfNecessary to run on IO thread pool.
  // Creates cache directory and its sub-directories if they don't exist,
  // or scans blobs sub-directory for files and their attributes and updates the
  // info into cache map.
  void InitializeCacheOnIOThreadPool();

  // Task posted from GetFromCacheInternal to run on IO thread pool.
  // Checks if file corresponding to |resource_id| and |md5| exists in cache
  // map.
  // Even though this task doesn't involve IO operations, it still runs on the
  // IO thread pool, to force synchronization of all tasks on IO thread pool,
  // e.g. this absolute must execute after InitailizeCacheOnIOTheadPool.
  void GetFromCacheOnIOThreadPool(
      const std::string& resource_id,
      const std::string& md5,
      const FilePath& gdata_file_path,
      base::PlatformFileError* error,
      FilePath* cache_file_path);

  // Task posted from GetCacheState to run on IO thread pool.
  // Checks if file corresponding to |resource_id| and |md5| exists in cache
  // map.  If yes, returns its cache state; otherwise, returns CACHE_STATE_NONE.
  // Even though this task doesn't involve IO operations, it still runs on the
  // IO thread pool, to force synchronization of all tasks on IO thread pool,
  // e.g. this absolutely must execute after InitailizeCacheOnIOTheadPool.
  void GetCacheStateOnIOThreadPool(
      const std::string& resource_id,
      const std::string& md5,
      base::PlatformFileError* error,
      int* cache_state);

  // Task posted from StoreToCache to run on IO thread pool:
  // - moves or copies (per |file_operation_type|) |source_path|
  //   to |dest_path| in the cache dir
  // - if necessary, creates symlink
  // - deletes stale cached versions of |resource_id| in
  //   |dest_path|'s directory.
  void StoreToCacheOnIOThreadPool(
      const std::string& resource_id,
      const std::string& md5,
      const FilePath& source_path,
      FileOperationType file_operation_type,
      base::PlatformFileError* error);

  // Task posted from Pin to modify cache state on the IO thread pool, which
  // involves the following:
  // - moves |source_path| to |dest_path| in persistent dir if
  //   file is not dirty
  // - creates symlink in pinned dir that references downloaded or locally
  //   modified file
  void PinOnIOThreadPool(const std::string& resource_id,
                         const std::string& md5,
                         FileOperationType file_operation_type,
                         base::PlatformFileError* error);

  // Task posted from Unpin to modify cache state on the IO thread pool, which
  // involves the following:
  // - moves |source_path| to |dest_path| in tmp dir if file is
  //   not dirty
  // - deletes symlink from pinned dir
  void UnpinOnIOThreadPool(const std::string& resource_id,
                           const std::string& md5,
                           FileOperationType file_operation_type,
                           base::PlatformFileError* error);

  // Task posted from MarkDirtyInCache to modify cache state on the IO thread
  // pool, which involves the following:
  // - moves |source_path| to |dest_path| in persistent dir, where
  //   |source_path| has .<md5> extension and |dest_path| has .local extension
  // - if file is pinned, updates symlink in pinned dir to reference dirty file
  void MarkDirtyInCacheOnIOThreadPool(const std::string& resource_id,
                                      const std::string& md5,
                                      FileOperationType file_operation_type,
                                      base::PlatformFileError* error,
                                      FilePath* cache_file_path);

  // Task posted from CommitDirtyInCache to modify cache state on the IO thread
  // pool, i.e. creates symlink in outgoing dir to reference dirty file in
  // persistent dir.
  void CommitDirtyInCacheOnIOThreadPool(const std::string& resource_id,
                                        const std::string& md5,
                                        FileOperationType file_operation_type,
                                        base::PlatformFileError* error);

  // Task posted from ClearDirtyInCache to modify cache state on the IO thread
  // pool, which involves the following:
  // - moves |source_path| to |dest_path| in persistent dir if
  //   file is pinned or tmp dir otherwise, where |source_path| has .local
  //   extension and |dest_path| has .<md5> extension
  // - deletes symlink in outgoing dir
  // - if file is pinned, updates symlink in pinned dir to reference
  //   |dest_path|
  void ClearDirtyInCacheOnIOThreadPool(const std::string& resource_id,
                                       const std::string& md5,
                                       FileOperationType file_operation_type,
                                       base::PlatformFileError* error);

  // Task posted from RemoveFromCache to do the following on the IO thread pool:
  // - remove all delete stale cache versions corresponding to |resource_id| in
  //   persistent, tmp and pinned directories
  // - remove entry corresponding to |resource_id| from cache map.
  void RemoveFromCacheOnIOThreadPool(const std::string& resource_id,
                                     base::PlatformFileError* error);

  // Cache intermediate callbacks, that run on calling thread, for above cache
  // tasks that were run on IO thread pool.

  // Callback for Pin. Runs |callback| and notifies the observers.
  void OnFilePinned(base::PlatformFileError* error,
                    const std::string& resource_id,
                    const std::string& md5,
                    const CacheOperationCallback& callback);

  // Callback for Unpin. Runs |callback| and notifies the observers.
  void OnFileUnpinned(base::PlatformFileError* error,
                      const std::string& resource_id,
                      const std::string& md5,
                      const CacheOperationCallback& callback);

  // Helper function for internally handling responses from GetFromCache()
  // calls during processing of GetFile() request.
  void OnGetFileFromCache(const GetFileFromCacheParams& params,
                          base::PlatformFileError error,
                          const std::string& resource_id,
                          const std::string& md5,
                          const FilePath& gdata_file_path,
                          const FilePath& cache_file_path);

  // Cache internal helper functions.

  // Unsafe (unlocked) version of InitializeCacheIfnecessary method.
  void UnsafeInitializeCacheIfNecessary();

  // Scans cache subdirectory |sub_dir_type| and build or update |cache_map|
  // with found file blobs or symlinks.
  void ScanCacheDirectory(
      GDataRootDirectory::CacheSubDirectoryType sub_dir_type,
      GDataRootDirectory::CacheMap* cache_map);

  // Called from GetFromCache and GetFromCacheForPath.
  void GetFromCacheInternal(const std::string& resource_id,
                            const std::string& md5,
                            const FilePath& gdata_file_path,
                            const GetFromCacheCallback& callback);

  // Wrapper task around any sequenced task that runs on IO thread pool that
  // makes sure |in_shutdown_| and |on_io_completed_| are handled properly in
  // the right order.
  void RunTaskOnIOThreadPool(const base::Closure& task);

  // Wrapper around BrowserThread::PostTask to post
  // RunTaskOnIOThreadPool task to the blocking thread pool.
  // TODO(satorux): As of now, it's posting to FILE thread.
  void PostBlockingPoolSequencedTask(
      const std::string& sequence_token_name,
      const tracked_objects::Location& from_here,
      const base::Closure& task);

  // Similar to PostBlockingPoolSequencedTask() but this one takes a reply
  // callback that runs on the calling thread.
  // TODO(satorux): As of now, it's posting to FILE thread.
  void PostBlockingPoolSequencedTaskAndReply(
    const std::string& sequence_token_name,
    const tracked_objects::Location& from_here,
    const base::Closure& request_task,
    const base::Closure& reply_task);

  // Helper function used to perform file search on the calling thread of
  // FindFileByPath() request.
  void FindFileByPathOnCallingThread(const FilePath& search_file_path,
                                     const FindFileCallback& callback);

  void OnSetPinStateCompleted(const FileOperationCallback& callback,
                              base::PlatformFileError error,
                              const std::string& resource_id,
                              const std::string& md5);

  // Changes state of hosted documents visibility, triggers directory refresh.
  void SetHideHostedDocuments(bool hide);

  // Initializes preference change observer.
  void InitializePreferenceObserver();

  scoped_ptr<GDataRootDirectory> root_;

  // This guards regular states.
  base::Lock lock_;

  // The profile hosts the GDataFileSystem via GDataSystemService.
  Profile* profile_;

  // The document service for the GDataFileSystem.
  scoped_ptr<DocumentsServiceInterface> documents_service_;

  // Base path for GData cache, e.g. <user_profile_dir>/user/GCache/v1.
  FilePath gdata_cache_path_;

  // Paths for all subdirectories of GCache, one for each
  // GDataRootDirectory::CacheSubDirectoryType enum.
  std::vector<FilePath> cache_paths_;

  scoped_ptr<base::WaitableEvent> on_io_completed_;

  // True if cache initialization has started, is in progress or has completed,
  // we only want to initialize cache once.
  bool cache_initialization_started_;

  // Number of pending tasks on the blocking thread pool.
  int num_pending_tasks_;
  base::Lock num_pending_tasks_lock_;

  // True if hosted documents should be hidden.
  bool hide_hosted_docs_;

  PrefChangeRegistrar pref_registrar_;

  // WeakPtrFactory and WeakPtr bound to the UI thread.
  scoped_ptr<base::WeakPtrFactory<GDataFileSystem> > ui_weak_ptr_factory_;
  base::WeakPtr<GDataFileSystem> ui_weak_ptr_;

  // WeakPtrFactory bound to the IO thread. Created when needed.
  scoped_ptr<base::WeakPtrFactory<GDataFileSystem> > io_weak_ptr_factory_;

  ObserverList<Observer> observers_;
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_
