// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_HANDLERS_APP_FILE_HANDLER_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_HANDLERS_APP_FILE_HANDLER_UTIL_H_

#include <set>
#include <string>
#include <vector>

#include "chrome/common/extensions/api/file_handlers/file_handlers_parser.h"
#include "chrome/common/extensions/extension.h"

class Profile;

namespace extensions {

// TODO(benwells): move this to platform_apps namespace.
namespace app_file_handler_util {

// Returns the file handler with the specified |handler_id|, or NULL if there
// is no such handler.
const FileHandlerInfo* FileHandlerForId(const Extension& app,
                                        const std::string& handler_id);

// Returns the first file handler that can handle the given MIME type, or NULL
// if is no such handler.
const FileHandlerInfo* FirstFileHandlerForMimeType(const Extension& app,
    const std::string& mime_type);

std::vector<const FileHandlerInfo*>
FindFileHandlersForMimeTypes(const Extension& extension,
                             const std::set<std::string>& mime_types);

bool FileHandlerCanHandleFileWithMimeType(const FileHandlerInfo& handler,
                                          const std::string& mime_type);

// Represents a file entry that a user has given an extension permission to
// access. Intended to be persisted to disk (in the Preferences file), so should
// remain serializable.
struct SavedFileEntry {
  SavedFileEntry(const std::string& id,
                 const base::FilePath& path,
                 bool writable)
      : id(id),
        path(path),
        writable(writable) {}

  std::string id;
  base::FilePath path;
  bool writable;
};

// Refers to a file entry that a renderer has been given access to.
struct GrantedFileEntry {
  std::string id;
  std::string filesystem_id;
  std::string registered_name;
};

// Creates a new file entry and allows |renderer_id| to access |path|. This
// registers a new file system for |path|.
GrantedFileEntry CreateFileEntry(
    Profile* profile,
    const std::string& extension_id,
    int renderer_id,
    const base::FilePath& path,
    bool writable);

}  // namespace app_file_handler_util

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_HANDLERS_APP_FILE_HANDLER_UTIL_H_
