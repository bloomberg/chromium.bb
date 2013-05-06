// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system_proxy.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/platform_file.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_system_service.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/google_apis/task_util.h"
#include "chrome/browser/google_apis/time_util.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/blob/file_stream_reader.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"

using base::MessageLoopProxy;
using content::BrowserThread;
using fileapi::FileSystemURL;
using fileapi::FileSystemOperation;
using webkit_blob::ShareableFileReference;

namespace drive {

namespace {

typedef fileapi::RemoteFileSystemProxyInterface::OpenFileCallback
    OpenFileCallback;

const char kFeedField[] = "feed";

// Helper function to run reply on results of base::CreatePlatformFile() on
// IO thread.
void OnPlatformFileOpened(
    const OpenFileCallback& callback,
    base::ProcessHandle peer_handle,
    base::PlatformFileError* open_error,
    base::PlatformFile platform_file) {
  callback.Run(*open_error, platform_file, peer_handle);
}

// Helper function to run OpenFileCallback from
// FileSystemProxy::OpenFile().
void OnGetFileByPathForOpen(
    const OpenFileCallback& callback,
    int file_flags,
    base::ProcessHandle peer_handle,
    FileError file_error,
    const base::FilePath& local_path,
    const std::string& unused_mime_type,
    DriveFileType file_type) {
  base::PlatformFileError error =
      FileErrorToPlatformError(file_error);
  if (error != base::PLATFORM_FILE_OK) {
    callback.Run(error, base::kInvalidPlatformFileValue, peer_handle);
    return;
  }

  base::PlatformFileError* open_error =
      new base::PlatformFileError(base::PLATFORM_FILE_ERROR_FAILED);
  base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&base::CreatePlatformFile,
                 local_path,
                 file_flags,
                 static_cast<bool*>(NULL),
                 open_error),
      base::Bind(&OnPlatformFileOpened,
                 callback,
                 peer_handle,
                 base::Owned(open_error)));
}

// Helper function to run SnapshotFileCallback from
// FileSystemProxy::CreateSnapshotFile().
void CallSnapshotFileCallback(
    const FileSystemOperation::SnapshotFileCallback& callback,
    const base::PlatformFileInfo& file_info,
    FileError file_error,
    const base::FilePath& local_path,
    const std::string& unused_mime_type,
    DriveFileType file_type) {
  scoped_refptr<ShareableFileReference> file_ref;
  base::PlatformFileError error =
      FileErrorToPlatformError(file_error);

  // If the file is a hosted document, a temporary JSON file is created to
  // represent the document. The JSON file is not cached and its lifetime
  // is managed by ShareableFileReference.
  if (error == base::PLATFORM_FILE_OK && file_type == HOSTED_DOCUMENT) {
    file_ref = ShareableFileReference::GetOrCreate(
        local_path, ShareableFileReference::DELETE_ON_FINAL_RELEASE,
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  }

  // When reading file, last modified time specified in file info will be
  // compared to the last modified time of the local version of the drive file.
  // Since those two values don't generally match (last modification time on the
  // drive server vs. last modification time of the local, downloaded file), so
  // we have to opt out from this check. We do this by unsetting last_modified
  // value in the file info passed to the CreateSnapshot caller.
  base::PlatformFileInfo final_file_info(file_info);
  final_file_info.last_modified = base::Time();

  callback.Run(error, final_file_info, local_path, file_ref);
}

// Emits debug log when FileSystem::CloseFile() is complete.
void EmitDebugLogForCloseFile(const base::FilePath& local_path,
                              FileError file_error) {
  DVLOG(1) << "Closed: " << local_path.AsUTF8Unsafe() << ": " << file_error;
}

base::PlatformFileError DoTruncateOnBlockingPool(
    const base::FilePath& local_cache_path,
    int64 length) {
  base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;

  base::PlatformFile file = base::CreatePlatformFile(
      local_cache_path,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
      NULL,
      &result);
  if (result == base::PLATFORM_FILE_OK) {
    DCHECK_NE(base::kInvalidPlatformFileValue, file);
    if (!base::TruncatePlatformFile(file, length))
      result = base::PLATFORM_FILE_ERROR_FAILED;
    base::ClosePlatformFile(file);
  }
  return result;
}

void DidCloseFileForTruncate(
    const FileSystemOperation::StatusCallback& callback,
    base::PlatformFileError truncate_result,
    FileError close_result) {
  // Reports the first error.
  callback.Run(truncate_result == base::PLATFORM_FILE_OK ?
               FileErrorToPlatformError(close_result) :
               truncate_result);
}

}  // namespace

