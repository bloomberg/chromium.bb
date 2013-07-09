// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/close_file_operation.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

CloseFileOperation::CloseFileOperation(
    base::SequencedTaskRunner* blocking_task_runner,
    OperationObserver* observer,
    internal::ResourceMetadata* metadata,
    std::map<base::FilePath, int>* open_files)
    : blocking_task_runner_(blocking_task_runner),
      observer_(observer),
      metadata_(metadata),
      open_files_(open_files),
      weak_ptr_factory_(this) {
}

CloseFileOperation::~CloseFileOperation() {
}

void CloseFileOperation::CloseFile(const base::FilePath& file_path,
                                   const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (open_files_->find(file_path) == open_files_->end()) {
    // The file is not being opened.
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(callback, FILE_ERROR_NOT_FOUND));
    return;
  }

  ResourceEntry* entry = new ResourceEntry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&internal::ResourceMetadata::GetResourceEntryByPath,
                 base::Unretained(metadata_), file_path, entry),
      base::Bind(&CloseFileOperation::CloseFileAfterGetResourceEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path, callback, base::Owned(entry)));
}

void CloseFileOperation::CloseFileAfterGetResourceEntry(
    const base::FilePath& file_path,
    const FileOperationCallback& callback,
    const ResourceEntry* entry,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(entry);

  if (error == FILE_ERROR_OK && entry->file_info().is_directory())
    error = FILE_ERROR_NOT_FOUND;

  DCHECK_GT((*open_files_)[file_path], 0);
  if (--(*open_files_)[file_path] == 0) {
    // All clients closes this file, so notify to upload the file.
    open_files_->erase(file_path);
    observer_->OnCacheFileUploadNeededByOperation(entry->resource_id());
  }

  // Then invokes the user-supplied callback function.
  callback.Run(error);
}

}  // namespace file_system
}  // namespace drive
