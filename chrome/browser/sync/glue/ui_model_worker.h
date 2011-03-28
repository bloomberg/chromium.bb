// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_UI_MODEL_WORKER_H_
#define CHROME_BROWSER_SYNC_GLUE_UI_MODEL_WORKER_H_
#pragma once

#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/condition_variable.h"
#include "base/task.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"

namespace base {
class WaitableEvent;
}

class MessageLoop;

namespace browser_sync {

// A ModelSafeWorker for UI models (e.g. bookmarks) that accepts work requests
// from the syncapi that need to be fulfilled from the MessageLoop home to the
// native model.
//
// Lifetime note: Instances of this class will generally be owned by the
// SyncerThread. When the SyncerThread _object_ is destroyed, the
// UIModelWorker will be destroyed. The SyncerThread object is destroyed
// after the actual syncer pthread has exited.
class UIModelWorker : public browser_sync::ModelSafeWorker {
 public:
  UIModelWorker();
  virtual ~UIModelWorker();

  // A simple task to signal a waitable event after Run()ning a Closure.
  class CallDoWorkAndSignalTask : public Task {
   public:
    CallDoWorkAndSignalTask(Callback0::Type* work,
                            base::WaitableEvent* work_done,
                            UIModelWorker* scheduler)
        : work_(work), work_done_(work_done), scheduler_(scheduler) {
    }
    virtual ~CallDoWorkAndSignalTask() { }

    // Task implementation.
    virtual void Run();

   private:
    // Task data - a closure and a waitable event to signal after the work has
    // been done.
    Callback0::Type* work_;
    base::WaitableEvent* work_done_;

    // The UIModelWorker responsible for scheduling us.
    UIModelWorker* const scheduler_;

    DISALLOW_COPY_AND_ASSIGN(CallDoWorkAndSignalTask);
  };

  // Called by the UI thread on shutdown of the sync service. Blocks until
  // the UIModelWorker has safely met termination conditions, namely that
  // no task scheduled by CallDoWorkFromModelSafeThreadAndWait remains un-
  // processed and that syncapi will not schedule any further work for us to do.
  void Stop();

  // ModelSafeWorker implementation. Called on syncapi SyncerThread.
  virtual void DoWorkAndWaitUntilDone(Callback0::Type* work);
  virtual ModelSafeGroup GetModelSafeGroup();
  virtual bool CurrentThreadIsWorkThread();

  // Upon receiving this idempotent call, the ModelSafeWorker can
  // assume no work will ever be scheduled again from now on. If it has any work
  // that it has not yet completed, it must make sure to run it as soon as
  // possible as the Syncer is trying to shut down. Called from the CoreThread.
  void OnSyncerShutdownComplete();

  // Callback from |pending_work_| to notify us that it has been run.
  // Called on ui loop.
  void OnTaskCompleted() { pending_work_ = NULL; }

 private:
  // The life-cycle of a UIModelWorker in three states.
  enum State {
    // We hit the ground running in this state and remain until
    // the UI loop calls Stop().
    WORKING,
    // Stop() sequence has been initiated, but we have not received word that
    // the SyncerThread has terminated and doesn't need us anymore. Since the
    // UI MessageLoop is not running at this point, we manually process any
    // last pending_task_ that the Syncer throws at us, effectively dedicating
    // the UI thread to terminating the Syncer.
    RUNNING_MANUAL_SHUTDOWN_PUMP,
    // We have come to a complete stop, no scheduled work remains, and no work
    // will be scheduled from now until our destruction.
    STOPPED,
  };

  // This is set by the UI thread, but is not explicitly thread safe, so only
  // read this value from other threads when you know it is absolutely safe.
  State state_;

  // We keep a reference to any task we have scheduled so we can gracefully
  // force them to run if the syncer is trying to shutdown.
  Task* pending_work_;

  // Set by the SyncCoreThread when Syncapi shutdown has completed and the
  // SyncerThread has terminated, so no more work will be scheduled. Read by
  // the UI thread in Stop().
  bool syncapi_has_shutdown_;

  // We use a Lock for all data members and a ConditionVariable to synchronize.
  // We do this instead of using a WaitableEvent and a bool condition in order
  // to guard against races that could arise due to the fact that the lack of a
  // barrier permits instructions to be reordered by compiler optimizations.
  // Possible or not, that route makes for very fragile code due to existence
  // of theoretical races.
  base::Lock lock_;

  // Used as a barrier at shutdown to ensure the SyncerThread terminates before
  // we allow the UI thread to return from Stop(). This gets signalled whenever
  // one of two events occur: a new pending_work_ task was scheduled, or the
  // SyncerThread has terminated. We only care about (1) when we are in Stop(),
  // because we have to manually Run() the task.
  base::ConditionVariable syncapi_event_;

  DISALLOW_COPY_AND_ASSIGN(UIModelWorker);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_UI_MODEL_WORKER_H_
