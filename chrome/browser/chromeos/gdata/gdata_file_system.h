// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/platform_file.h"
#include "base/timer.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/gdata/gdata_cache.h"
#include "chrome/browser/chromeos/gdata/gdata_files.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"

namespace gdata {

class DocumentsServiceInterface;
class DriveWebAppsRegistryInterface;
class GDataFileProto;
struct UploadFileInfo;

namespace {
struct LoadRootFeedParams;
}  // namespace

// Information about search result returned by Search Async callback.
// This is data needed to create a file system entry that will be used by file
// browser.
struct SearchResultInfo {
  SearchResultInfo(const FilePath& in_path, bool in_is_directory)
      : path(in_path),
        is_directory(in_is_directory) {
  }

  FilePath path;
  bool is_directory;
};

// Used for file operations like removing files.
typedef base::Callback<void(base::PlatformFileError error)>
    FileOperationCallback;

// Used to get files from the file system.
typedef base::Callback<void(base::PlatformFileError error,
                            const FilePath& file_path,
                            const std::string& mime_type,
                            GDataFileType file_type)>
    GetFileCallback;

// Used to get file info from the file system.
// If |error| is not PLATFORM_FILE_OK, |file_info| is set to NULL.
typedef base::Callback<void(base::PlatformFileError error,
                            scoped_ptr<GDataFileProto> file_proto)>
    GetFileInfoCallback;

// Used to get file info from the file system, with the gdata file path.
// If |error| is not PLATFORM_FILE_OK, |file_info| is set to NULL.
//
// |gdata_file_path| parameter is provided as GDataFileProto does not contain
// the gdata file path (i.e. only contains the base name without parent
// directory names).
typedef base::Callback<void(base::PlatformFileError error,
                            const FilePath& gdata_file_path,
                            scoped_ptr<GDataFileProto> file_proto)>
    GetFileInfoWithFilePathCallback;

// Used to get entry info from the file system.
// If |error| is not PLATFORM_FILE_OK, |entry_info| is set to NULL.
typedef base::Callback<void(base::PlatformFileError error,
                            const FilePath& entry_path,
                            scoped_ptr<GDataEntryProto> entry_proto)>
    GetEntryInfoCallback;

// Used to read a directory from the file system.
// If |error| is not PLATFORM_FILE_OK, |directory_info| is set to NULL.
typedef base::Callback<void(base::PlatformFileError error,
                            bool hide_hosted_documents,
                            scoped_ptr<GDataDirectoryProto> directory_proto)>
    ReadDirectoryCallback;

// Used to get drive content search results.
// If |error| is not PLATFORM_FILE_OK, |result_paths| is empty.
typedef base::Callback<void(
    base::PlatformFileError error,
    scoped_ptr<std::vector<SearchResultInfo> > result_paths)> SearchCallback;

// Used to open files from the file system. |file_path| is the path on the local
// file system for the opened file.
typedef base::Callback<void(base::PlatformFileError error,
                            const FilePath& file_path)>
    OpenFileCallback;

// Used for file operations like removing files.
typedef base::Callback<void(base::PlatformFileError error,
                            base::ListValue* feed_list)>
    GetJsonDocumentCallback;

// Used to get available space for the account from GData.
typedef base::Callback<void(base::PlatformFileError error,
                            int64 bytes_total,
                            int64 bytes_used)>
    GetAvailableSpaceCallback;

// Used by GDataFileSystem::GetDocumentResourceIdOnBlockingPool to return
// the resource ID read from a document JSON file on the local file system.
typedef base::Callback<void(const std::string& resource_id)>
    GetDocumentResourceIdCallback;

// GData file system abstraction layer.
// The interface is defined to make GDataFileSystem mockable.
class GDataFileSystemInterface {
 public:
  virtual ~GDataFileSystemInterface() {}

  // Used to notify events on the file system.
  // All events are notified on UI thread.
  class Observer {
   public:
    // Triggered when a content of a directory has been changed.
    // |directory_path| is a virtual directory path (/gdata/...) representing
    // changed directory.
    virtual void OnDirectoryChanged(const FilePath& directory_path) {}

    // Triggered when the file system is initially loaded.
    virtual void OnInitialLoadFinished() {}

    // Triggered when a document feed is fetched. |num_accumulated_entries|
    // tells the number of entries fetched so far.
    virtual void OnDocumentFeedFetched(int num_accumulated_entries) {}

