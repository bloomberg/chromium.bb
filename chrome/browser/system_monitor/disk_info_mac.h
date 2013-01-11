// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_DISK_INFO_MAC_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_DISK_INFO_MAC_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/file_path.h"
#include "chrome/browser/system_monitor/media_storage_util.h"

namespace chrome {

// This class stores information about a particular disk.
class DiskInfoMac {
 public:
  DiskInfoMac();
  ~DiskInfoMac();

  // Creates a disk info object based on information from the given
  // dictionary. This function must be called on the file thread.
  static DiskInfoMac BuildDiskInfoOnFileThread(CFDictionaryRef dict);

  // Construct a disk info object from info from an ImageCature device.
  static DiskInfoMac BuildDiskInfoFromICDevice(std::string device_id,
                                               string16 device_name,
                                               FilePath mount_point);

  const std::string& bsd_name() const { return bsd_name_; }
  const std::string& device_id() const { return device_id_; }
  const std::string& model_name() const { return model_name_; }
  const string16& device_name() const { return device_name_; }
  const FilePath& mount_point() const { return mount_point_; }
  MediaStorageUtil::Type type() const { return type_; }
  uint64 total_size_in_bytes() const { return total_size_in_bytes_; }

 private:
  std::string bsd_name_;
  std::string device_id_;
  std::string model_name_;
  string16 device_name_;
  FilePath mount_point_;
  MediaStorageUtil::Type type_;
  uint64 total_size_in_bytes_;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_DISK_INFO_MAC_H_
