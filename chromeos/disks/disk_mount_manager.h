// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DISKS_DISK_MOUNT_MANAGER_H_
#define CHROMEOS_DISKS_DISK_MOUNT_MANAGER_H_

#include <map>

#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/cros_disks_client.h"

namespace chromeos {
namespace disks {

// Types of events DiskMountManager sends to its observers.
enum DiskMountManagerEventType {
  MOUNT_DISK_ADDED,
  MOUNT_DISK_REMOVED,
  MOUNT_DISK_CHANGED,
  MOUNT_DISK_MOUNTED,
  MOUNT_DISK_UNMOUNTED,
  MOUNT_DEVICE_ADDED,
  MOUNT_DEVICE_REMOVED,
  MOUNT_DEVICE_SCANNED,
  MOUNT_FORMATTING_STARTED,
  MOUNT_FORMATTING_FINISHED,
};

// Condition of mounted filesystem.
enum MountCondition {
  MOUNT_CONDITION_NONE,
  MOUNT_CONDITION_UNKNOWN_FILESYSTEM,
  MOUNT_CONDITION_UNSUPPORTED_FILESYSTEM,
};

// This class handles the interaction with cros-disks.
// Other classes can add themselves as observers.
class CHROMEOS_EXPORT DiskMountManager {
 public:
  // Event type given to observers' MountCompleted method.
  enum MountEvent {
    MOUNTING,
    UNMOUNTING,
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
         const std::string& vendor_id,
         const std::string& vendor_name,
         const std::string& product_id,
         const std::string& product_name,
         const std::string& fs_uuid,
         const std::string& system_path_prefix,
         DeviceType device_type,
         uint64 total_size_in_bytes,
         bool is_parent,
         bool is_read_only,
         bool has_media,
         bool on_boot_device,
         bool is_hidden);
    ~Disk();

    // The path of the device, used by devicekit-disks.
    // (e.g. /sys/devices/pci0000:00/.../8:0:0:0/block/sdb/sdb1)
    const std::string& device_path() const { return device_path_; }

    // The path to the mount point of this device. Will be empty if not mounted.
    // (e.g. /media/removable/VOLUME)
    const std::string&  mount_path() const { return mount_path_; }

    // The path of the device according to the udev system.
    // (e.g. /sys/devices/pci0000:00/.../8:0:0:0/block/sdb/sdb1)
    const std::string& system_path() const { return system_path_; }

    // The path of the device according to filesystem.
    // (e.g. /dev/sdb)
    const std::string& file_path() const { return file_path_; }

    // Device's label.
    const std::string& device_label() const { return device_label_; }

    // If disk is a parent, then its label, else parents label.
    // (e.g. "TransMemory")
    const std::string& drive_label() const { return drive_label_; }

    // Vendor ID of the device (e.g. "18d1").
    const std::string& vendor_id() const { return vendor_id_; }

    // Vendor name of the device (e.g. "Google Inc.").
    const std::string& vendor_name() const { return vendor_name_; }

    // Product ID of the device (e.g. "4e11").
    const std::string& product_id() const { return product_id_; }

    // Product name of the device (e.g. "Nexus One").
    const std::string& product_name() const { return product_name_; }

    // Returns the file system uuid string.
    const std::string& fs_uuid() const { return fs_uuid_; }

    // Path of the system device this device's block is a part of.
    // (e.g. /sys/devices/pci0000:00/.../8:0:0:0/)
    const std::string& system_path_prefix() const {
      return system_path_prefix_;
    }

    // Device type.
    DeviceType device_type() const { return device_type_; }

    // Total size of the device in bytes.
    uint64 total_size_in_bytes() const { return total_size_in_bytes_; }

    // Is the device is a parent device (i.e. sdb rather than sdb1).
    bool is_parent() const { return is_parent_; }

    // Is the device read only.
    bool is_read_only() const { return is_read_only_; }

    // Does the device contains media.
    bool has_media() const { return has_media_; }

    // Is the device on the boot device.
    bool on_boot_device() const { return on_boot_device_; }

    // Shoud the device be shown in the UI, or automounted.
    bool is_hidden() const { return is_hidden_; }

    void set_mount_path(const std::string& mount_path) {
      mount_path_ = mount_path;
    }

    void clear_mount_path() { mount_path_.clear(); }

