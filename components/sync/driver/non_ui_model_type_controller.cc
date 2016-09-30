// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/non_ui_model_type_controller.h"

namespace syncer {

NonUIModelTypeController::NonUIModelTypeController(
    ModelType type,
    const base::Closure& dump_stack,
    SyncClient* sync_client)
    : NonBlockingDataTypeController(type, dump_stack, sync_client) {}

NonUIModelTypeController::~NonUIModelTypeController() {}

}  // namespace syncer
