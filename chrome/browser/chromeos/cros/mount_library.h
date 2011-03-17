// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOUNT_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOUNT_LIBRARY_H_
#pragma once

#include <string>
#include <map>

#include "base/observer_list.h"
#include "base/singleton.h"
#include "base/time.h"
#include "third_party/cros/chromeos_mount.h"

namespace chromeos {

typedef enum MountLibraryEventType {
  MOUNT_DISK_ADDED,
  MOUNT_DISK_REMOVED,
  MOUNT_DISK_CHANGED,
  MOUNT_DISK_MOUNTED,
  MOUNT_DISK_UNMOUNTED,
  MOUNT_DEVICE_ADDED,
  MOUNT_DEVICE_REMOVED,
  MOUNT_DEVICE_SCANNED
} MountLibraryEventType;

// This class handles the interaction with the ChromeOS mount library APIs.
// Classes can add themselves as observers. Users can get an instance of this
// library class like this: chromeos::CrosLibrary::Get()->GetMountLibrary()
class MountLibrary {
 public:
  // Used to house an instance of each found mount device.
  class Disk {
   public:
    Disk(const std::string& device_path,
         const std::string& mount_path,
         const std::string& system_path,
         bool is_parent,
         bool has_media,
         bool on_boot_device)
        : device_path_(device_path),
          mount_path_(mount_path),
          system_path_(system_path),
          is_parent_(is_parent),
          has_media_(has_media),
          on_boot_device_(on_boot_device) {}
    // The path of the device, used by devicekit-disks.
    const std::string& device_path() const { return device_path_; }
    // The path to the mount point of this device. Will be empty if not mounted.
    const std::string&  mount_path() const { return mount_path_; }
    // The path of the device according to the udev system.
    const std::string& system_path() const { return system_path_; }
    // Is the device is a parent device (i.e. sdb rather than sdb1)
    bool is_parent() const { return is_parent_; }
    // Does the device contains media.
    bool has_media() const { return has_media_; }
    // Is the device ont
    bool on_boot_device() const { return on_boot_device_; }

    void set_mount_path(const char* mount_path) { mount_path_ = mount_path; }
    void clear_mount_path() { mount_path_.clear(); }
   private:
    std::string device_path_;
    std::string mount_path_;
    std::string system_path_;
    bool is_parent_;
    bool has_media_;
    bool on_boot_device_;
  };
  typedef std::map<std::string, Disk*> DiskMap;

  class Observer {
   public:
    virtual ~Observer() {}
    // Async API events.
    virtual void DiskChanged(MountLibraryEventType event,
                             const Disk* disk) = 0;
    virtual void DeviceChanged(MountLibraryEventType event,
                               const std::string& device_path ) = 0;
  };

  virtual ~MountLibrary() {}
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual const DiskMap& disks() const = 0;

  virtual void RequestMountInfoRefresh() = 0;
  virtual void MountPath(const char* device_path) = 0;
  virtual void UnmountPath(const char* device_path) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static MountLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOUNT_LIBRARY_H_
