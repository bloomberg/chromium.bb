// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/bookmark_model_worker.h"

#include "base/message_loop.h"
#include "base/waitable_event.h"

namespace browser_sync {

void BookmarkModelWorker::CallDoWorkFromModelSafeThreadAndWait(
    ModelSafeWorkerInterface::Visitor* visitor) {
  // It is possible this gets called when we are in the STOPPING state, because
  // the UI loop has initiated shutdown but the syncer hasn't got the memo yet.
  // This is fine, the work will get scheduled and run normally or run by our
  // code handling this case in Stop().
  DCHECK_NE(state_, STOPPED);
  if (state_ == STOPPED)
    return;
  if (MessageLoop::current() == bookmark_model_loop_) {
    DLOG(WARNING) << "CallDoWorkFromModelSafeThreadAndWait called from "
      << "bookmark_model_loop_. Probably a nested invocation?";
    visitor->DoWork();
    return;
  }

  // Create an unsignaled event to wait on.
  base::WaitableEvent work_done(false, false);
  {
    // We lock only to avoid PostTask'ing a NULL pending_work_ (because it
    // could get Run() in Stop() and call OnTaskCompleted before we post).
    // The task is owned by the message loop as per usual.
    AutoLock lock(lock_);
    DCHECK(!pending_work_);
    pending_work_ = new CallDoWorkAndSignalTask(visitor, &work_done, this);
    bookmark_model_loop_->PostTask(FROM_HERE, pending_work_);
  }
  syncapi_event_.Signal();  // Notify that the syncapi produced work for us.
  work_done.Wait();
}

BookmarkModelWorker::~BookmarkModelWorker() {
  DCHECK_EQ(state_, STOPPED);
}

void BookmarkModelWorker::OnSyncerShutdownComplete() {
  AutoLock lock(lock_);
  // The SyncerThread has terminated and we are no longer needed by syncapi.
  // The UI loop initiated shutdown and is (or will be) waiting in Stop().
  // We could either be WORKING or RUNNING_MANUAL_SHUTDOWN_PUMP, depending
  // on where we timeslice the UI thread in Stop; but we can't be STOPPED,
  // because that would imply OnSyncerShutdownComplete already signaled.
  DCHECK_NE(state_, STOPPED);

  syncapi_has_shutdown_ = true;
  syncapi_event_.Signal();
}

void BookmarkModelWorker::Stop() {
  DCHECK_EQ(MessageLoop::current(), bookmark_model_loop_);

  AutoLock lock(lock_);
  DCHECK_EQ(state_, WORKING);

  // We're on our own now, the beloved UI MessageLoop is no longer running.
  // Any tasks scheduled or to be scheduled on the UI MessageLoop will not run.
  state_ = RUNNING_MANUAL_SHUTDOWN_PUMP;

  // Drain any final tasks manually until the SyncerThread tells us it has
  // totally finished. There should only ever be 0 or 1 tasks Run() here.
  while (!syncapi_has_shutdown_) {
    if (pending_work_)
      pending_work_->Run();  // OnTaskCompleted will set pending_work_ to NULL.

    // Wait for either a new task or SyncerThread termination.
    syncapi_event_.Wait();
  }

  state_ = STOPPED;
}

void BookmarkModelWorker::CallDoWorkAndSignalTask::Run() {
  if (!visitor_) {
    // This can happen during tests or cases where there are more than just the
    // default BookmarkModelWorker in existence and it gets destroyed before
    // the main UI loop has terminated.  There is no easy way to assert the
    // loop is running / not running at the moment, so we just provide cancel
    // semantics here and short-circuit.
    // TODO(timsteele): Maybe we should have the message loop destruction
    // observer fire when the loop has ended, just a bit before it
    // actually gets destroyed.
    return;
  }
  visitor_->DoWork();

  // Sever ties with visitor_ to allow the sanity-checking above that we don't
  // get run twice.
  visitor_ = NULL;

  // Notify the BookmarkModelWorker that scheduled us that we have run
  // successfully.
  scheduler_->OnTaskCompleted();
  work_done_->Signal();  // Unblock the syncer thread that scheduled us.
}

}  // namespace browser_sync
