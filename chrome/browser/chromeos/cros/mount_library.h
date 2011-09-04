// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOUNT_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOUNT_LIBRARY_H_
#pragma once

#include <map>
#include <string>

#include "base/memory/singleton.h"
#include "base/observer_list.h"
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
  MOUNT_DEVICE_SCANNED,
  MOUNT_FORMATTING_STARTED,
  MOUNT_FORMATTING_FINISHED
} MountLibraryEventType;

typedef enum MountCondition {
  MOUNT_CONDITION_NONE,
  MOUNT_CONDITION_UNKNOWN_FILESYSTEM,
  MOUNT_CONDITION_UNSUPPORTED_FILESYSTEM
} MountCondition;

class MountLibcrosProxy {
 public:
  virtual ~MountLibcrosProxy() {}
  virtual void CallMountPath(const char* source_path,
                             MountType type,
                             const MountPathOptions& options,
                             MountCompletedMonitor callback,
                             void* object) = 0;
  virtual void CallUnmountPath(const char* path,
                               UnmountRequestCallback callback,
                               void* object) = 0;
  virtual void CallRequestMountInfo(RequestMountInfoCallback callback,
                                    void* object) = 0;
  virtual void CallFormatDevice(const char* device_path,
                                const char* filesystem,
                                FormatRequestCallback callback,
                                void* object) = 0;
  virtual void CallGetDiskProperties(const char* device_path,
                                     GetDiskPropertiesCallback callback,
                                     void* object) = 0;
  virtual MountEventConnection MonitorCrosDisks(MountEventMonitor monitor,
      MountCompletedMonitor mount_complete_monitor,
      void* object) = 0;
  virtual void DisconnectCrosDisksMonitorIfSet(MountEventConnection connection)
      = 0;
};

// This class handles the interaction with the ChromeOS mount library APIs.
// Classes can add themselves as observers. Users can get an instance of this
// library class like this: chromeos::CrosLibrary::Get()->GetMountLibrary()
class MountLibrary {
 public:
  enum MountEvent {
    MOUNTING,
    UNMOUNTING
  };
  // Used to house an instance of each found mount device.
  class Disk {
   public:
    Disk(const std::string& device_path,
         const std::string& mount_path,
         const std::string& system_path,
         const std::string& file_path,
         const std::string& device_label,
         const std::string& drive_label,
         const std::string& parent_path,
         const std::string& system_path_prefix,
         DeviceType device_type,
         uint64 total_size,
         bool is_parent,
         bool is_read_only,
         bool has_media,
         bool on_boot_device);
    ~Disk();

    // The path of the device, used by devicekit-disks.
    const std::string& device_path() const { return device_path_; }
    // The path to the mount point of this device. Will be empty if not mounted.
    const std::string&  mount_path() const { return mount_path_; }
    // The path of the device according to the udev system.
    const std::string& system_path() const { return system_path_; }
    // The path of the device according to filesystem.
    const std::string& file_path() const { return file_path_; }
    // Device's label.
    const std::string& device_label() const { return device_label_; }
    // If disk is a parent, then its label, else parents label.
    const std::string& drive_label() const { return drive_label_; }
    // Parents device path. If device has no parent, then empty string.
    const std::string& parent_path() const { return parent_path_; }
    // Path of the system device this device's block is a part of.
    const std::string& system_path_prefix() const {
      return system_path_prefix_;
    }
    // Device type.
    DeviceType device_type() const { return device_type_; }
    // Total size of the device.
    uint64 total_size() const { return total_size_; }
    // Is the device is a parent device (i.e. sdb rather than sdb1).
    bool is_parent() const { return is_parent_; }
    // Is the device read only.
    bool is_read_only() const { return is_read_only_; }
    // Does the device contains media.
    bool has_media() const { return has_media_; }
    // Is the device on the boot device.
    bool on_boot_device() const { return on_boot_device_; }

    void set_mount_path(const char* mount_path) { mount_path_ = mount_path; }
    void clear_mount_path() { mount_path_.clear(); }

   private:
    std::string device_path_;
    std::string mount_path_;
    std::string system_path_;
    std::string file_path_;
    std::string device_label_;
    std::string drive_label_;
    std::string parent_path_;
    std::string system_path_prefix_;
    DeviceType device_type_;
    uint64 total_size_;
    bool is_parent_;
    bool is_read_only_;
    bool has_media_;
    bool on_boot_device_;
  };
  typedef std::map<std::string, Disk*> DiskMap;
  typedef std::map<std::string, std::string> PathMap;

  struct MountPointInfo {
    std::string source_path;
    std::string mount_path;
    MountType mount_type;
    MountCondition mount_condition;

    MountPointInfo(const char* source, const char* mount, const MountType type,
        MountCondition condition)
        : source_path(source ? source : ""),
          mount_path(mount ? mount : ""),
          mount_type(type),
          mount_condition(condition) {
    }
  };

  // MountPointMap key is mount_path.
  typedef std::map<std::string, MountPointInfo> MountPointMap;

  typedef void(*UnmountDeviceRecursiveCallbackType)(void*, bool);

  class Observer {
   public:
    virtual ~Observer() {}
    // Async API events.
    virtual void DiskChanged(MountLibraryEventType event,
                             const Disk* disk) = 0;
    virtual void DeviceChanged(MountLibraryEventType event,
                               const std::string& device_path) = 0;
    virtual void MountCompleted(MountEvent event_type,
                                MountError error_code,
                                const MountPointInfo& mount_info) = 0;
  };

  virtual ~MountLibrary() {}
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual const DiskMap& disks() const = 0;
  virtual const MountPointMap& mount_points() const = 0;

  virtual void RequestMountInfoRefresh() = 0;
  virtual void MountPath(const char* source_path,
                         MountType type,
                         const MountPathOptions& options) = 0;
  // |path| is device's mount path.
  virtual void UnmountPath(const char* path) = 0;

  // Formats device given its file path.
  // Example: file_path: /dev/sdb1
  virtual void FormatUnmountedDevice(const char* file_path) = 0;

  // Formats Device given its mount path. Unmount's the device
  // Example: mount_path: /media/VOLUME_LABEL
  virtual void FormatMountedDevice(const char* mount_path) = 0;

  // Unmounts device_poath and all of its known children.
  virtual void UnmountDeviceRecursive(const char* device_path,
      UnmountDeviceRecursiveCallbackType callback, void* user_data) = 0;

  // Helper functions for parameter conversions.
  static std::string MountTypeToString(MountType type);
  static MountType MountTypeFromString(const std::string& type_str);
  static std::string MountConditionToString(MountCondition type);

  // Used in testing. Enables mocking libcros.
  virtual void SetLibcrosProxy(MountLibcrosProxy* proxy) {}

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static MountLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOUNT_LIBRARY_H_
