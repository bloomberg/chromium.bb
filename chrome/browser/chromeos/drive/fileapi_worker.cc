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
#include "content/public/browser/browser_thread.h"
#include "webkit/common/fileapi/directory_entry.h"

using content::BrowserThread;

namespace drive {
namespace internal {
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
void RunStatusCallbackByFileError(
    const FileApiWorker::StatusCallback& callback,
    FileError error) {
  callback.Run(FileErrorToPlatformError(error));
}

// Runs |callback| with arguments converted from |error| and |entry|.
void RunGetFileInfoCallback(const FileApiWorker::GetFileInfoCallback& callback,
                            FileError error,
                            scoped_ptr<ResourceEntry> entry) {
  if (error != FILE_ERROR_OK) {
    callback.Run(FileErrorToPlatformError(error), base::PlatformFileInfo());
    return;
  }

  DCHECK(entry);
  base::PlatformFileInfo file_info;
  util::ConvertResourceEntryToPlatformFileInfo(entry->file_info(), &file_info);
  callback.Run(base::PLATFORM_FILE_OK, file_info);
}

// Runs |callback| with arguments converted from |error| and |resource_entries|.
void RunReadDirectoryCallback(
    const FileApiWorker::ReadDirectoryCallback& callback,
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
void RunCreateSnapshotFileCallback(
    const FileApiWorker::CreateSnapshotFileCallback& callback,
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
  util::ConvertResourceEntryToPlatformFileInfo(entry->file_info(), &file_info);
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

// Runs |callback| with |error| and |platform_file|.
void RunOpenFileCallback(
    const FileApiWorker::OpenFileCallback& callback,
    base::PlatformFileError* error,
    base::PlatformFile platform_file) {
  callback.Run(*error, platform_file);
}

// Part of FileApiWorker::OpenFile(). Called after FileSystem::OpenFile().
void OpenFileAfterFileSystemOpenFile(
    int file_flags,
    const FileApiWorker::OpenFileCallback& callback,
    FileError error,
    const base::FilePath& local_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != FILE_ERROR_OK) {
    callback.Run(FileErrorToPlatformError(error),
                 base::kInvalidPlatformFileValue);
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
      base::Bind(&RunOpenFileCallback, callback, base::Owned(result)));
  DCHECK(posted);
}

// Emits debug log when FileSystem::CloseFile() is complete.
void EmitDebugLogForCloseFile(const base::FilePath& local_path,
                              FileError file_error) {
  DVLOG(1) << "Closed: " << local_path.AsUTF8Unsafe() << ": " << file_error;
}

}  // namespace

FileApiWorker::FileApiWorker(FileSystemInterface* file_system)
    : file_system_(file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(file_system);
}

FileApiWorker::~FileApiWorker() {
}

void FileApiWorker::GetFileInfo(const base::FilePath& file_path,
                                const GetFileInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->GetResourceEntryByPath(
      file_path,
      base::Bind(&RunGetFileInfoCallback, callback));
}

void FileApiWorker::Copy(const base::FilePath& src_file_path,
                         const base::FilePath& dest_file_path,
                         const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->Copy(src_file_path, dest_file_path,
                     base::Bind(&RunStatusCallbackByFileError, callback));
}

void FileApiWorker::Move(const base::FilePath& src_file_path,
                         const base::FilePath& dest_file_path,
                         const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->Move(src_file_path, dest_file_path,
                     base::Bind(&RunStatusCallbackByFileError, callback));
}

void FileApiWorker::ReadDirectory(const base::FilePath& file_path,
                                  const ReadDirectoryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->ReadDirectoryByPath(
      file_path,
      base::Bind(&RunReadDirectoryCallback, callback));
}

void FileApiWorker::Remove(const base::FilePath& file_path,
                           bool is_recursive,
                           const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->Remove(file_path, is_recursive,
                       base::Bind(&RunStatusCallbackByFileError, callback));
}

void FileApiWorker::CreateDirectory(const base::FilePath& file_path,
                                    bool is_exclusive,
                                    bool is_recursive,
                                    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->CreateDirectory(
      file_path, is_exclusive, is_recursive,
      base::Bind(&RunStatusCallbackByFileError, callback));
}

void FileApiWorker::CreateFile(const base::FilePath& file_path,
                               bool is_exclusive,
                               const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->CreateFile(file_path, is_exclusive,
                           base::Bind(&RunStatusCallbackByFileError, callback));
}

void FileApiWorker::Truncate(const base::FilePath& file_path,
                             int64 length,
                             const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->TruncateFile(
      file_path, length,
      base::Bind(&RunStatusCallbackByFileError, callback));
}

void FileApiWorker::CreateSnapshotFile(
    const base::FilePath& file_path,
    const CreateSnapshotFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->GetFileByPath(
      file_path,
      base::Bind(&RunCreateSnapshotFileCallback, callback));
}

void FileApiWorker::OpenFile(const base::FilePath& file_path,
                             int file_flags,
                             const OpenFileCallback& callback) {
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
                   base::kInvalidPlatformFileValue));
    return;
  }

  file_system_->OpenFile(
      file_path, GetOpenMode(file_flags),
      base::Bind(&OpenFileAfterFileSystemOpenFile, file_flags, callback));
}

void FileApiWorker::CloseFile(const base::FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->CloseFile(file_path,
                          base::Bind(&EmitDebugLogForCloseFile, file_path));
}

void FileApiWorker::TouchFile(const base::FilePath& file_path,
                              const base::Time& last_access_time,
                              const base::Time& last_modified_time,
                              const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->TouchFile(file_path, last_access_time, last_modified_time,
                          base::Bind(&RunStatusCallbackByFileError, callback));

}

}  // namespace internal
}  // namespace drive
