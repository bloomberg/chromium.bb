// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/device_info_data_type_controller.h"

#include "base/callback.h"
#include "components/sync_driver/local_device_info_provider.h"

namespace sync_driver {

DeviceInfoDataTypeController::DeviceInfoDataTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const base::Closure& error_callback,
    SyncClient* sync_client,
    LocalDeviceInfoProvider* local_device_info_provider)
    : UIDataTypeController(ui_thread,
                           error_callback,
                           syncer::DEVICE_INFO,
                           sync_client),
      local_device_info_provider_(local_device_info_provider) {}

DeviceInfoDataTypeController::~DeviceInfoDataTypeController() {
}

bool DeviceInfoDataTypeController::StartModels() {
  // Start the data type as soon as the local device info gets available.
  if (local_device_info_provider_->GetLocalDeviceInfo()) {
    return true;
  }

  subscription_ = local_device_info_provider_->RegisterOnInitializedCallback(
      base::Bind(&DeviceInfoDataTypeController::OnLocalDeviceInfoLoaded, this));

  return false;
}

void DeviceInfoDataTypeController::StopModels() {
  subscription_.reset();
}

void DeviceInfoDataTypeController::OnLocalDeviceInfoLoaded() {
  DCHECK_EQ(state_, MODEL_STARTING);
  DCHECK(local_device_info_provider_->GetLocalDeviceInfo());

  subscription_.reset();
  OnModelLoaded();
}

}  // namespace sync_driver
