// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system_proxy.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/platform_file.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/fileapi_worker.h"
#include "chrome/browser/chromeos/drive/webkit_file_stream_reader_impl.h"
#include "chrome/browser/google_apis/task_util.h"
#include "chrome/browser/google_apis/time_util.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/blob/shareable_file_reference.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"

using base::MessageLoopProxy;
using content::BrowserThread;
using fileapi::DirectoryEntry;
using fileapi::FileSystemURL;
using fileapi::FileSystemOperation;
using webkit_blob::ShareableFileReference;

namespace drive {

namespace {

typedef fileapi::RemoteFileSystemProxyInterface::OpenFileCallback
    OpenFileCallback;

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
    scoped_ptr<ResourceEntry> entry) {
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

// Runs |callback| with the arguments based on the given arguments.
void RunSnapshotFileCallback(
    const fileapi::FileSystemOperation::SnapshotFileCallback& callback,
    base::PlatformFileError error,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& local_path,
    webkit_blob::ScopedFile::ScopeOutPolicy scope_out_policy) {
  // ShareableFileReference is thread *unsafe* class. So it is necessary to
  // create the instance (by invoking GetOrCreate) on IO thread, though
  // most drive file system related operations run on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_refptr<webkit_blob::ShareableFileReference> file_reference =
      webkit_blob::ShareableFileReference::GetOrCreate(
          webkit_blob::ScopedFile(
              local_path, scope_out_policy,
              BrowserThread::GetMessageLoopProxyForThread(
                  BrowserThread::FILE)));
  callback.Run(error, file_info, local_path, file_reference);
}

// Emits debug log when FileSystem::CloseFile() is complete.
void EmitDebugLogForCloseFile(const base::FilePath& local_path,
                              FileError file_error) {
  DVLOG(1) << "Closed: " << local_path.AsUTF8Unsafe() << ": " << file_error;
}

}  // namespace

FileSystemProxy::FileSystemProxy(
    FileSystemInterface* file_system)
    : file_system_(file_system),
      worker_(new internal::FileApiWorker(file_system)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FileSystemProxy::DetachFromFileSystem() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_ = NULL;
  worker_.reset();
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
                   base::PlatformFileInfo()));
    return;
  }

  CallFileSystemMethodOnUIThread(
      base::Bind(&internal::FileApiWorker::GetFileInfo,
                 base::Unretained(worker_.get()),
                 file_path, google_apis::CreateRelayCallback(callback)));
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
      base::Bind(&internal::FileApiWorker::Copy,
                 base::Unretained(worker_.get()),
                 src_file_path, dest_file_path,
                 google_apis::CreateRelayCallback(callback)));
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
      base::Bind(&internal::FileApiWorker::Move,
                 base::Unretained(worker_.get()),
                 src_file_path, dest_file_path,
                 google_apis::CreateRelayCallback(callback)));
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
                   std::vector<DirectoryEntry>(),
                   false));
    return;
  }

  CallFileSystemMethodOnUIThread(
      base::Bind(&internal::FileApiWorker::ReadDirectory,
                 base::Unretained(worker_.get()),
                 file_path, google_apis::CreateRelayCallback(callback)));
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
      base::Bind(&internal::FileApiWorker::Remove,
                 base::Unretained(worker_.get()),
                 file_path, recursive,
                 google_apis::CreateRelayCallback(callback)));
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
      base::Bind(&internal::FileApiWorker::CreateDirectory,
                 base::Unretained(worker_.get()),
                 file_path, exclusive, recursive,
                 google_apis::CreateRelayCallback(callback)));
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
      base::Bind(&internal::FileApiWorker::CreateFile,
                 base::Unretained(worker_.get()),
                 file_path, exclusive,
                 google_apis::CreateRelayCallback(callback)));
}

void FileSystemProxy::Truncate(
    const FileSystemURL& file_url,
    int64 length,
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
      base::Bind(&internal::FileApiWorker::Truncate,
                 base::Unretained(worker_.get()),
                 file_path, length,
                 google_apis::CreateRelayCallback(callback)));
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

  base::FilePath file_path;
  if (!ValidateUrl(url, &file_path))
    return;

  CallFileSystemMethodOnUIThread(
      base::Bind(&internal::FileApiWorker::TouchFile,
                 base::Unretained(worker_.get()),
                 file_path, last_access_time, last_modified_time,
                 google_apis::CreateRelayCallback(callback)));
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
                   scoped_refptr<ShareableFileReference>()));
    return;
  }

  CallFileSystemMethodOnUIThread(
      base::Bind(&internal::FileApiWorker::CreateSnapshotFile,
                 base::Unretained(worker_.get()), file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&RunSnapshotFileCallback, callback))));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath drive_file_path;
  if (!ValidateUrl(url, &drive_file_path))
    return scoped_ptr<webkit_blob::FileStreamReader>();

  return scoped_ptr<webkit_blob::FileStreamReader>(
      new internal::WebkitFileStreamReaderImpl(
          base::Bind(&FileSystemProxy::GetFileSystemOnUIThread, this),
          file_task_runner,
          drive_file_path,
          offset,
          expected_modification_time));
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

FileSystemInterface* FileSystemProxy::GetFileSystemOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return file_system_;
}

}  // namespace drive
