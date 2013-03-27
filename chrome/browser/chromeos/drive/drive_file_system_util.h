// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_UTIL_H_

#include <string>

#include "base/callback_forward.h"
#include "base/platform_file.h"
#include "chrome/browser/chromeos/drive/drive_file_error.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "googleurl/src/gurl.h"

class Profile;

namespace base {
class FilePath;
}

namespace drive {

class PlatformFileInfoProto;

namespace util {

// Path constants.

// The extension for dirty files. The file names look like
// "<resource-id>.local".
const char kLocallyModifiedFileExtension[] = "local";
// The extension for mounted files. The file names look like
// "<resource-id>.<md5>.mounted".
const char kMountedArchiveFileExtension[] = "mounted";
const char kWildCard[] = "*";
// The path is used for creating a symlink in "pinned" directory for a file
// which is not yet fetched.
const char kSymLinkToDevNull[] = "/dev/null";

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

// TODO(haruki): Change this to "drive/root" in order to use separate namespace.
// http://crbug.com/174233
const base::FilePath::CharType kDriveMyDriveRootPath[] =
    FILE_PATH_LITERAL("drive");

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

// Returns the Drive mount path as string.
const std::string& GetDriveMountPointPathAsString();

// Returns the 'local' root of remote file system as "/special".
const base::FilePath& GetSpecialRemoteRootPath();

// Returns the gdata file resource url formatted as
// chrome://drive/<resource_id>/<file_name>.
GURL GetFileResourceUrl(const std::string& resource_id,
                        const std::string& file_name);

// Given a profile and a drive_cache_path, return the file resource url.
void ModifyDriveFileResourceUrl(Profile* profile,
                                const base::FilePath& drive_cache_path,
                                GURL* url);

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
// Examples: ExtractGDatPath("/special/drive/foo.txt") => "drive/foo.txt"
base::FilePath ExtractDrivePath(const base::FilePath& path);

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
typedef base::Callback<void (DriveFileError, const base::FilePath& path)>
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
// interpret error codes of either DRIVE_FILE_OK or DRIVE_FILE_ERROR_EXISTS as
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
DriveFileError GDataToDriveFileError(google_apis::GDataErrorCode status);

// Converts the proto representation to the platform file.
void ConvertProtoToPlatformFileInfo(const PlatformFileInfoProto& proto,
                                    base::PlatformFileInfo* file_info);

// Converts the platform file info to the proto representation.
void ConvertPlatformFileInfoToProto(const base::PlatformFileInfo& file_info,
                                    PlatformFileInfoProto* proto);

// Does nothing with |error|. Used with functions taking FileOperationCallback.
void EmptyFileOperationCallback(DriveFileError error);

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

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_UTIL_H_
