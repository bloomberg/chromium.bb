// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MTPDeviceObjectEnumerator implementation.

#include "chrome/browser/media_gallery/win/mtp_device_object_enumerator.h"

#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_thread.h"

namespace chrome {

MTPDeviceObjectEnumerator::MTPDeviceObjectEnumerator(
    const MTPDeviceObjectEntries& entries)
    : object_entries_(entries),
      object_entry_iter_(object_entries_.begin()),
      current_object_(NULL) {
  base::ThreadRestrictions::AssertIOAllowed();
}

MTPDeviceObjectEnumerator::~MTPDeviceObjectEnumerator() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

FilePath MTPDeviceObjectEnumerator::Next() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (object_entry_iter_ == object_entries_.end()) {
    current_object_ = NULL;
    return FilePath();
  }

  current_object_ = &(*(object_entry_iter_++));
  return FilePath(current_object_->name);
}

int64 MTPDeviceObjectEnumerator::Size() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return current_object_ ? current_object_->size : 0;
}

bool MTPDeviceObjectEnumerator::IsDirectory() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return current_object_ ? current_object_->is_directory : false;
}

base::Time MTPDeviceObjectEnumerator::LastModifiedTime() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return current_object_ ? current_object_->last_modified_time : base::Time();
}

}  // namespace chrome
