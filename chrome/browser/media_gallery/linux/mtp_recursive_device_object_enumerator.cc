// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/linux/mtp_recursive_device_object_enumerator.h"

#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/media_gallery/linux/mtp_device_object_enumerator.h"
#include "chrome/browser/media_gallery/linux/mtp_read_directory_worker.h"

namespace chrome {

MTPRecursiveDeviceObjectEnumerator::MTPRecursiveDeviceObjectEnumerator(
    const std::string& handle,
    base::SequencedTaskRunner* task_runner,
    const std::vector<MtpFileEntry>& entries,
    base::WaitableEvent* task_completed_event,
    base::WaitableEvent* shutdown_event)
    : device_handle_(handle),
      media_task_runner_(task_runner),
      file_entries_(entries),
      file_entry_iter_(file_entries_.begin()),
      on_task_completed_event_(task_completed_event),
      on_shutdown_event_(shutdown_event) {
  DCHECK(on_task_completed_event_);
  DCHECK(on_shutdown_event_);
  current_enumerator_.reset(new MTPDeviceObjectEnumerator(entries));
}

MTPRecursiveDeviceObjectEnumerator::~MTPRecursiveDeviceObjectEnumerator() {
}

FilePath MTPRecursiveDeviceObjectEnumerator::Next() {
  if (on_shutdown_event_->IsSignaled()) {
    // Process is in shut down mode.
    return FilePath();
  }

  FilePath path = current_enumerator_->Next();
  if (!path.empty())
    return path;

  // We reached the end.
  if (file_entry_iter_ == file_entries_.end())
    return FilePath();

  // Enumerate subdirectories of the next media file entry.
  MtpFileEntry next_file_entry = *file_entry_iter_;
  ++file_entry_iter_;

  // Create a MTPReadDirectoryWorker object to enumerate sub directories.
  scoped_refptr<MTPReadDirectoryWorker> worker(new MTPReadDirectoryWorker(
      device_handle_, next_file_entry.item_id(), media_task_runner_,
      on_task_completed_event_, on_shutdown_event_));
  worker->Run();
  if (!worker->get_file_entries().empty()) {
    current_enumerator_.reset(
        new MTPDeviceObjectEnumerator(worker->get_file_entries()));
  } else {
    current_enumerator_.reset(
        new fileapi::FileSystemFileUtil::EmptyFileEnumerator());
  }
  return current_enumerator_->Next();
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

}  // namespace chrome
