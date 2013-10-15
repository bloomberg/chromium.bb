// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/scoped_mtp_device_map_entry.h"

#include "chrome/browser/media_galleries/media_file_system_context.h"

ScopedMTPDeviceMapEntry::ScopedMTPDeviceMapEntry(
    const base::FilePath::StringType& device_location,
    MediaFileSystemContext* context)
    : device_location_(device_location),
      context_(context) {
}

ScopedMTPDeviceMapEntry::~ScopedMTPDeviceMapEntry() {
  context_->RemoveScopedMTPDeviceMapEntry(device_location_);
}
