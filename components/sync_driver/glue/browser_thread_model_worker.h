// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GLUE_BROWSER_THREAD_MODEL_WORKER_H_
#define COMPONENTS_SYNC_DRIVER_GLUE_BROWSER_THREAD_MODEL_WORKER_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/util/syncer_error.h"

namespace base {
class SingleThreadTaskRunner;
class WaitableEvent;
}

namespace browser_sync {

// A syncer::ModelSafeWorker for models that accept requests from the
// syncapi that need to be fulfilled on a browser thread, for example
// autofill on the DB thread.
// TODO(sync): Try to generalize other ModelWorkers (e.g. history, etc).
class BrowserThreadModelWorker : public syncer::ModelSafeWorker {
 public:
  BrowserThreadModelWorker(
      const scoped_refptr<base::SingleThreadTaskRunner>& runner,
      syncer::ModelSafeGroup group,
      syncer::WorkerLoopDestructionObserver* observer);

  // syncer::ModelSafeWorker implementation. Called on the sync thread.
  void RegisterForLoopDestruction() override;
  syncer::ModelSafeGroup GetModelSafeGroup() override;

 protected:
  ~BrowserThreadModelWorker() override;

  syncer::SyncerError DoWorkAndWaitUntilDoneImpl(
      const syncer::WorkCallback& work) override;

  // Marked pure virtual so subclasses have to override, but there is
  // an implementation that subclasses should use.  This is so that
  // (subclass)::CallDoWorkAndSignalTask shows up in callstacks.
  virtual void CallDoWorkAndSignalTask(const syncer::WorkCallback& work,
                                       base::WaitableEvent* done,
                                       syncer::SyncerError* error);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> runner_;
  syncer::ModelSafeGroup group_;

  DISALLOW_COPY_AND_ASSIGN(BrowserThreadModelWorker);
};

}  // namespace browser_sync

#endif  // COMPONENTS_SYNC_DRIVER_GLUE_BROWSER_THREAD_MODEL_WORKER_H_
