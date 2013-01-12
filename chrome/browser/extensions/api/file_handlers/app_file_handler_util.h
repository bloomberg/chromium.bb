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

}  // namespace app_file_handler_util

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_HANDLERS_APP_FILE_HANDLER_UTIL_H_