    // Triggered when the feed from the server is loaded.
    virtual void OnFeedFromServerLoaded() {}

   protected:
    virtual ~Observer() {}
  };

  // Initializes the object. This function should be called before any
  // other functions.
  virtual void Initialize() = 0;

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Starts and stops periodic updates.
  virtual void StartUpdates() = 0;
  virtual void StopUpdates() = 0;

  // Checks for updates on the server.
  virtual void CheckForUpdates() = 0;

  // Finds a file (not directory) by using |resource_id|. This call does not
  // initiate content refreshing.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  virtual void GetFileInfoByResourceId(
      const std::string& resource_id,
      const GetFileInfoWithFilePathCallback& callback) = 0;

  // Initiates transfer of |remote_src_file_path| to |local_dest_file_path|.
  // |remote_src_file_path| is the virtual source path on the gdata file system.
  // |local_dest_file_path| is the destination path on the local file system.
  //
  // Must be called from *UI* thread. |callback| is run on the calling thread.
  virtual void TransferFileFromRemoteToLocal(
      const FilePath& remote_src_file_path,
      const FilePath& local_dest_file_path,
      const FileOperationCallback& callback) = 0;

  // Initiates transfer of |local_src_file_path| to |remote_dest_file_path|.
  // |local_src_file_path| must be a file from the local file system.
  // |remote_dest_file_path| is the virtual destination path within gdata file
  // system.
  //
  // Must be called from *UI* thread. |callback| is run on the calling thread.
  virtual void TransferFileFromLocalToRemote(
      const FilePath& local_src_file_path,
      const FilePath& remote_dest_file_path,
      const FileOperationCallback& callback) = 0;

  // Retrieves a file at the virtual path |file_path| on the gdata file system
  // onto the cache, and mark it dirty. The local path to the cache file is
  // returned to |callback|. After opening the file, both read and write
  // on the file can be done with normal local file operations.
  //
  // |CloseFile| must be called when the modification to the cache is done.
  // Otherwise, GData file system does not pick up the file for uploading.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  virtual void OpenFile(const FilePath& file_path,
                        const OpenFileCallback& callback) = 0;

  // Closes a file at the virtual path |file_path| on the gdata file system,
  // which is opened via OpenFile(). It commits the dirty flag on the cache.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  virtual void CloseFile(const FilePath& file_path,
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

  // Creates a file at |file_path|. If the flag |is_exclusive| is true, an
  // error is raised when a file already exists at the path. It is
  // an error if a directory or a hosted document is already present at the
  // path, or the parent directory of the path is not present yet.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread
  virtual void CreateFile(const FilePath& file_path,
                          bool is_exclusive,
                          const FileOperationCallback& callback) = 0;

  // Gets |file_path| from the file system. The file entry represented by
  // |file_path| needs to be present in in-memory representation of the file
  // system in order to be retrieved. If the file is not cached, the file
  // will be downloaded through gdata api.
  //
  // Can be called from UI/IO thread. |get_file_callback| and
  // |get_download_data| are run on the calling thread.
  virtual void GetFileByPath(
      const FilePath& file_path,
      const GetFileCallback& get_file_callback,
      const GetDownloadDataCallback& get_download_data_callback) = 0;

  // Gets a file by the given |resource_id| from the gdata server. Used for
  // fetching pinned-but-not-fetched files.
  //
  // Can be called from UI/IO thread. |get_file_callback| and
  // |get_download_data_callback| are run on the calling thread.
  virtual void GetFileByResourceId(
      const std::string& resource_id,
      const GetFileCallback& get_file_callback,
      const GetDownloadDataCallback& get_download_data_callback) = 0;

  // Updates a file by the given |resource_id| on the gdata server by
  // uploading an updated version. Used for uploading dirty files. The file
  // should already be present in the cache.
  //
  // TODO(satorux): As of now, the function only handles files with the dirty
  // bit committed. We should eliminate the restriction. crbug.com/134558.
  //
  // Can be called from UI/IO thread. |callback| and is run on the calling
  // thread.
  virtual void UpdateFileByResourceId(
      const std::string& resource_id,
      const FileOperationCallback& callback) = 0;

  // Finds an entry (a file or a directory) by |file_path|. This call will also
  // retrieve and refresh file system content from server and disk cache.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  virtual void GetEntryInfoByPath(const FilePath& file_path,
                                  const GetEntryInfoCallback& callback) = 0;

  // Finds a file (not a directory) by |file_path|. This call will also
  // retrieve and refresh file system content from server and disk cache.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  virtual void GetFileInfoByPath(const FilePath& file_path,
                                 const GetFileInfoCallback& callback) = 0;

  // Finds and reads a directory by |file_path|. This call will also retrieve
  // and refresh file system content from server and disk cache.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  virtual void ReadDirectoryByPath(const FilePath& file_path,
                                   const ReadDirectoryCallback& callback) = 0;

  // Requests a refresh of the directory pointed by |file_path| (i.e. fetches
  // the latest metadata of files in the target directory).
  //
  // In particular, this function is used to get the latest thumbnail
  // URLs. Thumbnail URLs change periodically even if contents of files are
  // not changed, hence we should get the new thumbnail URLs manually if we
  // detect that the existing thumnail URLs are stale.
  //
  // Upon success, the metadata of files in the target directory is updated,
  // and the change is notified via Observer::OnDirectoryChanged(). Note that
  // this function ignores changes in directories in the target
  // directory. Changes in directories are handled via the delta feeds.
  //
  // Can be called from UI/IO thread.
  virtual void RequestDirectoryRefresh(const FilePath& file_path) = 0;

  // Does server side content search for |search_query|.
  // Search results will be returned as a list of results' |SearchResultInfo|
  // structs, which contains file's path and is_directory flag.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  virtual void Search(const std::string& search_query,
                      const SearchCallback& callback) = 0;

  // Fetches the user's Account Metadata to find out current quota information
  // and returns it to the callback.
  virtual void GetAvailableSpace(const GetAvailableSpaceCallback& callback) = 0;

  // Adds a file entry from |entry| under |virtual_dir_path|, and modifies
  // the cache state.
  //
  // When uploading a new file, adds a new file entry, and store its content
  // from |file_content_path| into the cache.
  //
  // When uploading an existing file, replaces the file entry with a new one,
  // and clears the dirty bit in the cache.
  //
  // |callback| will be called on the UI thread upon completion of operation.
  virtual void AddUploadedFile(UploadMode upload_mode,
                               const FilePath& virtual_dir_path,
                               DocumentEntry* entry,
                               const FilePath& file_content_path,
                               GDataCache::FileOperationType cache_operation,
                               const base::Closure& callback) = 0;
};

