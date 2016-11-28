// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/device_info/device_info_data_type_controller.h"

#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"

namespace syncer {

DeviceInfoDataTypeController::DeviceInfoDataTypeController(
    const base::Closure& dump_stack,
    SyncClient* sync_client,
    LocalDeviceInfoProvider* local_device_info_provider)
    : NonUIDataTypeController(DEVICE_INFO,
                              dump_stack,
                              sync_client,
                              GROUP_UI,
                              base::ThreadTaskRunnerHandle::Get()),
      local_device_info_provider_(local_device_info_provider) {}

DeviceInfoDataTypeController::~DeviceInfoDataTypeController() {}

bool DeviceInfoDataTypeController::StartModels() {
  DCHECK(CalledOnValidThread());
  // Start the data type as soon as the local device info gets available.
  if (local_device_info_provider_->GetLocalDeviceInfo()) {
    return true;
  }

  subscription_ = local_device_info_provider_->RegisterOnInitializedCallback(
      base::Bind(&DeviceInfoDataTypeController::OnLocalDeviceInfoLoaded,
                 base::AsWeakPtr(this)));

  return false;
}

void DeviceInfoDataTypeController::StopModels() {
  DCHECK(CalledOnValidThread());
  subscription_.reset();
}

void DeviceInfoDataTypeController::OnLocalDeviceInfoLoaded() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state(), MODEL_STARTING);
  DCHECK(local_device_info_provider_->GetLocalDeviceInfo());

  subscription_.reset();
  OnModelLoaded();
}

}  // namespace syncer
