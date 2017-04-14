// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/passive_model_worker.h"

#include "base/callback.h"

namespace syncer {

PassiveModelWorker::PassiveModelWorker() = default;

PassiveModelWorker::~PassiveModelWorker() {}

SyncerError PassiveModelWorker::DoWorkAndWaitUntilDoneImpl(
    const WorkCallback& work) {
  // Simply do the work on the current thread.
  return work.Run();
}

ModelSafeGroup PassiveModelWorker::GetModelSafeGroup() {
  return GROUP_PASSIVE;
}

bool PassiveModelWorker::IsOnModelThread() {
  // Passive types are checked by SyncBackendRegistrar.
  NOTREACHED();
  return false;
}

}  // namespace syncer
