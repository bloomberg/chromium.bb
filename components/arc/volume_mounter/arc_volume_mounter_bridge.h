// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_VOLUME_MOUNTER_ARC_VOLUME_MOUNTER_BRIDGE_H_
#define COMPONENTS_ARC_VOLUME_MOUNTER_ARC_VOLUME_MOUNTER_BRIDGE_H_

#include <string>

#include "base/macros.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/volume_mounter.mojom.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class ArcBridgeService;

// This class handles Volume mount/unmount requests from cros-disks and
// send them to Android.
class ArcVolumeMounterBridge
    : public ArcService,
      public chromeos::disks::DiskMountManager::Observer,
      public InstanceHolder<mojom::VolumeMounterInstance>::Observer {
 public:
  explicit ArcVolumeMounterBridge(ArcBridgeService* bridge_service);
  ~ArcVolumeMounterBridge() override;

  // InstanceHolder<mojom::VolumeMounterInstance>::Observer overrides:
  void OnInstanceReady() override;

  // chromeos::disks::DiskMountManager::Observer overrides:
  void OnDiskEvent(
      chromeos::disks::DiskMountManager::DiskEvent event,
      const chromeos::disks::DiskMountManager::Disk* disk) override;
  void OnDeviceEvent(chromeos::disks::DiskMountManager::DeviceEvent event,
                     const std::string& device_path) override;
  void OnMountEvent(chromeos::disks::DiskMountManager::MountEvent event,
                    chromeos::MountError error_code,
                    const chromeos::disks::DiskMountManager::MountPointInfo&
                        mount_info) override;
  void OnFormatEvent(chromeos::disks::DiskMountManager::FormatEvent event,
                     chromeos::FormatError error_code,
                     const std::string& device_path) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcVolumeMounterBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_VOLUME_MOUNTER_ARC_VOLUME_MOUNTER_BRIDGE_H_