// The production implementation of GDataFileSystemInterface.
class GDataFileSystem : public GDataFileSystemInterface,
                        public content::NotificationObserver {
 public:
  GDataFileSystem(
      Profile* profile,
      GDataCache* cache,
      DocumentsServiceInterface* documents_service,
      GDataUploaderInterface* uploader,
      DriveWebAppsRegistryInterface* webapps_registry,
      const base::SequencedWorkerPool::SequenceToken& sequence_token);
  virtual ~GDataFileSystem();

  // GDataFileSystem overrides.
  virtual void Initialize() OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual void StartUpdates() OVERRIDE;
  virtual void StopUpdates() OVERRIDE;
  virtual void CheckForUpdates() OVERRIDE;
  virtual void GetFileInfoByResourceId(
      const std::string& resource_id,
      const GetFileInfoWithFilePathCallback& callback) OVERRIDE;
  virtual void Search(const std::string& search_query,
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
  virtual void GetFileInfoByPath(
      const FilePath& file_path,
      const GetFileInfoCallback& callback) OVERRIDE;
  virtual void ReadDirectoryByPath(
      const FilePath& file_path,
      const ReadDirectoryCallback& callback) OVERRIDE;
  virtual void RequestDirectoryRefresh(
      const FilePath& file_path) OVERRIDE;
  virtual void GetAvailableSpace(
      const GetAvailableSpaceCallback& callback) OVERRIDE;
  virtual void AddUploadedFile(UploadMode upload_mode,
                               const FilePath& virtual_dir_path,
                               DocumentEntry* entry,
                               const FilePath& file_content_path,
                               GDataCache::FileOperationType cache_operation,
                               const base::Closure& callback) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Used in tests to inject mock document service.
  void SetDocumentsServiceForTesting(DocumentsServiceInterface*
      new_document_service) {
    documents_service_ = new_document_service;
  }

  // Finds and returns upload url of a given directory. Returns empty url
  // if directory can't be found.
  GURL GetUploadUrlForDirectory(const FilePath& destination_directory);

 private:
  friend class GDataFileSystemTest;
  FRIEND_TEST_ALL_PREFIXES(GDataFileSystemTest,
                           FindFirstMissingParentDirectory);
  FRIEND_TEST_ALL_PREFIXES(GDataFileSystemTest,
                           GetGDataEntryByPath);
  FRIEND_TEST_ALL_PREFIXES(GDataFileSystemTest,
                           GetFileFromCacheByPath);
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
  // execution of GetFileByPath() method.
  struct GetFileFromCacheParams {
    GetFileFromCacheParams(const FilePath& virtual_file_path,
        const FilePath& local_tmp_path,
        const GURL& content_url,
        const std::string& resource_id,
        const std::string& md5,
        const std::string& mime_type,
        const GetFileCallback& get_file_callback,
        const GetDownloadDataCallback& get_download_data_callback);
    ~GetFileFromCacheParams();

    FilePath virtual_file_path;
    FilePath local_tmp_path;
    GURL content_url;
    std::string resource_id;
    std::string md5;
    std::string mime_type;
    const GetFileCallback get_file_callback;
    const GetDownloadDataCallback get_download_data_callback;
  };

  // Defines set of parameters sent to callback OnGetDocuments().
  struct GetDocumentsParams {
    GetDocumentsParams(int start_changestamp,
                       int root_feed_changestamp,
                       std::vector<DocumentFeed*>* feed_list,
                       bool should_fetch_multiple_feeds,
                       const FilePath& search_file_path,
                       const std::string& search_query,
                       const std::string& directory_resource_id,
                       const FindEntryCallback& callback);
    ~GetDocumentsParams();

    // Changestamps are positive numbers in increasing order. The difference
    // between two changestamps is proportional equal to number of items in
    // delta feed between them - bigger the difference, more likely bigger
    // number of items in delta feeds.
    int start_changestamp;
    int root_feed_changestamp;
    scoped_ptr<std::vector<DocumentFeed*> > feed_list;
    // Should we stop after getting first feed chunk, even if there is more
    // data.
    bool should_fetch_multiple_feeds;
    FilePath search_file_path;
    std::string search_query;
    std::string directory_resource_id;
    FindEntryCallback callback;
  };

  // Defines set of parameters passed to an intermediate callback
  // OnGetFileCompleteForOpen, during execution of OpenFile() method.
  struct GetFileCompleteForOpenParams {
    GetFileCompleteForOpenParams(const std::string& resource_id,
                                 const std::string& md5);
    ~GetFileCompleteForOpenParams();
    std::string resource_id;
    std::string md5;
  };

  typedef std::map<std::string /* resource_id */, GDataEntry*>
      FileResourceIdMap;

  // Callback similar to FileOperationCallback but with a given |file_path|.
  typedef base::Callback<void(base::PlatformFileError error,
                              const FilePath& file_path)>
      FilePathUpdateCallback;

  // Callback run as a response to LoadFeedFromServer.
  typedef base::Callback<void(GetDocumentsParams* params,
                              base::PlatformFileError error)>
      LoadDocumentFeedCallback;

  // Struct used to record UMA stats with FeedToFileResourceMap().
  struct FeedToFileResourceMapUmaStats;

  // Finds entry object by |file_path| and returns the entry object.
  // Returns NULL if it does not find the entry.
  GDataEntry* GetGDataEntryByPath(const FilePath& file_path);

  // Callback passed to |LoadFeedFromServer| from |Search| method.
  // |callback| is that should be run with data received from
  // |LoadFeedFromServer|.
  // |params| params used for getting document feed for content search.
  // |error| error code returned by |LoadFeedFromServer|.
  void OnSearch(const SearchCallback& callback,
                GetDocumentsParams* params,
                base::PlatformFileError error);

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
      base::PlatformFileError result,
      GDataEntry* entry);
  void DoUploadForCreateBrandNewFile(
      const FilePath& remote_path,
      FilePath* local_path,
      const FileOperationCallback& callback);
  void DidUploadForCreateBrandNewFile(
      const FilePath& local_path,
      const FileOperationCallback& callback,
      base::PlatformFileError result);

  // Invoked upon completion of GetFileInfoByPath initiated by
  // GetFileByPath. It then continues to invoke GetResolvedFileByPath.
  void OnGetFileInfoCompleteForGetFileByPath(
      const FilePath& file_path,
      const GetFileCallback& get_file_callback,
      const GetDownloadDataCallback& get_download_data_callback,
      base::PlatformFileError error,
      scoped_ptr<GDataFileProto> file_info);

  // Invoked upon completion of GetFileInfoByPath initiated by OpenFile.
  // It then continues to invoke GetResolvedFileByPath and proceeds to
  // OnGetFileCompleteForOpenFile.
  void OnGetFileInfoCompleteForOpenFile(const FilePath& file_path,
                                        const OpenFileCallback& callback,
                                        base::PlatformFileError error,
                                        scoped_ptr<GDataFileProto> file_info);

  // Invoked at the last step of OpenFile. It removes |file_path| from the
  // current set of opened files if |result| is an error, and then invokes the
  // |callback| function.
  void OnOpenFileFinished(const FilePath& file_path,
                          const OpenFileCallback& callback,
                          base::PlatformFileError result,
                          const FilePath& cache_file_path);

  // Invoked during the process of CloseFile. It first gets the path of local
  // cache and receives it with OnGetFileCompleteForCloseFile. Then it reads the
  // metadata of the modified cache and send the information to
  // OnGetModifiedFileInfoCompleteForCloseFile./ Then it continues to get and
  // update the GData entry by FindEntryByPathAsyncOnUIThread
  // and invokes OnGetFileInfoCompleteForCloseFile. It then continues to
  // invoke CommitDirtyInCache to commit the change, and finally proceeds to
  // OnCommitDirtyInCacheCompleteForCloseFile and calls OnCloseFileFinished,
  // which removes the file from the "opened" list and invokes user-supplied
  // callback.
  void OnGetFileCompleteForCloseFile(
      const FilePath& file_path,
      const FileOperationCallback& callback,
      base::PlatformFileError error,
      const FilePath& local_cache_path,
      const std::string& mime_type,
      GDataFileType file_type);
  void OnGetModifiedFileInfoCompleteForCloseFile(
      const FilePath& file_path,
      base::PlatformFileInfo* file_info,
      bool* get_file_info_result,
      const FileOperationCallback& callback);
  void OnGetFileInfoCompleteForCloseFile(
      const FilePath& file_path,
      const base::PlatformFileInfo& file_info,
      const FileOperationCallback& callback,
      base::PlatformFileError error,
      GDataEntry* entry);
  void OnCommitDirtyInCacheCompleteForCloseFile(
      const FileOperationCallback& callback,
      base::PlatformFileError error,
      const std::string& resource_id,
      const std::string& md5);
  void OnCloseFileFinished(const FilePath& file_path,
                           const FileOperationCallback& callback,
                           base::PlatformFileError result);

  // Invoked upon completion of GetFileByPath initiated by Copy. If
  // GetFileByPath reports no error, calls TransferRegularFile to transfer
  // |local_file_path| to |remote_dest_file_path|.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  void OnGetFileCompleteForCopy(const FilePath& remote_dest_file_path,
                                const FileOperationCallback& callback,
                                base::PlatformFileError error,
                                const FilePath& local_file_path,
                                const std::string& unused_mime_type,
                                GDataFileType file_type);

  // Invoked upon completion of GetFileByPath initiated by
  // TransferFileFromRemoteToLocal. If GetFileByPath reports no error, calls
  // CopyLocalFileOnBlockingPool to copy |local_file_path| to
  // |local_dest_file_path|.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  void OnGetFileCompleteForTransferFile(const FilePath& local_dest_file_path,
                                        const FileOperationCallback& callback,
                                        base::PlatformFileError error,
                                        const FilePath& local_file_path,
                                        const std::string& unused_mime_type,
                                        GDataFileType file_type);

  // Invoked upon completion of GetFileByPath initiated by OpenFile. If
  // GetFileByPath is successful, calls MarkDirtyInCache to mark the cache
  // file as dirty for the file identified by |file_info.resource_id| and
  // |file_info.md5|.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  void OnGetFileCompleteForOpenFile(
      const OpenFileCallback& callback,
      const GetFileCompleteForOpenParams& file_info,
      base::PlatformFileError error,
      const FilePath& file_path,
      const std::string& mime_type,
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
  void AddEntryToDirectory(const FilePath& dir_path,
                           const FileOperationCallback& callback,
                           base::PlatformFileError error,
                           const FilePath& file_path);

  // Removes a file or directory at |file_path| from the directory at
  // |dir_path| and moves it to the root directory.
  //
  // Can be called from UI/IO thread. |callback| is run on the calling thread.
  void RemoveEntryFromDirectory(const FilePath& dir_path,
                                const FilePathUpdateCallback& callback,
                                base::PlatformFileError error,
                                const FilePath& file_path);

  // Removes file under |file_path| from in-memory snapshot of the file system.
  // |resource_id| contains the resource id of the removed file if it was a
  // file.
  // Return PLATFORM_FILE_OK if successful.
  base::PlatformFileError RemoveEntryFromGData(const FilePath& file_path,
                                               std::string* resource_id);

  // Callback for handling response from |GDataDocumentsService::GetDocuments|.
  // Invokes |callback| when done.
  void OnGetDocuments(ContentOrigin initial_origin,
                      const LoadDocumentFeedCallback& callback,
                      GetDocumentsParams* params,
                      base::TimeTicks start_time,
                      GDataErrorCode status,
                      scoped_ptr<base::Value> data);

  // A pass-through callback used for bridging from
  // FilePathUpdateCallback to FileOperationCallback.
  void OnFilePathUpdated(const FileOperationCallback& cllback,
                         base::PlatformFileError error,
                         const FilePath& file_path);

  // Invoked upon completion of MarkDirtyInCache initiated by OpenFile. Invokes
  // |callback| with |cache_file_path|, which is the path of the cache file
  // ready for modification.
  //
  // Must be called on UI thread.
  void OnMarkDirtyInCacheCompleteForOpenFile(
      const OpenFileCallback& callback,
      base::PlatformFileError error,
      const std::string& resource_id,
      const std::string& md5,
      const FilePath& cache_file_path);

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
  void OnAddEntryToDirectoryCompleted(const FileOperationCallback& callback,
                                      const FilePath& file_path,
                                      const FilePath& dir_path,
                                      GDataErrorCode status,
                                      const GURL& document_url);

  // Callback for handling an attempt to remove a file or directory from
  // another directory.
  void OnRemoveEntryFromDirectoryCompleted(
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

  // Unpins file if cache entry is pinned.
  void UnpinIfPinned(const std::string& resource_id,
                     const std::string& md5,
                     bool success,
                     const GDataCache::CacheEntry& cache_entry);

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
  void OnDownloadStoredToCache(base::PlatformFileError error,
                               const std::string& resource_id,
                               const std::string& md5);

  // Renames a file or directory at |file_path| on in-memory snapshot
  // of the file system. Returns PLATFORM_FILE_OK if successful.
  base::PlatformFileError RenameFileOnFilesystem(
      const FilePath& file_path, const FilePath::StringType& new_name,
      FilePath* updated_file_path);

  // Adds a file or directory at |file_path| to another directory at
  // |dir_path| on in-memory snapshot of the file system.
  // Returns PLATFORM_FILE_OK if successful.
  base::PlatformFileError AddEntryToDirectoryOnFilesystem(
      const FilePath& file_path, const FilePath& dir_path);

  // Removes a file or directory at |file_path| from another directory at
  // |dir_path| on in-memory snapshot of the file system.
  // Returns PLATFORM_FILE_OK if successful.
  base::PlatformFileError RemoveEntryFromDirectoryOnFilesystem(
      const FilePath& file_path, const FilePath& dir_path,
      FilePath* updated_file_path);

  // Removes a file or directory under |file_path| from in-memory snapshot of
  // the file system and the corresponding file from cache if it exists.
  // Return PLATFORM_FILE_OK if successful.
  base::PlatformFileError RemoveEntryFromFileSystem(const FilePath& file_path);

  // Updates whole directory structure feeds collected in |feed_list|.
  // On success, returns PLATFORM_FILE_OK. Record file statistics as UMA
  // histograms.
  base::PlatformFileError UpdateFromFeed(
      const std::vector<DocumentFeed*>& feed_list,
      ContentOrigin origin,
      int largest_changestamp,
      int root_feed_changestamp);

  // Updates UMA histograms about file counts.
  void UpdateFileCountUmaHistograms(
      const FeedToFileResourceMapUmaStats& uma_stats) const;

  // Applies the pre-processed feed from |file_map| map onto the file system.
  // All entries in |file_map| will be erased (i.e. the map becomes empty),
  // and values are deleted.
  void ApplyFeedFromFileUrlMap(bool is_delta_feed,
                               int feed_changestamp,
                               FileResourceIdMap* file_map);

  // Finds directory where new |file| should be added to during feed processing.
  // |orphaned_entries_dir| collects files/dirs that don't have a parent in
  // either locally cached file system or in this new feed.
  GDataDirectory* FindDirectoryForNewEntry(
      GDataEntry* new_entry,
      const FileResourceIdMap& file_map,
      GDataRootDirectory* orphaned_entries);

  // Converts list of document feeds from collected feeds into
  // FileResourceIdMap.
  base::PlatformFileError FeedToFileResourceMap(
      const std::vector<DocumentFeed*>& feed_list,
      FileResourceIdMap* file_map,
      int* feed_changestamp,
      FeedToFileResourceMapUmaStats* uma_stats);

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

  // Retrieves account metadata and determines from the last change timestamp
  // if the feed content loading from the server needs to be initiated.
  void ReloadFeedFromServerIfNeeded(ContentOrigin initial_origin,
                                    int local_changestamp,
                                    const FilePath& search_file_path,
                                    const FindEntryCallback& callback);

  // Helper callback for handling results of metadata retrieval initiated from
  // ReloadFeedFromServerIfNeeded(). This method makes a decision about fetching
  // the content of the root feed during the root directory refresh process.
  void OnGetAccountMetadata(ContentOrigin initial_origin,
                            int local_changestamp,
                            const FilePath& search_file_path,
                            const FindEntryCallback& callback,
                            GDataErrorCode error,
                            scoped_ptr<base::Value> feed_data);

  // Starts root feed load from the server. Value of |start_changestamp|
  // determines the type of feed to load - 0 means root feed, every other
  // value would trigger delta feed.
  // In the case of loading the root feed we use |root_feed_changestamp| as its
  // initial changestamp value since it does not come with that info.
  // When done |load_feed_callback| is invoked.
  // |entry_found_callback| is used only when this is invoked while searching
  // for file info, and is used in |load_feed_callback|. If successful, it will
  // try to find the file upon retrieval completion.
  // |should_fetch_multiple_feeds| is true iff don't want to stop feed loading
  // after we retrieve first feed chunk.
  // If invoked as a part of content search, query will be set in
  // |search_query|.
  void LoadFeedFromServer(ContentOrigin initial_origin,
                          int start_changestamp,
                          int root_feed_changestamp,
                          bool should_fetch_multiple_feeds,
                          const FilePath& search_file_path,
                          const std::string& search_query,
                          const std::string& directory_resource_id,
                          const FindEntryCallback& entry_found_callback,
                          const LoadDocumentFeedCallback& load_feed_callback);

  // Callback for handling feed content fetching while searching for file info.
  // This callback is invoked after async feed fetch operation that was
  // invoked by StartDirectoryRefresh() completes. This callback will update
  // the content of the refreshed directory object and continue initially
  // started FindEntryByPath() request.
  void OnFeedFromServerLoaded(GetDocumentsParams* params,
                              base::PlatformFileError status);

  // Callback for handling results of ReloadFeedFromServerIfNeeded() initiated
  // from CheckForUpdates(). This callback checks whether feed is successfully
  // reloaded, and in case of failure, restores the content origin of the root
  // directory.
  void OnUpdateChecked(ContentOrigin initial_origin,
                       base::PlatformFileError error,
                       GDataEntry* entry);

  // Starts root feed load from the cache. If successful, it will try to find
  // the file upon retrieval completion. In addition to that, it will
  // initiate retrieval of the root feed from the server if
  // |should_load_from_server| is set.
  void LoadRootFeedFromCache(bool should_load_from_server,
                             const FilePath& search_file_path,
                             const FindEntryCallback& callback);

  // Callback for handling root directory refresh from the cache.
  void OnProtoLoaded(LoadRootFeedParams* params);

  // Save filesystem as proto file.
  void SaveFileSystemAsProto();

  // Notifies events to observers on UI thread.
  void NotifyDirectoryChanged(const FilePath& directory_path);
  void NotifyDocumentFeedFetched(int num_accumulated_entries);

  // Runs the callback and notifies that the initial load is finished.
  void RunAndNotifyInitialLoadFinished(
    const FindEntryCallback& callback,
    base::PlatformFileError error,
    GDataEntry* entry);

  // Helper function that completes bookkeeping tasks related to
  // completed file transfer.
  void OnTransferCompleted(
      const FileOperationCallback& callback,
      base::PlatformFileError error,
      scoped_ptr<UploadFileInfo> upload_file_info);

  // Kicks off file upload once it receives |file_size| and |content_type|.
  void StartFileUploadOnUIThread(
      const FilePath& local_file,
      const FilePath& remote_dest_file,
      const FileOperationCallback& callback,
      base::PlatformFileError* error,
      int64* file_size,
      std::string* content_type);

  // Cache intermediate callbacks, that run on calling thread, for above cache
  // tasks that were run on blocking pool.

  // Helper function for internally handling responses from
  // GetFileFromCacheByResourceIdAndMd5() calls during processing of
  // GetFileByPath() request.
  void OnGetFileFromCache(const GetFileFromCacheParams& params,
                          base::PlatformFileError error,
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
                      base::PlatformFileError error,
                      GDataEntry* entry);

  // Called when an entry is found for GetFileInfoByPath().
  void OnGetFileInfo(const GetFileInfoCallback& callback,
                     base::PlatformFileError error,
                     GDataEntry* entry);

  // Called when an entry is found for ReadDirectoryByPath().
  void OnReadDirectory(const ReadDirectoryCallback& callback,
                       base::PlatformFileError error,
                       GDataEntry* entry);

  // Finds file info by using virtual |file_path|. This call will also
  // retrieve and refresh file system content from server and disk cache.
  void FindEntryByPathAsyncOnUIThread(const FilePath& search_file_path,
                                      const FindEntryCallback& callback);

  // Gets |file_path| from the file system after the file info is already
  // resolved with GetFileInfoByPath(). This function is called by
  // OnGetFileInfoCompleteForGetFileByPath and OnGetFileInfoCompleteForOpenFile.
  void GetResolvedFileByPath(
      const FilePath& file_path,
      const GetFileCallback& get_file_callback,
      const GetDownloadDataCallback& get_download_data_callback,
      base::PlatformFileError error,
      const GDataFileProto* file_proto);

  // Called when GDataCache::GetFileOnUIThread() is completed for
  // UpdateFileByResourceId().
  void OnGetFileCompleteForUpdateFile(
      const FileOperationCallback& callback,
      base::PlatformFileError error,
      const std::string& resource_id,
      const std::string& md5,
      const FilePath& cache_file_path);

  // Called when GDataUploader::UploadUpdatedFile() is completed for
  // UpdateFileByResourceId().
  void OnUpdatedFileUploaded(
    const FileOperationCallback& callback,
    base::PlatformFileError error,
    scoped_ptr<UploadFileInfo> upload_file_info);

  // The following functions are used to forward calls to asynchronous public
  // member functions to UI thread.
  void SearchAsyncOnUIThread(const std::string& search_query,
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
  void UpdateFileByResourceIdOnUIThread(
      const std::string& resource_id,
      const FileOperationCallback& callback);
  void GetEntryInfoByPathAsyncOnUIThread(
      const FilePath& file_path,
      const GetEntryInfoCallback& callback);
  void GetFileInfoByPathAsyncOnUIThread(
      const FilePath& file_path,
      const GetFileInfoCallback& callback);
  void GetFileInfoByResourceIdOnUIThread(
      const std::string& resource_id,
      const GetFileInfoWithFilePathCallback& callback);
  void ReadDirectoryByPathAsyncOnUIThread(
      const FilePath& file_path,
      const ReadDirectoryCallback& callback);
  void RequestDirectoryRefreshOnUIThread(
      const FilePath& file_path);
  void OnRequestDirectoryRefresh(GetDocumentsParams* params,
                                 base::PlatformFileError error);
  void GetAvailableSpaceOnUIThread(const GetAvailableSpaceCallback& callback);
  void AddUploadedFileOnUIThread(UploadMode upload_mode,
                                 const FilePath& virtual_dir_path,
                                 DocumentEntry* entry,
                                 const FilePath& file_content_path,
                                 GDataCache::FileOperationType cache_operation,
                                 const base::Closure& callback);

  // All members should be accessed only on UI thread. Do not post tasks to
  // other threads with base::Unretained(this).

  scoped_ptr<GDataRootDirectory> root_;

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

  // WeakPtrFactory and WeakPtr bound to the UI thread.
  base::WeakPtrFactory<GDataFileSystem> ui_weak_ptr_factory_;
  base::WeakPtr<GDataFileSystem> ui_weak_ptr_;

  ObserverList<Observer> observers_;

  // The token is used to post tasks to the blocking pool in sequence.
  const base::SequencedWorkerPool::SequenceToken sequence_token_;
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_
