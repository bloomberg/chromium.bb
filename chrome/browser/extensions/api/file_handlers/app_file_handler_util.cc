// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/granted_file_entry.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/mime_util.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "storage/common/fileapi/file_system_mount_option.h"
#include "storage/common/fileapi/file_system_types.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/file_manager/filesystem_api_util.h"
#endif

namespace extensions {

namespace app_file_handler_util {

const char kInvalidParameters[] = "Invalid parameters";
const char kSecurityError[] = "Security error";

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

bool DoCheckWritableFile(const base::FilePath& path, bool is_directory) {
  // Don't allow links.
  if (base::PathExists(path) && base::IsLink(path))
    return false;

  if (is_directory)
    return base::DirectoryExists(path);

  // Create the file if it doesn't already exist.
  int creation_flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ;
  base::File file(path, creation_flags);
  return file.IsValid();
}

// Checks whether a list of paths are all OK for writing and calls a provided
// on_success or on_failure callback when done. A file is OK for writing if it
// is not a symlink, is not in a blacklisted path and can be opened for writing;
// files are created if they do not exist.
class WritableFileChecker
    : public base::RefCountedThreadSafe<WritableFileChecker> {
 public:
  WritableFileChecker(
      const std::vector<base::FilePath>& paths,
      Profile* profile,
      bool is_directory,
      const base::Closure& on_success,
      const base::Callback<void(const base::FilePath&)>& on_failure);

  void Check();

 private:
  friend class base::RefCountedThreadSafe<WritableFileChecker>;
  virtual ~WritableFileChecker();

  // Called when a work item is completed. If all work items are done, this
  // calls the success or failure callback.
  void TaskDone();

  // Reports an error in completing a work item. This may be called more than
  // once, but only the last message will be retained.
  void Error(const base::FilePath& error_path);

  void CheckLocalWritableFiles();

#if defined(OS_CHROMEOS)
  void NonNativeLocalPathCheckDone(const base::FilePath& path, bool success);
#endif

  const std::vector<base::FilePath> paths_;
  Profile* profile_;
  const bool is_directory_;
  int outstanding_tasks_;
  base::FilePath error_path_;
  base::Closure on_success_;
  base::Callback<void(const base::FilePath&)> on_failure_;
};

WritableFileChecker::WritableFileChecker(
    const std::vector<base::FilePath>& paths,
    Profile* profile,
    bool is_directory,
    const base::Closure& on_success,
    const base::Callback<void(const base::FilePath&)>& on_failure)
    : paths_(paths),
      profile_(profile),
      is_directory_(is_directory),
      outstanding_tasks_(1),
      on_success_(on_success),
      on_failure_(on_failure) {}

void WritableFileChecker::Check() {
#if defined(OS_CHROMEOS)
  if (file_manager::util::IsUnderNonNativeLocalPath(profile_, paths_[0])) {
    outstanding_tasks_ = paths_.size();
    for (std::vector<base::FilePath>::const_iterator it = paths_.begin();
         it != paths_.end();
         ++it) {
      if (is_directory_) {
        file_manager::util::IsNonNativeLocalPathDirectory(
            profile_,
            *it,
            base::Bind(&WritableFileChecker::NonNativeLocalPathCheckDone,
                       this, *it));
      } else {
        file_manager::util::PrepareNonNativeLocalFileForWritableApp(
            profile_,
            *it,
            base::Bind(&WritableFileChecker::NonNativeLocalPathCheckDone,
                       this, *it));
      }
    }
    return;
  }
#endif
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&WritableFileChecker::CheckLocalWritableFiles, this));
}

WritableFileChecker::~WritableFileChecker() {}

void WritableFileChecker::TaskDone() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (--outstanding_tasks_ == 0) {
    if (error_path_.empty())
      on_success_.Run();
    else
      on_failure_.Run(error_path_);
  }
}

// Reports an error in completing a work item. This may be called more than
// once, but only the last message will be retained.
void WritableFileChecker::Error(const base::FilePath& error_path) {
  DCHECK(!error_path.empty());
  error_path_ = error_path;
  TaskDone();
}

void WritableFileChecker::CheckLocalWritableFiles() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
  std::string error;
  for (std::vector<base::FilePath>::const_iterator it = paths_.begin();
       it != paths_.end();
       ++it) {
    if (!DoCheckWritableFile(*it, is_directory_)) {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI,
          FROM_HERE,
          base::Bind(&WritableFileChecker::Error, this, *it));
      return;
    }
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WritableFileChecker::TaskDone, this));
}

