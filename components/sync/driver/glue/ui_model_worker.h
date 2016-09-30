// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GLUE_UI_MODEL_WORKER_H_
#define COMPONENTS_SYNC_DRIVER_GLUE_UI_MODEL_WORKER_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "components/sync/base/unrecoverable_error_info.h"
#include "components/sync/engine/model_safe_worker.h"

namespace syncer {

// A ModelSafeWorker for UI models (e.g. bookmarks) that
// accepts work requests from the syncapi that need to be fulfilled
// from the MessageLoop home to the native model.
class UIModelWorker : public ModelSafeWorker {
 public:
  UIModelWorker(const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
                WorkerLoopDestructionObserver* observer);

  // ModelSafeWorker implementation. Called on syncapi SyncerThread.
  void RegisterForLoopDestruction() override;
  ModelSafeGroup GetModelSafeGroup() override;

 protected:
  SyncerError DoWorkAndWaitUntilDoneImpl(const WorkCallback& work) override;

 private:
  ~UIModelWorker() override;

  // A reference to the UI thread's task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(UIModelWorker);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_GLUE_UI_MODEL_WORKER_H_
