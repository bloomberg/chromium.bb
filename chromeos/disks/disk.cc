// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/disks/disk.h"

namespace chromeos {
namespace disks {

namespace {
constexpr char kStatefulPartition[] = "/mnt/stateful_partition";
}

Disk::Disk(const DiskInfo& disk_info,
           bool write_disabled_by_policy,
           const std::string& system_path_prefix,
           const std::string& base_mount_path)
    : device_path_(disk_info.device_path()),
      mount_path_(disk_info.mount_path()),
      write_disabled_by_policy_(write_disabled_by_policy),
      system_path_(disk_info.system_path()),
      file_path_(disk_info.file_path()),
      device_label_(disk_info.label()),
      drive_label_(disk_info.drive_label()),
      vendor_id_(disk_info.vendor_id()),
      vendor_name_(disk_info.vendor_name()),
      product_id_(disk_info.product_id()),
      product_name_(disk_info.product_name()),
      fs_uuid_(disk_info.uuid()),
      system_path_prefix_(system_path_prefix),
      device_type_(disk_info.device_type()),
      total_size_in_bytes_(disk_info.total_size_in_bytes()),
      is_parent_(disk_info.is_drive()),
      is_read_only_hardware_(disk_info.is_read_only()),
      has_media_(disk_info.has_media()),
      on_boot_device_(disk_info.on_boot_device()),
      on_removable_device_(disk_info.on_removable_device()),
      is_hidden_(disk_info.is_hidden()),
      file_system_type_(disk_info.file_system_type()),
      base_mount_path_(base_mount_path) {}

Disk::Disk(const std::string& device_path,
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
           const std::string& base_mount_path)
    : device_path_(device_path),
      mount_path_(mount_path),
      write_disabled_by_policy_(write_disabled_by_policy),
      system_path_(system_path),
      file_path_(file_path),
      device_label_(device_label),
      drive_label_(drive_label),
      vendor_id_(vendor_id),
      vendor_name_(vendor_name),
      product_id_(product_id),
      product_name_(product_name),
      fs_uuid_(fs_uuid),
      system_path_prefix_(system_path_prefix),
      device_type_(device_type),
      total_size_in_bytes_(total_size_in_bytes),
      is_parent_(is_parent),
      is_read_only_hardware_(is_read_only_hardware),
      has_media_(has_media),
      on_boot_device_(on_boot_device),
      on_removable_device_(on_removable_device),
      is_hidden_(is_hidden),
      file_system_type_(file_system_type),
      base_mount_path_(base_mount_path) {}

Disk::Disk(const Disk&) = default;

Disk::~Disk() = default;

void Disk::SetMountPath(const std::string& mount_path) {
  mount_path_ = mount_path;

  if (base_mount_path_.empty())
    base_mount_path_ = mount_path;
}

bool Disk::IsAutoMountable() const {
  // Disks are considered auto-mountable if they are:
  // 1. Non-virtual
  // 2. Not on boot device
  // Only the second condition is checked here, because Disks are created from
  // non-virtual mount devices only.
  return !on_boot_device_;
}

bool Disk::IsStatefulPartition() const {
  return mount_path_ == kStatefulPartition;
}

}  // namespace disks
}  // namespace chromeos
