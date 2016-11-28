// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "components/sync/device_info/local_device_info_provider.h"
#include "components/sync/driver/non_ui_data_type_controller.h"

namespace syncer {

// DataTypeController for DEVICE_INFO model type.
class DeviceInfoDataTypeController : public NonUIDataTypeController {
 public:
  // |dump_stack| is called when an unrecoverable error occurs.
  DeviceInfoDataTypeController(
      const base::Closure& dump_stack,
      SyncClient* sync_client,
      LocalDeviceInfoProvider* local_device_info_provider);
  ~DeviceInfoDataTypeController() override;

 private:
  // NonUIDataTypeController implementation.
  bool StartModels() override;
  void StopModels() override;

  // Called by LocalDeviceInfoProvider when the local device into becomes
  // available.
  void OnLocalDeviceInfoLoaded();

  LocalDeviceInfoProvider* const local_device_info_provider_;
  std::unique_ptr<LocalDeviceInfoProvider::Subscription> subscription_;
  DISALLOW_COPY_AND_ASSIGN(DeviceInfoDataTypeController);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_DATA_TYPE_CONTROLLER_H_
