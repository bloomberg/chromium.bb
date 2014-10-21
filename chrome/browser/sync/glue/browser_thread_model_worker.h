// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_BROWSER_THREAD_MODEL_WORKER_H_
#define CHROME_BROWSER_SYNC_GLUE_BROWSER_THREAD_MODEL_WORKER_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "content/public/browser/browser_thread.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/util/syncer_error.h"

namespace base {
class WaitableEvent;
}

namespace browser_sync {

// A syncer::ModelSafeWorker for models that accept requests from the
// syncapi that need to be fulfilled on a browser thread, for example
// autofill on the DB thread.
// TODO(sync): Try to generalize other ModelWorkers (e.g. history, etc).
class BrowserThreadModelWorker : public syncer::ModelSafeWorker {
 public:
  BrowserThreadModelWorker(content::BrowserThread::ID thread,
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
  virtual void CallDoWorkAndSignalTask(
      const syncer::WorkCallback& work,
      base::WaitableEvent* done,
      syncer::SyncerError* error) = 0;

 private:
  content::BrowserThread::ID thread_;
  syncer::ModelSafeGroup group_;

  DISALLOW_COPY_AND_ASSIGN(BrowserThreadModelWorker);
};

// Subclass BrowserThreadModelWorker so that we can distinguish them
// from stack traces alone.

class DatabaseModelWorker : public BrowserThreadModelWorker {
 public:
  explicit DatabaseModelWorker(syncer::WorkerLoopDestructionObserver* observer);

 protected:
  void CallDoWorkAndSignalTask(const syncer::WorkCallback& work,
                               base::WaitableEvent* done,
                               syncer::SyncerError* error) override;

 private:
  ~DatabaseModelWorker() override;
};

class FileModelWorker : public BrowserThreadModelWorker {
 public:
  explicit FileModelWorker(syncer::WorkerLoopDestructionObserver* observer);

 protected:
  void CallDoWorkAndSignalTask(const syncer::WorkCallback& work,
                               base::WaitableEvent* done,
                               syncer::SyncerError* error) override;

 private:
  ~FileModelWorker() override;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_BROWSER_THREAD_MODEL_WORKER_H_
