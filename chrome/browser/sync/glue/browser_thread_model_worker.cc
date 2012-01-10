// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/browser_thread_model_worker.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "content/public/browser/browser_thread.h"

using base::WaitableEvent;
using content::BrowserThread;

namespace browser_sync {

BrowserThreadModelWorker::BrowserThreadModelWorker(
    BrowserThread::ID thread, ModelSafeGroup group)
    : thread_(thread), group_(group) {}

BrowserThreadModelWorker::~BrowserThreadModelWorker() {}

SyncerError BrowserThreadModelWorker::DoWorkAndWaitUntilDone(
    const WorkCallback& work) {
  SyncerError error = UNSET;
  if (BrowserThread::CurrentlyOn(thread_)) {
    DLOG(WARNING) << "Already on thread " << thread_;
    return work.Run();
  }
  WaitableEvent done(false, false);
  if (!BrowserThread::PostTask(
      thread_,
      FROM_HERE,
      base::Bind(&BrowserThreadModelWorker::CallDoWorkAndSignalTask, this,
                 work, &done, &error))) {
    NOTREACHED() << "Failed to post task to thread " << thread_;
    return error;
  }
  done.Wait();
  return error;
}

void BrowserThreadModelWorker::CallDoWorkAndSignalTask(
    const WorkCallback& work,
    WaitableEvent* done,
    SyncerError* error) {
  DCHECK(BrowserThread::CurrentlyOn(thread_));
  *error = work.Run();
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
    SyncerError* error) {
  BrowserThreadModelWorker::CallDoWorkAndSignalTask(work, done, error);
}

FileModelWorker::FileModelWorker()
    : BrowserThreadModelWorker(BrowserThread::FILE, GROUP_FILE) {}

FileModelWorker::~FileModelWorker() {}

void FileModelWorker::CallDoWorkAndSignalTask(
    const WorkCallback& work,
    WaitableEvent* done,
    SyncerError* error) {
  BrowserThreadModelWorker::CallDoWorkAndSignalTask(work, done, error);
}

}  // namespace browser_sync