#if defined(OS_CHROMEOS)
void WritableFileChecker::NonNativeLocalPathCheckDone(
    const base::FilePath& path,
    bool success) {
  if (success)
    TaskDone();
  else
    Error(path);
}
#endif

}  // namespace

const FileHandlerInfo* FileHandlerForId(const Extension& app,
                                        const std::string& handler_id) {
  const FileHandlersInfo* file_handlers = FileHandlers::GetFileHandlers(&app);
  if (!file_handlers)
    return NULL;

  for (FileHandlersInfo::const_iterator i = file_handlers->begin();
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
  const FileHandlersInfo* file_handlers = FileHandlers::GetFileHandlers(&app);
  if (!file_handlers)
    return NULL;

  for (FileHandlersInfo::const_iterator i = file_handlers->begin();
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
  const FileHandlersInfo* file_handlers = FileHandlers::GetFileHandlers(&app);
  if (!file_handlers)
    return handlers;

  for (FileHandlersInfo::const_iterator data = file_handlers->begin();
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
    const Extension* extension,
    int renderer_id,
    const base::FilePath& path,
    bool is_directory) {
  GrantedFileEntry result;
  storage::IsolatedContext* isolated_context =
      storage::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  result.filesystem_id = isolated_context->RegisterFileSystemForPath(
      storage::kFileSystemTypeNativeForPlatformApp,
      std::string(),
      path,
      &result.registered_name);

  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  policy->GrantReadFileSystem(renderer_id, result.filesystem_id);
  if (HasFileSystemWritePermission(extension)) {
    if (is_directory) {
      policy->GrantCreateReadWriteFileSystem(renderer_id, result.filesystem_id);
    } else {
      policy->GrantWriteFileSystem(renderer_id, result.filesystem_id);
      policy->GrantDeleteFromFileSystem(renderer_id, result.filesystem_id);
    }
  }

  result.id = result.filesystem_id + ":" + result.registered_name;
  return result;
}

void PrepareFilesForWritableApp(
    const std::vector<base::FilePath>& paths,
    Profile* profile,
    bool is_directory,
    const base::Closure& on_success,
    const base::Callback<void(const base::FilePath&)>& on_failure) {
  scoped_refptr<WritableFileChecker> checker(new WritableFileChecker(
      paths, profile, is_directory, on_success, on_failure));
  checker->Check();
}

bool HasFileSystemWritePermission(const Extension* extension) {
  return extension->permissions_data()->HasAPIPermission(
      APIPermission::kFileSystemWrite);
}

bool ValidateFileEntryAndGetPath(
    const std::string& filesystem_name,
    const std::string& filesystem_path,
    const content::RenderViewHost* render_view_host,
    base::FilePath* file_path,
    std::string* error) {
  if (filesystem_path.empty()) {
    *error = kInvalidParameters;
    return false;
  }

  std::string filesystem_id;
  if (!storage::CrackIsolatedFileSystemName(filesystem_name, &filesystem_id)) {
    *error = kInvalidParameters;
    return false;
  }

  // Only return the display path if the process has read access to the
  // filesystem.
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  if (!policy->CanReadFileSystem(render_view_host->GetProcess()->GetID(),
                                 filesystem_id)) {
    *error = kSecurityError;
    return false;
  }

  storage::IsolatedContext* context = storage::IsolatedContext::GetInstance();
  base::FilePath relative_path =
      base::FilePath::FromUTF8Unsafe(filesystem_path);
  base::FilePath virtual_path = context->CreateVirtualRootPath(filesystem_id)
      .Append(relative_path);
  storage::FileSystemType type;
  storage::FileSystemMountOption mount_option;
  std::string cracked_id;
  if (!context->CrackVirtualPath(
          virtual_path, &filesystem_id, &type, &cracked_id, file_path,
          &mount_option)) {
    *error = kInvalidParameters;
    return false;
  }

  // The file system API is only intended to operate on file entries that
  // correspond to a native file, selected by the user so only allow file
  // systems returned by the file system API or from a drag and drop operation.
  if (type != storage::kFileSystemTypeNativeForPlatformApp &&
      type != storage::kFileSystemTypeDragged) {
    *error = kInvalidParameters;
    return false;
  }

  return true;
}

}  // namespace app_file_handler_util

}  // namespace extensions