base::FileUtilProxy::Entry ResourceEntryToFileUtilProxyEntry(
    const ResourceEntry& resource_entry) {
  base::PlatformFileInfo file_info;
  util::ConvertResourceEntryToPlatformFileInfo(
      resource_entry.file_info(), &file_info);

  base::FileUtilProxy::Entry entry;
  entry.name = resource_entry.base_name();
  entry.is_directory = file_info.is_directory;
  entry.size = file_info.size;
  entry.last_modified_time = file_info.last_modified;
  return entry;
}

// FileSystemProxy class implementation.

FileSystemProxy::FileSystemProxy(
    FileSystemInterface* file_system)
    : file_system_(file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FileSystemProxy::DetachFromFileSystem() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_ = NULL;
}

void FileSystemProxy::GetFileInfo(
    const FileSystemURL& file_url,
    const FileSystemOperation::GetMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   base::PLATFORM_FILE_ERROR_NOT_FOUND,
                   base::PlatformFileInfo(),
                   base::FilePath()));
    return;
  }

  CallFileSystemMethodOnUIThread(
      base::Bind(&FileSystemInterface::GetEntryInfoByPath,
                 base::Unretained(file_system_),
                 file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&FileSystemProxy::OnGetMetadata,
                                this,
                                file_path,
                                callback))));
}

void FileSystemProxy::Copy(
    const FileSystemURL& src_file_url,
    const FileSystemURL& dest_file_url,
    const FileSystemOperation::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath src_file_path, dest_file_path;
  if (!ValidateUrl(src_file_url, &src_file_path) ||
      !ValidateUrl(dest_file_url, &dest_file_path)) {
    MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  CallFileSystemMethodOnUIThread(
      base::Bind(&FileSystemInterface::Copy,
                 base::Unretained(file_system_),
                 src_file_path,
                 dest_file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&FileSystemProxy::OnStatusCallback,
                                this,
                                callback))));
}

void FileSystemProxy::Move(
    const FileSystemURL& src_file_url,
    const FileSystemURL& dest_file_url,
    const FileSystemOperation::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath src_file_path, dest_file_path;
  if (!ValidateUrl(src_file_url, &src_file_path) ||
      !ValidateUrl(dest_file_url, &dest_file_path)) {
    MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  CallFileSystemMethodOnUIThread(
      base::Bind(&FileSystemInterface::Move,
                 base::Unretained(file_system_),
                 src_file_path,
                 dest_file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&FileSystemProxy::OnStatusCallback,
                                this,
                                callback))));
}

void FileSystemProxy::ReadDirectory(
    const FileSystemURL& file_url,
    const FileSystemOperation::ReadDirectoryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   base::PLATFORM_FILE_ERROR_NOT_FOUND,
                   std::vector<base::FileUtilProxy::Entry>(),
                   false));
    return;
  }

  CallFileSystemMethodOnUIThread(
      base::Bind(&FileSystemInterface::ReadDirectoryByPath,
                 base::Unretained(file_system_),
                 file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&FileSystemProxy::OnReadDirectory,
                                this,
                                callback))));
}

void FileSystemProxy::Remove(
    const FileSystemURL& file_url,
    bool recursive,
    const FileSystemOperation::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  CallFileSystemMethodOnUIThread(
      base::Bind(&FileSystemInterface::Remove,
                 base::Unretained(file_system_),
                 file_path,
                 recursive,
                 google_apis::CreateRelayCallback(
                     base::Bind(&FileSystemProxy::OnStatusCallback,
                                this,
                                callback))));
}

void FileSystemProxy::CreateDirectory(
    const FileSystemURL& file_url,
    bool exclusive,
    bool recursive,
    const FileSystemOperation::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  CallFileSystemMethodOnUIThread(
      base::Bind(&FileSystemInterface::CreateDirectory,
                 base::Unretained(file_system_),
                 file_path,
                 exclusive,
                 recursive,
                 google_apis::CreateRelayCallback(
                     base::Bind(&FileSystemProxy::OnStatusCallback,
                                this,
                                callback))));
}

void FileSystemProxy::CreateFile(
    const FileSystemURL& file_url,
    bool exclusive,
    const FileSystemOperation::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  CallFileSystemMethodOnUIThread(
      base::Bind(&FileSystemInterface::CreateFile,
                 base::Unretained(file_system_),
                 file_path,
                 exclusive,
                 google_apis::CreateRelayCallback(
                     base::Bind(&FileSystemProxy::OnStatusCallback,
                                this,
                                callback))));
}

