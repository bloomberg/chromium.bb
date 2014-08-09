// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DEVICE_INFO_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_DEVICE_INFO_DATA_TYPE_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/glue/local_device_info_provider.h"
#include "components/sync_driver/ui_data_type_controller.h"

namespace browser_sync {

// DataTypeController for DEVICE_INFO model type.
class DeviceInfoDataTypeController : public sync_driver::UIDataTypeController {
 public:
  DeviceInfoDataTypeController(
      sync_driver::SyncApiComponentFactory* sync_factory,
      LocalDeviceInfoProvider* local_device_info_provider);

 private:
  virtual ~DeviceInfoDataTypeController();

  // UIDataTypeController implementations.
  virtual bool StartModels() OVERRIDE;

  // Called by LocalDeviceInfoProvider when the local device into becomes
  // available.
  void OnLocalDeviceInfoLoaded();

  LocalDeviceInfoProvider* const local_device_info_provider_;
  scoped_ptr<LocalDeviceInfoProvider::Subscription> subscription_;
  DISALLOW_COPY_AND_ASSIGN(DeviceInfoDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DEVICE_INFO_DATA_TYPE_CONTROLLER_H_
