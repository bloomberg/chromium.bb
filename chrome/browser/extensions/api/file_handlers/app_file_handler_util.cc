// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"

#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "content/public/browser/child_process_security_policy.h"
#include "net/base/mime_util.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/isolated_context.h"

namespace extensions {

namespace app_file_handler_util {

namespace {

bool FileHandlerCanHandleFileWithExtension(
    const FileHandlerInfo& handler,
    const base::FilePath& path) {
  for (std::set<std::string>::const_iterator extension =
       handler.extensions.begin();
       extension != handler.extensions.end(); ++extension) {
    if (*extension == "*")
      return true;

    if (path.MatchesExtension(
        base::FilePath::kExtensionSeparator +
        base::FilePath::FromUTF8Unsafe(*extension).value())) {
      return true;
    }

    // Also accept files with no extension for handlers that support an
    // empty extension, i.e. both "foo" and "foo." match.
    if (extension->empty() &&
        path.MatchesExtension(base::FilePath::StringType())) {
      return true;
    }
  }
  return false;
}

}  // namespace

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

const FileHandlerInfo* FirstFileHandlerForFile(
    const Extension& app,
    const std::string& mime_type,
    const base::FilePath& path) {
  const FileHandlerList* file_handlers = FileHandlers::GetFileHandlers(&app);
  if (!file_handlers)
    return NULL;

  for (FileHandlerList::const_iterator i = file_handlers->begin();
       i != file_handlers->end(); i++) {
    if (FileHandlerCanHandleFile(*i, mime_type, path))
      return &*i;
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

bool FileHandlerCanHandleFile(
    const FileHandlerInfo& handler,
    const std::string& mime_type,
    const base::FilePath& path) {
  return FileHandlerCanHandleFileWithMimeType(handler, mime_type) ||
      FileHandlerCanHandleFileWithExtension(handler, path);
}

bool FileHandlerCanHandleFileWithMimeType(
    const FileHandlerInfo& handler,
    const std::string& mime_type) {
  for (std::set<std::string>::const_iterator type = handler.types.begin();
       type != handler.types.end(); ++type) {
    if (net::MatchesMimeType(*type, mime_type))
      return true;
  }
  return false;
}

GrantedFileEntry CreateFileEntry(
    Profile* profile,
    const std::string& extension_id,
    int renderer_id,
    const base::FilePath& path,
    bool writable) {
  GrantedFileEntry result;
  fileapi::IsolatedContext* isolated_context =
      fileapi::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  result.filesystem_id = isolated_context->RegisterFileSystemForPath(
      fileapi::kFileSystemTypeNativeForPlatformApp, path,
      &result.registered_name);

  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  policy->GrantReadFileSystem(renderer_id, result.filesystem_id);
  if (writable)
    policy->GrantWriteFileSystem(renderer_id, result.filesystem_id);

  result.id = result.filesystem_id + ":" + result.registered_name;

  // We only need file level access for reading FileEntries. Saving FileEntries
  // just needs the file system to have read/write access, which is granted
  // above if required.
  if (!policy->CanReadFile(renderer_id, path))
    policy->GrantReadFile(renderer_id, path);

  ExtensionPrefs* prefs = extensions::ExtensionSystem::Get(profile)->
      extension_service()->extension_prefs();
  // Save this file entry in the prefs.
  prefs->AddSavedFileEntry(extension_id, result.id, path, writable);

  return result;
}

}  // namespace app_file_handler_util

}  // namespace extensions
