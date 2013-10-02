// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNCABLE_FILE_SYSTEM_UTIL_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNCABLE_FILE_SYSTEM_UTIL_H_

#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace fileapi {
class FileSystemContext;
class FileSystemURL;
}

namespace tracked_objects {
class Location;
}

namespace sync_file_system {

// Registers a syncable filesystem.
void RegisterSyncableFileSystem();

// Revokes the syncable filesystem.
void RevokeSyncableFileSystem();

// Returns the root URI of the syncable filesystem for |origin|.
GURL GetSyncableFileSystemRootURI(const GURL& origin);

// Creates a FileSystem URL for the |path| in a syncable filesystem for
// |origin|.
//
// Example: Assume following arguments are given:
//   origin: 'http://www.example.com/',
//   path: '/foo/bar',
// returns 'filesystem:http://www.example.com/external/syncfs/foo/bar'
fileapi::FileSystemURL
CreateSyncableFileSystemURL(const GURL& origin, const base::FilePath& path);

// Creates a special filesystem URL for synchronizing |syncable_url|.
fileapi::FileSystemURL CreateSyncableFileSystemURLForSync(
    fileapi::FileSystemContext* file_system_context,
    const fileapi::FileSystemURL& syncable_url);

// Serializes a given FileSystemURL |url| and sets the serialized string to
// |serialized_url|. If the URL does not represent a syncable filesystem,
// |serialized_url| is not filled in, and returns false. Separators of the
// path will be normalized depending on its platform.
//
// Example: Assume a following FileSystemURL object is given:
//   origin() returns 'http://www.example.com/',
//   type() returns the kFileSystemTypeSyncable,
//   filesystem_id() returns 'syncfs',
//   path() returns '/foo/bar',
// this URL will be serialized to
// (on Windows)
//   'filesystem:http://www.example.com/external/syncfs/foo\\bar'
// (on others)
//   'filesystem:http://www.example.com/external/syncfs/foo/bar'
bool SerializeSyncableFileSystemURL(
    const fileapi::FileSystemURL& url, std::string* serialized_url);

// Deserializes a serialized FileSystem URL string |serialized_url| and sets the
// deserialized value to |url|. If the reconstructed object is invalid or does
// not represent a syncable filesystem, returns false.
//
// NOTE: On any platform other than Windows, this function assumes that
// |serialized_url| does not contain '\\'. If it contains '\\' on such
// platforms, '\\' may be replaced with '/' (It would not be an expected
// behavior).
//
// See the comment of SerializeSyncableFileSystemURL() for more details.
bool DeserializeSyncableFileSystemURL(
    const std::string& serialized_url, fileapi::FileSystemURL* url);

// Enables or disables directory operations in Sync FileSystem API.
// TODO(nhiroki): This method should be used only for testing and should go
// away when we fully support directory operations. (http://crbug.com/161442)
void SetEnableSyncFSDirectoryOperation(bool flag);

// Returns true if we allow directory operations in Sync FileSystem API.
// It is disabled by default but can be overridden by a command-line switch
// (--enable-syncfs-directory-operations) or by calling
// SetEnableSyncFSDirectoryOperation().
// TODO(nhiroki): This method should be used only for testing and should go
// away when we fully support directory operations. (http://crbug.com/161442)
bool IsSyncFSDirectoryOperationEnabled();

// Returns SyncFileSystem sub-directory path.
base::FilePath GetSyncFileSystemDir(const base::FilePath& profile_base_dir);

// Enables directory operation for syncable filesystems temporarily for testing.
class ScopedEnableSyncFSDirectoryOperation {
 public:
  ScopedEnableSyncFSDirectoryOperation();
  ~ScopedEnableSyncFSDirectoryOperation();

 private:
  bool was_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ScopedEnableSyncFSDirectoryOperation);
};

// Posts |callback| to the current thread.
void RunSoon(const tracked_objects::Location& from_here,
             const base::Closure& callback);

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNCABLE_FILE_SYSTEM_UTIL_H_
