// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_INTERFACE_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_INTERFACE_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/gdata/gdata_cache.h"
#include "chrome/browser/chromeos/gdata/gdata_files.h"
#include "chrome/browser/chromeos/gdata/gdata_params.h"
#include "chrome/browser/chromeos/gdata/gdata_upload_file_info.h"

namespace gdata {

class DocumentEntry;
class GDataDirectoryProto;
class GDataEntryProto;
class GDataFileProto;

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
typedef base::Callback<void(GDataFileError error)>
    FileOperationCallback;

// Used to get files from the file system.
typedef base::Callback<void(GDataFileError error,
                            const FilePath& file_path,
                            const std::string& mime_type,
                            GDataFileType file_type)> GetFileCallback;

// Used to get file info from the file system.
// If |error| is not PLATFORM_FILE_OK, |file_info| is set to NULL.
typedef base::Callback<void(GDataFileError error,
                            scoped_ptr<GDataFileProto> file_proto)>
    GetFileInfoCallback;

// Used to get file info from the file system, with the gdata file path.
// If |error| is not PLATFORM_FILE_OK, |file_info| is set to NULL.
//
// |gdata_file_path| parameter is provided as GDataFileProto does not contain
// the gdata file path (i.e. only contains the base name without parent
// directory names).
typedef base::Callback<void(GDataFileError error,
                            const FilePath& gdata_file_path,
                            scoped_ptr<GDataFileProto> file_proto)>
    GetFileInfoWithFilePathCallback;

// Used to get entry info from the file system.
// If |error| is not PLATFORM_FILE_OK, |entry_info| is set to NULL.
typedef base::Callback<void(GDataFileError error,
                            scoped_ptr<GDataEntryProto> entry_proto)>
    GetEntryInfoCallback;

// Used to read a directory from the file system.
// If |error| is not PLATFORM_FILE_OK, |directory_info| is set to NULL.
typedef base::Callback<void(GDataFileError error,
                            bool hide_hosted_documents,
                            scoped_ptr<GDataDirectoryProto> directory_proto)>
    ReadDirectoryCallback;

// Used to get drive content search results.
// If |error| is not PLATFORM_FILE_OK, |result_paths| is empty.
typedef base::Callback<void(
    GDataFileError error,
    scoped_ptr<std::vector<SearchResultInfo> > result_paths)> SearchCallback;

// Used to open files from the file system. |file_path| is the path on the local
// file system for the opened file.
typedef base::Callback<void(GDataFileError error,
                            const FilePath& file_path)> OpenFileCallback;

// Used to get available space for the account from GData.
typedef base::Callback<void(GDataFileError error,
                            int64 bytes_total,
                            int64 bytes_used)> GetAvailableSpaceCallback;

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
                               scoped_ptr<DocumentEntry> entry,
                               const FilePath& file_content_path,
                               GDataCache::FileOperationType cache_operation,
                               const base::Closure& callback) = 0;
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_INTERFACE_H_
