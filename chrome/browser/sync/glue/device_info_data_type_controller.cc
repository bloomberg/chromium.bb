// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/device_info_data_type_controller.h"

#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/glue/local_device_info_provider.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace browser_sync {

DeviceInfoDataTypeController::DeviceInfoDataTypeController(
    sync_driver::SyncApiComponentFactory* sync_factory,
    LocalDeviceInfoProvider* local_device_info_provider)
    : UIDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          base::Bind(&ChromeReportUnrecoverableError),
          syncer::DEVICE_INFO,
          sync_factory),
      local_device_info_provider_(local_device_info_provider) {
}

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

void DeviceInfoDataTypeController::OnLocalDeviceInfoLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state_, MODEL_STARTING);
  DCHECK(local_device_info_provider_->GetLocalDeviceInfo());

  subscription_.reset();
  OnModelLoaded();
}

}  // namespace browser_sync
