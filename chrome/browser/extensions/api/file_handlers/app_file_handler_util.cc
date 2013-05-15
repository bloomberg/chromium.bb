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
// Preference keys

// The file entries that an extension has permission to access.
const char kFileEntries[] = "file_entries";

// The path to a file entry that an extension had permission to access.
const char kFileEntryPath[] = "path";

// Whether or not an extension had write access to a file entry.
const char kFileEntryWritable[] = "writable";

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

std::vector<const FileHandlerInfo*> FindFileHandlersForFiles(
    const Extension& app, const PathAndMimeTypeSet& files) {
  std::vector<const FileHandlerInfo*> handlers;
  if (files.empty())
    return handlers;

  // Look for file handlers which can handle all the MIME types specified.
  const FileHandlerList* file_handlers = FileHandlers::GetFileHandlers(&app);
  if (!file_handlers)
    return handlers;

  for (FileHandlerList::const_iterator data = file_handlers->begin();
       data != file_handlers->end(); ++data) {
    bool handles_all_types = true;
    for (PathAndMimeTypeSet::const_iterator it = files.begin();
         it != files.end(); ++it) {
      if (!FileHandlerCanHandleFile(*data, it->second, it->first)) {
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

  // Save this file entry in the prefs.
  AddSavedFileEntry(ExtensionSystem::Get(profile)->extension_prefs(),
                    extension_id,
                    result.id,
                    path,
                    writable);
  return result;
}

void AddSavedFileEntry(ExtensionPrefs* prefs,
                       const std::string& extension_id,
                       const std::string& file_entry_id,
                       const base::FilePath& file_path,
                       bool writable) {
  ExtensionPrefs::ScopedDictionaryUpdate update(
      prefs,
      extension_id,
      kFileEntries);
  DictionaryValue* file_entries = update.Get();
  if (!file_entries)
    file_entries = update.Create();

  // Once a file's permissions are set, they can't be changed.
  DictionaryValue* file_entry_dict = NULL;
  if (file_entries->GetDictionary(file_entry_id, &file_entry_dict))
    return;

  file_entry_dict = new DictionaryValue();
  file_entry_dict->SetString(kFileEntryPath, file_path.value());
  file_entry_dict->SetBoolean(kFileEntryWritable, writable);
  file_entries->SetWithoutPathExpansion(file_entry_id, file_entry_dict);
}

void GetSavedFileEntries(
    const ExtensionPrefs* prefs,
    const std::string& extension_id,
    std::vector<SavedFileEntry>* out) {
  const DictionaryValue* file_entries = NULL;
  if (!prefs || !prefs->ReadPrefAsDictionary(extension_id,
                                             kFileEntries,
                                             &file_entries)) {
    return;
  }

  for (DictionaryValue::Iterator iter(*file_entries);
       !iter.IsAtEnd(); iter.Advance()) {
    const DictionaryValue* file_entry = NULL;
    if (!iter.value().GetAsDictionary(&file_entry))
      continue;
    base::FilePath::StringType path_string;
    if (!file_entry->GetString(kFileEntryPath, &path_string))
      continue;
    bool writable = false;
    if (!file_entry->GetBoolean(kFileEntryWritable, &writable))
      continue;
    base::FilePath file_path(path_string);
    out->push_back(SavedFileEntry(iter.key(), file_path, writable));
  }
}

void ClearSavedFileEntries(ExtensionPrefs* prefs,
                           const std::string& extension_id) {
  prefs->UpdateExtensionPref(extension_id, kFileEntries, NULL);
}

}  // namespace app_file_handler_util

}  // namespace extensions
