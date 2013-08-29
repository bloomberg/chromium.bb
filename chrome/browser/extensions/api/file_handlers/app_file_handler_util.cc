// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "net/base/mime_util.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/common/fileapi/file_system_types.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/file_system_util.h"
#endif

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

bool DoCheckWritableFile(const base::FilePath& path) {
  // Don't allow links.
  if (base::PathExists(path) && file_util::IsLink(path))
    return false;

  // Create the file if it doesn't already exist.
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  int creation_flags = base::PLATFORM_FILE_CREATE |
                       base::PLATFORM_FILE_READ |
                       base::PLATFORM_FILE_WRITE;
  base::PlatformFile file = base::CreatePlatformFile(path, creation_flags,
                                                     NULL, &error);
  // Close the file so we don't keep a lock open.
  if (file != base::kInvalidPlatformFileValue)
    base::ClosePlatformFile(file);
  if (error != base::PLATFORM_FILE_OK &&
      error != base::PLATFORM_FILE_ERROR_EXISTS) {
    return false;
  }

  return true;
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
  void CheckRemoteWritableFile(const base::FilePath& remote_path,
                               drive::FileError error,
                               const base::FilePath& local_path);
#endif

  const std::vector<base::FilePath> paths_;
  Profile* profile_;
  int outstanding_tasks_;
  base::FilePath error_path_;
  base::Closure on_success_;
  base::Callback<void(const base::FilePath&)> on_failure_;
};

WritableFileChecker::WritableFileChecker(
    const std::vector<base::FilePath>& paths,
    Profile* profile,
    const base::Closure& on_success,
    const base::Callback<void(const base::FilePath&)>& on_failure)
    : paths_(paths),
      profile_(profile),
      outstanding_tasks_(1),
      on_success_(on_success),
      on_failure_(on_failure) {}

void WritableFileChecker::Check() {
#if defined(OS_CHROMEOS)
  if (drive::util::IsUnderDriveMountPoint(paths_[0])) {
    outstanding_tasks_ = paths_.size();
    for (std::vector<base::FilePath>::const_iterator it = paths_.begin();
         it != paths_.end();
         ++it) {
      DCHECK(drive::util::IsUnderDriveMountPoint(*it));
      drive::util::PrepareWritableFileAndRun(
          profile_,
          *it,
          base::Bind(&WritableFileChecker::CheckRemoteWritableFile, this, *it));
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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  std::string error;
  for (std::vector<base::FilePath>::const_iterator it = paths_.begin();
       it != paths_.end();
       ++it) {
    if (!DoCheckWritableFile(*it)) {
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
void WritableFileChecker::CheckRemoteWritableFile(
    const base::FilePath& remote_path,
    drive::FileError error,
    const base::FilePath& /* local_path */) {
  if (error == drive::FILE_ERROR_OK) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&WritableFileChecker::TaskDone, this));
  } else {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&WritableFileChecker::Error, this, remote_path));
  }
}
#endif

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
    const Extension* extension,
    int renderer_id,
    const base::FilePath& path) {
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
  if (HasFileSystemWritePermission(extension))
    policy->GrantWriteFileSystem(renderer_id, result.filesystem_id);

  result.id = result.filesystem_id + ":" + result.registered_name;
  return result;
}

void CheckWritableFiles(
    const std::vector<base::FilePath>& paths,
    Profile* profile,
    const base::Closure& on_success,
    const base::Callback<void(const base::FilePath&)>& on_failure) {
  scoped_refptr<WritableFileChecker> checker(
      new WritableFileChecker(paths, profile, on_success, on_failure));
  checker->Check();
}

bool HasFileSystemWritePermission(const Extension* extension) {
  return extension->HasAPIPermission(APIPermission::kFileSystemWrite);
}

}  // namespace app_file_handler_util

}  // namespace extensions
