// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/open_file_operation.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system/download_operation.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {
namespace {

FileError UpdateFileLocalState(internal::FileCache* cache,
                               const std::string& resource_id,
                               const std::string& md5,
                               base::FilePath* local_file_path) {
  FileError error = cache->MarkDirty(resource_id, md5);
  if (error != FILE_ERROR_OK)
    return error;

  return cache->GetFile(resource_id, md5, local_file_path);
}

}  // namespace

OpenFileOperation::OpenFileOperation(
    base::SequencedTaskRunner* blocking_task_runner,
    OperationObserver* observer,
    JobScheduler* scheduler,
    internal::ResourceMetadata* metadata,
    internal::FileCache* cache,
    const base::FilePath& temporary_file_directory,
    std::set<base::FilePath>* open_files)
    : blocking_task_runner_(blocking_task_runner),
      cache_(cache),
      download_operation_(new DownloadOperation(
          blocking_task_runner, observer, scheduler,
          metadata, cache, temporary_file_directory)),
      open_files_(open_files),
      weak_ptr_factory_(this) {
  DCHECK(open_files);
}

OpenFileOperation::~OpenFileOperation() {
}

void OpenFileOperation::OpenFile(const base::FilePath& file_path,
                                 const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // If the file is already opened, it cannot be opened again before closed.
  // This is for avoiding simultaneous modification to the file, and moreover
  // to avoid an inconsistent cache state (suppose an operation sequence like
  // Open->Open->modify->Close->modify->Close; the second modify may not be
  // synchronized to the server since it is already Closed on the cache).
  if (!open_files_->insert(file_path).second) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, FILE_ERROR_IN_USE, base::FilePath()));
    return;
  }

  download_operation_->EnsureFileDownloadedByPath(
      file_path,
      ClientContext(USER_INITIATED),
      GetFileContentInitializedCallback(),
      google_apis::GetContentCallback(),
      base::Bind(
          &OpenFileOperation::OpenFileAfterFileDownloaded,
          weak_ptr_factory_.GetWeakPtr(),
          base::Bind(&OpenFileOperation::FinalizeOpenFile,
                     weak_ptr_factory_.GetWeakPtr(), file_path, callback)));
}

void OpenFileOperation::OpenFileAfterFileDownloaded(
    const OpenFileCallback& callback,
    FileError error,
    const base::FilePath& local_file_path,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == FILE_ERROR_OK) {
    DCHECK(entry);
    DCHECK(entry->has_file_specific_info());
    if (entry->file_specific_info().is_hosted_document())
      // No support for opening a hosted document.
      error = FILE_ERROR_INVALID_OPERATION;
  }

  if (error != FILE_ERROR_OK) {
    callback.Run(error, base::FilePath());
    return;
  }

  // Note: after marking the file dirty, the local file path may be changed.
  // So, it is necessary to take the path again.
  base::FilePath* new_local_file_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&UpdateFileLocalState,
                 cache_,
                 entry->resource_id(),
                 entry->file_specific_info().md5(),
                 new_local_file_path),
      base::Bind(&OpenFileOperation::OpenFileAfterUpdateLocalState,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(new_local_file_path)));
}

void OpenFileOperation::OpenFileAfterUpdateLocalState(
    const OpenFileCallback& callback,
    const base::FilePath* local_file_path,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  callback.Run(error, *local_file_path);
}

void OpenFileOperation::FinalizeOpenFile(
    const base::FilePath& drive_file_path,
    const OpenFileCallback& callback,
    FileError result,
    const base::FilePath& local_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // All the invocation of |callback| from operations initiated from OpenFile
  // must go through here. Removes the |drive_file_path| from the remembered
  // set when the file was not successfully opened.
  if (result != FILE_ERROR_OK)
    open_files_->erase(drive_file_path);

  callback.Run(result, local_file_path);
}

}  // namespace file_system
}  // namespace drive
