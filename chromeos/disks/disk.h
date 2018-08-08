// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DISKS_DISK_H_
#define CHROMEOS_DISKS_DISK_H_

#include <string>

#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/cros_disks_client.h"

namespace chromeos {
namespace disks {

class CHROMEOS_EXPORT Disk {
 public:
  Disk(const DiskInfo& disk_info,
       // Whether the device is mounted in read-only mode by the policy.
       // Valid only when the device mounted and mount_path_ is non-empty.
       bool write_disabled_by_policy,
       const std::string& system_path_prefix,
       const std::string& base_mount_path);

  // For tests.
  // TODO(amistry): Replace with a builder.
  Disk(const std::string& device_path,
       // The path to the mount point of this device. Empty if not mounted.
       // (e.g. /media/removable/VOLUME)
       const std::string& mount_path,
       bool write_disabled_by_policy,
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
       uint64_t total_size_in_bytes,
       bool is_parent,
       bool is_read_only_hardware,
       bool has_media,
       bool on_boot_device,
       bool on_removable_device,
       bool is_hidden,
       const std::string& file_system_type,
       const std::string& base_mount_path);
  Disk(const Disk&);

  ~Disk();

  // The path of the device, used by devicekit-disks.
  // (e.g. /sys/devices/pci0000:00/.../8:0:0:0/block/sdb/sdb1)
  const std::string& device_path() const { return device_path_; }

  // The path to the mount point of this device. Will be empty if not mounted.
  // (e.g. /media/removable/VOLUME)
  const std::string& mount_path() const { return mount_path_; }

  // The path of the device according to the udev system.
  // (e.g. /sys/devices/pci0000:00/.../8:0:0:0/block/sdb/sdb1)
  const std::string& system_path() const { return system_path_; }

  // The path of the device according to filesystem.
  // (e.g. /dev/sdb)
  const std::string& file_path() const { return file_path_; }

  // Device's label.
  const std::string& device_label() const { return device_label_; }

  void set_device_label(const std::string& device_label) {
    device_label_ = device_label;
  }

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
  const std::string& system_path_prefix() const { return system_path_prefix_; }

  // Device type.
  DeviceType device_type() const { return device_type_; }

  // Total size of the device in bytes.
  uint64_t total_size_in_bytes() const { return total_size_in_bytes_; }

  // Is the device is a parent device (i.e. sdb rather than sdb1).
  bool is_parent() const { return is_parent_; }

  // Whether the user can write to the device. True if read-only.
  bool is_read_only() const {
    return is_read_only_hardware_ || write_disabled_by_policy_;
  }

  // Is the device read only.
  bool is_read_only_hardware() const { return is_read_only_hardware_; }

  // Does the device contains media.
  bool has_media() const { return has_media_; }

  // Is the device on the boot device.
  bool on_boot_device() const { return on_boot_device_; }

  // Is the device on the removable device.
  bool on_removable_device() const { return on_removable_device_; }

  // Shoud the device be shown in the UI, or automounted.
  bool is_hidden() const { return is_hidden_; }

  // Is the disk auto-mountable.
  bool is_auto_mountable() const { return is_auto_mountable_; }

  void set_write_disabled_by_policy(bool disable) {
    write_disabled_by_policy_ = disable;
  }

  void clear_mount_path() { mount_path_.clear(); }

  bool is_mounted() const { return !mount_path_.empty(); }

  const std::string& file_system_type() const { return file_system_type_; }

  void set_file_system_type(const std::string& file_system_type) {
    file_system_type_ = file_system_type;
  }
  // Name of the first mount path of the disk.
  const std::string& base_mount_path() const { return base_mount_path_; }

  void SetMountPath(const std::string& mount_path);

  bool IsStatefulPartition() const;

 private:
  Disk() = delete;

  std::string device_path_;
  std::string mount_path_;
  bool write_disabled_by_policy_;
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
  uint64_t total_size_in_bytes_;
  bool is_parent_;
  bool is_read_only_hardware_;
  bool has_media_;
  bool on_boot_device_;
  bool on_removable_device_;
  bool is_hidden_;
  bool is_auto_mountable_ = false;
  std::string file_system_type_;
  std::string base_mount_path_;
};

}  // namespace disks
}  // namespace chromeos

#endif  // CHROMEOS_DISKS_DISK_H_
