// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_HANDLERS_APP_FILE_HANDLER_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_HANDLERS_APP_FILE_HANDLER_UTIL_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "chrome/common/extensions/api/file_handlers/file_handlers_parser.h"
#include "chrome/common/extensions/extension.h"

class Profile;

namespace extensions {
class ExtensionPrefs;

// TODO(benwells): move this to platform_apps namespace.
namespace app_file_handler_util {

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

// Refers to a file entry that a renderer has been given access to.
struct GrantedFileEntry {
  GrantedFileEntry();

  std::string id;
  std::string filesystem_id;
  std::string registered_name;
};

// Creates a new file entry and allows |renderer_id| to access |path|. This
// registers a new file system for |path|.
GrantedFileEntry CreateFileEntry(
    Profile* profile,
    const Extension* extension,
    int renderer_id,
    const base::FilePath& path,
    bool is_directory);

void CheckWritableFiles(
    const std::vector<base::FilePath>& paths,
    Profile* profile,
    bool is_directory,
    const base::Closure& on_success,
    const base::Callback<void(const base::FilePath&)>& on_failure);

// Returns whether |extension| has the fileSystem.write permission.
bool HasFileSystemWritePermission(const Extension* extension);

}  // namespace app_file_handler_util

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_HANDLERS_APP_FILE_HANDLER_UTIL_H_
