// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_INTERFACE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_INTERFACE_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_metadata.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"
#include "chrome/browser/google_apis/gdata_wapi_operations.h"

namespace google_apis {
class ResourceEntry;
}

namespace drive {

class DriveFileSystemObserver;
class DriveResourceMetadata;

typedef std::vector<DriveEntryProto> DriveEntryProtoVector;

// Information about search result returned by Search Async callback.
// This is data needed to create a file system entry that will be used by file
// browser.
struct SearchResultInfo {
  SearchResultInfo(const base::FilePath& path,
                   const DriveEntryProto& entry_proto)
      : path(path),
        entry_proto(entry_proto) {
  }

  base::FilePath path;
  DriveEntryProto entry_proto;
};

// Used to get files from the file system.
typedef base::Callback<void(DriveFileError error,
                            const base::FilePath& file_path,
                            const std::string& mime_type,
                            DriveFileType file_type)> GetFileCallback;

// Used to read a directory from the file system.
// Similar to ReadDirectoryCallback but this one provides
// |hide_hosted_documents|
// If |error| is not DRIVE_FILE_OK, |entries| is set to NULL.
// |entries| are contents, both files and directories, of the directory.
typedef base::Callback<void(DriveFileError error,
                            bool hide_hosted_documents,
                            scoped_ptr<DriveEntryProtoVector> entries)>
    ReadDirectoryWithSettingCallback;

// Used to get drive content search results.
// If |error| is not DRIVE_FILE_OK, |result_paths| is empty.
typedef base::Callback<void(
    DriveFileError error,
    const GURL& next_feed,
    scoped_ptr<std::vector<SearchResultInfo> > result_paths)> SearchCallback;

// Used to open files from the file system. |file_path| is the path on the local
// file system for the opened file.
typedef base::Callback<void(DriveFileError error,
                            const base::FilePath& file_path)> OpenFileCallback;

// Used to get available space for the account from Drive.
typedef base::Callback<void(DriveFileError error,
                            int64 bytes_total,
                            int64 bytes_used)> GetAvailableSpaceCallback;

// Used to get drive filesystem metadata.
typedef base::Callback<void(const DriveFileSystemMetadata&)>
    GetFilesystemMetadataCallback;

// Priority of a job.  Higher values are lower priority.
enum ContextType {
  USER_INITIATED,
  BACKGROUND,
  PREFETCH,
};

struct DriveClientContext {
  explicit DriveClientContext(ContextType in_type) : type(in_type) {}
  ContextType type;
};

// Drive file system abstraction layer.
// The interface is defined to make DriveFileSystem mockable.
class DriveFileSystemInterface {
 public:
  virtual ~DriveFileSystemInterface() {}

  // Initializes the object. This function should be called before any
  // other functions.
  virtual void Initialize() = 0;

  // Adds and removes the observer.
  virtual void AddObserver(DriveFileSystemObserver* observer) = 0;
  virtual void RemoveObserver(DriveFileSystemObserver* observer) = 0;

  // Starts initial feed fetch from the server.
  virtual void StartInitialFeedFetch() = 0;

  // Starts and stops periodic polling.
  virtual void StartPolling() = 0;
  virtual void StopPolling() = 0;

  // Sets the availability of push notification when its status is changed.
  virtual void SetPushNotificationEnabled(bool enabled) = 0;

  // Notifies the file system was just mounted.
  virtual void NotifyFileSystemMounted() = 0;

  // Notifies the file system is going to be unmounted.
  virtual void NotifyFileSystemToBeUnmounted() = 0;

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

  // Requests a refresh of the directory pointed by |file_path| (i.e. fetches
  // the latest metadata of files in the target directory).
  //
  // In particular, this function is used to get the latest thumbnail
  // URLs. Thumbnail URLs change periodically even if contents of files are
  // not changed, hence we should get the new thumbnail URLs manually if we
  // detect that the existing thumbnail URLs are stale.
  //
  // Upon success, the metadata of files in the target directory is updated,
  // and the change is notified via Observer::OnDirectoryChanged(). Note that
  // this function ignores changes in directories in the target
  // directory. Changes in directories are handled via the delta feeds.
  virtual void RequestDirectoryRefresh(const base::FilePath& file_path) = 0;

  // Does server side content search for |search_query|.
  // If |shared_with_me| is true, it searches for the files shared to the user,
  // otherwise searches for the files owned by the user.
  // If |next_feed| is set, this is the feed url that will be fetched.
  // Search results will be returned as a list of results' |SearchResultInfo|
  // structs, which contains file's path and is_directory flag.
  //
  // |callback| must not be null.
  virtual void Search(const std::string& search_query,
                      bool shared_with_me,
                      const GURL& next_feed,
                      const SearchCallback& callback) = 0;

  // Fetches the user's Account Metadata to find out current quota information
  // and returns it to the callback.
  virtual void GetAvailableSpace(const GetAvailableSpaceCallback& callback) = 0;

  // Adds a file entry from |doc_entry| under |directory_path|, and modifies
  // the cache state. Adds a new file entry, and store its content from
  // |file_content_path| into the cache.
  //
  // |callback| must not be null.
  virtual void AddUploadedFile(const base::FilePath& directory_path,
                               scoped_ptr<google_apis::ResourceEntry> doc_entry,
                               const base::FilePath& file_content_path,
                               const FileOperationCallback& callback) = 0;

  // Returns miscellaneous metadata of the file system like the largest
  // timestamp. Used in chrome:drive-internals. |callback| must not be null.
  virtual void GetMetadata(
      const GetFilesystemMetadataCallback& callback) = 0;

  // Reloads the file system feeds from the server.
  virtual void Reload() = 0;
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_INTERFACE_H_
