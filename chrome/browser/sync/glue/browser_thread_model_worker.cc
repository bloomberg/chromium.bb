// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/browser_thread_model_worker.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "content/public/browser/browser_thread.h"

using base::WaitableEvent;

namespace browser_sync {

BrowserThreadModelWorker::BrowserThreadModelWorker(
    BrowserThread::ID thread, ModelSafeGroup group)
    : thread_(thread), group_(group) {}

BrowserThreadModelWorker::~BrowserThreadModelWorker() {}

UnrecoverableErrorInfo BrowserThreadModelWorker::DoWorkAndWaitUntilDone(
    const WorkCallback& work) {
  UnrecoverableErrorInfo error_info;
  if (BrowserThread::CurrentlyOn(thread_)) {
    DLOG(WARNING) << "Already on thread " << thread_;
    return work.Run();
  }
  WaitableEvent done(false, false);
  if (!BrowserThread::PostTask(
      thread_,
      FROM_HERE,
      base::Bind(&BrowserThreadModelWorker::CallDoWorkAndSignalTask, this,
                 work, &done, &error_info))) {
    NOTREACHED() << "Failed to post task to thread " << thread_;
    return error_info;
  }
  done.Wait();
  return error_info;
}

void BrowserThreadModelWorker::CallDoWorkAndSignalTask(
    const WorkCallback& work,
    WaitableEvent* done,
    UnrecoverableErrorInfo* error_info) {
  DCHECK(BrowserThread::CurrentlyOn(thread_));
  *error_info = work.Run();
  done->Signal();
}

ModelSafeGroup BrowserThreadModelWorker::GetModelSafeGroup() {
  return group_;
}

DatabaseModelWorker::DatabaseModelWorker()
    : BrowserThreadModelWorker(BrowserThread::DB, GROUP_DB) {}

DatabaseModelWorker::~DatabaseModelWorker() {}

void DatabaseModelWorker::CallDoWorkAndSignalTask(
    const WorkCallback& work,
    WaitableEvent* done,
    UnrecoverableErrorInfo* error_info) {
  BrowserThreadModelWorker::CallDoWorkAndSignalTask(work, done, error_info);
}

FileModelWorker::FileModelWorker()
    : BrowserThreadModelWorker(BrowserThread::FILE, GROUP_FILE) {}

FileModelWorker::~FileModelWorker() {}

void FileModelWorker::CallDoWorkAndSignalTask(
    const WorkCallback& work,
    WaitableEvent* done,
    UnrecoverableErrorInfo* error_info) {
  BrowserThreadModelWorker::CallDoWorkAndSignalTask(work, done, error_info);
}

}  // namespace browser_sync
