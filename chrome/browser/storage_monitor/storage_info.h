// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_STORAGE_INFO_H_
#define CHROME_BROWSER_STORAGE_MONITOR_STORAGE_INFO_H_

#include "base/files/file_path.h"
#include "base/string16.h"

namespace chrome {

struct StorageInfo {
  StorageInfo();
  StorageInfo(const std::string& id,
              const string16& device_name,
              const base::FilePath::StringType& device_location,
              const string16& label,
              const string16& vendor,
              const string16& model,
              uint64 size_in_bytes);
  ~StorageInfo();

  // Unique device id - persists between device attachments.
  // This is the string that should be used as the label for a particular
  // storage device when interacting with the API. Clients should treat
  // this as an opaque string.
  std::string device_id;

  // Human readable removable storage device name.
  string16 name;

  // Current attached removable storage device location.
  base::FilePath::StringType location;

  // Label given to this storage device by the user.
  // May be empty if not found or the device is unlabeled.
  string16 storage_label;

  // Vendor name for the removable device. (Human readable)
  // May be empty if not collected.
  string16 vendor_name;

  // Model name for the removable device. (Human readable)
  // May be empty if not collected.
  string16 model_name;

  // Size of the removable device in bytes.
  // Zero if not collected or unknown.
  uint64 total_size_in_bytes;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_STORAGE_INFO_H_
