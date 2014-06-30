// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/syncable_file_system_operation.h"

#include "base/logging.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_context.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/sync_file_system/local/syncable_file_operation_runner.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "net/url_request/url_request.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/file_writer_delegate.h"
#include "webkit/common/blob/shareable_file_reference.h"

using fileapi::FileSystemURL;

namespace sync_file_system {

namespace {

void WriteCallbackAdapter(
    const SyncableFileSystemOperation::WriteCallback& callback,
    base::File::Error status) {
  callback.Run(status, 0, true);
}

}  // namespace

class SyncableFileSystemOperation::QueueableTask
    : public SyncableFileOperationRunner::Task {
 public:
  QueueableTask(base::WeakPtr<SyncableFileSystemOperation> operation,
                const base::Closure& task)
      : operation_(operation),
        task_(task),
        target_paths_(operation->target_paths_) {}

  virtual ~QueueableTask() {
    DCHECK(!operation_);
  }

  virtual void Run() OVERRIDE {
    if (!operation_)
      return;
    DCHECK(!task_.is_null());
    task_.Run();
    operation_.reset();
  }

  virtual void Cancel() OVERRIDE {
    DCHECK(!task_.is_null());
    if (operation_)
      operation_->OnCancelled();
    task_.Reset();
    operation_.reset();
  }

  virtual const std::vector<FileSystemURL>& target_paths() const OVERRIDE {
    return target_paths_;
  }

 private:
  base::WeakPtr<SyncableFileSystemOperation> operation_;
  base::Closure task_;
  std::vector<FileSystemURL> target_paths_;
  DISALLOW_COPY_AND_ASSIGN(QueueableTask);
};

SyncableFileSystemOperation::~SyncableFileSystemOperation() {}

void SyncableFileSystemOperation::CreateFile(
    const FileSystemURL& url,
    bool exclusive,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_.get()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(url);
  completion_callback_ = callback;
  scoped_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      weak_factory_.GetWeakPtr(),
      base::Bind(&FileSystemOperation::CreateFile,
                 base::Unretained(impl_.get()),
                 url, exclusive,
                 base::Bind(&self::DidFinish, weak_factory_.GetWeakPtr()))));
  operation_runner_->PostOperationTask(task.Pass());
}

void SyncableFileSystemOperation::CreateDirectory(
    const FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_.get()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  if (!is_directory_operation_enabled_) {
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(url);
  completion_callback_ = callback;
  scoped_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      weak_factory_.GetWeakPtr(),
      base::Bind(&FileSystemOperation::CreateDirectory,
                 base::Unretained(impl_.get()),
                 url, exclusive, recursive,
                 base::Bind(&self::DidFinish, weak_factory_.GetWeakPtr()))));
  operation_runner_->PostOperationTask(task.Pass());
}

void SyncableFileSystemOperation::Copy(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyProgressCallback& progress_callback,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_.get()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(dest_url);
  completion_callback_ = callback;
  scoped_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      weak_factory_.GetWeakPtr(),
      base::Bind(&FileSystemOperation::Copy,
                 base::Unretained(impl_.get()),
                 src_url, dest_url, option, progress_callback,
                 base::Bind(&self::DidFinish, weak_factory_.GetWeakPtr()))));
  operation_runner_->PostOperationTask(task.Pass());
}

void SyncableFileSystemOperation::Move(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_.get()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(src_url);
  target_paths_.push_back(dest_url);
  completion_callback_ = callback;
  scoped_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      weak_factory_.GetWeakPtr(),
      base::Bind(&FileSystemOperation::Move,
                 base::Unretained(impl_.get()),
                 src_url, dest_url, option,
                 base::Bind(&self::DidFinish, weak_factory_.GetWeakPtr()))));
  operation_runner_->PostOperationTask(task.Pass());
}

void SyncableFileSystemOperation::DirectoryExists(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  impl_->DirectoryExists(url, callback);
}

void SyncableFileSystemOperation::FileExists(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  impl_->FileExists(url, callback);
}

void SyncableFileSystemOperation::GetMetadata(
    const FileSystemURL& url,
    const GetMetadataCallback& callback) {
  DCHECK(CalledOnValidThread());
  impl_->GetMetadata(url, callback);
}

void SyncableFileSystemOperation::ReadDirectory(
    const FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  DCHECK(CalledOnValidThread());
  // This is a read operation and there'd be no hard to let it go even if
  // directory operation is disabled. (And we should allow this if it's made
  // on the root directory)
  impl_->ReadDirectory(url, callback);
}

void SyncableFileSystemOperation::Remove(
    const FileSystemURL& url, bool recursive,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_.get()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(url);
  completion_callback_ = callback;
  scoped_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      weak_factory_.GetWeakPtr(),
      base::Bind(&FileSystemOperation::Remove,
                 base::Unretained(impl_.get()),
                 url, recursive,
                 base::Bind(&self::DidFinish, weak_factory_.GetWeakPtr()))));
  operation_runner_->PostOperationTask(task.Pass());
}

