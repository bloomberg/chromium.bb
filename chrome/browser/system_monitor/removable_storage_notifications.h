// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_STORAGE_NOTIFICATIONS_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_STORAGE_NOTIFICATIONS_H_

#include "base/file_path.h"
#include "base/observer_list_threadsafe.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"
#include "base/system_monitor/system_monitor.h"

namespace chrome {

class RemovableStorageObserver;

// Base class for platform-specific instances watching for removable storage
// attachments/detachments.
class RemovableStorageNotifications {
 public:
  struct StorageInfo {
    StorageInfo();
    StorageInfo(const std::string& id,
                const string16& device_name,
                const FilePath::StringType& device_location);

    // Unique device id - persists between device attachments.
    std::string device_id;

    // Human readable removable storage device name.
    string16 name;

    // Current attached removable storage device location.
    FilePath::StringType location;
  };

  virtual ~RemovableStorageNotifications();

  // Returns a pointer to an object owned by the BrowserMainParts, with lifetime
  // somewhat shorter than a process Singleton.
  static RemovableStorageNotifications* GetInstance();

  // Finds the device that contains |path| and populates |device_info|.
  // Should be able to handle any path on the local system, not just removable
  // storage. Returns false if unable to find the device.
  // TODO(gbillock): Change this method signature to use StorageInfo.
  virtual bool GetDeviceInfoForPath(
      const FilePath& path,
      base::SystemMonitor::RemovableStorageInfo* device_info) const = 0;

  // Returns the storage size of the device present at |location|. If the
  // device information is unavailable, returns zero.
  virtual uint64 GetStorageSize(const std::string& location) const = 0;

  // Returns information for attached removable storage.
  std::vector<StorageInfo> GetAttachedStorage() const;

  void AddObserver(RemovableStorageObserver* obs);
  void RemoveObserver(RemovableStorageObserver* obs);

 protected:
  RemovableStorageNotifications();

  void ProcessAttach(const std::string& id,
                     const string16& name,
                     const FilePath::StringType& location);
  void ProcessDetach(const std::string& id);

 private:
  typedef std::map<std::string, StorageInfo> RemovableStorageMap;

  scoped_refptr<ObserverListThreadSafe<RemovableStorageObserver> >
      observer_list_;

  // For manipulating removable_storage_map_ structure.
  mutable base::Lock storage_lock_;

  // Map of all the attached removable storage devices.
  RemovableStorageMap storage_map_;
};

} // namespace chrome

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_STORAGE_NOTIFICATIONS_H_
