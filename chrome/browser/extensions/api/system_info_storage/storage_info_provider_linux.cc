// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider_linux.h"

#include <mntent.h>
#include <sys/vfs.h>

namespace extensions {

using api::experimental_system_info_storage::StorageUnitInfo;
using api::experimental_system_info_storage::ParseStorageUnitType;
using chrome::ScopedUdevDeviceObject;

namespace {

const char kMtabPath[] = "/etc/mtab";
const char kDevPath[] = "/dev/";

}  // namespace

StorageInfoProviderLinux::StorageInfoProviderLinux()
  : udev_context_(udev_new()),
    mtab_file_path_(kMtabPath) {
}

StorageInfoProviderLinux::~StorageInfoProviderLinux() {}

StorageInfoProviderLinux::StorageInfoProviderLinux(const FilePath& mtab_path)
  : udev_context_(udev_new()),
    mtab_file_path_(mtab_path) {
}

bool StorageInfoProviderLinux::QueryInfo(StorageInfo* info) {
  info->clear();
  FILE* fp = setmntent(mtab_file_path_.value().c_str(), "r");
  if (!fp) {
    DPLOG(INFO) << "Failed to open " << kMtabPath;
    return false;
  }
  struct mntent* p_mntent = NULL;
  while ((p_mntent = getmntent(fp))) {
    if (strncmp(p_mntent->mnt_fsname, kDevPath, strlen(kDevPath)) != 0)
      continue;
    linked_ptr<StorageUnitInfo> unit(new StorageUnitInfo());
    if (QueryUnitInfo(p_mntent->mnt_dir, unit.get())) {
        info->push_back(unit);
    }
  }
  endmntent(fp);
  return true;
}

bool StorageInfoProviderLinux::QueryUnitInfo(const std::string& mount_path,
                                             StorageUnitInfo* info) {
  std::string type;
  if (!QueryStorageType(mount_path, &type))
    return false;

  struct statfs fs_info;
  if (statfs(mount_path.c_str(), &fs_info) != 0) {
    DPLOG(INFO) << "Failed to get filesystem information about " << mount_path;
    return false;
  }
  // Currently the mount path is used as the identifier of a storage unit.
  info->id = mount_path;
  info->type = ParseStorageUnitType(type);
  info->capacity = static_cast<double>(fs_info.f_blocks) * fs_info.f_bsize;
  info->available_capacity =
    static_cast<double>(fs_info.f_bavail) * fs_info.f_bsize;

  return true;
}

bool StorageInfoProviderLinux::QueryStorageType(const std::string& mount_path,
                                                std::string* type) {
  struct stat stat_info;
  if (stat(mount_path.c_str(), &stat_info) != 0) {
    DPLOG(INFO) << "Failed to get information about " << mount_path;
    return false;
  }

  // Create a udev device from a block device number.
  ScopedUdevDeviceObject device(udev_device_new_from_devnum(
        udev_context_, 'b', stat_info.st_dev));
  if (!device)
    return false;
  if (GetUdevDevicePropertyValue(device, "ID_BUS") == "usb") {
    *type = systeminfo::kStorageTypeRemovable;
  } else if (GetUdevDevicePropertyValue(device, "ID_TYPE") == "disk") {
    *type = systeminfo::kStorageTypeHardDisk;
  } else {
    *type = systeminfo::kStorageTypeUnknown;
  }
  return true;
}

// static
StorageInfoProvider* StorageInfoProvider::Get() {
  return StorageInfoProvider::GetInstance<StorageInfoProviderLinux>();
}

}  // namespace extensions
