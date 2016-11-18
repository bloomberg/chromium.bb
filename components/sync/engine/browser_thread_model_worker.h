// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_BROWSER_THREAD_MODEL_WORKER_H_
#define COMPONENTS_SYNC_ENGINE_BROWSER_THREAD_MODEL_WORKER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "components/sync/base/scoped_event_signal.h"
#include "components/sync/base/syncer_error.h"
#include "components/sync/engine/model_safe_worker.h"

namespace syncer {

// A ModelSafeWorker for models that accept requests from the
// syncapi that need to be fulfilled on a browser thread, for example
// autofill on the DB thread.
// TODO(sync): Try to generalize other ModelWorkers (e.g. history, etc).
class BrowserThreadModelWorker : public ModelSafeWorker {
 public:
  BrowserThreadModelWorker(
      const scoped_refptr<base::SingleThreadTaskRunner>& runner,
      ModelSafeGroup group);

  // ModelSafeWorker implementation.
  ModelSafeGroup GetModelSafeGroup() override;
  bool IsOnModelThread() override;

 protected:
  ~BrowserThreadModelWorker() override;

  SyncerError DoWorkAndWaitUntilDoneImpl(const WorkCallback& work) override;

  void CallDoWorkAndSignalTask(const WorkCallback& work,
                               syncer::ScopedEventSignal scoped_event_signal,
                               SyncerError* error);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> runner_;
  ModelSafeGroup group_;

  DISALLOW_COPY_AND_ASSIGN(BrowserThreadModelWorker);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_BROWSER_THREAD_MODEL_WORKER_H_
