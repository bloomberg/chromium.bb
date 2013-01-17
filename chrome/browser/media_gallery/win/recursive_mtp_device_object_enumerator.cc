// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RecursiveMTPDeviceObjectEnumerator implementation.

#include "chrome/browser/media_gallery/win/recursive_mtp_device_object_enumerator.h"

#include "base/file_path.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/media_gallery/win/mtp_device_object_entry.h"
#include "chrome/browser/media_gallery/win/mtp_device_object_enumerator.h"
#include "chrome/browser/media_gallery/win/mtp_device_operations_util.h"
#include "content/public/browser/browser_thread.h"

namespace chrome {

RecursiveMTPDeviceObjectEnumerator::RecursiveMTPDeviceObjectEnumerator(
    IPortableDevice* device,
    const MTPDeviceObjectEntries& entries)
    : device_(device),
      curr_object_entries_(entries),
      object_entry_iter_(curr_object_entries_.begin()) {
  base::ThreadRestrictions::AssertIOAllowed();
  current_enumerator_.reset(new MTPDeviceObjectEnumerator(entries));
}

RecursiveMTPDeviceObjectEnumerator::~RecursiveMTPDeviceObjectEnumerator() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

FilePath RecursiveMTPDeviceObjectEnumerator::Next() {
  DCHECK(thread_checker_.CalledOnValidThread());
  MaybeUpdateCurrentObjectList();
  if (curr_object_entries_.empty())
    return FilePath();

  DCHECK(object_entry_iter_ != curr_object_entries_.end());
  const MTPDeviceObjectEntry& current_object_entry = *(object_entry_iter_++);
  if (current_object_entry.is_directory) {
    // If |current_object_entry| is a directory, add it to
    // |unparsed_directory_object_ids_| to scan after scanning this directory.
    unparsed_directory_object_ids_.push(current_object_entry.object_id);
  }
  return current_enumerator_->Next();
}

int64 RecursiveMTPDeviceObjectEnumerator::Size() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return current_enumerator_->Size();
}

bool RecursiveMTPDeviceObjectEnumerator::IsDirectory() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return current_enumerator_->IsDirectory();
}

base::Time RecursiveMTPDeviceObjectEnumerator::LastModifiedTime() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return current_enumerator_->LastModifiedTime();
}

void RecursiveMTPDeviceObjectEnumerator::MaybeUpdateCurrentObjectList() {
  if (object_entry_iter_ != curr_object_entries_.end())
    return;

  curr_object_entries_.clear();
  while (curr_object_entries_.empty() &&
         !unparsed_directory_object_ids_.empty()) {
    DirectoryObjectId object_id = unparsed_directory_object_ids_.front();
    unparsed_directory_object_ids_.pop();
    if (media_transfer_protocol::GetDirectoryEntries(device_.get(), object_id,
                                                     &curr_object_entries_) &&
        !curr_object_entries_.empty()) {
      current_enumerator_.reset(
          new MTPDeviceObjectEnumerator(curr_object_entries_));
      break;
    }
  }

  if (curr_object_entries_.empty()) {
    // Parsed all the objects.
    current_enumerator_.reset(
        new fileapi::FileSystemFileUtil::EmptyFileEnumerator());
  }
  object_entry_iter_ = curr_object_entries_.begin();
}

}  // namespace chrome
