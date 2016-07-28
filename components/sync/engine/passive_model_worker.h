// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_PASSIVE_MODEL_WORKER_H_
#define COMPONENTS_SYNC_ENGINE_PASSIVE_MODEL_WORKER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/sync/base/sync_export.h"
#include "components/sync/base/syncer_error.h"
#include "components/sync/engine/model_safe_worker.h"

namespace syncer {

// Implementation of ModelSafeWorker for passive types.  All work is
// done on the same thread DoWorkAndWaitUntilDone (i.e., the sync
// thread).
class SYNC_EXPORT PassiveModelWorker : public ModelSafeWorker {
 public:
  explicit PassiveModelWorker(WorkerLoopDestructionObserver* observer);

  // ModelSafeWorker implementation. Called on the sync thread.
  void RegisterForLoopDestruction() override;
  ModelSafeGroup GetModelSafeGroup() override;

 protected:
  SyncerError DoWorkAndWaitUntilDoneImpl(const WorkCallback& work) override;

 private:
  ~PassiveModelWorker() override;

  DISALLOW_COPY_AND_ASSIGN(PassiveModelWorker);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_PASSIVE_MODEL_WORKER_H_
