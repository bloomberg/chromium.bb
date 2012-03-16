// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_DEVICE_NOTIFICATIONS_LINUX_H_
#define CONTENT_BROWSER_MEDIA_DEVICE_NOTIFICATIONS_LINUX_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/ref_counted.h"
#include "base/system_monitor/system_monitor.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT MediaDeviceNotificationsLinux
    : public base::RefCountedThreadSafe<MediaDeviceNotificationsLinux> {
 public:
  explicit MediaDeviceNotificationsLinux(const FilePath& path);

  // Must be called for MediaDeviceNotificationsLinux to work.
  void Init();

 protected:
  virtual ~MediaDeviceNotificationsLinux();

  virtual void OnFilePathChanged(const FilePath& path);

 private:
  friend class base::RefCountedThreadSafe<MediaDeviceNotificationsLinux>;

  typedef std::string MountDevice;
  typedef std::string MountPoint;
  typedef std::pair<MountDevice,
                    base::SystemMonitor::DeviceIdType> MountDeviceAndId;
  typedef std::map<MountPoint, MountDeviceAndId> MountMap;

  // A simple pass-through class. MediaDeviceNotificationsLinux cannot directly
  // inherit from FilePathWatcher::Delegate due to multiple inheritance.
  class WatcherDelegate : public base::files::FilePathWatcher::Delegate {
   public:
    explicit WatcherDelegate(MediaDeviceNotificationsLinux* notifier);

    // base::files::FilePathWatcher::Delegate implementation.
    virtual void OnFilePathChanged(const FilePath& path) OVERRIDE;

   private:
    friend class base::RefCountedThreadSafe<WatcherDelegate>;

    virtual ~WatcherDelegate();
    MediaDeviceNotificationsLinux* notifier_;

    DISALLOW_COPY_AND_ASSIGN(WatcherDelegate);
  };

  void InitOnFileThread();

  // Parse the mtab file and find all changes.
  void UpdateMtab();

  // Read the mtab file entries into |mtab|.
  void ReadMtab(MountMap* mtab);

  // For a new device mounted at |mount_point|, see if it is a media device by
  // checking for the existence of a DCIM directory.
  // If it is a media device, return true, otherwise return false.
  // Mac OS X behaves similarly, but this is not the only heuristic it uses.
  // TODO(vandebo) Try to figure out how Mac OS X decides this.
  bool IsMediaDevice(const MountPoint& mount_point);

  // Add a media device with a given device and mount device. Assign it a device
  // id as well.
  void AddNewDevice(const MountDevice& mount_device,
                    const MountPoint& mount_point,
                    base::SystemMonitor::DeviceIdType* device_id);

  // Remove a media device with a given device id.
  void RemoveOldDevice(const base::SystemMonitor::DeviceIdType& device_id);

  // Whether Init() has been called or not.
  bool initialized_;

  // Mtab file that lists the mount points.
  const FilePath mtab_path_;
  // Watcher for |mtab_path_|.
  base::files::FilePathWatcher file_watcher_;
  // Delegate to receive watcher notifications.
  scoped_refptr<WatcherDelegate> watcher_delegate_;

  // Mapping of relevent mount points and their corresponding mount devices.
  // Keep in mind on Linux, a device can be mounted at multiple mount points,
  // and multiple devices can be mounted at a mount point.
  MountMap mtab_;

  // The lowest available device id number.
  base::SystemMonitor::DeviceIdType current_device_id_;

  // Set of known file systems that we care about.
  std::set<std::string> known_file_systems_;

  DISALLOW_COPY_AND_ASSIGN(MediaDeviceNotificationsLinux);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_DEVICE_NOTIFICATIONS_LINUX_H_
