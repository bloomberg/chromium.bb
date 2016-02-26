// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/non_ui_model_type_controller.h"

#include "components/sync_driver/sync_client.h"
#include "sync/api/model_type_service.h"
#include "sync/internal_api/public/activation_context.h"
#include "sync/internal_api/public/base/model_type.h"

namespace sync_driver_v2 {

using sync_driver::SyncClient;

NonUIModelTypeController::NonUIModelTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const base::Closure& error_callback,
    syncer::ModelType model_type,
    SyncClient* sync_client)
    : NonBlockingDataTypeController(ui_thread,
                                    error_callback,
                                    model_type,
                                    sync_client) {}

NonUIModelTypeController::~NonUIModelTypeController() {}

void NonUIModelTypeController::RunOnUIThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK(!BelongsToUIThread());
  ui_thread()->PostTask(from_here, task);
}

}  // namespace sync_driver_v2
