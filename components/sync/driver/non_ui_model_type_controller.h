// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_NON_UI_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_NON_UI_MODEL_TYPE_CONTROLLER_H_

#include "components/sync/driver/non_blocking_data_type_controller.h"

namespace syncer {

class SyncClient;

// Implementation for Unified Sync and Storage datatypes whose model thread is
// not the UI thread.
// Derived types must implement the following methods:
// - RunOnModelThread
class NonUIModelTypeController : public NonBlockingDataTypeController {
 public:
  // |dump_stack| is called when an unrecoverable error occurs.
  NonUIModelTypeController(ModelType type,
                           const base::Closure& dump_stack,
                           SyncClient* sync_client);
  ~NonUIModelTypeController() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonUIModelTypeController);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_NON_UI_MODEL_TYPE_CONTROLLER_H_
