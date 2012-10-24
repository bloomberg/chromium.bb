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
    BrowserThread::ID thread, syncer::ModelSafeGroup group)
    : thread_(thread), group_(group) {
}

syncer::SyncerError BrowserThreadModelWorker::DoWorkAndWaitUntilDone(
    const syncer::WorkCallback& work) {
  syncer::SyncerError error = syncer::UNSET;
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
    DLOG(WARNING) << "Failed to post task to thread " << thread_;
    error = syncer::CANNOT_DO_WORK;
    return error;
  }
  done.Wait();
  return error;
}

syncer::ModelSafeGroup BrowserThreadModelWorker::GetModelSafeGroup() {
  return group_;
}

BrowserThreadModelWorker::~BrowserThreadModelWorker() {}

void BrowserThreadModelWorker::CallDoWorkAndSignalTask(
    const syncer::WorkCallback& work,
    WaitableEvent* done,
    syncer::SyncerError* error) {
  DCHECK(BrowserThread::CurrentlyOn(thread_));
  *error = work.Run();
  done->Signal();
}

DatabaseModelWorker::DatabaseModelWorker()
    : BrowserThreadModelWorker(BrowserThread::DB, syncer::GROUP_DB) {
}

void DatabaseModelWorker::CallDoWorkAndSignalTask(
    const syncer::WorkCallback& work,
    WaitableEvent* done,
    syncer::SyncerError* error) {
  BrowserThreadModelWorker::CallDoWorkAndSignalTask(work, done, error);
}

DatabaseModelWorker::~DatabaseModelWorker() {}

FileModelWorker::FileModelWorker()
    : BrowserThreadModelWorker(BrowserThread::FILE, syncer::GROUP_FILE) {
}

void FileModelWorker::CallDoWorkAndSignalTask(
    const syncer::WorkCallback& work,
    WaitableEvent* done,
    syncer::SyncerError* error) {
  BrowserThreadModelWorker::CallDoWorkAndSignalTask(work, done, error);
}

FileModelWorker::~FileModelWorker() {}

}  // namespace browser_sync
