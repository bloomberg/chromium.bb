// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_NON_UI_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_NON_UI_MODEL_TYPE_CONTROLLER_H_

#include "components/sync_driver/non_blocking_data_type_controller.h"

namespace sync_driver {
class SyncClient;
}

namespace sync_driver_v2 {

// Implementation for Unified Sync and Storage datatypes whose model thread is
// not the UI thread.
// Derived types must implement the following methods:
// - RunOnModelThread
class NonUIModelTypeController : public NonBlockingDataTypeController {
 public:
  NonUIModelTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const base::Closure& error_callback,
      syncer::ModelType model_type,
      sync_driver::SyncClient* sync_client);

  void RunOnUIThread(const tracked_objects::Location& from_here,
                     const base::Closure& task) override;

 protected:
  ~NonUIModelTypeController() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonUIModelTypeController);
};

}  // namespace sync_driver_v2

#endif  // COMPONENTS_SYNC_DRIVER_NON_UI_MODEL_TYPE_CONTROLLER_H_
