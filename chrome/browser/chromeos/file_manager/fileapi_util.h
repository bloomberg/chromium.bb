// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides File API related utilities.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILEAPI_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILEAPI_UTIL_H_

#include <string>
#include "url/gurl.h"

class Profile;

namespace base {
class FilePath;
}

namespace content {
class RenderViewHost;
}

namespace fileapi {
class FileSystemContext;
}

namespace file_manager {
namespace util {

// Returns a file system context associated with the given profile and the
// extension ID.
fileapi::FileSystemContext* GetFileSystemContextForExtensionId(
    Profile* profile,
    const std::string& extension_id);

// Returns a file system context associated with the given profile and the
// render view host.
fileapi::FileSystemContext* GetFileSystemContextForRenderViewHost(
    Profile* profile,
    content::RenderViewHost* render_view_host);

// Converts DrivePath (e.g., "drive/root", which always starts with the fixed
// "drive" directory) to a RelativeFileSystemPathrelative (e.g.,
// "drive-xxx/root/foo". which starts from the "mount point" in the FileSystem
// API that may be distinguished for each profile by the appended "xxx" part.)
base::FilePath ConvertDrivePathToRelativeFileSystemPath(
    Profile* profile,
    const std::string& extension_id,
    const base::FilePath& drive_path);

// Converts DrivePath to FileSystem URL.
// E.g., "drive/root" to filesystem://id/external/drive-xxx/root.
GURL ConvertDrivePathToFileSystemUrl(Profile* profile,
                                     const base::FilePath& drive_path,
                                     const std::string& extension_id);

// Converts AbsolutePath (e.g., "/special/drive-xxx/root" or
// "/home/chronos/u-xxx/Downloads") into filesystem URL. Returns false
// if |absolute_path| is not managed by the external filesystem provider.
bool ConvertAbsoluteFilePathToFileSystemUrl(Profile* profile,
                                            const base::FilePath& absolute_path,
                                            const std::string& extension_id,
                                            GURL* url);

// Converts AbsolutePath into RelativeFileSystemPath (e.g.,
// "/special/drive-xxx/root/foo" => "drive-xxx/root/foo".) Returns false if
// |absolute_path| is not managed by the external filesystem provider.
bool ConvertAbsoluteFilePathToRelativeFileSystemPath(
    Profile* profile,
    const std::string& extension_id,
    const base::FilePath& absolute_path,
    base::FilePath* relative_path);

}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILEAPI_UTIL_H_
