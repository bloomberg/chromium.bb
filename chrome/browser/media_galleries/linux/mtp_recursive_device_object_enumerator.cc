// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/linux/mtp_recursive_device_object_enumerator.h"

#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/media_galleries/linux/mtp_device_object_enumerator.h"
#include "chrome/browser/media_galleries/linux/mtp_read_directory_worker.h"

namespace chrome {

MTPRecursiveDeviceObjectEnumerator::MTPRecursiveDeviceObjectEnumerator(
    const std::string& handle,
    base::SequencedTaskRunner* task_runner,
    const std::vector<MtpFileEntry>& entries,
    base::WaitableEvent* task_completed_event,
    base::WaitableEvent* shutdown_event)
    : device_handle_(handle),
      media_task_runner_(task_runner),
      on_task_completed_event_(task_completed_event),
      on_shutdown_event_(shutdown_event) {
  DCHECK(on_task_completed_event_);
  DCHECK(on_shutdown_event_);
  DCHECK(!entries.empty());
  current_enumerator_.reset(new MTPDeviceObjectEnumerator(entries));
}

MTPRecursiveDeviceObjectEnumerator::~MTPRecursiveDeviceObjectEnumerator() {
}

base::FilePath MTPRecursiveDeviceObjectEnumerator::Next() {
  if (on_shutdown_event_->IsSignaled()) {
    // Process is in shut down mode.
    return base::FilePath();
  }
  base::FilePath path = current_enumerator_->Next();
  if (path.empty()) {
    scoped_ptr<MTPDeviceObjectEnumerator> next_enumerator =
        GetNextSubdirectoryEnumerator();
    if (next_enumerator) {
      current_enumerator_ = next_enumerator.Pass();
      path = current_enumerator_->Next();
    } else {
      // If there's no |next_enumerator|, then |current_enumerator_| is the
      // last enumerator and it remains in its end state. Thus it is retained
      // to act as an EmptyFileEnumerator. Return early since this is the end.
      return base::FilePath();
    }
  }

  if (IsDirectory()) {
    // If the current entry is a directory, add it to
    // |untraversed_directory_entry_ids_| to scan after scanning this directory.
    DirectoryEntryId dir_entry_id;
    if (current_enumerator_->GetEntryId(&dir_entry_id))
      untraversed_directory_entry_ids_.push(dir_entry_id);
  }
  return path;
}

int64 MTPRecursiveDeviceObjectEnumerator::Size() {
  return current_enumerator_->Size();
}

bool MTPRecursiveDeviceObjectEnumerator::IsDirectory() {
  return current_enumerator_->IsDirectory();
}

base::Time MTPRecursiveDeviceObjectEnumerator::LastModifiedTime() {
  return current_enumerator_->LastModifiedTime();
}

scoped_ptr<MTPDeviceObjectEnumerator>
MTPRecursiveDeviceObjectEnumerator::GetNextSubdirectoryEnumerator() {
  while (!untraversed_directory_entry_ids_.empty()) {
    // Create a MTPReadDirectoryWorker object to enumerate sub directories.
    DirectoryEntryId dir_entry_id = untraversed_directory_entry_ids_.front();
    untraversed_directory_entry_ids_.pop();
    scoped_refptr<MTPReadDirectoryWorker> worker(new MTPReadDirectoryWorker(
        device_handle_, dir_entry_id, media_task_runner_,
        on_task_completed_event_, on_shutdown_event_));
    worker->Run();
    if (!worker->get_file_entries().empty()) {
      return scoped_ptr<MTPDeviceObjectEnumerator>(
          new MTPDeviceObjectEnumerator(worker->get_file_entries()));
    }
  }

  // Reached the end. Traversed all the sub directories.
  return scoped_ptr<MTPDeviceObjectEnumerator>();
}

}  // namespace chrome
