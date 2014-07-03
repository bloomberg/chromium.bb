// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_HISTORY_MODEL_WORKER_H_
#define CHROME_BROWSER_SYNC_GLUE_HISTORY_MODEL_WORKER_H_

#include "sync/internal_api/public/engine/model_safe_worker.h"

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_service.h"

class HistoryService;

namespace browser_sync {

// A syncer::ModelSafeWorker for history models that accepts requests
// from the syncapi that need to be fulfilled on the history thread.
class HistoryModelWorker : public syncer::ModelSafeWorker {
 public:
  explicit HistoryModelWorker(
      const base::WeakPtr<HistoryService>& history_service,
      syncer::WorkerLoopDestructionObserver* observer);

  // syncer::ModelSafeWorker implementation. Called on syncapi SyncerThread.
  virtual void RegisterForLoopDestruction() OVERRIDE;
  virtual syncer::ModelSafeGroup GetModelSafeGroup() OVERRIDE;

  // Called on history DB thread to register HistoryModelWorker to observe
  // destruction of history backend loop.
  void RegisterOnDBThread();

 protected:
  virtual syncer::SyncerError DoWorkAndWaitUntilDoneImpl(
      const syncer::WorkCallback& work) OVERRIDE;

 private:
  virtual ~HistoryModelWorker();

  const base::WeakPtr<HistoryService> history_service_;
  // Helper object to make sure we don't leave tasks running on the history
  // thread.
  scoped_ptr<base::CancelableTaskTracker> cancelable_tracker_;
  DISALLOW_COPY_AND_ASSIGN(HistoryModelWorker);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_HISTORY_MODEL_WORKER_H_
