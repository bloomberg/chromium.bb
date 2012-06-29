// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaDeviceNotificationsLinux implementation.

#include "chrome/browser/media_gallery/media_device_notifications_linux.h"

#include <mntent.h>
#include <stdio.h>

#include <vector>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/system_monitor/system_monitor.h"
#include "chrome/browser/media_gallery/media_device_notifications_utils.h"

namespace {

// List of file systems we care about.
const char* const kKnownFileSystems[] = {
  "ext2",
  "ext3",
  "ext4",
  "fat",
  "hfsplus",
  "iso9660",
  "msdos",
  "ntfs",
  "udf",
  "vfat",
};

}  // namespace

namespace chrome {

using base::SystemMonitor;
using content::BrowserThread;

MediaDeviceNotificationsLinux::MediaDeviceNotificationsLinux(
    const FilePath& path)
    : initialized_(false),
      mtab_path_(path),
      current_device_id_(0U) {
  CHECK(!path.empty());

  // Put |kKnownFileSystems| in std::set to get O(log N) access time.
  for (size_t i = 0; i < arraysize(kKnownFileSystems); ++i) {
    known_file_systems_.insert(kKnownFileSystems[i]);
  }
}

MediaDeviceNotificationsLinux::~MediaDeviceNotificationsLinux() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

void MediaDeviceNotificationsLinux::Init() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MediaDeviceNotificationsLinux::InitOnFileThread, this));
}

void MediaDeviceNotificationsLinux::OnFilePathChanged(const FilePath& path,
                                                      bool error) {
  if (path != mtab_path_) {
    // This cannot happen unless FilePathWatcher is buggy. Just ignore this
    // notification and do nothing.
    NOTREACHED();
    return;
  }
  if (error) {
    LOG(ERROR) << "Error watching " << mtab_path_.value();
    return;
  }

  UpdateMtab();
}

void MediaDeviceNotificationsLinux::InitOnFileThread() {
  DCHECK(!initialized_);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  initialized_ = true;

  // The callback passed to Watch() has to be unretained. Otherwise
  // MediaDeviceNotificationsLinux will live longer than expected, and
  // FilePathWatcher will get in trouble at shutdown time.
  bool ret = file_watcher_.Watch(
      mtab_path_,
      base::Bind(&MediaDeviceNotificationsLinux::OnFilePathChanged,
                 base::Unretained(this)));
  if (!ret) {
    LOG(ERROR) << "Adding watch for " << mtab_path_.value() << " failed";
    return;
  }

  UpdateMtab();
}

void MediaDeviceNotificationsLinux::UpdateMtab() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  MountMap new_mtab;
  ReadMtab(&new_mtab);

  // Check existing mtab entries for unaccounted mount points.
  // These mount points must have been removed in the new mtab.
  std::vector<std::string> mount_points_to_erase;
  for (MountMap::const_iterator it = mtab_.begin(); it != mtab_.end(); ++it) {
    const std::string& mount_point = it->first;
    // |mount_point| not in |new_mtab|.
    if (new_mtab.find(mount_point) == new_mtab.end()) {
      const SystemMonitor::DeviceIdType& device_id = it->second.second;
      RemoveOldDevice(device_id);
      mount_points_to_erase.push_back(mount_point);
    }
  }
  // Erase the |mtab_| entries afterwards. Erasing in the loop above using the
  // iterator is slightly more efficient, but more tricky, since calling
  // std::map::erase() on an iterator invalidates it.
  for (size_t i = 0; i < mount_points_to_erase.size(); ++i)
    mtab_.erase(mount_points_to_erase[i]);

  // Check new mtab entries against existing ones.
  for (MountMap::iterator newiter = new_mtab.begin();
       newiter != new_mtab.end();
       ++newiter) {
    const std::string& mount_point = newiter->first;
    const MountDeviceAndId& mount_device_and_id = newiter->second;
    const std::string& mount_device = mount_device_and_id.first;
    SystemMonitor::DeviceIdType& id = newiter->second.second;
    MountMap::iterator olditer = mtab_.find(mount_point);
    // Check to see if it is a new mount point.
    if (olditer == mtab_.end()) {
      if (IsMediaDevice(mount_point)) {
        AddNewDevice(mount_device, mount_point, &id);
        mtab_.insert(std::make_pair(mount_point, mount_device_and_id));
      }
      continue;
    }

    // Existing mount point. Check to see if a new device is mounted there.
    const MountDeviceAndId& old_mount_device_and_id = olditer->second;
    if (mount_device == old_mount_device_and_id.first)
      continue;

    // New device mounted.
    RemoveOldDevice(old_mount_device_and_id.second);
    if (IsMediaDevice(mount_point)) {
      AddNewDevice(mount_device, mount_point, &id);
      olditer->second = mount_device_and_id;
    }
  }
}

void MediaDeviceNotificationsLinux::ReadMtab(MountMap* mtab) {
  FILE* fp = setmntent(mtab_path_.value().c_str(), "r");
  if (!fp)
    return;

  MountMap& new_mtab = *mtab;
  mntent entry;
  char buf[512];
  SystemMonitor::DeviceIdType mount_position = 0;
  typedef std::pair<std::string, SystemMonitor::DeviceIdType> MountPointAndId;
  typedef std::map<std::string, MountPointAndId> DeviceMap;
  DeviceMap device_map;
  while (getmntent_r(fp, &entry, buf, sizeof(buf))) {
    // We only care about real file systems.
    if (known_file_systems_.find(entry.mnt_type) == known_file_systems_.end())
      continue;
    // Add entries, but overwrite entries for the same mount device. Keep track
    // of the entry positions in the device id field and use that below to
    // resolve multiple devices mounted at the same mount point.
    MountPointAndId mount_point_and_id =
        std::make_pair(entry.mnt_dir, mount_position++);
    DeviceMap::iterator it = device_map.find(entry.mnt_fsname);
    if (it == device_map.end()) {
      device_map.insert(it,
                        std::make_pair(entry.mnt_fsname, mount_point_and_id));
    } else {
      it->second = mount_point_and_id;
    }
  }
  endmntent(fp);

  for (DeviceMap::const_iterator device_it = device_map.begin();
       device_it != device_map.end();
       ++device_it) {
    const std::string& device = device_it->first;
    const std::string& mount_point = device_it->second.first;
    const SystemMonitor::DeviceIdType& position = device_it->second.second;

    // No device at |mount_point|, save |device| to it.
    MountMap::iterator mount_it = new_mtab.find(mount_point);
    if (mount_it == new_mtab.end()) {
      new_mtab.insert(std::make_pair(mount_point,
                                     std::make_pair(device, position)));
      continue;
    }

    // There is already a device mounted at |mount_point|. Check to see if
    // the existing mount entry is newer than the current one.
    std::string& existing_device = mount_it->second.first;
    SystemMonitor::DeviceIdType& existing_position = mount_it->second.second;
    if (existing_position > position)
      continue;

    // The current entry is newer, update the mount point entry.
    existing_device = device;
    existing_position = position;
  }
}

void MediaDeviceNotificationsLinux::AddNewDevice(
    const std::string& mount_device,
    const std::string& mount_point,
    base::SystemMonitor::DeviceIdType* device_id) {
  *device_id = current_device_id_++;
  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();
  system_monitor->ProcessMediaDeviceAttached(*device_id,
                                             mount_device,
                                             FilePath(mount_point));
}

void MediaDeviceNotificationsLinux::RemoveOldDevice(
    const base::SystemMonitor::DeviceIdType& device_id) {
  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();
  system_monitor->ProcessMediaDeviceDetached(device_id);
}

}  // namespace chrome
