// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_VOLUME_MOUNT_WATCHER_WIN_H_
#define CHROME_BROWSER_STORAGE_MONITOR_VOLUME_MOUNT_WATCHER_WIN_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"

namespace chrome {

// This class watches the volume mount points and sends notifications to
// StorageMonitor about the device attach/detach events.
// This is a singleton class instantiated by StorageMonitorWin.
class VolumeMountWatcherWin {
 public:
  VolumeMountWatcherWin();
  virtual ~VolumeMountWatcherWin();

  // Returns the volume file path of the drive specified by the |drive_number|.
  // |drive_number| inputs of 0 - 25 are valid. Returns an empty file path if
  // the |drive_number| is invalid.
  static base::FilePath DriveNumberToFilePath(int drive_number);

  void Init();

  // Gets the information about the device mounted at |device_path|. On success,
  // returns true and fills in |location|, |unique_id|, |name|, |removable|, and
  // |total_size_in_bytes|.
  // Can block during startup while device info is still loading.
  virtual bool GetDeviceInfo(const base::FilePath& device_path,
                             string16* location,
                             std::string* unique_id,
                             string16* name,
                             bool* removable,
                             uint64* total_size_in_bytes) const;

  // Returns the partition size of the given mount location. Returns 0 if the
  // location is unknown.
  uint64 GetStorageSize(const base::FilePath::StringType& mount_point) const;

  // Processes DEV_BROADCAST_VOLUME messages and triggers a
  // notification if appropriate.
  void OnWindowMessage(UINT event_type, LPARAM data);

  // Set the volume notifications object to be used when new
  // removable volumes are found.
  void SetNotifications(StorageMonitor::Receiver* notifications);

 protected:
  struct MountPointInfo {
    std::string device_id;
    string16 location;
    std::string unique_id;
    string16 name;
    bool removable;
    uint64 total_size_in_bytes;
  };

  // Handles mass storage device attach event on UI thread.
  void HandleDeviceAttachEventOnUIThread(const base::FilePath& device_path,
                                         const MountPointInfo& info);

  // Handles mass storage device detach event on UI thread.
  void HandleDeviceDetachEventOnUIThread(const string16& device_location);

  static void VolumeMountWatcherWin::FindExistingDevicesAndAdd(
      base::Callback<std::vector<base::FilePath>(void)>
          get_attached_devices_callback,
      base::WeakPtr<chrome::VolumeMountWatcherWin> volume_watcher);

  // UI thread delegate to set up adding storage devices.
  void AddDevicesOnUIThread(std::vector<base::FilePath> removable_devices);

  static void VolumeMountWatcherWin::RetrieveInfoForDeviceAndAdd(
      const base::FilePath& device_path,
      base::Callback<bool(const base::FilePath&, string16*,
                          std::string*, string16*,
                          bool*, uint64*)> get_device_details_callback,
      base::WeakPtr<chrome::VolumeMountWatcherWin> volume_watcher);

  // Mark that a device we started a metadata check for has completed.
  virtual void DeviceCheckComplete(const base::FilePath& device_path);

  // Worker pool used to collect device information. Used because some
  // devices freeze workers trying to get device info, resulting in
  // shutdown hangs.
  scoped_refptr<base::SequencedWorkerPool> device_info_worker_pool_;

  // These closures can be overridden for testing to remove calling Windows API
  // functions.
  base::Callback<std::vector<base::FilePath>(void)>
      get_attached_devices_callback_;
  base::Callback<
      bool(const base::FilePath&, string16*,
           std::string*, string16*, bool*, uint64*)>
          get_device_details_callback_;

 private:
  // Key: Mass storage device mount point.
  // Value: Mass storage device metadata.
  typedef std::map<string16, MountPointInfo> MountPointDeviceMetadataMap;

  // Maintain a set of device attribute check calls in-flight. Only accessed
  // on the UI thread. This is to try and prevent the same device from
  // occupying our worker pool in case of windows API call hangs.
  std::set<base::FilePath> pending_device_checks_;

  // A map from device mount point to device metadata. Only accessed on the UI
  // thread.
  MountPointDeviceMetadataMap device_metadata_;

  base::WeakPtrFactory<VolumeMountWatcherWin> weak_factory_;

  // The notifications object to use to signal newly attached volumes. Only
  // removable devices will be notified.
  StorageMonitor::Receiver* notifications_;

  DISALLOW_COPY_AND_ASSIGN(VolumeMountWatcherWin);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_VOLUME_MOUNT_WATCHER_WIN_H_
