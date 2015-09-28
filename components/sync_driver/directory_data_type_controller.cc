// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/directory_data_type_controller.h"

#include "components/sync_driver/backend_data_type_configurer.h"

namespace sync_driver {

DirectoryDataTypeController::DirectoryDataTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const base::Closure& error_callback)
    : DataTypeController(ui_thread, error_callback) {}

DirectoryDataTypeController::~DirectoryDataTypeController() {}

void DirectoryDataTypeController::ActivateDataType(
    BackendDataTypeConfigurer* configurer) {
  // Tell the backend about the change processor for this type so it can
  // begin routing changes to it.
  configurer->ActivateDirectoryDataType(type(), model_safe_group(),
                                        GetChangeProcessor());
}

void DirectoryDataTypeController::DeactivateDataType(
    BackendDataTypeConfigurer* configurer) {
  configurer->DeactivateDirectoryDataType(type());
}

}  // namespace sync_driver
