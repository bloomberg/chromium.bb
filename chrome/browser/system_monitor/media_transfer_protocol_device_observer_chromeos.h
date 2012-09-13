// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_MEDIA_TRANSFER_PROTOCOL_DEVICE_OBSERVER_CHROMEOS_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_MEDIA_TRANSFER_PROTOCOL_DEVICE_OBSERVER_CHROMEOS_H_

#include <map>
#include <string>

#include "base/string16.h"
#include "chrome/browser/chromeos/mtp/media_transfer_protocol_manager.h"

namespace chromeos {
namespace mtp {

// Gets the mtp device information given a |storage_name|. On success,
// fills in |id|, |name| and |location|.
typedef void (*GetStorageInfoFunc)(const std::string& storage_name,
                                   std::string* id,
                                   string16* name,
                                   std::string* location);

// Helper class to send MTP storage attachment and detachment events to
// SystemMonitor.
class MediaTransferProtocolDeviceObserverCros
    : public MediaTransferProtocolManager::Observer {
 public:
  MediaTransferProtocolDeviceObserverCros();
  virtual ~MediaTransferProtocolDeviceObserverCros();

 protected:
  // Only used in unit tests.
  explicit MediaTransferProtocolDeviceObserverCros(
      GetStorageInfoFunc get_storage_info_func);

  // MediaTransferProtocolManager::Observer implementation.
  // Exposed for unit tests.
  virtual void StorageChanged(bool is_attached,
                              const std::string& storage_name) OVERRIDE;

 private:
  // Mapping of storage name and device id.
  typedef std::map<std::string, std::string> StorageNameToIdMap;

  // Enumerate existing mtp storage devices.
  void EnumerateStorages();

  // Map of all attached mtp devices.
  StorageNameToIdMap storage_map_;

  // Function handler to get storage information. This is useful to set a mock
  // handler for unit testing.
  GetStorageInfoFunc get_storage_info_func_;

  DISALLOW_COPY_AND_ASSIGN(MediaTransferProtocolDeviceObserverCros);
};

}  // namespace mtp
}  // namespace chromeos

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_MEDIA_TRANSFER_PROTOCOL_DEVICE_OBSERVER_CHROMEOS_H_
