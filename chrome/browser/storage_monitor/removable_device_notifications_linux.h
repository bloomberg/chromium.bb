// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// RemovableDeviceNotificationsLinux listens for mount point changes, notifies
// listeners about the addition and deletion of media devices, and
// answers queries about mounted devices.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_LINUX_H_
#define CHROME_BROWSER_STORAGE_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_LINUX_H_

#if defined(OS_CHROMEOS)
#error "Use the ChromeOS-specific implementation instead."
#endif

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/storage_monitor/removable_storage_notifications.h"
#include "content/public/browser/browser_thread.h"

namespace base {
class FilePath;
}

// Gets device information given a |device_path|. On success, fills in
// |unique_id|, |name|, |removable| and |partition_size_in_bytes|.
typedef void (*GetDeviceInfoFunc)(const base::FilePath& device_path,
                                  std::string* unique_id,
                                  string16* name,
                                  bool* removable,
                                  uint64* partition_size_in_bytes);

namespace chrome {

class RemovableDeviceNotificationsLinux
    : public RemovableStorageNotifications,
      public base::RefCountedThreadSafe<RemovableDeviceNotificationsLinux,
          content::BrowserThread::DeleteOnFileThread> {
 public:
  // Should only be called by browser start up code.  Use GetInstance() instead.
  explicit RemovableDeviceNotificationsLinux(const base::FilePath& path);

  // Must be called for RemovableDeviceNotificationsLinux to work.
  void Init();

  // Finds the device that contains |path| and populates |device_info|.
  // Returns false if unable to find the device.
  virtual bool GetDeviceInfoForPath(
      const base::FilePath& path,
      StorageInfo* device_info) const OVERRIDE;

  // Returns the storage partition size of the device present at |location|.
  // If the requested information is unavailable, returns 0.
  virtual uint64 GetStorageSize(const std::string& location) const OVERRIDE;

 protected:
  // Only for use in unit tests.
  RemovableDeviceNotificationsLinux(const base::FilePath& path,
                                    GetDeviceInfoFunc getDeviceInfo);

  // Avoids code deleting the object while there are references to it.
  // Aside from the base::RefCountedThreadSafe friend class, and derived
  // classes, any attempts to call this dtor will result in a compile-time
  // error.
  virtual ~RemovableDeviceNotificationsLinux();

  virtual void OnFilePathChanged(const base::FilePath& path, bool error);

 private:
  friend class base::RefCountedThreadSafe<RemovableDeviceNotificationsLinux>;
  friend class base::DeleteHelper<RemovableDeviceNotificationsLinux>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::FILE>;

  // Structure to save mounted device information such as device path, unique
  // identifier, device name and partition size.
  struct MountPointInfo {
    MountPointInfo();

    base::FilePath mount_device;
    std::string device_id;
    string16 device_name;
    uint64 partition_size_in_bytes;
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

  // Do initialization on the File Thread.
  void InitOnFileThread();

  // Parses mtab file and find all changes.
  void UpdateMtab();

  // Adds |mount_device| as mounted on |mount_point|.  If the device is a new
  // device any listeners are notified.
  void AddNewMount(const base::FilePath& mount_device,
                   const base::FilePath& mount_point);

  // Whether Init() has been called or not.
  bool initialized_;

  // Mtab file that lists the mount points.
  const base::FilePath mtab_path_;

  // Watcher for |mtab_path_|.
  base::FilePathWatcher file_watcher_;

  // Set of known file systems that we care about.
  std::set<std::string> known_file_systems_;

  // Function handler to get device information. This is useful to set a mock
  // handler for unit testing.
  GetDeviceInfoFunc get_device_info_func_;

  // Mapping of relevant mount points and their corresponding mount devices.
  // Keep in mind on Linux, a device can be mounted at multiple mount points,
  // and multiple devices can be mounted at a mount point.
  MountMap mount_info_map_;

  // Because a device can be mounted to multiple places, we only want to
  // notify about one of them. If (and only if) that one is unmounted, we need
  // to notify about it's departure and notify about another one of it's mount
  // points.
  MountPriorityMap mount_priority_map_;

  DISALLOW_COPY_AND_ASSIGN(RemovableDeviceNotificationsLinux);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_LINUX_H_
