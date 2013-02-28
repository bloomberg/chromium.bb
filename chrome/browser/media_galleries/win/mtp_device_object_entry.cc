// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MTPDeviceObjectEntry implementation.

#include "chrome/browser/media_galleries/win/mtp_device_object_entry.h"

namespace chrome {

MTPDeviceObjectEntry::MTPDeviceObjectEntry() : is_directory(false), size(0) {
}

MTPDeviceObjectEntry::MTPDeviceObjectEntry(const string16& object_id,
                                           const string16& object_name,
                                           bool is_directory,
                                           int64 size,
                                           const base::Time& last_modified_time)
    : object_id(object_id),
      name(object_name),
      is_directory(is_directory),
      size(size),
      last_modified_time(last_modified_time) {
}

}  // namespace chrome
