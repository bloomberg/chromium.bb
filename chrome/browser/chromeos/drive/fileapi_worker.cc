// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/fileapi_worker.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/common/fileapi/directory_entry.h"

using content::BrowserThread;

namespace drive {
namespace fileapi_internal {
namespace {

// The summary of opening mode is:
// - PLATFORM_FILE_OPEN: Open the existing file. Fail if not exists.
// - PLATFORM_FILE_CREATE: Create the file if not exists. Fail if exists.
// - PLATFORM_FILE_OPEN_ALWAYS: Open the existing file. Create a new file
//     if not exists.
// - PLATFORM_FILE_CREATE_ALWAYS: Create a new file if not exists. If exists
//     open it with truncate.
// - PLATFORM_FILE_OPEN_TRUNCATE: Open the existing file with truncate.
//     Fail if not exists.
OpenMode GetOpenMode(int file_flag) {
  if (file_flag & (base::PLATFORM_FILE_OPEN |
                   base::PLATFORM_FILE_OPEN_TRUNCATED))
    return OPEN_FILE;

  if (file_flag & base::PLATFORM_FILE_CREATE)
    return CREATE_FILE;

  DCHECK(file_flag & (base::PLATFORM_FILE_OPEN_ALWAYS |
                      base::PLATFORM_FILE_CREATE_ALWAYS));
  return OPEN_OR_CREATE_FILE;
}

// Runs |callback| with the PlatformFileError converted from |error|.
void RunStatusCallbackByFileError(const StatusCallback& callback,
                                  FileError error) {
  callback.Run(FileErrorToPlatformError(error));
}

// Runs |callback| with arguments converted from |error| and |entry|.
void RunGetFileInfoCallback(const GetFileInfoCallback& callback,
                            FileError error,
                            scoped_ptr<ResourceEntry> entry) {
  if (error != FILE_ERROR_OK) {
    callback.Run(FileErrorToPlatformError(error), base::PlatformFileInfo());
    return;
  }

  DCHECK(entry);
  base::PlatformFileInfo file_info;
  ConvertResourceEntryToPlatformFileInfo(*entry, &file_info);
  callback.Run(base::PLATFORM_FILE_OK, file_info);
}

// Runs |callback| with arguments converted from |error| and |resource_entries|.
void RunReadDirectoryCallback(
    const ReadDirectoryCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntryVector> resource_entries) {
  if (error != FILE_ERROR_OK) {
    callback.Run(FileErrorToPlatformError(error),
                 std::vector<fileapi::DirectoryEntry>(), false);
    return;
  }

  DCHECK(resource_entries);

  std::vector<fileapi::DirectoryEntry> entries;
  // Convert drive files to File API's directory entry.
  entries.reserve(resource_entries->size());
  for (size_t i = 0; i < resource_entries->size(); ++i) {
    const ResourceEntry& resource_entry = (*resource_entries)[i];
    fileapi::DirectoryEntry entry;
    entry.name = resource_entry.base_name();

    const PlatformFileInfoProto& file_info = resource_entry.file_info();
    entry.is_directory = file_info.is_directory();
    entry.size = file_info.size();
    entry.last_modified_time =
        base::Time::FromInternalValue(file_info.last_modified());
    entries.push_back(entry);
  }

  callback.Run(base::PLATFORM_FILE_OK, entries, false);
}

// Runs |callback| with arguments based on |error|, |local_path| and |entry|.
void RunCreateSnapshotFileCallback(const CreateSnapshotFileCallback& callback,
                                   FileError error,
                                   const base::FilePath& local_path,
                                   scoped_ptr<ResourceEntry> entry) {
  if (error != FILE_ERROR_OK) {
    callback.Run(
        FileErrorToPlatformError(error),
        base::PlatformFileInfo(), base::FilePath(),
        webkit_blob::ScopedFile::ScopeOutPolicy());
    return;
  }

  DCHECK(entry);

  // When reading file, last modified time specified in file info will be
  // compared to the last modified time of the local version of the drive file.
  // Since those two values don't generally match (last modification time on the
  // drive server vs. last modification time of the local, downloaded file), so
  // we have to opt out from this check. We do this by unsetting last_modified
  // value in the file info passed to the CreateSnapshot caller.
  base::PlatformFileInfo file_info;
  ConvertResourceEntryToPlatformFileInfo(*entry, &file_info);
  file_info.last_modified = base::Time();

  // If the file is a hosted document, a temporary JSON file is created to
  // represent the document. The JSON file is not cached and its lifetime
  // is managed by ShareableFileReference.
  webkit_blob::ScopedFile::ScopeOutPolicy scope_out_policy =
      entry->file_specific_info().is_hosted_document() ?
      webkit_blob::ScopedFile::DELETE_ON_SCOPE_OUT :
      webkit_blob::ScopedFile::DONT_DELETE_ON_SCOPE_OUT;

  callback.Run(base::PLATFORM_FILE_OK, file_info, local_path, scope_out_policy);
}

// Runs |callback| with arguments converted from |error| and |local_path|.
void RunCreateWritableSnapshotFileCallback(
    const CreateWritableSnapshotFileCallback& callback,
    FileError error,
    const base::FilePath& local_path,
    const base::Closure& close_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback.Run(FileErrorToPlatformError(error), local_path, close_callback);
}

// Runs |callback| with |error| and |platform_file|.
void RunOpenFileCallback(const OpenFileCallback& callback,
                         const base::Closure& close_callback,
                         base::PlatformFileError* error,
                         base::PlatformFile platform_file) {
  callback.Run(*error, platform_file, close_callback);
}

// Part of OpenFile(). Called after FileSystem::OpenFile().
void OpenFileAfterFileSystemOpenFile(int file_flags,
                                     const OpenFileCallback& callback,
                                     FileError error,
                                     const base::FilePath& local_path,
                                     const base::Closure& close_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != FILE_ERROR_OK) {
    callback.Run(FileErrorToPlatformError(error),
                 base::kInvalidPlatformFileValue,
                 base::Closure());
    return;
  }

