// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/storage_info.h"

namespace chrome {

StorageInfo::StorageInfo() : total_size_in_bytes(0) {
}

StorageInfo::StorageInfo(const std::string& id, const string16& device_name,
                         const base::FilePath::StringType& device_location)
    : device_id(id),
      name(device_name),
      location(device_location),
      total_size_in_bytes(0) {
}

StorageInfo::~StorageInfo() {
}

}  // namespace chrome
