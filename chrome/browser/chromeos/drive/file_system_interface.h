// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_INTERFACE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_INTERFACE_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system_metadata.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/google_apis/gdata_wapi_operations.h"

namespace google_apis {
class ResourceEntry;
}

namespace drive {

class FileSystemObserver;

typedef std::vector<ResourceEntry> ResourceEntryVector;

// File type on the drive file system can be either a regular file or
// a hosted document.
enum DriveFileType {
  REGULAR_FILE,
  HOSTED_DOCUMENT,
};

// Information about search result returned by Search Async callback.
// This is data needed to create a file system entry that will be used by file
// browser.
struct SearchResultInfo {
  SearchResultInfo(const base::FilePath& path,
                   const ResourceEntry& entry)
      : path(path),
        entry(entry) {
  }

  base::FilePath path;
  ResourceEntry entry;
};

// Struct to represent a search result for SearchMetadata().
struct MetadataSearchResult {
  MetadataSearchResult(const base::FilePath& in_path,
                       const ResourceEntry& in_entry,
                       const std::string& in_highlighted_base_name)
      : path(in_path),
        entry(in_entry),
        highlighted_base_name(in_highlighted_base_name) {
  }

  // The two members are used to create FileEntry object.
  base::FilePath path;
  ResourceEntry entry;

  // The base name to be displayed in the UI. The parts matched the search
  // query are highlighted with <b> tag. Meta characters are escaped like &lt;
  //
  // Why HTML? we could instead provide matched ranges using pairs of
  // integers, but this is fragile as we'll eventually converting strings
  // from UTF-8 (StringValue in base/values.h uses std::string) to UTF-16
  // when sending strings from C++ to JavaScript.
  //
  // Why <b> instead of <strong>? Because <b> is shorter.
  std::string highlighted_base_name;
};

typedef std::vector<MetadataSearchResult> MetadataSearchResultVector;

// Used to get files from the file system.
typedef base::Callback<void(FileError error,
                            const base::FilePath& file_path,
                            const std::string& mime_type,
                            DriveFileType file_type)> GetFileCallback;

// Used to get file content from the file system.
// If the file content is available in local cache, |local_file| is filled with
// the path to the cache file and |cancel_download_closure| is null. If the file
// content starts to be downloaded from the server, |local_file| is empty and
// |cancel_download_closure| is filled with a closure by calling which the
// download job can be cancelled.
// |cancel_download_closure| must be called on the UI thread.
typedef base::Callback<void(FileError error,
                            scoped_ptr<ResourceEntry> entry,
                            const base::FilePath& local_file,
                            const base::Closure& cancel_download_closure)>
    GetFileContentInitializedCallback;

// Used to read a directory from the file system.
// Similar to ReadDirectoryCallback but this one provides
// |hide_hosted_documents|
// If |error| is not FILE_ERROR_OK, |entries| is set to NULL.
// |entries| are contents, both files and directories, of the directory.
typedef base::Callback<void(FileError error,
                            bool hide_hosted_documents,
                            scoped_ptr<ResourceEntryVector> entries)>
    ReadDirectoryWithSettingCallback;

// Used to get drive content search results.
// If |error| is not FILE_ERROR_OK, |result_paths| is empty.
typedef base::Callback<void(
    FileError error,
    const GURL& next_feed,
    scoped_ptr<std::vector<SearchResultInfo> > result_paths)> SearchCallback;

// Callback for SearchMetadata(). On success, |error| is FILE_ERROR_OK, and
// |result| contains the search result.
typedef base::Callback<void(
    FileError error,
    scoped_ptr<MetadataSearchResultVector> result)> SearchMetadataCallback;

// Used to open files from the file system. |file_path| is the path on the local
// file system for the opened file.
typedef base::Callback<void(FileError error,
                            const base::FilePath& file_path)> OpenFileCallback;

// Used to get available space for the account from Drive.
typedef base::Callback<void(FileError error,
                            int64 bytes_total,
                            int64 bytes_used)> GetAvailableSpaceCallback;

// Used to get filesystem metadata.
typedef base::Callback<void(const FileSystemMetadata&)>
    GetFilesystemMetadataCallback;

// Priority of a job.  Higher values are lower priority.
enum ContextType {
  USER_INITIATED,
  BACKGROUND,
};

struct DriveClientContext {
  explicit DriveClientContext(ContextType in_type) : type(in_type) {}
  ContextType type;
};

// Option enum to control eligible entries for searchMetadata().
// SEARCH_METADATA_ALL is the default to investigate all the entries.
// SEARCH_METADATA_EXCLUDE_HOSTED_DOCUMENTS excludes the hosted documents.
// SEARCH_METADATA_EXCLUDE_DIRECTORIES excludes the directories from the result.
// SEARCH_METADATA_SHARED_WITH_ME targets only "shared-with-me" entries.
// TODO(haruki): Add option for offline.
enum SearchMetadataOptions {
  SEARCH_METADATA_ALL = 0,
  SEARCH_METADATA_EXCLUDE_HOSTED_DOCUMENTS = 1,
  SEARCH_METADATA_EXCLUDE_DIRECTORIES = 1 << 1,
  SEARCH_METADATA_SHARED_WITH_ME = 1 << 2,
};

// Drive file system abstraction layer.
// The interface is defined to make FileSystem mockable.
class FileSystemInterface {
 public:
  virtual ~FileSystemInterface() {}

