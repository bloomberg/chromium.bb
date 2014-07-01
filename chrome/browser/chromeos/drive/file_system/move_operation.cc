// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/move_operation.h"

#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_change.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {
namespace {

// Looks up ResourceEntry for source entry and the destination directory.
FileError UpdateLocalState(internal::ResourceMetadata* metadata,
                           const base::FilePath& src_path,
                           const base::FilePath& dest_path,
                           FileChange* changed_files,
                           std::string* local_id) {
  ResourceEntry entry;
  FileError error = metadata->GetResourceEntryByPath(src_path, &entry);
  if (error != FILE_ERROR_OK)
    return error;
  *local_id = entry.local_id();

  ResourceEntry parent_entry;
  error = metadata->GetResourceEntryByPath(dest_path.DirName(), &parent_entry);
  if (error != FILE_ERROR_OK)
    return error;

  // The parent must be a directory.
  if (!parent_entry.file_info().is_directory())
    return FILE_ERROR_NOT_A_DIRECTORY;

  changed_files->Update(src_path, entry, FileChange::DELETE);

  // Strip the extension for a hosted document if necessary.
  const std::string new_extension =
      base::FilePath(dest_path.Extension()).AsUTF8Unsafe();
  const bool has_hosted_document_extension =
      entry.has_file_specific_info() &&
      entry.file_specific_info().is_hosted_document() &&
      new_extension == entry.file_specific_info().document_extension();
  const std::string new_title =
      has_hosted_document_extension ?
      dest_path.BaseName().RemoveExtension().AsUTF8Unsafe() :
      dest_path.BaseName().AsUTF8Unsafe();

  entry.set_title(new_title);
  entry.set_parent_local_id(parent_entry.local_id());
  entry.set_metadata_edit_state(ResourceEntry::DIRTY);
  entry.set_modification_date(base::Time::Now().ToInternalValue());
  error = metadata->RefreshEntry(entry);
  if (error != FILE_ERROR_OK)
    return error;

  changed_files->Update(dest_path, entry, FileChange::ADD_OR_UPDATE);
  return FILE_ERROR_OK;
}

}  // namespace

MoveOperation::MoveOperation(base::SequencedTaskRunner* blocking_task_runner,
                             OperationObserver* observer,
                             internal::ResourceMetadata* metadata)
    : blocking_task_runner_(blocking_task_runner),
      observer_(observer),
      metadata_(metadata),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

MoveOperation::~MoveOperation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void MoveOperation::Move(const base::FilePath& src_file_path,
                         const base::FilePath& dest_file_path,
                         const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileChange* changed_files = new FileChange;
  std::string* local_id = new std::string;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&UpdateLocalState,
                 metadata_,
                 src_file_path,
                 dest_file_path,
                 changed_files,
                 local_id),
      base::Bind(&MoveOperation::MoveAfterUpdateLocalState,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(changed_files),
                 base::Owned(local_id)));
}

void MoveOperation::MoveAfterUpdateLocalState(
    const FileOperationCallback& callback,
    const FileChange* changed_files,
    const std::string* local_id,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (error == FILE_ERROR_OK) {
    // Notify the change of directory.
    observer_->OnFileChangedByOperation(*changed_files);
    observer_->OnEntryUpdatedByOperation(*local_id);
  }
  callback.Run(error);
}

}  // namespace file_system
}  // namespace drive
