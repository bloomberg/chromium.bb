// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/volume_mounter/arc_volume_mounter_bridge.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/task_scheduler/post_task.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/arc/arc_bridge_service.h"

using chromeos::disks::DiskMountManager;

namespace arc {

namespace {

// Sends MountEvents of all existing MountPoints in cros-disks.
void SendAllMountEvents(ArcVolumeMounterBridge* bridge) {
  for (const auto& keyValue : DiskMountManager::GetInstance()->mount_points()) {
    bridge->OnMountEvent(DiskMountManager::MountEvent::MOUNTING,
                         chromeos::MountError::MOUNT_ERROR_NONE,
                         keyValue.second);
  }
}

}  // namespace

ArcVolumeMounterBridge::ArcVolumeMounterBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service) {
  arc_bridge_service()->volume_mounter()->AddObserver(this);
  DCHECK(DiskMountManager::GetInstance());
  DiskMountManager::GetInstance()->AddObserver(this);
}

ArcVolumeMounterBridge::~ArcVolumeMounterBridge() {
  DiskMountManager::GetInstance()->RemoveObserver(this);
  arc_bridge_service()->volume_mounter()->RemoveObserver(this);
}

void ArcVolumeMounterBridge::OnInstanceReady() {
  base::PostTaskWithTraits(FROM_HERE,
                           {base::TaskPriority::USER_BLOCKING,
                            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
                           base::BindOnce(&SendAllMountEvents, this));
}

void ArcVolumeMounterBridge::OnDiskEvent(
    chromeos::disks::DiskMountManager::DiskEvent event,
    const chromeos::disks::DiskMountManager::Disk* disk) {
  // Ignored. DiskEvents will be maintained in Vold during MountEvents.
}

void ArcVolumeMounterBridge::OnDeviceEvent(
    chromeos::disks::DiskMountManager::DeviceEvent event,
    const std::string& device_path) {
  // Ignored. ARC doesn't care about events other than Disk and Mount events.
}

void ArcVolumeMounterBridge::OnFormatEvent(
    chromeos::disks::DiskMountManager::FormatEvent event,
    chromeos::FormatError error_code,
    const std::string& device_path) {
  // Ignored. ARC doesn't care about events other than Disk and Mount events.
}

void ArcVolumeMounterBridge::OnMountEvent(
    DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const chromeos::disks::DiskMountManager::MountPointInfo& mount_info) {
  if (error_code != chromeos::MountError::MOUNT_ERROR_NONE) {
    DVLOG(1) << "Error " << error_code << "occurs during MountEvent " << event;
    return;
  }

  // Get disks informations that are needed by Android MountService.
  const chromeos::disks::DiskMountManager::Disk* disk =
      DiskMountManager::GetInstance()->FindDiskBySourcePath(
          mount_info.source_path);
  std::string fs_uuid, device_label;
  chromeos::DeviceType device_type = chromeos::DeviceType::DEVICE_TYPE_UNKNOWN;
  // There are several cases where disk can be null:
  // 1. The disk is removed physically before being ejected/unmounted.
  // 2. The disk is inserted, but then immediately removed physically. The
  //    disk removal will race with mount event in this case.
  if (disk) {
    fs_uuid = disk->fs_uuid();
    device_label = disk->device_label();
    device_type = disk->device_type();
  } else {
    DVLOG(1) << "Disk at " << mount_info.source_path
             << " is null during MountEvent " << event;
  }

  mojom::VolumeMounterInstance* volume_mounter_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service()->volume_mounter(),
                                  OnMountEvent);

  if (!volume_mounter_instance)
    return;

  volume_mounter_instance->OnMountEvent(mojom::MountPointInfo::New(
      event, mount_info.source_path, mount_info.mount_path, fs_uuid,
      device_label, device_type));
}

}  // namespace arc
