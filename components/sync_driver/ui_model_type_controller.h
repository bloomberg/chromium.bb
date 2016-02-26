// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_UI_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_UI_MODEL_TYPE_CONTROLLER_H_

#include "components/sync_driver/non_blocking_data_type_controller.h"

namespace sync_driver {
class SyncClient;
}

namespace sync_driver_v2 {

// Implementation for Unified Sync and Storage datatypes that reside on the UI
// thread.
class UIModelTypeController : public NonBlockingDataTypeController {
 public:
  UIModelTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const base::Closure& error_callback,
      syncer::ModelType model_type,
      sync_driver::SyncClient* sync_client);

 protected:
  ~UIModelTypeController() override;

  void RunOnUIThread(const tracked_objects::Location& from_here,
                     const base::Closure& task) override;

 private:
  // NonBlockingDataTypeController implementations.
  // Since this is UI model type controller, we hide this function here.
  bool RunOnModelThread(const tracked_objects::Location& from_here,
                        const base::Closure& task) override;

  DISALLOW_COPY_AND_ASSIGN(UIModelTypeController);
};

}  // namespace sync_driver_v2

#endif  // COMPONENTS_SYNC_DRIVER_UI_MODEL_TYPE_CONTROLLER_H_
