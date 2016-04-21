// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DEVICE_INFO_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_DEVICE_INFO_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "components/sync_driver/local_device_info_provider.h"
#include "components/sync_driver/ui_data_type_controller.h"

namespace sync_driver {

// DataTypeController for DEVICE_INFO model type.
class DeviceInfoDataTypeController : public UIDataTypeController {
 public:
  DeviceInfoDataTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const base::Closure& error_callback,
      SyncClient* sync_client,
      LocalDeviceInfoProvider* local_device_info_provider);

 private:
  ~DeviceInfoDataTypeController() override;

  // UIDataTypeController implementations.
  bool StartModels() override;
  void StopModels() override;

  // Called by LocalDeviceInfoProvider when the local device into becomes
  // available.
  void OnLocalDeviceInfoLoaded();

  LocalDeviceInfoProvider* const local_device_info_provider_;
  std::unique_ptr<LocalDeviceInfoProvider::Subscription> subscription_;
  DISALLOW_COPY_AND_ASSIGN(DeviceInfoDataTypeController);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_DEVICE_INFO_DATA_TYPE_CONTROLLER_H_