void SyncableFileSystemOperation::Write(
    const FileSystemURL& url,
    scoped_ptr<fileapi::FileWriterDelegate> writer_delegate,
    scoped_ptr<net::URLRequest> blob_request,
    const WriteCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_.get()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND, 0, true);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(url);
  completion_callback_ = base::Bind(&WriteCallbackAdapter, callback);
  scoped_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      weak_factory_.GetWeakPtr(),
      base::Bind(&FileSystemOperation::Write,
                 base::Unretained(impl_.get()),
                 url,
                 base::Passed(&writer_delegate),
                 base::Passed(&blob_request),
                 base::Bind(&self::DidWrite, weak_factory_.GetWeakPtr(),
                            callback))));
  operation_runner_->PostOperationTask(task.Pass());
}

void SyncableFileSystemOperation::Truncate(
    const FileSystemURL& url, int64 length,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_.get()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(url);
  completion_callback_ = callback;
  scoped_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      weak_factory_.GetWeakPtr(),
      base::Bind(&FileSystemOperation::Truncate,
                 base::Unretained(impl_.get()),
                 url, length,
                 base::Bind(&self::DidFinish, weak_factory_.GetWeakPtr()))));
  operation_runner_->PostOperationTask(task.Pass());
}

void SyncableFileSystemOperation::TouchFile(
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  impl_->TouchFile(url, last_access_time, last_modified_time, callback);
}

void SyncableFileSystemOperation::OpenFile(
    const FileSystemURL& url,
    int file_flags,
    const OpenFileCallback& callback) {
  NOTREACHED();
}

void SyncableFileSystemOperation::Cancel(
    const StatusCallback& cancel_callback) {
  DCHECK(CalledOnValidThread());
  impl_->Cancel(cancel_callback);
}

void SyncableFileSystemOperation::CreateSnapshotFile(
    const FileSystemURL& path,
    const SnapshotFileCallback& callback) {
  DCHECK(CalledOnValidThread());
  impl_->CreateSnapshotFile(path, callback);
}

void SyncableFileSystemOperation::CopyInForeignFile(
    const base::FilePath& src_local_disk_path,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_.get()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(dest_url);
  completion_callback_ = callback;
  scoped_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      weak_factory_.GetWeakPtr(),
      base::Bind(&FileSystemOperation::CopyInForeignFile,
                 base::Unretained(impl_.get()),
                 src_local_disk_path, dest_url,
                 base::Bind(&self::DidFinish, weak_factory_.GetWeakPtr()))));
  operation_runner_->PostOperationTask(task.Pass());
}

void SyncableFileSystemOperation::RemoveFile(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  impl_->RemoveFile(url, callback);
}

void SyncableFileSystemOperation::RemoveDirectory(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  impl_->RemoveDirectory(url, callback);
}

void SyncableFileSystemOperation::CopyFileLocal(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyFileProgressCallback& progress_callback,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  impl_->CopyFileLocal(src_url, dest_url, option, progress_callback, callback);
}

void SyncableFileSystemOperation::MoveFileLocal(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  impl_->MoveFileLocal(src_url, dest_url, option, callback);
}

base::File::Error SyncableFileSystemOperation::SyncGetPlatformPath(
    const FileSystemURL& url,
    base::FilePath* platform_path) {
  return impl_->SyncGetPlatformPath(url, platform_path);
}

SyncableFileSystemOperation::SyncableFileSystemOperation(
    const FileSystemURL& url,
    fileapi::FileSystemContext* file_system_context,
    scoped_ptr<fileapi::FileSystemOperationContext> operation_context)
    : url_(url),
      weak_factory_(this) {
  DCHECK(file_system_context);
  SyncFileSystemBackend* backend =
      SyncFileSystemBackend::GetBackend(file_system_context);
  DCHECK(backend);
  if (!backend->sync_context()) {
    // Syncable FileSystem is opened in a file system context which doesn't
    // support (or is not initialized for) the API.
    // Returning here to leave operation_runner_ as NULL.
    return;
  }
  impl_.reset(fileapi::FileSystemOperation::Create(
      url_, file_system_context, operation_context.Pass()));
  operation_runner_ = backend->sync_context()->operation_runner();
  is_directory_operation_enabled_ = IsV2EnabledForOrigin(url.origin());
}

void SyncableFileSystemOperation::DidFinish(base::File::Error status) {
  DCHECK(CalledOnValidThread());
  DCHECK(!completion_callback_.is_null());
  if (operation_runner_.get())
    operation_runner_->OnOperationCompleted(target_paths_);
  completion_callback_.Run(status);
}

void SyncableFileSystemOperation::DidWrite(
    const WriteCallback& callback,
    base::File::Error result,
    int64 bytes,
    bool complete) {
  DCHECK(CalledOnValidThread());
  if (!complete) {
    callback.Run(result, bytes, complete);
    return;
  }
  if (operation_runner_.get())
    operation_runner_->OnOperationCompleted(target_paths_);
  callback.Run(result, bytes, complete);
}

void SyncableFileSystemOperation::OnCancelled() {
  DCHECK(!completion_callback_.is_null());
  completion_callback_.Run(base::File::FILE_ERROR_ABORT);
}

}  // namespace sync_file_system