  // Here, the file should be at |local_path|, but there may be timing issue.
  // Because the file is managed by Drive file system, so, in order to avoid
  // unexpected file creation, CREATE, OPEN_ALWAYS and CREATE_ALWAYS are
  // translated into OPEN or OPEN_TRUNCATED, here. Keep OPEN and OPEN_TRUNCATED
  // as is.
  if (file_flags & (base::PLATFORM_FILE_CREATE |
                    base::PLATFORM_FILE_OPEN_ALWAYS)) {
    file_flags &= ~(base::PLATFORM_FILE_CREATE |
                    base::PLATFORM_FILE_OPEN_ALWAYS);
    file_flags |= base::PLATFORM_FILE_OPEN;
  } else if (file_flags & base::PLATFORM_FILE_CREATE_ALWAYS) {
    file_flags &= ~base::PLATFORM_FILE_CREATE_ALWAYS;
    file_flags |= base::PLATFORM_FILE_OPEN_TRUNCATED;
  }

  // Cache file prepared for modification is available. Open it locally.
  base::PlatformFileError* result =
      new base::PlatformFileError(base::PLATFORM_FILE_ERROR_FAILED);
  bool posted = base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(), FROM_HERE,
      base::Bind(&base::CreatePlatformFile,
                 local_path, file_flags, static_cast<bool*>(NULL), result),
      base::Bind(&RunOpenFileCallback,
                 callback, close_callback, base::Owned(result)));
  DCHECK(posted);
}

}  // namespace

void RunFileSystemCallback(
    const FileSystemGetter& file_system_getter,
    const base::Callback<void(FileSystemInterface*)>& callback,
    const base::Closure& on_error_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FileSystemInterface* file_system = file_system_getter.Run();

  if (!file_system) {
    if (!on_error_callback.is_null())
      on_error_callback.Run();
    return;
  }

  callback.Run(file_system);
}

void GetFileInfo(const base::FilePath& file_path,
                 const GetFileInfoCallback& callback,
                 FileSystemInterface* file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system->GetResourceEntryByPath(
      file_path,
      base::Bind(&RunGetFileInfoCallback, callback));
}

void Copy(const base::FilePath& src_file_path,
          const base::FilePath& dest_file_path,
          bool preserve_last_modified,
          const StatusCallback& callback,
          FileSystemInterface* file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system->Copy(src_file_path, dest_file_path, preserve_last_modified,
                    base::Bind(&RunStatusCallbackByFileError, callback));
}

