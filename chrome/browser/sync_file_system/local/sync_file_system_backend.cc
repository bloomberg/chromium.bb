// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"

#include "base/logging.h"
#include "chrome/browser/sync_file_system/local/local_file_change_tracker.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_context.h"
#include "chrome/browser/sync_file_system/local/syncable_file_system_operation.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_stream_reader.h"
#include "webkit/browser/fileapi/file_system_operation_impl.h"
#include "webkit/browser/fileapi/sandbox_file_stream_writer.h"
#include "webkit/browser/fileapi/sandbox_quota_observer.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace sync_file_system {

SyncFileSystemBackend::SyncFileSystemBackend()
    : delegate_(NULL) {
}

SyncFileSystemBackend::~SyncFileSystemBackend() {
  if (change_tracker_) {
    delegate_->file_task_runner()->DeleteSoon(
        FROM_HERE, change_tracker_.release());
  }
}

bool SyncFileSystemBackend::CanHandleType(
    fileapi::FileSystemType type) const {
  return type == fileapi::kFileSystemTypeSyncable ||
         type == fileapi::kFileSystemTypeSyncableForInternalSync;
}

void SyncFileSystemBackend::Initialize(fileapi::FileSystemContext* context) {
  DCHECK(context);
  DCHECK(!delegate_);
  delegate_ = context->sandbox_delegate();

  delegate_->AddFileUpdateObserver(
      fileapi::kFileSystemTypeSyncable,
      delegate_->quota_observer(),
      delegate_->file_task_runner());
  delegate_->AddFileUpdateObserver(
      fileapi::kFileSystemTypeSyncableForInternalSync,
      delegate_->quota_observer(),
      delegate_->file_task_runner());
}

void SyncFileSystemBackend::OpenFileSystem(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    fileapi::OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  DCHECK(CanHandleType(type));
  DCHECK(delegate_);
  delegate_->OpenFileSystem(origin_url, type, mode, callback,
                            GetSyncableFileSystemRootURI(origin_url));
}

fileapi::AsyncFileUtil* SyncFileSystemBackend::GetAsyncFileUtil(
    fileapi::FileSystemType type) {
  DCHECK(delegate_);
  return delegate_->file_util();
}

fileapi::CopyOrMoveFileValidatorFactory*
SyncFileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    fileapi::FileSystemType type,
    base::PlatformFileError* error_code) {
  DCHECK(error_code);
  *error_code = base::PLATFORM_FILE_OK;
  return NULL;
}

fileapi::FileSystemOperation*
SyncFileSystemBackend::CreateFileSystemOperation(
    const fileapi::FileSystemURL& url,
    fileapi::FileSystemContext* context,
    base::PlatformFileError* error_code) const {
  DCHECK(CanHandleType(url.type()));
  DCHECK(context);
  DCHECK(error_code);

  DCHECK(delegate_);
  scoped_ptr<fileapi::FileSystemOperationContext> operation_context =
      delegate_->CreateFileSystemOperationContext(url, context, error_code);
  if (!operation_context)
    return NULL;

  if (url.type() == fileapi::kFileSystemTypeSyncableForInternalSync) {
    return new fileapi::FileSystemOperationImpl(
        url, context, operation_context.Pass());
  }

  return new SyncableFileSystemOperation(
      url, context, operation_context.Pass());
}

scoped_ptr<webkit_blob::FileStreamReader>
SyncFileSystemBackend::CreateFileStreamReader(
    const fileapi::FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    fileapi::FileSystemContext* context) const {
  DCHECK(CanHandleType(url.type()));
  DCHECK(delegate_);
  return delegate_->CreateFileStreamReader(
      url, offset, expected_modification_time, context);
}

scoped_ptr<fileapi::FileStreamWriter>
SyncFileSystemBackend::CreateFileStreamWriter(
    const fileapi::FileSystemURL& url,
    int64 offset,
    fileapi::FileSystemContext* context) const {
  DCHECK(CanHandleType(url.type()));
  DCHECK(delegate_);
  return delegate_->CreateFileStreamWriter(
      url, offset, context, fileapi::kFileSystemTypeSyncableForInternalSync);
}

fileapi::FileSystemQuotaUtil* SyncFileSystemBackend::GetQuotaUtil() {
  return delegate_;
}

// static
SyncFileSystemBackend* SyncFileSystemBackend::GetBackend(
    const fileapi::FileSystemContext* file_system_context) {
  DCHECK(file_system_context);
  return static_cast<SyncFileSystemBackend*>(
      file_system_context->GetFileSystemBackend(
          fileapi::kFileSystemTypeSyncable));
}

void SyncFileSystemBackend::SetLocalFileChangeTracker(
    scoped_ptr<LocalFileChangeTracker> tracker) {
  DCHECK(!change_tracker_);
  DCHECK(tracker);
  change_tracker_ = tracker.Pass();

  DCHECK(delegate_);
  delegate_->AddFileUpdateObserver(
      fileapi::kFileSystemTypeSyncable,
      change_tracker_.get(),
      delegate_->file_task_runner());
  delegate_->AddFileChangeObserver(
      fileapi::kFileSystemTypeSyncable,
      change_tracker_.get(),
      delegate_->file_task_runner());
}

void SyncFileSystemBackend::set_sync_context(
    LocalFileSyncContext* sync_context) {
  DCHECK(!sync_context_);
  sync_context_ = sync_context;
}

}  // namespace sync_file_system
