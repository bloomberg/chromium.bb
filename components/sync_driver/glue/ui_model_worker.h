// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GLUE_UI_MODEL_WORKER_H_
#define COMPONENTS_SYNC_DRIVER_GLUE_UI_MODEL_WORKER_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/util/unrecoverable_error_info.h"

namespace browser_sync {

// A syncer::ModelSafeWorker for UI models (e.g. bookmarks) that
// accepts work requests from the syncapi that need to be fulfilled
// from the MessageLoop home to the native model.
class UIModelWorker : public syncer::ModelSafeWorker {
 public:
  UIModelWorker(const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
                syncer::WorkerLoopDestructionObserver* observer);

  // syncer::ModelSafeWorker implementation. Called on syncapi SyncerThread.
  void RegisterForLoopDestruction() override;
  syncer::ModelSafeGroup GetModelSafeGroup() override;

 protected:
  syncer::SyncerError DoWorkAndWaitUntilDoneImpl(
      const syncer::WorkCallback& work) override;

 private:
  ~UIModelWorker() override;

  // A reference to the UI thread's task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(UIModelWorker);
};

}  // namespace browser_sync

#endif  // COMPONENTS_SYNC_DRIVER_GLUE_UI_MODEL_WORKER_H_
