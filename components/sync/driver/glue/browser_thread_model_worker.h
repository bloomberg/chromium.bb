// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GLUE_BROWSER_THREAD_MODEL_WORKER_H_
#define COMPONENTS_SYNC_DRIVER_GLUE_BROWSER_THREAD_MODEL_WORKER_H_

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/sync/base/syncer_error.h"
#include "components/sync/engine/model_safe_worker.h"

namespace base {
class SingleThreadTaskRunner;
class WaitableEvent;
}  // namespace base

namespace syncer {

// A ModelSafeWorker for models that accept requests from the
// syncapi that need to be fulfilled on a browser thread, for example
// autofill on the DB thread.
// TODO(sync): Try to generalize other ModelWorkers (e.g. history, etc).
class BrowserThreadModelWorker : public ModelSafeWorker {
 public:
  BrowserThreadModelWorker(
      const scoped_refptr<base::SingleThreadTaskRunner>& runner,
      ModelSafeGroup group,
      WorkerLoopDestructionObserver* observer);

  // ModelSafeWorker implementation. Called on the sync thread.
  void RegisterForLoopDestruction() override;
  ModelSafeGroup GetModelSafeGroup() override;

 protected:
  ~BrowserThreadModelWorker() override;

  SyncerError DoWorkAndWaitUntilDoneImpl(const WorkCallback& work) override;

  // Marked pure virtual so subclasses have to override, but there is
  // an implementation that subclasses should use.  This is so that
  // (subclass)::CallDoWorkAndSignalTask shows up in callstacks.
  virtual void CallDoWorkAndSignalTask(const WorkCallback& work,
                                       base::WaitableEvent* done,
                                       SyncerError* error);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> runner_;
  ModelSafeGroup group_;

  DISALLOW_COPY_AND_ASSIGN(BrowserThreadModelWorker);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_GLUE_BROWSER_THREAD_MODEL_WORKER_H_
