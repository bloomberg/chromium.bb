// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StorageMonitorLinux processes mount point change events, notifies listeners
// about the addition and deletion of media devices, and answers queries about
// mounted devices.
// StorageMonitorLinux lives on the UI thread, and uses a MtabWatcherLinux on
// the FILE thread to get mount point change events.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_STORAGE_MONITOR_LINUX_H_
#define CHROME_BROWSER_STORAGE_MONITOR_STORAGE_MONITOR_LINUX_H_

#if defined(OS_CHROMEOS)
#error "Use the ChromeOS-specific implementation instead."
#endif

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/storage_monitor/mtab_watcher_linux.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_thread.h"

namespace chrome {

class MediaTransferProtocolDeviceObserverLinux;

class StorageMonitorLinux : public StorageMonitor,
                            public MtabWatcherLinux::Delegate {
 public:
  // Should only be called by browser start up code.  Use GetInstance() instead.
  explicit StorageMonitorLinux(const base::FilePath& path);
  virtual ~StorageMonitorLinux();

  // Must be called for StorageMonitorLinux to work.
  void Init();

  // Finds the device that contains |path| and populates |device_info|.
  // Returns false if unable to find the device.
  virtual bool GetStorageInfoForPath(
      const base::FilePath& path,
      StorageInfo* device_info) const OVERRIDE;

  // Returns the storage partition size of the device present at |location|.
  // If the requested information is unavailable, returns 0.
  virtual uint64 GetStorageSize(const std::string& location) const OVERRIDE;

 protected:
  // Gets device information given a |device_path| and |mount_point|.
  typedef base::Callback<scoped_ptr<StorageInfo>(
      const base::FilePath& device_path,
      const base::FilePath& mount_point)> GetDeviceInfoCallback;

  void SetGetDeviceInfoCallbackForTest(
      const GetDeviceInfoCallback& get_device_info_callback);

  // MtabWatcherLinux::Delegate implementation.
  virtual void UpdateMtab(
      const MtabWatcherLinux::MountPointDeviceMap& new_mtab) OVERRIDE;

 private:
  // Structure to save mounted device information such as device path, unique
  // identifier, device name and partition size.
  struct MountPointInfo {
    base::FilePath mount_device;
    StorageInfo storage_info;
  };

  // For use with scoped_ptr.
  struct MtabWatcherLinuxDeleter {
    void operator()(MtabWatcherLinux* mtab_watcher) {
      content::BrowserThread::DeleteSoon(content::BrowserThread::FILE,
                                         FROM_HERE, mtab_watcher);
    }
  };

  // Mapping of mount points to MountPointInfo.
  typedef std::map<base::FilePath, MountPointInfo> MountMap;

  // (mount point, priority)
  // For devices that are mounted to multiple mount points, this helps us track
  // which one we've notified system monitor about.
  typedef std::map<base::FilePath, bool> ReferencedMountPoint;

  // (mount device, map of known mount points)
  // For each mount device, track the places it is mounted and which one (if
  // any) we have notified system monitor about.
  typedef std::map<base::FilePath, ReferencedMountPoint> MountPriorityMap;

  // Called when the MtabWatcher has been created.
  void OnMtabWatcherCreated(MtabWatcherLinux* watcher);

  bool IsDeviceAlreadyMounted(const base::FilePath& mount_device) const;

  // Assuming |mount_device| is already mounted, and it gets mounted again at
  // |mount_point|, update the mappings.
  void HandleDeviceMountedMultipleTimes(const base::FilePath& mount_device,
                                        const base::FilePath& mount_point);

  // Adds |mount_device| to the mappings and notify listeners, if any.
  void AddNewMount(const base::FilePath& mount_device,
                   scoped_ptr<StorageInfo> storage_info);

  // Mtab file that lists the mount points.
  const base::FilePath mtab_path_;

  // Callback to get device information. Set this to a custom callback for
  // testing.
  GetDeviceInfoCallback get_device_info_callback_;

  // Mapping of relevant mount points and their corresponding mount devices.
  // Keep in mind on Linux, a device can be mounted at multiple mount points,
  // and multiple devices can be mounted at a mount point.
  MountMap mount_info_map_;

  // Because a device can be mounted to multiple places, we only want to
  // notify about one of them. If (and only if) that one is unmounted, we need
  // to notify about it's departure and notify about another one of it's mount
  // points.
  MountPriorityMap mount_priority_map_;

  scoped_ptr<MediaTransferProtocolDeviceObserverLinux>
      media_transfer_protocol_device_observer_;

  scoped_ptr<MtabWatcherLinux, MtabWatcherLinuxDeleter> mtab_watcher_;

  base::WeakPtrFactory<StorageMonitorLinux> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(StorageMonitorLinux);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_STORAGE_MONITOR_LINUX_H_