void Move(const base::FilePath& src_file_path,
          const base::FilePath& dest_file_path,
          bool preserve_last_modified,
          const StatusCallback& callback,
          FileSystemInterface* file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system->Move(src_file_path, dest_file_path, preserve_last_modified,
                    base::Bind(&RunStatusCallbackByFileError, callback));
}

void CopyInForeignFile(const base::FilePath& src_foreign_file_path,
                       const base::FilePath& dest_file_path,
                       const StatusCallback& callback,
                       FileSystemInterface* file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system->TransferFileFromLocalToRemote(
      src_foreign_file_path, dest_file_path,
      base::Bind(&RunStatusCallbackByFileError, callback));
}

void ReadDirectory(const base::FilePath& file_path,
                   const ReadDirectoryCallback& callback,
                   FileSystemInterface* file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system->ReadDirectoryByPath(
      file_path,
      base::Bind(&RunReadDirectoryCallback, callback));
}

void Remove(const base::FilePath& file_path,
            bool is_recursive,
            const StatusCallback& callback,
            FileSystemInterface* file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system->Remove(file_path, is_recursive,
                      base::Bind(&RunStatusCallbackByFileError, callback));
}

void CreateDirectory(const base::FilePath& file_path,
                     bool is_exclusive,
                     bool is_recursive,
                     const StatusCallback& callback,
                     FileSystemInterface* file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system->CreateDirectory(
      file_path, is_exclusive, is_recursive,
      base::Bind(&RunStatusCallbackByFileError, callback));
}

void CreateFile(const base::FilePath& file_path,
                bool is_exclusive,
                const StatusCallback& callback,
                FileSystemInterface* file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system->CreateFile(file_path, is_exclusive,
                          std::string(),  // no mime type; guess from file_path
                          base::Bind(&RunStatusCallbackByFileError, callback));
}

void Truncate(const base::FilePath& file_path,
              int64 length,
              const StatusCallback& callback,
              FileSystemInterface* file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system->TruncateFile(
      file_path, length,
      base::Bind(&RunStatusCallbackByFileError, callback));
}

void CreateSnapshotFile(const base::FilePath& file_path,
                        const CreateSnapshotFileCallback& callback,
                        FileSystemInterface* file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system->GetFileByPath(
      file_path,
      base::Bind(&RunCreateSnapshotFileCallback, callback));
}

void CreateWritableSnapshotFile(
    const base::FilePath& file_path,
    const CreateWritableSnapshotFileCallback& callback,
    FileSystemInterface* file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system->OpenFile(
      file_path,
      OPEN_FILE,
      std::string(),  // no mime type; we never create a new file here.
      base::Bind(&RunCreateWritableSnapshotFileCallback, callback));
}

void OpenFile(const base::FilePath& file_path,
              int file_flags,
              const OpenFileCallback& callback,
              FileSystemInterface* file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Returns an error if any unsupported flag is found.
  if (file_flags & ~(base::PLATFORM_FILE_OPEN |
                     base::PLATFORM_FILE_CREATE |
                     base::PLATFORM_FILE_OPEN_ALWAYS |
                     base::PLATFORM_FILE_CREATE_ALWAYS |
                     base::PLATFORM_FILE_OPEN_TRUNCATED |
                     base::PLATFORM_FILE_READ |
                     base::PLATFORM_FILE_WRITE |
                     base::PLATFORM_FILE_WRITE_ATTRIBUTES |
                     base::PLATFORM_FILE_APPEND)) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   base::PLATFORM_FILE_ERROR_FAILED,
                   base::kInvalidPlatformFileValue,
                   base::Closure()));
    return;
  }

  file_system->OpenFile(
      file_path, GetOpenMode(file_flags),
      std::string(),  // no mime type; guess from file_path
      base::Bind(&OpenFileAfterFileSystemOpenFile, file_flags, callback));
}

void TouchFile(const base::FilePath& file_path,
               const base::Time& last_access_time,
               const base::Time& last_modified_time,
               const StatusCallback& callback,
               FileSystemInterface* file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system->TouchFile(file_path, last_access_time, last_modified_time,
                         base::Bind(&RunStatusCallbackByFileError, callback));

}

}  // namespace fileapi_internal
}  // namespace drive
