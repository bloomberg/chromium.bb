// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UTIL_H_

#include <string>

#include "base/callback_forward.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "url/gurl.h"

class Profile;

namespace base {
class FilePath;
}

namespace fileapi {
class FileSystemURL;
}

namespace drive {

class DriveAppRegistry;
class DriveServiceInterface;
class FileSystemInterface;
class ResourceEntry;

namespace util {

// Special resource IDs introduced to manage pseudo directory tree locally.
// These strings are supposed to be different from any resource ID used on the
// server, and are never sent to the server. Practical resource IDs used so far
// have only alphabets/numbers ([a-zA-Z0-9]) and ':'.
// Hence '<' and '>' around the directory name have been added to make them
// different from normal server-side IDs.
const char kDriveGrandRootSpecialResourceId[] = "<drive>";

const char kDriveOtherDirSpecialResourceId[] = "<other>";

// The directory names used for the Google Drive file system tree. These names
// are used in URLs for the file manager, hence user-visible.
const base::FilePath::CharType kDriveGrandRootDirName[] =
    FILE_PATH_LITERAL("drive");

const base::FilePath::CharType kDriveMyDriveRootDirName[] =
    FILE_PATH_LITERAL("root");

const base::FilePath::CharType kDriveOtherDirName[] =
    FILE_PATH_LITERAL("other");

// Returns the path of the top root of the pseudo tree.
const base::FilePath& GetDriveGrandRootPath();

// Returns the path of the directory representing "My Drive".
const base::FilePath& GetDriveMyDriveRootPath();

// Returns the Drive mount point path, which looks like "/special/drive".
const base::FilePath& GetDriveMountPointPath();

// Returns the FileSystem for the |profile|. If not available (not mounted
// or disabled), returns NULL.
FileSystemInterface* GetFileSystemByProfile(Profile* profile);

// Returns a FileSystemInterface instance for the |profile_id|, or NULL
// if the Profile for |profile_id| is destructed or Drive File System is
// disabled for the profile.
// Note: |profile_id| should be the pointer of the Profile instance if it is
// alive. Considering timing issues due to task posting across threads,
// this function can accept a dangling pointer as |profile_id| (and will return
// NULL for such a case).
// This function must be called on UI thread.
FileSystemInterface* GetFileSystemByProfileId(void* profile_id);

// Returns the DriveAppRegistry for the |profile|. If not available (not
// mounted or disabled), returns NULL.
DriveAppRegistry* GetDriveAppRegistryByProfile(Profile* profile);

// Returns the DriveService for the |profile|. If not available (not mounted
// or disabled), returns NULL.
DriveServiceInterface* GetDriveServiceByProfile(Profile* profile);

// Checks if the resource ID is a special one, which is effective only in our
// implementation and is not supposed to be sent to the server.
bool IsSpecialResourceId(const std::string& resource_id);

// Returns a ResourceEntry for "/drive/root" directory.
ResourceEntry CreateMyDriveRootEntry(const std::string& root_resource_id);

// Returns the Drive mount path as string.
const std::string& GetDriveMountPointPathAsString();

// Returns the gdata file resource url formatted as "drive:<path>"
GURL FilePathToDriveURL(const base::FilePath& path);

// Converts a drive: URL back to a path that can be passed to FileSystem.
base::FilePath DriveURLToFilePath(const GURL& url);

// Overwrites |url| with a Drive URL when appropriate.
void MaybeSetDriveURL(Profile* profile, const base::FilePath& path, GURL* url);

// Returns true if the given path is under the Drive mount point.
bool IsUnderDriveMountPoint(const base::FilePath& path);

// Returns true if the given path is under the Drive mount point and needs to be
// migrated to the new namespace. http://crbug.com/174233.
bool NeedsNamespaceMigration(const base::FilePath& path);

// Returns new FilePath with a namespace "root" inserted at the 3rd component.
// e.g. "/special/drive/root/dir" for "/special/drive/dir".
// NeedsNamespaceMigration(path) should be true (after the TODOs are resolved).
base::FilePath ConvertToMyDriveNamespace(const base::FilePath& path);

// Extracts the Drive path from the given path located under the Drive mount
// point. Returns an empty path if |path| is not under the Drive mount point.
// Examples: ExtractDrivePath("/special/drive/foo.txt") => "drive/foo.txt"
base::FilePath ExtractDrivePath(const base::FilePath& path);

// Extracts the Drive path (e.g., "drive/foo.txt") from the filesystem URL.
// Returns an empty path if |url| does not point under Drive mount point.
base::FilePath ExtractDrivePathFromFileSystemUrl(
    const fileapi::FileSystemURL& url);

// Escapes a file name in Drive cache.
// Replaces percent ('%'), period ('.') and slash ('/') with %XX (hex)
std::string EscapeCacheFileName(const std::string& filename);

// Unescapes a file path in Drive cache.
// This is the inverse of EscapeCacheFileName.
std::string UnescapeCacheFileName(const std::string& filename);

// Converts the given string to a form suitable as a file name. Specifically,
// - Normalizes in Unicode Normalization Form C.
// - Replaces slashes '/' with \u2215 that pretty much looks the same in UI.
// |input| must be a valid UTF-8 encoded string.
std::string NormalizeFileName(const std::string& input);

// Gets the cache root path (i.e. <user_profile_dir>/GCache/v1) from the
// profile.
base::FilePath GetCacheRootPath(Profile* profile);

// Migrates cache files from old "persistent" and "tmp" directories to the new
// "files" directory (see crbug.com/248905).
// TODO(hashimoto): Remove this function at some point.
void MigrateCacheFilesFromOldDirectories(
    const base::FilePath& cache_root_directory,
    const base::FilePath::StringType& cache_file_directory_name);

// Callback type for PrepareWritableFileAndRun.
typedef base::Callback<void (FileError, const base::FilePath& path)>
    PrepareWritableFileCallback;

// Invokes |callback| on blocking thread pool, after converting virtual |path|
// string like "/special/drive/foo.txt" to the concrete local cache file path.
// After |callback| returns, the written content is synchronized to the server.
//
// The |path| must be a path under Drive. Must be called from UI thread.
void PrepareWritableFileAndRun(Profile* profile,
                               const base::FilePath& path,
                               const PrepareWritableFileCallback& callback);

// Ensures the existence of |directory| of '/special/drive/foo'.  This will
// create |directory| and its ancestors if they don't exist.  |callback| is
// invoked after making sure that |directory| exists.  |callback| should
// interpret error codes of either FILE_ERROR_OK or FILE_ERROR_EXISTS as
// indicating that |directory| now exists.
//
// If |directory| is not a Drive path, it won't check the existence and just
// runs |callback|.
//
// Must be called from UI thread.
void EnsureDirectoryExists(Profile* profile,
                           const base::FilePath& directory,
                           const FileOperationCallback& callback);

// Does nothing with |error|. Used with functions taking FileOperationCallback.
void EmptyFileOperationCallback(FileError error);

// Helper to destroy objects which needs Destroy() to be called on destruction.
struct DestroyHelper {
  template<typename T>
  void operator()(T* object) const {
    if (object)
      object->Destroy();
  }
};

// Creates a GDoc file with given values.
//
// GDoc files are used to represent hosted documents on local filesystems.
// A GDoc file contains a JSON whose content is a URL to view the document and
// a resource ID of the entry.
bool CreateGDocFile(const base::FilePath& file_path,
                    const GURL& url,
                    const std::string& resource_id);

// Returns true if |file_path| has a GDoc file extension. (e.g. ".gdoc")
bool HasGDocFileExtension(const base::FilePath& file_path);

// Reads URL from a GDoc file.
GURL ReadUrlFromGDocFile(const base::FilePath& file_path);

// Reads resource ID from a GDoc file.
std::string ReadResourceIdFromGDocFile(const base::FilePath& file_path);

// Returns the (base-16 encoded) MD5 digest of the file content at |file_path|,
// or an empty string if an error is found.
std::string GetMd5Digest(const base::FilePath& file_path);

// Returns true if Drive is enabled for the given Profile.
bool IsDriveEnabledForProfile(Profile* profile);

}  // namespace util
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UTIL_H_