void FileSystemProxy::Truncate(
    const FileSystemURL& file_url,
    int64 length,
    const FileSystemOperation::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (length < 0) {
    MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, base::PLATFORM_FILE_ERROR_INVALID_OPERATION));
    return;
  }

  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  // TODO(kinaba): http://crbug.com/132780.
  // Optimize the cases for small |length|, at least for |length| == 0.
  // CreateWritableSnapshotFile downloads the whole content unnecessarily.
  CallFileSystemMethodOnUIThread(
      base::Bind(&FileSystemInterface::OpenFile,
                 base::Unretained(file_system_),
                 file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&FileSystemProxy::OnFileOpenedForTruncate,
                                this,
                                file_path,
                                length,
                                callback))));
}

void FileSystemProxy::OnOpenFileForWriting(
    int file_flags,
    base::ProcessHandle peer_handle,
    const OpenFileCallback& callback,
    FileError file_error,
    const base::FilePath& local_cache_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::PlatformFileError error =
      FileErrorToPlatformError(file_error);

  if (error != base::PLATFORM_FILE_OK) {
    callback.Run(error, base::kInvalidPlatformFileValue, peer_handle);
    return;
  }

  // Cache file prepared for modification is available. Truncate it.
  base::PlatformFileError* result =
      new base::PlatformFileError(base::PLATFORM_FILE_ERROR_FAILED);
  bool posted = base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(), FROM_HERE,
      base::Bind(&base::CreatePlatformFile,
                 local_cache_path,
                 file_flags,
                 static_cast<bool*>(NULL),
                 result),
      base::Bind(&OnPlatformFileOpened,
                 callback,
                 peer_handle,
                 base::Owned(result)));
  DCHECK(posted);
}

void FileSystemProxy::OnCreateFileForOpen(
    const base::FilePath& file_path,
    int file_flags,
    base::ProcessHandle peer_handle,
    const OpenFileCallback& callback,
    FileError file_error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::PlatformFileError create_result =
      FileErrorToPlatformError(file_error);

  if ((create_result == base::PLATFORM_FILE_OK) ||
      ((create_result == base::PLATFORM_FILE_ERROR_EXISTS) &&
       (file_flags & base::PLATFORM_FILE_CREATE_ALWAYS))) {
    // If we are trying to always create an existing file, then
    // if it really exists open it as truncated.
    file_flags &= ~base::PLATFORM_FILE_CREATE;
    file_flags &= ~base::PLATFORM_FILE_CREATE_ALWAYS;
    file_flags |= base::PLATFORM_FILE_OPEN_TRUNCATED;
  } else {
    callback.Run(create_result, base::kInvalidPlatformFileValue, peer_handle);
    return;
  }

  // Open created (or existing) file for writing.
  CallFileSystemMethodOnUIThread(
      base::Bind(&FileSystemInterface::OpenFile,
                 base::Unretained(file_system_),
                 file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&FileSystemProxy::OnOpenFileForWriting,
                                this,
                                file_flags,
                                peer_handle,
                                callback))));
}

void FileSystemProxy::OnFileOpenedForTruncate(
    const base::FilePath& virtual_path,
    int64 length,
    const fileapi::FileSystemOperation::StatusCallback& callback,
    FileError open_result,
    const base::FilePath& local_cache_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (open_result != FILE_ERROR_OK) {
    callback.Run(FileErrorToPlatformError(open_result));
    return;
  }

  // Cache file prepared for modification is available. Truncate it.
  bool posted = base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&DoTruncateOnBlockingPool,
                 local_cache_path,
                 length),
      base::Bind(&FileSystemProxy::DidTruncate,
                 this,
                 virtual_path,
                 callback));
  DCHECK(posted);
}

void FileSystemProxy::DidTruncate(
    const base::FilePath& virtual_path,
    const FileSystemOperation::StatusCallback& callback,
    base::PlatformFileError truncate_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Truncation finished. We must close the file no matter |truncate_result|
  // indicates an error or not.
  CallFileSystemMethodOnUIThread(
      base::Bind(&FileSystemInterface::CloseFile,
                 base::Unretained(file_system_),
                 virtual_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&DidCloseFileForTruncate,
                                callback,
                                truncate_result))));
}

