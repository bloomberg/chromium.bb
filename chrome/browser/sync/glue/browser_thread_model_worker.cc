// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/browser_thread_model_worker.h"

#include "base/synchronization/waitable_event.h"
#include "content/browser/browser_thread.h"

using base::WaitableEvent;

namespace browser_sync {

BrowserThreadModelWorker::BrowserThreadModelWorker(
    BrowserThread::ID thread, ModelSafeGroup group)
    : thread_(thread), group_(group) {}

BrowserThreadModelWorker::~BrowserThreadModelWorker() {}

void BrowserThreadModelWorker::DoWorkAndWaitUntilDone(Callback0::Type* work) {
  if (BrowserThread::CurrentlyOn(thread_)) {
    DLOG(WARNING) << "Already on thread " << thread_;
    work->Run();
    return;
  }
  WaitableEvent done(false, false);
  if (!BrowserThread::PostTask(
      thread_,
      FROM_HERE,
      NewRunnableMethod(
          this,
          &BrowserThreadModelWorker::CallDoWorkAndSignalTask,
          work,
          &done))) {
    NOTREACHED() << "Failed to post task to thread " << thread_;
    return;
  }
  done.Wait();
}

void BrowserThreadModelWorker::CallDoWorkAndSignalTask(
    Callback0::Type* work, WaitableEvent* done) {
  DCHECK(BrowserThread::CurrentlyOn(thread_));
  work->Run();
  done->Signal();
}

ModelSafeGroup BrowserThreadModelWorker::GetModelSafeGroup() {
  return group_;
}

}  // namespace browser_sync
