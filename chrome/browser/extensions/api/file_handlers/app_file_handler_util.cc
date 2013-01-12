// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"

#include "net/base/mime_util.h"

namespace extensions {

namespace app_file_handler_util {

typedef std::vector<FileHandlerInfo> FileHandlerList;

const FileHandlerInfo* FileHandlerForId(const Extension& app,
                                        const std::string& handler_id) {
  const FileHandlerList* file_handlers = FileHandlers::GetFileHandlers(&app);
  if (!file_handlers)
    return NULL;

  for (FileHandlerList::const_iterator i = file_handlers->begin();
       i != file_handlers->end(); i++) {
    if (i->id == handler_id)
      return &*i;
  }
  return NULL;
}

const FileHandlerInfo* FirstFileHandlerForMimeType(const Extension& app,
    const std::string& mime_type) {
  const FileHandlerList* file_handlers = FileHandlers::GetFileHandlers(&app);
  if (!file_handlers)
    return NULL;

  for (FileHandlerList::const_iterator i = file_handlers->begin();
       i != file_handlers->end(); i++) {
    for (std::set<std::string>::const_iterator t = i->types.begin();
         t != i->types.end(); t++) {
      if (net::MatchesMimeType(*t, mime_type))
        return &*i;
    }
  }
  return NULL;
}

std::vector<const FileHandlerInfo*> FindFileHandlersForMimeTypes(
    const Extension& app, const std::set<std::string>& mime_types) {
  std::vector<const FileHandlerInfo*> handlers;
  if (mime_types.empty())
    return handlers;

  // Look for file handlers which can handle all the MIME types specified.
  const FileHandlerList* file_handlers = FileHandlers::GetFileHandlers(&app);
  if (!file_handlers)
    return handlers;

  for (FileHandlerList::const_iterator data = file_handlers->begin();
       data != file_handlers->end(); ++data) {
    bool handles_all_types = true;
    for (std::set<std::string>::const_iterator type_iter = mime_types.begin();
         type_iter != mime_types.end(); ++type_iter) {
      if (!FileHandlerCanHandleFileWithMimeType(*data, *type_iter)) {
        handles_all_types = false;
        break;
      }
    }
    if (handles_all_types)
      handlers.push_back(&*data);
  }
  return handlers;
}

bool FileHandlerCanHandleFileWithMimeType(
    const FileHandlerInfo& handler,
    const std::string& mime_type) {
  // TODO(benwells): this should check the file's extension as well.
  for (std::set<std::string>::const_iterator type = handler.types.begin();
       type != handler.types.end(); ++type) {
    if (net::MatchesMimeType(*type, mime_type))
      return true;
  }
  return false;
}

}  // namespace app_file_handler_util

}  // namespace extensions