void FileSystemProxy::OpenFile(
    const FileSystemURL& file_url,
    int file_flags,
    base::ProcessHandle peer_handle,
    const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   base::PLATFORM_FILE_ERROR_NOT_FOUND,
                   base::kInvalidPlatformFileValue,
                   peer_handle));
    return;
  }

  // TODO(zelidrag): Wire all other file open operations.
  if ((file_flags & base::PLATFORM_FILE_DELETE_ON_CLOSE)) {
    NOTIMPLEMENTED() << "File create/write operations not yet supported "
                     << file_path.value();
    MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   base::PLATFORM_FILE_ERROR_FAILED,
                   base::kInvalidPlatformFileValue,
                   peer_handle));
    return;
  }

  if ((file_flags & base::PLATFORM_FILE_OPEN) ||
      (file_flags & base::PLATFORM_FILE_OPEN_ALWAYS) ||
      (file_flags & base::PLATFORM_FILE_OPEN_TRUNCATED)) {
    if ((file_flags & base::PLATFORM_FILE_OPEN_TRUNCATED) ||
        (file_flags & base::PLATFORM_FILE_OPEN_ALWAYS) ||
        (file_flags & base::PLATFORM_FILE_WRITE) ||
        (file_flags & base::PLATFORM_FILE_EXCLUSIVE_WRITE)) {
      // Open existing file for writing.
      CallFileSystemMethodOnUIThread(
          base::Bind(&FileSystemInterface::OpenFile,
                     base::Unretained(file_system_),
                     file_path,
                     google_apis::CreateRelayCallback(
                         base::Bind(&FileSystemProxy::OnOpenFileForWriting,
                                    this,
                                    file_flags,
                                    peer_handle,
                                    callback))));
    } else {
      // Read-only file open.
      CallFileSystemMethodOnUIThread(
          base::Bind(&FileSystemInterface::GetFileByPath,
                     base::Unretained(file_system_),
                     file_path,
                     google_apis::CreateRelayCallback(
                         base::Bind(&OnGetFileByPathForOpen,
                                    callback,
                                    file_flags,
                                    peer_handle))));
    }
  } else if ((file_flags & base::PLATFORM_FILE_CREATE) ||
             (file_flags & base::PLATFORM_FILE_CREATE_ALWAYS)) {
    // Open existing file for writing.
    CallFileSystemMethodOnUIThread(
        base::Bind(&FileSystemInterface::CreateFile,
                   base::Unretained(file_system_),
                   file_path,
                   file_flags & base::PLATFORM_FILE_EXCLUSIVE_WRITE,
                   google_apis::CreateRelayCallback(
                       base::Bind(&FileSystemProxy::OnCreateFileForOpen,
                                  this,
                                  file_path,
                                  file_flags,
                                  peer_handle,
                                  callback))));
  } else {
    NOTREACHED() << "Unhandled file flags combination " << file_flags;
    MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   base::PLATFORM_FILE_ERROR_FAILED,
                   base::kInvalidPlatformFileValue,
                   peer_handle));
  }
}

void FileSystemProxy::NotifyCloseFile(const FileSystemURL& url) {
  base::FilePath file_path;
  if (!ValidateUrl(url, &file_path))
    return;

  CallFileSystemMethodOnUIThread(
      base::Bind(&FileSystemInterface::CloseFile,
                 base::Unretained(file_system_),
                 file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&EmitDebugLogForCloseFile,
                                file_path))));
}

void FileSystemProxy::TouchFile(
    const fileapi::FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const FileSystemOperation::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // TODO(kinaba,kochi): crbug.com/144369. Support this operations once we have
  // migrated to the new Drive API.
  MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, base::PLATFORM_FILE_ERROR_INVALID_OPERATION));
}

void FileSystemProxy::CreateSnapshotFile(
    const FileSystemURL& file_url,
    const FileSystemOperation::SnapshotFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   base::PLATFORM_FILE_ERROR_NOT_FOUND,
                   base::PlatformFileInfo(),
                   base::FilePath(),
                   scoped_refptr<ShareableFileReference>(NULL)));
    return;
  }

  CallFileSystemMethodOnUIThread(
      base::Bind(&FileSystemInterface::GetEntryInfoByPath,
                 base::Unretained(file_system_),
                 file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&FileSystemProxy::OnGetEntryInfoByPath,
                                this,
                                file_path,
                                callback))));
}

void FileSystemProxy::OnGetEntryInfoByPath(
    const base::FilePath& entry_path,
    const FileSystemOperation::SnapshotFileCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (error != FILE_ERROR_OK || !entry.get()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND,
                 base::PlatformFileInfo(),
                 base::FilePath(),
                 scoped_refptr<ShareableFileReference>(NULL));
    return;
  }

  base::PlatformFileInfo file_info;
  util::ConvertResourceEntryToPlatformFileInfo(entry->file_info(), &file_info);

  CallFileSystemMethodOnUIThread(
      base::Bind(&FileSystemInterface::GetFileByPath,
                 base::Unretained(file_system_),
                 entry_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&CallSnapshotFileCallback,
                                callback,
                                file_info))));
}

