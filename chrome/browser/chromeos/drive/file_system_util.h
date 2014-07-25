// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UTIL_H_

#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "url/gurl.h"

class Profile;

namespace fileapi {
class FileSystemURL;
}

namespace drive {

class DriveAppRegistry;
class DriveServiceInterface;
class FileSystemInterface;


namespace util {

// "drive" diretory's local ID is fixed to this value.
const char kDriveGrandRootLocalId[] = "<drive>";

// "drive/other" diretory's local ID is fixed to this value.
const char kDriveOtherDirLocalId[] = "<other>";

// "drive/trash" diretory's local ID is fixed to this value.
const char kDriveTrashDirLocalId[] = "<trash>";

// The directory names used for the Google Drive file system tree. These names
// are used in URLs for the file manager, hence user-visible.
const base::FilePath::CharType kDriveGrandRootDirName[] =
    FILE_PATH_LITERAL("drive");

const base::FilePath::CharType kDriveMyDriveRootDirName[] =
    FILE_PATH_LITERAL("root");

const base::FilePath::CharType kDriveOtherDirName[] =
    FILE_PATH_LITERAL("other");

const base::FilePath::CharType kDriveTrashDirName[] =
    FILE_PATH_LITERAL("trash");

// Returns the path of the top root of the pseudo tree.
const base::FilePath& GetDriveGrandRootPath();

// Returns the path of the directory representing "My Drive".
const base::FilePath& GetDriveMyDriveRootPath();

// Returns the Drive mount point path, which looks like "/special/drive-<hash>".
base::FilePath GetDriveMountPointPath(Profile* profile);

// Returns the Drive mount point path, which looks like
// "/special/drive-<username_hash>", when provided with the |user_id_hash|.
base::FilePath GetDriveMountPointPathForUserIdHash(std::string user_id_hash);

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

// Returns the gdata file resource url formatted as "drive:<path>"
GURL FilePathToDriveURL(const base::FilePath& path);

// Converts a drive: URL back to a path that can be passed to FileSystem.
base::FilePath DriveURLToFilePath(const GURL& url);

// Overwrites |url| with a Drive URL when appropriate.
void MaybeSetDriveURL(Profile* profile, const base::FilePath& path, GURL* url);

// Returns true if the given path is under the Drive mount point.
bool IsUnderDriveMountPoint(const base::FilePath& path);

// Extracts the Drive path from the given path located under the Drive mount
// point. Returns an empty path if |path| is not under the Drive mount point.
// Examples: ExtractDrivePath("/special/drive-xxx/foo.txt") => "drive/foo.txt"
base::FilePath ExtractDrivePath(const base::FilePath& path);

// Extracts |profile| from the given paths located under
// GetDriveMountPointPath(profile). Returns NULL if it does not correspond to
// a valid mount point path. Must be called from UI thread.
Profile* ExtractProfileFromPath(const base::FilePath& path);

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
// - Replaces slashes '/' with '_'.
// - Replaces the whole input with "_" if the all input characters are '.'.
// |input| must be a valid UTF-8 encoded string.
std::string NormalizeFileName(const std::string& input);

// Gets the cache root path (i.e. <user_profile_dir>/GCache/v1) from the
// profile.
base::FilePath GetCacheRootPath(Profile* profile);

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

// Reads URL from a GDoc file.
GURL ReadUrlFromGDocFile(const base::FilePath& file_path);

// Reads resource ID from a GDoc file.
std::string ReadResourceIdFromGDocFile(const base::FilePath& file_path);

// Returns true if Drive is enabled for the given Profile.
bool IsDriveEnabledForProfile(Profile* profile);

// Enum type for describing the current connection status to Drive.
enum ConnectionStatusType {
  // Disconnected because Drive service is unavailable for this account (either
  // disabled by a flag or the account has no Google account (e.g., guests)).
  DRIVE_DISCONNECTED_NOSERVICE,
  // Disconnected because no network is available.
  DRIVE_DISCONNECTED_NONETWORK,
  // Disconnected because authentication is not ready.
  DRIVE_DISCONNECTED_NOTREADY,
  // Connected by cellular network. Background sync is disabled.
  DRIVE_CONNECTED_METERED,
  // Connected without condition (WiFi, Ethernet, or cellular with the
  // disable-sync preference turned off.)
  DRIVE_CONNECTED,
};

// Returns the Drive connection status for the |profile|.
ConnectionStatusType GetDriveConnectionStatus(Profile* profile);

}  // namespace util
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UTIL_H_
