// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RecursiveMTPDeviceObjectEnumerator implementation.

#include "chrome/browser/media_galleries/win/recursive_mtp_device_object_enumerator.h"

#include "base/files/file_path.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/media_galleries/win/mtp_device_object_entry.h"
#include "chrome/browser/media_galleries/win/mtp_device_object_enumerator.h"
#include "chrome/browser/media_galleries/win/mtp_device_operations_util.h"
#include "content/public/browser/browser_thread.h"

namespace chrome {

RecursiveMTPDeviceObjectEnumerator::RecursiveMTPDeviceObjectEnumerator(
    IPortableDevice* device,
    const MTPDeviceObjectEntries& entries)
    : device_(device) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!entries.empty());
  current_enumerator_.reset(new MTPDeviceObjectEnumerator(entries));
}

RecursiveMTPDeviceObjectEnumerator::~RecursiveMTPDeviceObjectEnumerator() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

base::FilePath RecursiveMTPDeviceObjectEnumerator::Next() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::FilePath path = current_enumerator_->Next();
  if (path.empty()) {
    // Reached the end of |current_enumerator_|.
    scoped_ptr<MTPDeviceObjectEnumerator> next_enumerator =
        GetNextSubdirectoryEnumerator();
    if (next_enumerator) {
      current_enumerator_ = next_enumerator.Pass();
      path = current_enumerator_->Next();
    } else {
      // Traversed all the sub directories.
      return base::FilePath();
    }
  }

  if (IsDirectory()) {
    // If the current entry is a directory, add it to
    // |untraversed_directory_object_ids_| to scan after scanning this
    // directory.
    string16 dir_object_entry_id = current_enumerator_->GetObjectId();
    if (!dir_object_entry_id.empty())
      untraversed_directory_object_ids_.push(dir_object_entry_id);
  }
  return path;
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

scoped_ptr<MTPDeviceObjectEnumerator>
RecursiveMTPDeviceObjectEnumerator::GetNextSubdirectoryEnumerator() {
  while (!untraversed_directory_object_ids_.empty()) {
    // Create a MTPReadDirectoryWorker object to enumerate sub directories.
    string16 dir_entry_id = untraversed_directory_object_ids_.front();
    untraversed_directory_object_ids_.pop();
    MTPDeviceObjectEntries curr_object_entries;
    if (media_transfer_protocol::GetDirectoryEntries(device_.get(),
                                                     dir_entry_id,
                                                     &curr_object_entries) &&
        !curr_object_entries.empty()) {
      return scoped_ptr<MTPDeviceObjectEnumerator>(
          new MTPDeviceObjectEnumerator(curr_object_entries));
    }
  }

  // Reached the end. Traversed all the sub directories.
  return scoped_ptr<MTPDeviceObjectEnumerator>();
}

}  // namespace chrome
