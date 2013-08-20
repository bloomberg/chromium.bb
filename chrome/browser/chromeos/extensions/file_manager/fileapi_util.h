// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides File API related utilities.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILEAPI_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILEAPI_UTIL_H_

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

// Converts |relative_path| (e.g., "drive/root" or "Downloads") into external
// filesystem URL (e.g., filesystem://id/external/drive/root).
GURL ConvertRelativeFilePathToFileSystemUrl(const base::FilePath& relative_path,
                                            const std::string& extension_id);

// Converts |absolute_path| (e.g., "/special/drive/root" or
// "/home/chronos/user/Downloads") into external filesystem URL. Returns false
// if |absolute_path| is not managed by the external filesystem provider.
bool ConvertAbsoluteFilePathToFileSystemUrl(
    Profile* profile,
    const base::FilePath& absolute_path,
    const std::string& extension_id,
    GURL* url);

// Converts |absolute_path| into |relative_path| within the external
// provider in File API. Returns false if |absolute_path| is not managed
// by the external filesystem provider.
bool ConvertAbsoluteFilePathToRelativeFileSystemPath(
    Profile* profile,
    const std::string& extension_id,
    const base::FilePath& absolute_path,
    base::FilePath* relative_path);

}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILEAPI_UTIL_H_
