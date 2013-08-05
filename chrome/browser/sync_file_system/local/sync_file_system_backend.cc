// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"

#include "base/logging.h"
#include "chrome/browser/sync_file_system/local/local_file_change_tracker.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_context.h"
#include "chrome/browser/sync_file_system/local/syncable_file_system_operation.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "webkit/browser/fileapi/async_file_util_adapter.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_stream_reader.h"
#include "webkit/browser/fileapi/file_system_operation_impl.h"
#include "webkit/browser/fileapi/obfuscated_file_util.h"
#include "webkit/browser/fileapi/sandbox_file_stream_writer.h"
#include "webkit/browser/fileapi/sandbox_quota_observer.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace sync_file_system {

SyncFileSystemBackend::SyncFileSystemBackend()
    : sandbox_context_(NULL) {
}

SyncFileSystemBackend::~SyncFileSystemBackend() {
  if (change_tracker_) {
    sandbox_context_->file_task_runner()->DeleteSoon(
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
  DCHECK(!sandbox_context_);
  sandbox_context_ = context->sandbox_context();
  update_observers_ = update_observers_.AddObserver(
      sandbox_context_->quota_observer(),
      sandbox_context_->file_task_runner());
  syncable_update_observers_ = syncable_update_observers_.AddObserver(
      sandbox_context_->quota_observer(),
      sandbox_context_->file_task_runner());
}

void SyncFileSystemBackend::OpenFileSystem(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    fileapi::OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  DCHECK(CanHandleType(type));
  DCHECK(sandbox_context_);
  sandbox_context_->OpenFileSystem(
      origin_url, type, mode, callback,
      GetSyncableFileSystemRootURI(origin_url));
}

fileapi::FileSystemFileUtil* SyncFileSystemBackend::GetFileUtil(
    fileapi::FileSystemType type) {
  DCHECK(sandbox_context_);
  return sandbox_context_->sync_file_util();
}

fileapi::AsyncFileUtil* SyncFileSystemBackend::GetAsyncFileUtil(
    fileapi::FileSystemType type) {
  DCHECK(sandbox_context_);
  return sandbox_context_->file_util();
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
  if (!sandbox_context_->IsAccessValid(url)) {
    *error_code = base::PLATFORM_FILE_ERROR_SECURITY;
    return NULL;
  }

  scoped_ptr<fileapi::FileSystemOperationContext> operation_context(
      new fileapi::FileSystemOperationContext(context));

  if (url.type() == fileapi::kFileSystemTypeSyncableForInternalSync) {
    operation_context->set_update_observers(update_observers_);
    operation_context->set_change_observers(change_observers_);
    return new fileapi::FileSystemOperationImpl(
        url, context, operation_context.Pass());
  }

  operation_context->set_update_observers(syncable_update_observers_);
  operation_context->set_change_observers(syncable_change_observers_);
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
  if (!sandbox_context_->IsAccessValid(url))
    return scoped_ptr<webkit_blob::FileStreamReader>();
  return scoped_ptr<webkit_blob::FileStreamReader>(
      new fileapi::FileSystemFileStreamReader(
          context, url, offset, expected_modification_time));
}

scoped_ptr<fileapi::FileStreamWriter>
SyncFileSystemBackend::CreateFileStreamWriter(
    const fileapi::FileSystemURL& url,
    int64 offset,
    fileapi::FileSystemContext* context) const {
  DCHECK(CanHandleType(url.type()));
  if (!sandbox_context_->IsAccessValid(url))
    return scoped_ptr<fileapi::FileStreamWriter>();
  return scoped_ptr<fileapi::FileStreamWriter>(
      new fileapi::SandboxFileStreamWriter(
          context, url, offset, update_observers_));
}

fileapi::FileSystemQuotaUtil* SyncFileSystemBackend::GetQuotaUtil() {
  return this;
}

base::PlatformFileError SyncFileSystemBackend::DeleteOriginDataOnFileThread(
    fileapi::FileSystemContext* context,
    quota::QuotaManagerProxy* proxy,
    const GURL& origin_url,
    fileapi::FileSystemType type) {
  DCHECK(CanHandleType(type));
  DCHECK(sandbox_context_);
  return sandbox_context_->DeleteOriginDataOnFileThread(
      context, proxy, origin_url, type);
}

void SyncFileSystemBackend::GetOriginsForTypeOnFileThread(
    fileapi::FileSystemType type,
    std::set<GURL>* origins) {
  DCHECK(CanHandleType(type));
  DCHECK(sandbox_context_);
  sandbox_context_->GetOriginsForTypeOnFileThread(type, origins);
}

void SyncFileSystemBackend::GetOriginsForHostOnFileThread(
    fileapi::FileSystemType type,
    const std::string& host,
    std::set<GURL>* origins) {
  DCHECK(CanHandleType(type));
  DCHECK(sandbox_context_);
  sandbox_context_->GetOriginsForHostOnFileThread(type, host, origins);
}

int64 SyncFileSystemBackend::GetOriginUsageOnFileThread(
    fileapi::FileSystemContext* context,
    const GURL& origin_url,
    fileapi::FileSystemType type) {
  DCHECK(CanHandleType(type));
  DCHECK(sandbox_context_);
  return sandbox_context_->GetOriginUsageOnFileThread(
      context, origin_url, type);
}

void SyncFileSystemBackend::InvalidateUsageCache(
    const GURL& origin_url,
    fileapi::FileSystemType type) {
  DCHECK(CanHandleType(type));
  DCHECK(sandbox_context_);
  sandbox_context_->InvalidateUsageCache(origin_url, type);
}

void SyncFileSystemBackend::StickyInvalidateUsageCache(
    const GURL& origin_url,
    fileapi::FileSystemType type) {
  DCHECK(CanHandleType(type));
  DCHECK(sandbox_context_);
  sandbox_context_->StickyInvalidateUsageCache(origin_url, type);
}

void SyncFileSystemBackend::AddFileUpdateObserver(
    fileapi::FileSystemType type,
    fileapi::FileUpdateObserver* observer,
    base::SequencedTaskRunner* task_runner) {
  DCHECK_EQ(fileapi::kFileSystemTypeSyncable, type);
  fileapi::UpdateObserverList* list = &syncable_update_observers_;
  *list = list->AddObserver(observer, task_runner);
}

void SyncFileSystemBackend::AddFileChangeObserver(
    fileapi::FileSystemType type,
    fileapi::FileChangeObserver* observer,
    base::SequencedTaskRunner* task_runner) {
  DCHECK_EQ(fileapi::kFileSystemTypeSyncable, type);
  fileapi::ChangeObserverList* list = &syncable_change_observers_;
  *list = list->AddObserver(observer, task_runner);
}

void SyncFileSystemBackend::AddFileAccessObserver(
    fileapi::FileSystemType type,
    fileapi::FileAccessObserver* observer,
    base::SequencedTaskRunner* task_runner) {
  NOTREACHED() << "SyncFileSystemBackend does not support access observers.";
}

const fileapi::UpdateObserverList* SyncFileSystemBackend::GetUpdateObservers(
    fileapi::FileSystemType type) const {
  DCHECK(CanHandleType(type));
  if (type != fileapi::kFileSystemTypeSyncable)
    return NULL;
  return &syncable_update_observers_;
}

const fileapi::ChangeObserverList* SyncFileSystemBackend::GetChangeObservers(
    fileapi::FileSystemType type) const {
  DCHECK(CanHandleType(type));
  DCHECK_EQ(fileapi::kFileSystemTypeSyncable, type);
  if (type != fileapi::kFileSystemTypeSyncable)
    return NULL;
  return &syncable_change_observers_;
}

const fileapi::AccessObserverList* SyncFileSystemBackend::GetAccessObservers(
    fileapi::FileSystemType type) const {
  return NULL;
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

  DCHECK(sandbox_context_);
  AddFileUpdateObserver(
      fileapi::kFileSystemTypeSyncable,
      change_tracker_.get(),
      sandbox_context_->file_task_runner());
  AddFileChangeObserver(
      fileapi::kFileSystemTypeSyncable,
      change_tracker_.get(),
      sandbox_context_->file_task_runner());
}

void SyncFileSystemBackend::set_sync_context(
    LocalFileSyncContext* sync_context) {
  DCHECK(!sync_context_);
  sync_context_ = sync_context;
}

}  // namespace sync_file_system
