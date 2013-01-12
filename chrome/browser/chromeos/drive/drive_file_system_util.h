// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_UTIL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "googleurl/src/gurl.h"

class FilePath;
class Profile;

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

// Returns the Drive mount point path, which looks like "/special/drive".
const FilePath& GetDriveMountPointPath();

// Returns the Drive mount path as string.
const std::string& GetDriveMountPointPathAsString();

// Returns the 'local' root of remote file system as "/special".
const FilePath& GetSpecialRemoteRootPath();

// Returns the gdata file resource url formatted as
// chrome://drive/<resource_id>/<file_name>.
GURL GetFileResourceUrl(const std::string& resource_id,
                        const std::string& file_name);

// Given a profile and a drive_cache_path, return the file resource url.
void ModifyDriveFileResourceUrl(Profile* profile,
                                const FilePath& drive_cache_path,
                                GURL* url);

// Returns true if the given path is under the Drive mount point.
bool IsUnderDriveMountPoint(const FilePath& path);

// Extracts the Drive path from the given path located under the Drive mount
// point. Returns an empty path if |path| is not under the Drive mount point.
// Examples: ExtractGDatPath("/special/drive/foo.txt") => "drive/foo.txt"
FilePath ExtractDrivePath(const FilePath& path);

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
void ParseCacheFilePath(const FilePath& path,
                        std::string* resource_id,
                        std::string* md5,
                        std::string* extra_extension);

// Callback type for PrepareWritableFilePathAndRun.
typedef base::Callback<void (DriveFileError, const FilePath& path)>
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
                               const FilePath& path,
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
                           const FilePath& directory,
                           const FileOperationCallback& callback);

// Converts GData error code into file platform error code.
DriveFileError GDataToDriveFileError(google_apis::GDataErrorCode status);

// Converts the proto representation to the platform file.
void ConvertProtoToPlatformFileInfo(const PlatformFileInfoProto& proto,
                                    base::PlatformFileInfo* file_info);

// Converts the platform file info to the proto representation.
void ConvertPlatformFileInfoToProto(const base::PlatformFileInfo& file_info,
                                    PlatformFileInfoProto* proto);
}  // namespace util
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_UTIL_H_
