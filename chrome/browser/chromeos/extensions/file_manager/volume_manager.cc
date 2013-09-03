// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/volume_manager.h"

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

namespace file_manager {
namespace {

// Factory to produce VolumeManager.
class VolumeManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns VolumeManager instance.
  static VolumeManager* Get(content::BrowserContext* context) {
    return static_cast<VolumeManager*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  }

  static VolumeManagerFactory* GetInstance() {
    return Singleton<VolumeManagerFactory>::get();
  }

 protected:
  // BrowserContextKeyedBaseFactory overrides:
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE {
    // Explicitly allow this manager in guest login mode.
    return chrome::GetBrowserContextOwnInstanceInIncognito(context);
  }

  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE {
    return true;
  }

  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE {
    return true;
  }

  // BrowserContextKeyedServiceFactory overrides:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE {
    return new VolumeManager();
  }

 private:
  // For Singleton.
  friend struct DefaultSingletonTraits<VolumeManagerFactory>;
  VolumeManagerFactory()
      : BrowserContextKeyedServiceFactory(
            "VolumeManagerFactory",
            BrowserContextDependencyManager::GetInstance()) {
  }

  virtual ~VolumeManagerFactory() {}

  DISALLOW_COPY_AND_ASSIGN(VolumeManagerFactory);
};

VolumeManager::VolumeType MountTypeToVolumeType(
    chromeos::MountType type) {
  switch (type) {
    case chromeos::MOUNT_TYPE_DEVICE:
      return VolumeManager::VOLUME_TYPE_REMOVABLE_DISK_PARTITION;
    case chromeos::MOUNT_TYPE_ARCHIVE:
      return VolumeManager::VOLUME_TYPE_MOUNTED_ARCHIVE_FILE;
    default:
      NOTREACHED();
  }

  return VolumeManager::VOLUME_TYPE_DOWNLOADS_DIRECTORY;
}

VolumeManager::VolumeInfo CreateDownloadsVolumeInfo(
    const base::FilePath& downloads_path) {
  VolumeManager::VolumeInfo volume_info;
  volume_info.type = VolumeManager::VOLUME_TYPE_DOWNLOADS_DIRECTORY;
  // Keep source_path empty.
  volume_info.mount_path = downloads_path;
  volume_info.mount_condition = chromeos::disks::MOUNT_CONDITION_NONE;
  return volume_info;
}

VolumeManager::VolumeInfo CreateVolumeInfoFromMountPointInfo(
    const chromeos::disks::DiskMountManager::MountPointInfo& mount_point) {
  VolumeManager::VolumeInfo volume_info;
  volume_info.type = MountTypeToVolumeType(mount_point.mount_type);
  volume_info.source_path = base::FilePath(mount_point.source_path);
  volume_info.mount_path = base::FilePath(mount_point.mount_path);
  volume_info.mount_condition = mount_point.mount_condition;
  return volume_info;
}

}  // namespace

VolumeManager::VolumeManager() {
}

VolumeManager::~VolumeManager() {
}

VolumeManager* VolumeManager::Get(content::BrowserContext* context) {
  return VolumeManagerFactory::Get(context);
}

std::vector<VolumeManager::VolumeInfo>
VolumeManager::GetVolumeInfoList() const {
  std::vector<VolumeManager::VolumeInfo> result;

  // TODO(hidehiko): Adds Drive if available.

  // Adds "Downloads".
  base::FilePath home_path;
  if (PathService::Get(base::DIR_HOME, &home_path)) {
    result.push_back(
        CreateDownloadsVolumeInfo(home_path.AppendASCII("Downloads")));
  }

  // Adds disks (both removable disks and zip archives).
  chromeos::disks::DiskMountManager* disk_mount_manager =
      chromeos::disks::DiskMountManager::GetInstance();
  const chromeos::disks::DiskMountManager::MountPointMap& mount_points =
      disk_mount_manager->mount_points();
  for (chromeos::disks::DiskMountManager::MountPointMap::const_iterator it =
           mount_points.begin();
       it != mount_points.end(); ++it) {
    if (it->second.mount_type == chromeos::MOUNT_TYPE_DEVICE ||
        it->second.mount_type == chromeos::MOUNT_TYPE_ARCHIVE)
      result.push_back(CreateVolumeInfoFromMountPointInfo(it->second));
  }

  return result;
}

}  // namespace file_manager
