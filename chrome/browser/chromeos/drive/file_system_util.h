// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UTIL_H_

#include <string>

#include "base/callback_forward.h"
#include "base/platform_file.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "googleurl/src/gurl.h"

class Profile;

namespace base {
class FilePath;
}

namespace fileapi {
class FileSystemURL;
}

namespace drive {

class PlatformFileInfoProto;

namespace util {

// Path constants.

// The extension for dirty files. The file names look like
// "<resource-id>.local".
const base::FilePath::CharType kLocallyModifiedFileExtension[] =
    FILE_PATH_LITERAL("local");
// The extension for mounted files. The file names look like
// "<resource-id>.<md5>.mounted".
const base::FilePath::CharType kMountedArchiveFileExtension[] =
    FILE_PATH_LITERAL("mounted");
const base::FilePath::CharType kWildCard[] =
    FILE_PATH_LITERAL("*");
// The path is used for creating a symlink in "pinned" directory for a file
// which is not yet fetched.
const base::FilePath::CharType kSymLinkToDevNull[] =
    FILE_PATH_LITERAL("/dev/null");

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

const base::FilePath::CharType kDriveMyDriveRootPath[] =
    FILE_PATH_LITERAL("drive/root");

const base::FilePath::CharType kDriveOtherDirPath[] =
    FILE_PATH_LITERAL("drive/other");

// Returns the path of the top root of the pseudo tree.
const base::FilePath& GetDriveGrandRootPath();

// Returns the path of the directory representing "My Drive".
const base::FilePath& GetDriveMyDriveRootPath();

// Returns the path of the directory representing entries other than "My Drive".
const base::FilePath& GetDriveOtherDirPath();

// Returns the Drive mount point path, which looks like "/special/drive".
const base::FilePath& GetDriveMountPointPath();

// Checks if the resource ID is a special one, which is effective only in our
// implementation and is not supposed to be sent to the server.
bool IsSpecialResourceId(const std::string& resource_id);

// Returns a ResourceEntry for "/drive/root" directory.
ResourceEntry CreateMyDriveRootEntry(const std::string& root_resource_id);

// Returns a ResourceEntry for "/drive/other" directory.
ResourceEntry CreateOtherDirEntry();

// Returns the Drive mount path as string.
const std::string& GetDriveMountPointPathAsString();

// Returns the 'local' root of remote file system as "/special".
const base::FilePath& GetSpecialRemoteRootPath();

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

// Escapes forward slashes from file names with magic unicode character
// \u2215 pretty much looks the same in UI.
std::string EscapeUtf8FileName(const std::string& input);

// Extracts resource_id out of edit url.
std::string ExtractResourceIdFromUrl(const GURL& url);

// Gets the cache root path (i.e. <user_profile_dir>/GCache/v1) from the
// profile.
base::FilePath GetCacheRootPath(Profile* profile);

// Extracts resource_id, md5, and extra_extension from cache path.
// Case 1: Pinned and outgoing symlinks only have resource_id.
// Example: path="/user/GCache/v1/pinned/pdf:a1b2" =>
//          resource_id="pdf:a1b2", md5="", extra_extension="";
// Case 2: Normal files have both resource_id and md5.
// Example: path="/user/GCache/v1/tmp/pdf:a1b2.01234567" =>
//          resource_id="pdf:a1b2", md5="01234567", extra_extension="";
// Case 3: Mounted files have all three parts.
// Example: path="/user/GCache/v1/persistent/pdf:a1b2.01234567.mounted" =>
//          resource_id="pdf:a1b2", md5="01234567", extra_extension="mounted".
void ParseCacheFilePath(const base::FilePath& path,
                        std::string* resource_id,
                        std::string* md5,
                        std::string* extra_extension);

// Callback type for PrepareWritablebase::FilePathAndRun.
typedef base::Callback<void (FileError, const base::FilePath& path)>
    OpenFileCallback;

// Invokes |callback| on blocking thread pool, after converting virtual |path|
// string like "/special/drive/foo.txt" to the concrete local cache file path.
// After |callback| returns, the written content is synchronized to the server.
//
// If |path| is not a Drive path, it is regarded as a local path and no path
// conversion takes place.
//
// Must be called from UI thread.
void PrepareWritableFileAndRun(Profile* profile,
                               const base::FilePath& path,
                               const OpenFileCallback& callback);

// Ensures the existence of |directory| of '/special/drive/foo'.  This will
// create |directory| and its ancestors if they don't exist.  |callback| is
// invoked after making sure that |directory| exists.  |callback| should
// interpret error codes of either FILE_ERROR_OK or FILE_ERROR_EXISTS as
// indicating that |directory| now exists.
//
// If |directory| is not a Drive path, it won't check the existence and just
// runs |callback|.
//
// Must be called from UI/IO thread.
void EnsureDirectoryExists(Profile* profile,
                           const base::FilePath& directory,
                           const FileOperationCallback& callback);

// Converts GData error code into file platform error code.
FileError GDataToFileError(google_apis::GDataErrorCode status);

// Converts the resource entry to the platform file.
void ConvertResourceEntryToPlatformFileInfo(
    const PlatformFileInfoProto& entry,
    base::PlatformFileInfo* file_info);

// Converts the platform file info to the resource entry.
void ConvertPlatformFileInfoToResourceEntry(
    const base::PlatformFileInfo& file_info,
    PlatformFileInfoProto* entry);

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

}  // namespace util
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UTIL_H_