  // Initializes the object. This function should be called before any
  // other functions.
  virtual void Initialize() = 0;

  // Adds and removes the observer.
  virtual void AddObserver(FileSystemObserver* observer) = 0;
  virtual void RemoveObserver(FileSystemObserver* observer) = 0;

  // Checks for updates on the server.
  virtual void CheckForUpdates() = 0;

  // Finds an entry (file or directory) by using |resource_id|. This call
  // does not initiate content refreshing.
  //
  // |callback| must not be null.
  virtual void GetEntryInfoByResourceId(
      const std::string& resource_id,
      const GetEntryInfoWithFilePathCallback& callback) = 0;

  // Initiates transfer of |remote_src_file_path| to |local_dest_file_path|.
  // |remote_src_file_path| is the virtual source path on the Drive file system.
  // |local_dest_file_path| is the destination path on the local file system.
  //
  // |callback| must not be null.
  virtual void TransferFileFromRemoteToLocal(
      const base::FilePath& remote_src_file_path,
      const base::FilePath& local_dest_file_path,
      const FileOperationCallback& callback) = 0;

  // Initiates transfer of |local_src_file_path| to |remote_dest_file_path|.
  // |local_src_file_path| must be a file from the local file system.
  // |remote_dest_file_path| is the virtual destination path within Drive file
  // system.
  //
  // |callback| must not be null.
  virtual void TransferFileFromLocalToRemote(
      const base::FilePath& local_src_file_path,
      const base::FilePath& remote_dest_file_path,
      const FileOperationCallback& callback) = 0;

  // Retrieves a file at the virtual path |file_path| on the Drive file system
  // onto the cache, and mark it dirty. The local path to the cache file is
  // returned to |callback|. After opening the file, both read and write
  // on the file can be done with normal local file operations.
  //
  // |CloseFile| must be called when the modification to the cache is done.
  // Otherwise, Drive file system does not pick up the file for uploading.
  //
  // |callback| must not be null.
  virtual void OpenFile(const base::FilePath& file_path,
                        const OpenFileCallback& callback) = 0;

