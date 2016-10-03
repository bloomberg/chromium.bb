// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/ui_model_type_controller.h"

#include "components/sync/api/model_type_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/core/activation_context.h"
#include "components/sync/driver/sync_client.h"

namespace sync_driver_v2 {

using sync_driver::SyncClient;

UIModelTypeController::UIModelTypeController(syncer::ModelType type,
                                             const base::Closure& dump_stack,
                                             SyncClient* sync_client)
    : NonBlockingDataTypeController(type, dump_stack, sync_client) {}

UIModelTypeController::~UIModelTypeController() {}

bool UIModelTypeController::RunOnModelThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK(CalledOnValidThread());
  task.Run();
  return true;
}

}  // namespace sync_driver_v2
