// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/storage_info.h"

namespace chrome {

StorageInfo::StorageInfo() : total_size_in_bytes(0) {
}

StorageInfo::StorageInfo(const std::string& id,
                         const string16& device_name,
                         const base::FilePath::StringType& device_location,
                         const string16& label,
                         const string16& vendor,
                         const string16& model,
                         uint64 size_in_bytes)
    : device_id(id),
      name(device_name),
      location(device_location),
      storage_label(label),
      vendor_name(vendor),
      model_name(model),
      total_size_in_bytes(size_in_bytes) {
}

StorageInfo::~StorageInfo() {
}

}  // namespace chrome
