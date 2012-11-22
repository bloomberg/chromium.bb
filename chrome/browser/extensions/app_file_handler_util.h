// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_FILE_HANDLER_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_APP_FILE_HANDLER_UTIL_H_

#include "chrome/common/extensions/extension.h"

// TODO(benwells): move this to platform_apps namespace.
namespace app_file_handler_util {

// Returns the file handler with the specified |handler_id|, or NULL if there
// is no such handler.
const extensions::Extension::FileHandlerInfo* FileHandlerForId(
    const extensions::Extension& app,
    const std::string& handler_id);

// Returns the first file handler that can handle the given MIME type, or NULL
// if is no such handler.
const extensions::Extension::FileHandlerInfo* FirstFileHandlerForMimeType(
    const extensions::Extension& app,
    const std::string& mime_type);

std::vector<const extensions::Extension::FileHandlerInfo*>
FindFileHandlersForMimeTypes(
    const extensions::Extension& extension,
    const std::set<std::string>& mime_types);

bool FileHandlerCanHandleFileWithMimeType(
    const extensions::Extension::FileHandlerInfo& handler,
    const std::string& mime_type);

}

#endif  // CHROME_BROWSER_EXTENSIONS_APP_FILE_HANDLER_UTIL_H_
