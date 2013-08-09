// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides File API related utilities.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILEAPI_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILEAPI_UTIL_H_

#include <string>

class Profile;

namespace content {
class RenderViewHost;
}

namespace fileapi {
class FileSystemContext;
}

namespace file_manager {
namespace fileapi_util {

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

}  // namespace fileapi_util
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILEAPI_UTIL_H_