void FileSystemProxy::CreateWritableSnapshotFile(
    const FileSystemURL& file_url,
    const fileapi::WritableSnapshotFile& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   base::PLATFORM_FILE_ERROR_NOT_FOUND,
                   base::FilePath(),
                   scoped_refptr<ShareableFileReference>(NULL)));
    return;
  }

  CallFileSystemMethodOnUIThread(
      base::Bind(&FileSystemInterface::OpenFile,
                 base::Unretained(file_system_),
                 file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(
                         &FileSystemProxy::OnCreateWritableSnapshotFile,
                         this,
                         file_path,
                         callback))));
}

scoped_ptr<webkit_blob::FileStreamReader>
FileSystemProxy::CreateFileStreamReader(
    base::SequencedTaskRunner* file_task_runner,
    const fileapi::FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time) {
  // TODO(hidehiko): Implement the FileStreamReader for drive files.
  // crbug.com/127129
  NOTIMPLEMENTED();
  return scoped_ptr<webkit_blob::FileStreamReader>();
}

FileSystemProxy::~FileSystemProxy() {
  // Should be deleted from the CrosMountPointProvider on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

// static.
bool FileSystemProxy::ValidateUrl(
    const FileSystemURL& url, base::FilePath* file_path) {
  *file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  return !file_path->empty();
}

void FileSystemProxy::CallFileSystemMethodOnUIThread(
    const base::Closure& method_call) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &FileSystemProxy::CallFileSystemMethodOnUIThreadInternal,
          this,
          method_call));
}

void FileSystemProxy::CallFileSystemMethodOnUIThreadInternal(
    const base::Closure& method_call) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // If |file_system_| is NULL, it means the file system has already shut down.
  if (file_system_)
    method_call.Run();
}

void FileSystemProxy::OnStatusCallback(
    const fileapi::FileSystemOperation::StatusCallback& callback,
    FileError error) {
  callback.Run(FileErrorToPlatformError(error));
}

void FileSystemProxy::OnGetMetadata(
    const base::FilePath& file_path,
    const FileSystemOperation::GetMetadataCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (error != FILE_ERROR_OK) {
    callback.Run(FileErrorToPlatformError(error),
                 base::PlatformFileInfo(),
                 base::FilePath());
    return;
  }
  DCHECK(entry.get());

  base::PlatformFileInfo file_info;
  util::ConvertResourceEntryToPlatformFileInfo(entry->file_info(), &file_info);

  callback.Run(base::PLATFORM_FILE_OK, file_info, file_path);
}

void FileSystemProxy::OnReadDirectory(
    const FileSystemOperation::ReadDirectoryCallback&
    callback,
    FileError error,
    bool hide_hosted_documents,
    scoped_ptr<ResourceEntryVector> resource_entries) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (error != FILE_ERROR_OK) {
    callback.Run(FileErrorToPlatformError(error),
                 std::vector<base::FileUtilProxy::Entry>(),
                 false);
    return;
  }
  DCHECK(resource_entries.get());

  std::vector<base::FileUtilProxy::Entry> entries;
  // Convert Drive files to something File API stack can understand.
  for (size_t i = 0; i < resource_entries->size(); ++i) {
    const ResourceEntry& resource_entry = (*resource_entries)[i];
    if (resource_entry.has_file_specific_info() &&
        resource_entry.file_specific_info().is_hosted_document() &&
        hide_hosted_documents) {
      continue;
    }
    entries.push_back(ResourceEntryToFileUtilProxyEntry(resource_entry));
  }

  callback.Run(base::PLATFORM_FILE_OK, entries, false);
}

void FileSystemProxy::OnCreateWritableSnapshotFile(
    const base::FilePath& virtual_path,
    const fileapi::WritableSnapshotFile& callback,
    FileError result,
    const base::FilePath& local_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_refptr<ShareableFileReference> file_ref;

  if (result == FILE_ERROR_OK) {
    file_ref = ShareableFileReference::GetOrCreate(
        local_path,
        ShareableFileReference::DONT_DELETE_ON_FINAL_RELEASE,
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
    file_ref->AddFinalReleaseCallback(
        base::Bind(&FileSystemProxy::CloseWritableSnapshotFile,
                   this,
                   virtual_path));
  }

  callback.Run(FileErrorToPlatformError(result), local_path, file_ref);
}

void FileSystemProxy::CloseWritableSnapshotFile(
    const base::FilePath& virtual_path,
    const base::FilePath& local_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  CallFileSystemMethodOnUIThread(
      base::Bind(&FileSystemInterface::CloseFile,
                 base::Unretained(file_system_),
                 virtual_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&EmitDebugLogForCloseFile,
                                virtual_path))));
}

}  // namespace drive
