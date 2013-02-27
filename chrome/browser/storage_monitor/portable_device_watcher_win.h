// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_PORTABLE_DEVICE_WATCHER_WIN_H_
#define CHROME_BROWSER_STORAGE_MONITOR_PORTABLE_DEVICE_WATCHER_WIN_H_

#include <portabledeviceapi.h>

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"

namespace base {
class SequencedTaskRunner;
}

namespace chrome {

namespace test {
class TestPortableDeviceWatcherWin;
}

// This class watches the portable device mount points and sends notifications
// about the attached/detached media transfer protocol (MTP) devices.
// This is a singleton class instantiated by
// RemovableDeviceNotificationsWindowWin. This class is created, destroyed and
// operates on the UI thread, except for long running tasks it spins off to a
// SequencedTaskRunner.
class PortableDeviceWatcherWin {
 public:
  typedef std::vector<string16> StorageObjectIDs;

  struct DeviceStorageObject {
    DeviceStorageObject(const string16& temporary_id,
                        const std::string& persistent_id);

    // Storage object temporary identifier, e.g. "s10001". This string ID
    // uniquely identifies the object on the device. This ID need not be
    // persistent across sessions. This ID is obtained from WPD_OBJECT_ID
    // property.
    string16 object_temporary_id;

    // Storage object persistent identifier,
    // e.g. "StorageSerial:<SID-{10001,D,31080448}>:<123456789>".
    std::string object_persistent_id;
  };
  typedef std::vector<DeviceStorageObject> StorageObjects;

  // Struct to store attached MTP device details.
  struct DeviceDetails {
    // Device name.
    string16 name;

    // Device interface path.
    string16 location;

    // Device storage details. A device can have multiple data partitions.
    StorageObjects storage_objects;
  };
  typedef std::vector<DeviceDetails> Devices;

  // TODO(gbillock): Change to take the device notifications object as
  // an argument.
  PortableDeviceWatcherWin();
  virtual ~PortableDeviceWatcherWin();

  // Must be called after the browser blocking pool is ready for use.
  // RemovableDeviceNotificationsWindowsWin::Init() will call this function.
  void Init(HWND hwnd);

  // Processes DEV_BROADCAST_DEVICEINTERFACE messages and triggers a
  // notification if appropriate.
  void OnWindowMessage(UINT event_type, LPARAM data);

  // Gets the information of the MTP storage specified by |storage_device_id|.
  // On success, returns true and fills in |device_location| with device
  // interface details and |storage_object_id| with storage object temporary
  // identifier.
  virtual bool GetMTPStorageInfoFromDeviceId(
      const std::string& storage_device_id,
      string16* device_location,
      string16* storage_object_id) const;

  // Constructs and returns a storage path from storage unique identifier.
  static string16 GetStoragePathFromStorageId(
      const std::string& storage_unique_id);

  // Set the volume notifications object to be used when new
  // devices are found.
  void SetNotifications(StorageMonitor::Receiver* notifications);

 private:
  friend class test::TestPortableDeviceWatcherWin;

  // Key: MTP device storage unique id.
  // Value: Metadata for the given storage.
  typedef std::map<std::string, StorageMonitor::StorageInfo>
      MTPStorageMap;

  // Key: MTP device plug and play ID string.
  // Value: Vector of device storage objects.
  typedef std::map<string16, StorageObjects> MTPDeviceMap;

  // Helpers to enumerate existing MTP storage devices.
  virtual void EnumerateAttachedDevices();
  void OnDidEnumerateAttachedDevices(const Devices* devices,
                                     const bool result);

  // Helpers to handle device attach event.
  virtual void HandleDeviceAttachEvent(const string16& pnp_device_id);
  void OnDidHandleDeviceAttachEvent(const DeviceDetails* device_details,
                                    const bool result);

  // Handles the detach event of the device specified by |pnp_device_id|.
  void HandleDeviceDetachEvent(const string16& pnp_device_id);

  // The portable device notifications handle.
  HDEVNOTIFY notifications_;

  // Attached media transfer protocol device map.
  MTPDeviceMap device_map_;

  // Attached media transfer protocol device storage objects map.
  MTPStorageMap storage_map_;

  // The task runner used to execute tasks that may take a long time and thus
  // should not be performed on the UI thread.
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // Used by |media_task_runner_| to create cancelable callbacks.
  base::WeakPtrFactory<PortableDeviceWatcherWin> weak_ptr_factory_;

  // The notifications object to use to signal newly attached devices.
  StorageMonitor::Receiver* storage_notifications_;

  DISALLOW_COPY_AND_ASSIGN(PortableDeviceWatcherWin);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_PORTABLE_DEVICE_WATCHER_WIN_H_
