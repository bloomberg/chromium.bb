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
    BrowserThread::ID thread, syncer::ModelSafeGroup group,
    syncer::WorkerLoopDestructionObserver* observer)
    : ModelSafeWorker(observer),
      thread_(thread), group_(group) {
}

syncer::SyncerError BrowserThreadModelWorker::DoWorkAndWaitUntilDoneImpl(
    const syncer::WorkCallback& work) {
  syncer::SyncerError error = syncer::UNSET;
  if (BrowserThread::CurrentlyOn(thread_)) {
    DLOG(WARNING) << "Already on thread " << thread_;
    return work.Run();
  }

  if (!BrowserThread::PostTask(
      thread_,
      FROM_HERE,
      base::Bind(&BrowserThreadModelWorker::CallDoWorkAndSignalTask,
                 this, work,
                 work_done_or_stopped(), &error))) {
    DLOG(WARNING) << "Failed to post task to thread " << thread_;
    error = syncer::CANNOT_DO_WORK;
    return error;
  }
  work_done_or_stopped()->Wait();
  return error;
}

syncer::ModelSafeGroup BrowserThreadModelWorker::GetModelSafeGroup() {
  return group_;
}

BrowserThreadModelWorker::~BrowserThreadModelWorker() {}

void BrowserThreadModelWorker::RegisterForLoopDestruction() {
  if (BrowserThread::CurrentlyOn(thread_)) {
    base::MessageLoop::current()->AddDestructionObserver(this);
    SetWorkingLoopToCurrent();
  } else {
    BrowserThread::PostTask(
        thread_, FROM_HERE,
        Bind(&BrowserThreadModelWorker::RegisterForLoopDestruction, this));
  }
}

void BrowserThreadModelWorker::CallDoWorkAndSignalTask(
    const syncer::WorkCallback& work,
    WaitableEvent* done,
    syncer::SyncerError* error) {
  DCHECK(BrowserThread::CurrentlyOn(thread_));
  if (!IsStopped())
    *error = work.Run();
  done->Signal();
}

DatabaseModelWorker::DatabaseModelWorker(
    syncer::WorkerLoopDestructionObserver* observer)
    : BrowserThreadModelWorker(BrowserThread::DB, syncer::GROUP_DB, observer) {
}

void DatabaseModelWorker::CallDoWorkAndSignalTask(
    const syncer::WorkCallback& work,
    WaitableEvent* done,
    syncer::SyncerError* error) {
  BrowserThreadModelWorker::CallDoWorkAndSignalTask(work, done, error);
}

DatabaseModelWorker::~DatabaseModelWorker() {}

FileModelWorker::FileModelWorker(
    syncer::WorkerLoopDestructionObserver* observer)
    : BrowserThreadModelWorker(BrowserThread::FILE, syncer::GROUP_FILE,
                               observer) {
}

void FileModelWorker::CallDoWorkAndSignalTask(
    const syncer::WorkCallback& work,
    WaitableEvent* done,
    syncer::SyncerError* error) {
  BrowserThreadModelWorker::CallDoWorkAndSignalTask(work, done, error);
}

FileModelWorker::~FileModelWorker() {}

}  // namespace browser_sync