   private:
    std::string device_path_;
    std::string mount_path_;
    std::string system_path_;
    std::string file_path_;
    std::string device_label_;
    std::string drive_label_;
    std::string vendor_id_;
    std::string vendor_name_;
    std::string product_id_;
    std::string product_name_;
    std::string fs_uuid_;
    std::string system_path_prefix_;
    DeviceType device_type_;
    uint64 total_size_in_bytes_;
    bool is_parent_;
    bool is_read_only_;
    bool has_media_;
    bool on_boot_device_;
    bool is_hidden_;
  };
  typedef std::map<std::string, Disk*> DiskMap;

  // A struct to store information about mount point.
  struct MountPointInfo {
    // Device's path.
    std::string source_path;
    // Mounted path.
    std::string mount_path;
    // Type of mount.
    MountType mount_type;
    // Condition of mount.
    MountCondition mount_condition;

    MountPointInfo(const std::string& source,
                   const std::string& mount,
                   const MountType type,
                   MountCondition condition)
        : source_path(source),
          mount_path(mount),
          mount_type(type),
          mount_condition(condition) {
    }
  };

  // MountPointMap key is mount_path.
  typedef std::map<std::string, MountPointInfo> MountPointMap;

  // A callback function type which is called after UnmountDeviceRecursive
  // finishes.
  typedef void(*UnmountDeviceRecursiveCallbackType)(void*, bool);

  // Implement this interface to be notified about disk/mount related events.
  class Observer {
   public:
    virtual ~Observer() {}

    // A function called when disk mount status is changed.
    virtual void DiskChanged(DiskMountManagerEventType event,
                             const Disk* disk) = 0;
    // A function called when device status is changed.
    virtual void DeviceChanged(DiskMountManagerEventType event,
                               const std::string& device_path) = 0;
    // A function called after mount is completed.
    virtual void MountCompleted(MountEvent event_type,
                                MountError error_code,
                                const MountPointInfo& mount_info) = 0;
  };

  virtual ~DiskMountManager() {}

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Gets the list of disks found.
  virtual const DiskMap& disks() const = 0;

  // Returns Disk object corresponding to |source_path| or NULL on failure.
  virtual const Disk* FindDiskBySourcePath(
      const std::string& source_path) const = 0;

  // Gets the list of mount points.
  virtual const MountPointMap& mount_points() const = 0;

  // Requests refreshing all the information about mounted disks.
  virtual void RequestMountInfoRefresh() = 0;

  // Mounts a device.
  virtual void MountPath(const std::string& source_path,
                         const std::string& source_format,
                         const std::string& mount_label,
                         MountType type) = 0;

  // Unmounts a mounted disk.
  virtual void UnmountPath(const std::string& mount_path) = 0;

  // Formats device given its file path.
  // Example: file_path: /dev/sdb1
  virtual void FormatUnmountedDevice(const std::string& file_path) = 0;

  // Formats Device given its mount path. Unmounts the device.
  // Example: mount_path: /media/VOLUME_LABEL
  virtual void FormatMountedDevice(const std::string& mount_path) = 0;

  // Unmounts device_path and all of its known children.
  virtual void UnmountDeviceRecursive(
      const std::string& device_path,
      UnmountDeviceRecursiveCallbackType callback,
      void* user_data) = 0;

  // Returns corresponding string to |type| like "device" or "file".
  static std::string MountTypeToString(MountType type);

  // The inverse function of MountTypeToString.
  static MountType MountTypeFromString(const std::string& type_str);

  // Returns corresponding string to |type| like "unknown_filesystem".
  static std::string MountConditionToString(MountCondition type);

  // Returns corresponding string to |type|, like "sd", "usb".
  static std::string DeviceTypeToString(DeviceType type);

  // Creates the global DiskMountManager instance.
  static void Initialize();

  // Similar to Initialize(), but can inject an alternative
  // DiskMountManager such as MockDiskMountManager for testing.
  // The injected object will be owned by the internal pointer and deleted
  // by Shutdown().
  static void InitializeForTesting(DiskMountManager* disk_mount_manager);

  // Destroys the global DiskMountManager instance if it exists.
  static void Shutdown();

  // Returns a pointer to the global DiskMountManager instance.
  // Initialize() should already have been called.
  static DiskMountManager* GetInstance();
};

}  // namespace disks
}  // namespace chromeos

#endif  // CHROMEOS_DISKS_DISK_MOUNT_MANAGER_H_
