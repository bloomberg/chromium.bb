// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_HANDLERS_APP_FILE_HANDLER_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_HANDLERS_APP_FILE_HANDLER_UTIL_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "content/public/browser/render_view_host.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/file_handler_info.h"

class Profile;

namespace extensions {

class ExtensionPrefs;
struct GrantedFileEntry;

// TODO(benwells): move this to platform_apps namespace.
namespace app_file_handler_util {

extern const char kInvalidParameters[];
extern const char kSecurityError[];

// A set of pairs of path and its corresponding MIME type.
typedef std::set<std::pair<base::FilePath, std::string> > PathAndMimeTypeSet;

// Returns the file handler with the specified |handler_id|, or NULL if there
// is no such handler.
const FileHandlerInfo* FileHandlerForId(const Extension& app,
                                        const std::string& handler_id);

// Returns the first file handler that can handle the given MIME type or
// filename, or NULL if is no such handler.
const FileHandlerInfo* FirstFileHandlerForFile(
    const Extension& app,
    const std::string& mime_type,
    const base::FilePath& path);

// Returns the handlers that can handle all files in |files|. The paths in
// |files| must be populated, but the MIME types are optional.
std::vector<const FileHandlerInfo*>
FindFileHandlersForFiles(const Extension& extension,
                         const PathAndMimeTypeSet& files);

bool FileHandlerCanHandleFile(
    const FileHandlerInfo& handler,
    const std::string& mime_type,
    const base::FilePath& path);

// Creates a new file entry and allows |renderer_id| to access |path|. This
// registers a new file system for |path|.
GrantedFileEntry CreateFileEntry(Profile* profile,
                                 const Extension* extension,
                                 int renderer_id,
                                 const base::FilePath& path,
                                 bool is_directory);

// When |is_directory| is true, it verifies that directories exist at each of
// the |paths| and calls back to |on_success| or otherwise to |on_failure|.
// When |is_directory| is false, it ensures regular files exists (not links and
// directories) at the |paths|, creating files if needed, and calls back to
// |on_success| or to |on_failure| depending on the result.
void PrepareFilesForWritableApp(
    const std::vector<base::FilePath>& paths,
    Profile* profile,
    bool is_directory,
    const base::Closure& on_success,
    const base::Callback<void(const base::FilePath&)>& on_failure);

// Returns whether |extension| has the fileSystem.write permission.
bool HasFileSystemWritePermission(const Extension* extension);

// Validates a file entry and populates |file_path| with the absolute path if it
// is valid.
bool ValidateFileEntryAndGetPath(
    const std::string& filesystem_name,
    const std::string& filesystem_path,
    const content::RenderViewHost* render_view_host,
    base::FilePath* file_path,
    std::string* error);

}  // namespace app_file_handler_util

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_HANDLERS_APP_FILE_HANDLER_UTIL_H_
