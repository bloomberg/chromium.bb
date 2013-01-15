// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/linux/mtp_device_object_enumerator.h"

#include "base/logging.h"

namespace chrome {

MTPDeviceObjectEnumerator::MTPDeviceObjectEnumerator(
    const MtpFileEntries& entries)
    : file_entries_(entries),
      file_entry_iter_(file_entries_.begin()) {
}

MTPDeviceObjectEnumerator::~MTPDeviceObjectEnumerator() {
}

FilePath MTPDeviceObjectEnumerator::Next() {
  if (file_entry_iter_ == file_entries_.end())
    return FilePath();

  current_file_info_ = *file_entry_iter_;
  ++file_entry_iter_;
  return FilePath(current_file_info_.file_name());
}

int64 MTPDeviceObjectEnumerator::Size() {
  return current_file_info_.file_size();
}

bool MTPDeviceObjectEnumerator::IsDirectory() {
  return current_file_info_.file_type() == MtpFileEntry::FILE_TYPE_FOLDER;
}

base::Time MTPDeviceObjectEnumerator::LastModifiedTime() {
  return base::Time::FromTimeT(current_file_info_.modification_time());
}

bool MTPDeviceObjectEnumerator::GetEntryId(uint32_t* entry_id) const {
  DCHECK(entry_id);
  if (file_entry_iter_ == file_entries_.end())
    return false;

  *entry_id = file_entry_iter_->item_id();
  return true;
}

bool MTPDeviceObjectEnumerator::HasMoreEntries() const {
  return file_entry_iter_ != file_entries_.end();
}

}  // namespace chrome
