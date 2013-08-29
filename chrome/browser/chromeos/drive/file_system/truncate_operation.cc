// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/truncate_operation.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_platform_file_closer.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system/download_operation.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {
namespace {

// Truncates the local file at |local_cache_path| to the |length| bytes,
// then marks the resource is dirty on |cache|.
FileError TruncateOnBlockingPool(internal::ResourceMetadata* metadata,
                                 internal::FileCache* cache,
                                 const std::string& resource_id,
                                 const base::FilePath& local_cache_path,
                                 int64 length,
                                 std::string* local_id) {
  DCHECK(metadata);
  DCHECK(cache);
  DCHECK(local_id);

  FileError error = metadata->GetIdByResourceId(resource_id, local_id);
  if (error != FILE_ERROR_OK)
    return error;

  error = cache->MarkDirty(*local_id);
  if (error != FILE_ERROR_OK)
    return error;

  base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
  base::PlatformFile file = base::CreatePlatformFile(
      local_cache_path,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
      NULL,
      &result);
  if (result != base::PLATFORM_FILE_OK)
    return FILE_ERROR_FAILED;

  DCHECK_NE(base::kInvalidPlatformFileValue, file);
  base::ScopedPlatformFileCloser file_closer(&file);

  if (!base::TruncatePlatformFile(file, length))
    return FILE_ERROR_FAILED;

  return FILE_ERROR_OK;
}

}  // namespace

TruncateOperation::TruncateOperation(
    base::SequencedTaskRunner* blocking_task_runner,
    OperationObserver* observer,
    JobScheduler* scheduler,
    internal::ResourceMetadata* metadata,
    internal::FileCache* cache,
    const base::FilePath& temporary_file_directory)
    : blocking_task_runner_(blocking_task_runner),
      observer_(observer),
      metadata_(metadata),
      cache_(cache),
      download_operation_(new DownloadOperation(blocking_task_runner,
                                                observer,
                                                scheduler,
                                                metadata,
                                                cache,
                                                temporary_file_directory)),
      weak_ptr_factory_(this) {
}

TruncateOperation::~TruncateOperation() {
}

void TruncateOperation::Truncate(const base::FilePath& file_path,
                                 int64 length,
                                 const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (length < 0) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, FILE_ERROR_INVALID_OPERATION));
    return;
  }

  // TODO(kinaba): http://crbug.com/132780.
  // Optimize the cases for small |length|, at least for |length| == 0.
  download_operation_->EnsureFileDownloadedByPath(
      file_path,
      ClientContext(USER_INITIATED),
      GetFileContentInitializedCallback(),
      google_apis::GetContentCallback(),
      base::Bind(&TruncateOperation::TruncateAfterEnsureFileDownloadedByPath,
                 weak_ptr_factory_.GetWeakPtr(), length, callback));
}

void TruncateOperation::TruncateAfterEnsureFileDownloadedByPath(
    int64 length,
    const FileOperationCallback& callback,
    FileError error,
    const base::FilePath& local_file_path,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }
  DCHECK(entry);
  DCHECK(entry->has_file_specific_info());

  if (entry->file_specific_info().is_hosted_document()) {
    callback.Run(FILE_ERROR_INVALID_OPERATION);
    return;
  }

  std::string* local_id = new std::string;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&TruncateOnBlockingPool,
                 metadata_, cache_, entry->resource_id(), local_file_path,
                 length, local_id),
      base::Bind(
          &TruncateOperation::TruncateAfterTruncateOnBlockingPool,
          weak_ptr_factory_.GetWeakPtr(), base::Owned(local_id), callback));
}

void TruncateOperation::TruncateAfterTruncateOnBlockingPool(
    const std::string* local_id,
    const FileOperationCallback& callback,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  observer_->OnCacheFileUploadNeededByOperation(*local_id);

  callback.Run(error);
}

}  // namespace file_system
}  // namespace drive
