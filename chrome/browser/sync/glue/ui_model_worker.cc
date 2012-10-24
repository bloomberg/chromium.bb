// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/ui_model_worker.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace browser_sync {

namespace {

// A simple callback to signal a waitable event after running a closure.
void CallDoWorkAndSignalCallback(const syncer::WorkCallback& work,
                                 base::WaitableEvent* work_done,
                                 UIModelWorker* const scheduler,
                                 syncer::SyncerError* error_info) {
  if (work.is_null()) {
    // This can happen during tests or cases where there are more than just the
    // default UIModelWorker in existence and it gets destroyed before
    // the main UI loop has terminated.  There is no easy way to assert the
    // loop is running / not running at the moment, so we just provide cancel
    // semantics here and short-circuit.
    // TODO(timsteele): Maybe we should have the message loop destruction
    // observer fire when the loop has ended, just a bit before it
    // actually gets destroyed.
    return;
  }

  *error_info = work.Run();

  // Notify the UIModelWorker that scheduled us that we have run
  // successfully.
  scheduler->OnTaskCompleted();
  work_done->Signal();  // Unblock the syncer thread that scheduled us.
}

}  // namespace

UIModelWorker::UIModelWorker()
    : state_(WORKING),
      syncapi_has_shutdown_(false),
      syncapi_event_(&lock_) {
}

void UIModelWorker::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::AutoLock lock(lock_);
  DCHECK_EQ(state_, WORKING);

  // We're on our own now, the beloved UI MessageLoop is no longer running.
  // Any tasks scheduled or to be scheduled on the UI MessageLoop will not run.
  state_ = RUNNING_MANUAL_SHUTDOWN_PUMP;

  // Drain any final tasks manually until the SyncerThread tells us it has
  // totally finished. There should only ever be 0 or 1 tasks Run() here.
  while (!syncapi_has_shutdown_) {
    if (!pending_work_.is_null())
      pending_work_.Run();  // OnTaskCompleted will set reset |pending_work_|.

    // http://crbug.com/19757
    base::ThreadRestrictions::ScopedAllowWait allow_wait;
    // Wait for either a new task or SyncerThread termination.
    syncapi_event_.Wait();
  }

  state_ = STOPPED;
}

syncer::SyncerError UIModelWorker::DoWorkAndWaitUntilDone(
    const syncer::WorkCallback& work) {
  // In most cases, this method is called in WORKING state. It is possible this
  // gets called when we are in the RUNNING_MANUAL_SHUTDOWN_PUMP state, because
  // the UI loop has initiated shutdown but the syncer hasn't got the memo yet.
  // This is fine, the work will get scheduled and run normally or run by our
  // code handling this case in Stop(). Note there _no_ way we can be in here
  // with state_ = STOPPED, so it is safe to read / compare in this case.
  CHECK_NE(ANNOTATE_UNPROTECTED_READ(state_), STOPPED);
  syncer::SyncerError error_info;
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DLOG(WARNING) << "DoWorkAndWaitUntilDone called from "
      << "ui_loop_. Probably a nested invocation?";
    return work.Run();
  }

  // Create an unsignaled event to wait on.
  base::WaitableEvent work_done(false, false);
  {
    // We lock only to avoid PostTask'ing a NULL pending_work_ (because it
    // could get Run() in Stop() and call OnTaskCompleted before we post).
    // The task is owned by the message loop as per usual.
    base::AutoLock lock(lock_);
    DCHECK(pending_work_.is_null());
    pending_work_ = base::Bind(&CallDoWorkAndSignalCallback, work, &work_done,
                               base::Unretained(this), &error_info);
    if (!BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, pending_work_)) {
      DLOG(WARNING) << "Could not post work to UI loop.";
      error_info = syncer::CANNOT_DO_WORK;
      pending_work_.Reset();
      syncapi_event_.Signal();
      return error_info;
    }
  }
  syncapi_event_.Signal();  // Notify that the syncapi produced work for us.
  work_done.Wait();
  return error_info;
}

syncer::ModelSafeGroup UIModelWorker::GetModelSafeGroup() {
  return syncer::GROUP_UI;
}

void UIModelWorker::OnSyncerShutdownComplete() {
  base::AutoLock lock(lock_);
  // The SyncerThread has terminated and we are no longer needed by syncapi.
  // The UI loop initiated shutdown and is (or will be) waiting in Stop().
  // We could either be WORKING or RUNNING_MANUAL_SHUTDOWN_PUMP, depending
  // on where we timeslice the UI thread in Stop; but we can't be STOPPED,
  // because that would imply OnSyncerShutdownComplete already signaled.
  DCHECK_NE(state_, STOPPED);

  syncapi_has_shutdown_ = true;
  syncapi_event_.Signal();
}

UIModelWorker::~UIModelWorker() {
  DCHECK_EQ(state_, STOPPED);
}

}  // namespace browser_sync
