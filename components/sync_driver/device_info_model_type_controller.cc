// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/device_info_model_type_controller.h"

#include "components/sync_driver/sync_client.h"
#include "sync/api/model_type_service.h"
#include "sync/internal_api/public/base/model_type.h"

namespace sync_driver_v2 {

using sync_driver::LocalDeviceInfoProvider;
using sync_driver::SyncClient;

DeviceInfoModelTypeController::DeviceInfoModelTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const base::Closure& error_callback,
    SyncClient* sync_client,
    LocalDeviceInfoProvider* local_device_info_provider)
    : UIModelTypeController(ui_thread,
                            error_callback,
                            syncer::DEVICE_INFO,
                            sync_client),
      local_device_info_provider_(local_device_info_provider) {
}

DeviceInfoModelTypeController::~DeviceInfoModelTypeController() {}

}  // namespace sync_driver_v2