  // Closes a file at the virtual path |file_path| on the Drive file system,
  // which is opened via OpenFile(). It commits the dirty flag on the cache.
  //
  // |callback| must not be null.
  virtual void CloseFile(const base::FilePath& file_path,
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
  // |callback| must not be null.
  virtual void Copy(const base::FilePath& src_file_path,
                    const base::FilePath& dest_file_path,
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
  // |callback| must not be null.
  virtual void Move(const base::FilePath& src_file_path,
                    const base::FilePath& dest_file_path,
                    const FileOperationCallback& callback) = 0;

  // Removes |file_path| from the file system.  If |is_recursive| is set and
  // |file_path| represents a directory, we will also delete all of its
  // contained children elements. The file entry represented by |file_path|
  // needs to be present in in-memory representation of the file system that
  // in order to be removed.
  //
  // TODO(satorux): is_recursive is not supported yet. crbug.com/138282
  //
  // |callback| must not be null.
  virtual void Remove(const base::FilePath& file_path,
                      bool is_recursive,
                      const FileOperationCallback& callback) = 0;

  // Creates new directory under |directory_path|. If |is_exclusive| is true,
  // an error is raised in case a directory is already present at the
  // |directory_path|. If |is_recursive| is true, the call creates parent
  // directories as needed just like mkdir -p does.
  //
  // |callback| must not be null.
  virtual void CreateDirectory(const base::FilePath& directory_path,
                               bool is_exclusive,
                               bool is_recursive,
                               const FileOperationCallback& callback) = 0;

  // Creates a file at |file_path|. If the flag |is_exclusive| is true, an
  // error is raised when a file already exists at the path. It is
  // an error if a directory or a hosted document is already present at the
  // path, or the parent directory of the path is not present yet.
  //
  // |callback| must not be null.
  virtual void CreateFile(const base::FilePath& file_path,
                          bool is_exclusive,
                          const FileOperationCallback& callback) = 0;

  // Pins a file at |file_path|.
  //
  // |callback| must not be null.
  virtual void Pin(const base::FilePath& file_path,
                   const FileOperationCallback& callback) = 0;

  // Unpins a file at |file_path|.
  //
  // |callback| must not be null.
  virtual void Unpin(const base::FilePath& file_path,
                     const FileOperationCallback& callback) = 0;

  // Gets |file_path| from the file system. The file entry represented by
  // |file_path| needs to be present in in-memory representation of the file
  // system in order to be retrieved. If the file is not cached, the file
  // will be downloaded through GData API or Drive V2 API.
  //
  // |callback| must not be null.
  virtual void GetFileByPath(const base::FilePath& file_path,
                             const GetFileCallback& callback) = 0;

  // Gets a file by the given |resource_id| from the Drive server. Used for
  // fetching pinned-but-not-fetched files.
  //
  // |get_file_callback| must not be null.
  // |get_content_callback| may be null.
  virtual void GetFileByResourceId(
      const std::string& resource_id,
      const DriveClientContext& context,
      const GetFileCallback& get_file_callback,
      const google_apis::GetContentCallback& get_content_callback) = 0;

  // Gets a file by the given |file_path|.
  // Calls |initialized_callback| when either:
  //   1) The cached file (or JSON file for hosted file) is found, or
  //   2) Starting to download the file from drive server.
  // In case of 2), the given FilePath is empty, and |get_content_callback| is
  // called repeatedly with downloaded content following the
  // |initialized_callback| invocation.
  // |completion_callback| is invoked if an error is found, or the operation
  // is successfully done.
  // |initialized_callback|, |get_content_callback| and |completion_callback|
  // must not be null.
  virtual void GetFileContentByPath(
      const base::FilePath& file_path,
      const GetFileContentInitializedCallback& initialized_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const FileOperationCallback& completion_callback) = 0;

  // Updates a file by the given |resource_id| on the Drive server by
  // uploading an updated version. Used for uploading dirty files. The file
  // should already be present in the cache.
  //
  // TODO(satorux): As of now, the function only handles files with the dirty
  // bit committed. We should eliminate the restriction. crbug.com/134558.
  //
  // |callback| must not be null.
  virtual void UpdateFileByResourceId(
      const std::string& resource_id,
      const DriveClientContext& context,
      const FileOperationCallback& callback) = 0;

  // Finds an entry (a file or a directory) by |file_path|. This call will also
  // retrieve and refresh file system content from server and disk cache.
  //
  // |callback| must not be null.
  virtual void GetEntryInfoByPath(const base::FilePath& file_path,
                                  const GetEntryInfoCallback& callback) = 0;

  // Finds and reads a directory by |file_path|. This call will also retrieve
  // and refresh file system content from server and disk cache.
  //
  // |callback| must not be null.
  virtual void ReadDirectoryByPath(
      const base::FilePath& file_path,
      const ReadDirectoryWithSettingCallback& callback) = 0;

  // Refreshes the directory pointed by |file_path| (i.e. fetches the latest
  // metadata of files in the target directory).
  //
  // In particular, this function is used to get the latest thumbnail
  // URLs. Thumbnail URLs change periodically even if contents of files are
  // not changed, hence we should get the new thumbnail URLs manually if we
  // detect that the existing thumbnail URLs are stale.
  //
  // Upon success, the metadata of files in the target directory is updated
  // and the change is notified via Observer::OnDirectoryChanged().
  // |callback| is called with an error code regardless of whether the
  // refresh was success or not. Note that this function ignores changes in
  // directories in the target directory. Changes in directories are handled
  // via the change lists.
  //
  // |callback| must not be null.
  virtual void RefreshDirectory(const base::FilePath& file_path,
                                const FileOperationCallback& callback) = 0;

  // Does server side content search for |search_query|.
  // If |next_feed| is set, this is the feed url that will be fetched.
  // Search results will be returned as a list of results' |SearchResultInfo|
  // structs, which contains file's path and is_directory flag.
  //
  // |callback| must not be null.
  virtual void Search(const std::string& search_query,
                      const GURL& next_feed,
                      const SearchCallback& callback) = 0;

  // Searches the local resource metadata, and returns the entries
  // |at_most_num_matches| that contain |query| in their base names. Search is
  // done in a case-insensitive fashion. The eligible entries are selected based
  // on the given |options|, which is a bit-wise OR of SearchMetadataOptions.
  // SEARCH_METADATA_EXCLUDE_HOSTED_DOCUMENTS will be automatically added based
  // on the preference. |callback| must not be null. Must be called on UI
  // thread. Empty |query| matches any base name. i.e. returns everything.
  virtual void SearchMetadata(const std::string& query,
                              int  options,
                              int at_most_num_matches,
                              const SearchMetadataCallback& callback) = 0;

  // Fetches the user's Account Metadata to find out current quota information
  // and returns it to the callback.
  virtual void GetAvailableSpace(const GetAvailableSpaceCallback& callback) = 0;

  // Adds a file entry from |doc_entry|, and modifies the cache state.
  // Adds a new file entry, and store its content from |file_content_path| into
  // the cache.
  //
  // |callback| must not be null.
  // TODO(kinaba): move to an internal operation class. http://crbug.com/236771.
  virtual void AddUploadedFile(scoped_ptr<google_apis::ResourceEntry> doc_entry,
                               const base::FilePath& file_content_path,
                               const FileOperationCallback& callback) = 0;

  // Returns miscellaneous metadata of the file system like the largest
  // timestamp. Used in chrome:drive-internals. |callback| must not be null.
  virtual void GetMetadata(
      const GetFilesystemMetadataCallback& callback) = 0;

  // Marks the cached file as mounted, and runs |callback| upon completion.
  // If succeeded, the cached file path will be passed to the |callback|.
  // |callback| must not be null.
  virtual void MarkCacheFileAsMounted(const base::FilePath& drive_file_path,
                                    const OpenFileCallback& callback) = 0;

  // Marks the cached file as unmounted, and runs |callback| upon completion.
  // Note that this method expects that the |cached_file_path| is the path
  // returned by MarkCacheFileAsMounted().
  // |callback| must not be null.
  virtual void MarkCacheFileAsUnmounted(
      const base::FilePath& cache_file_path,
      const FileOperationCallback& callback) = 0;

  // Gets the cache entry for file corresponding to |resource_id| and |md5|
  // and runs |callback| with true and the found entry if the entry exists
  // in the cache map. Otherwise, runs |callback| with false.
  // |md5| can be empty if only matching |resource_id| is desired, which may
  // happen when looking for pinned entries where symlinks' filenames have no
  // extension and hence no md5.
  // |callback| must not be null.
  virtual void GetCacheEntryByResourceId(
      const std::string& resource_id,
      const std::string& md5,
      const GetCacheEntryCallback& callback) = 0;

  // Iterates all files in the cache and calls |iteration_callback| for each
  // file. |completion_callback| is run upon completion.
  // Neither |iteration_callback| nor |completion_callback| must be null.
  virtual void IterateCache(const CacheIterateCallback& iteration_callback,
                            const base::Closure& completion_callback) = 0;

  // Reloads the file system feeds from the server.
  virtual void Reload() = 0;
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_INTERFACE_H_
