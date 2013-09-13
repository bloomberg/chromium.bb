// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/scoped_mtp_device_map_entry.h"

ScopedMTPDeviceMapEntry::ScopedMTPDeviceMapEntry(
    const base::FilePath::StringType& device_location,
    const base::Closure& on_destruction_callback)
    : device_location_(device_location),
      on_destruction_callback_(on_destruction_callback) {
}

ScopedMTPDeviceMapEntry::~ScopedMTPDeviceMapEntry() {
  on_destruction_callback_.Run();
}
