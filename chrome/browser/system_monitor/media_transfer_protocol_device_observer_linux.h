// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_MEDIA_TRANSFER_PROTOCOL_DEVICE_OBSERVER_LINUX_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_MEDIA_TRANSFER_PROTOCOL_DEVICE_OBSERVER_LINUX_H_

#include <map>
#include <string>

#include "base/string16.h"
#include "base/system_monitor/system_monitor.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"

class FilePath;

namespace chrome {

// Gets the mtp device information given a |storage_name|. On success,
// fills in |id|, |name| and |location|.
typedef void (*GetStorageInfoFunc)(const std::string& storage_name,
                                   std::string* id,
                                   string16* name,
                                   std::string* location);

// Helper class to send MTP storage attachment and detachment events to
// SystemMonitor.
class MediaTransferProtocolDeviceObserverLinux
    : public device::MediaTransferProtocolManager::Observer {
 public:
  // Should only be called by browser start up code. Use GetInstance() instead.
  MediaTransferProtocolDeviceObserverLinux();
  virtual ~MediaTransferProtocolDeviceObserverLinux();

  static MediaTransferProtocolDeviceObserverLinux* GetInstance();

  // Finds the storage that contains |path| and populates |storage_info|.
  // Returns false if unable to find the storage.
  bool GetStorageInfoForPath(
      const FilePath& path,
      base::SystemMonitor::RemovableStorageInfo* storage_info) const;

 protected:
  // Only used in unit tests.
  explicit MediaTransferProtocolDeviceObserverLinux(
      GetStorageInfoFunc get_storage_info_func);

  // device::MediaTransferProtocolManager::Observer implementation.
  // Exposed for unit tests.
  virtual void StorageChanged(bool is_attached,
                              const std::string& storage_name) OVERRIDE;

 private:
  // Mapping of storage location and mtp storage info object.
  typedef std::map<std::string, base::SystemMonitor::RemovableStorageInfo>
      StorageLocationToInfoMap;

  // Enumerate existing mtp storage devices.
  void EnumerateStorages();

  // Map of all attached mtp devices.
  StorageLocationToInfoMap storage_map_;

  // Function handler to get storage information. This is useful to set a mock
  // handler for unit testing.
  GetStorageInfoFunc get_storage_info_func_;

  DISALLOW_COPY_AND_ASSIGN(MediaTransferProtocolDeviceObserverLinux);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_MEDIA_TRANSFER_PROTOCOL_DEVICE_OBSERVER_LINUX_H_
