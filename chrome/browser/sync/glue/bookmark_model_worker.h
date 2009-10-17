// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(BROWSER_SYNC)

#ifndef CHROME_BROWSER_SYNC_GLUE_BOOKMARK_MODEL_WORKER_H_
#define CHROME_BROWSER_SYNC_GLUE_BOOKMARK_MODEL_WORKER_H_

#include "base/lock.h"
#include "base/task.h"
#include "base/waitable_event.h"
#include "chrome/browser/sync/engine/syncapi.h"

class MessageLoop;

namespace browser_sync {

// A ModelSafeWorker for bookmarks that accepts work requests from the syncapi
// that need to be fulfilled from the MessageLoop home to the BookmarkModel
// (this is typically the "main" UI thread).
//
// Lifetime note: Instances of this class will generally be owned by the
// SyncerThread. When the SyncerThread _object_ is destroyed, the
// BookmarkModelWorker will be destroyed. The SyncerThread object is destroyed
// after the actual syncer pthread has exited.
class BookmarkModelWorker
    : public sync_api::ModelSafeWorkerInterface {
 public:
  explicit BookmarkModelWorker(MessageLoop* bookmark_model_loop)
      : state_(WORKING),
        pending_work_(NULL),
        syncapi_has_shutdown_(false),
        bookmark_model_loop_(bookmark_model_loop),
        syncapi_event_(false, false) {
  }
  virtual ~BookmarkModelWorker();

  // A simple task to signal a waitable event after calling DoWork on a visitor.
  class CallDoWorkAndSignalTask : public Task {
   public:
    CallDoWorkAndSignalTask(ModelSafeWorkerInterface::Visitor* visitor,
                            base::WaitableEvent* work_done,
                            BookmarkModelWorker* scheduler)
        : visitor_(visitor), work_done_(work_done), scheduler_(scheduler) {
    }
    virtual ~CallDoWorkAndSignalTask() { }

    // Task implementation.
    virtual void Run();

   private:
    // Task data - a visitor that knows how to DoWork, and a waitable event
    // to signal after the work has been done.
    ModelSafeWorkerInterface::Visitor* visitor_;
    base::WaitableEvent* work_done_;

    // The BookmarkModelWorker responsible for scheduling us.
    BookmarkModelWorker* const scheduler_;

    DISALLOW_COPY_AND_ASSIGN(CallDoWorkAndSignalTask);
  };

  // Called by the UI thread on shutdown of the sync service. Blocks until
  // the BookmarkModelWorker has safely met termination conditions, namely that
  // no task scheduled by CallDoWorkFromModelSafeThreadAndWait remains un-
  // processed and that syncapi will not schedule any further work for us to do.
  void Stop();

  // ModelSafeWorkerInterface implementation. Called on syncapi SyncerThread.
  virtual void CallDoWorkFromModelSafeThreadAndWait(
      ModelSafeWorkerInterface::Visitor* visitor);

  // Upon receiving this idempotent call, the ModelSafeWorkerInterface can
  // assume no work will ever be scheduled again from now on. If it has any work
  // that it has not yet completed, it must make sure to run it as soon as
  // possible as the Syncer is trying to shut down. Called from the CoreThread.
  void OnSyncerShutdownComplete();

  // Callback from |pending_work_| to notify us that it has been run.
  // Called on |bookmark_model_loop_|.
  void OnTaskCompleted() { pending_work_ = NULL; }

 private:
  // The life-cycle of a BookmarkModelWorker in three states.
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
  // read this value from other threads when you know it is absolutely safe (e.g
  // there is _no_ way we can be in CallDoWork with state_ = STOPPED, so it is
  // safe to read / compare in this case).
  State state_;

  // We keep a reference to any task we have scheduled so we can gracefully
  // force them to run if the syncer is trying to shutdown.
  Task* pending_work_;
  Lock pending_work_lock_;

  // Set by the SyncCoreThread when Syncapi shutdown has completed and the
  // SyncerThread has terminated, so no more work will be scheduled. Read by
  // the UI thread in Stop().
  bool syncapi_has_shutdown_;

  // The BookmarkModel's home-sweet-home MessageLoop.
  MessageLoop* const bookmark_model_loop_;

  // Used as a barrier at shutdown to ensure the SyncerThread terminates before
  // we allow the UI thread to return from Stop(). This gets signalled whenever
  // one of two events occur: a new pending_work_ task was scheduled, or the
  // SyncerThread has terminated. We only care about (1) when we are in Stop(),
  // because we have to manually Run() the task.
  base::WaitableEvent syncapi_event_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelWorker);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_BOOKMARK_MODEL_WORKER_H_

#endif  // defined(BROWSER_SYNC)
