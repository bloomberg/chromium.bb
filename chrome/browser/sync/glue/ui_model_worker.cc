// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/ui_model_worker.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
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

  work_done->Signal();  // Unblock the syncer thread that scheduled us.
}

}  // namespace

UIModelWorker::UIModelWorker(syncer::WorkerLoopDestructionObserver* observer)
    : syncer::ModelSafeWorker(observer) {
}

void UIModelWorker::RegisterForLoopDestruction() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SetWorkingLoopToCurrent();
}

syncer::SyncerError UIModelWorker::DoWorkAndWaitUntilDoneImpl(
    const syncer::WorkCallback& work) {
  syncer::SyncerError error_info;
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DLOG(WARNING) << "DoWorkAndWaitUntilDone called from "
      << "ui_loop_. Probably a nested invocation?";
    return work.Run();
  }

  if (!BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CallDoWorkAndSignalCallback,
                 work, work_done_or_stopped(), &error_info))) {
    DLOG(WARNING) << "Could not post work to UI loop.";
    error_info = syncer::CANNOT_DO_WORK;
    return error_info;
  }
  work_done_or_stopped()->Wait();

  return error_info;
}

syncer::ModelSafeGroup UIModelWorker::GetModelSafeGroup() {
  return syncer::GROUP_UI;
}

UIModelWorker::~UIModelWorker() {
}

}  // namespace browser_sync
