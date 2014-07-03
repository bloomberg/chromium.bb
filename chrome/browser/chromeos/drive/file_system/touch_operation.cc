// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/touch_operation.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/drive/file_change.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

namespace {

// Updates the timestamps of the entry specified by |file_path|.
FileError UpdateLocalState(internal::ResourceMetadata* metadata,
                           const base::FilePath& file_path,
                           const base::Time& last_access_time,
                           const base::Time& last_modified_time,
                           ResourceEntry* entry,
                           std::string* local_id) {
  FileError error = metadata->GetResourceEntryByPath(file_path, entry);
  if (error != FILE_ERROR_OK)
    return error;
  *local_id = entry->local_id();

  PlatformFileInfoProto* file_info = entry->mutable_file_info();
  if (!last_access_time.is_null())
    file_info->set_last_accessed(last_access_time.ToInternalValue());
  if (!last_modified_time.is_null())
    file_info->set_last_modified(last_modified_time.ToInternalValue());
  entry->set_metadata_edit_state(ResourceEntry::DIRTY);
  entry->set_modification_date(base::Time::Now().ToInternalValue());
  return metadata->RefreshEntry(*entry);
}

}  // namespace

TouchOperation::TouchOperation(base::SequencedTaskRunner* blocking_task_runner,
                               OperationObserver* observer,
                               internal::ResourceMetadata* metadata)
    : blocking_task_runner_(blocking_task_runner),
      observer_(observer),
      metadata_(metadata),
      weak_ptr_factory_(this) {
}

TouchOperation::~TouchOperation() {
}

void TouchOperation::TouchFile(const base::FilePath& file_path,
                               const base::Time& last_access_time,
                               const base::Time& last_modified_time,
                               const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  std::string* local_id = new std::string;
  ResourceEntry* entry = new ResourceEntry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&UpdateLocalState,
                 metadata_,
                 file_path,
                 last_access_time,
                 last_modified_time,
                 entry,
                 local_id),
      base::Bind(&TouchOperation::TouchFileAfterUpdateLocalState,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 callback,
                 base::Owned(entry),
                 base::Owned(local_id)));
}

void TouchOperation::TouchFileAfterUpdateLocalState(
    const base::FilePath& file_path,
    const FileOperationCallback& callback,
    const ResourceEntry* entry,
    const std::string* local_id,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileChange changed_files;
  changed_files.Update(
      file_path,
      entry->file_info().is_directory() ?
          FileChange::FILE_TYPE_DIRECTORY : FileChange::FILE_TYPE_FILE,
      FileChange::ADD_OR_UPDATE);

  if (error == FILE_ERROR_OK) {
    observer_->OnFileChangedByOperation(changed_files);
    observer_->OnEntryUpdatedByOperation(*local_id);
  }
  callback.Run(error);
}

}  // namespace file_system
}  // namespace drive
