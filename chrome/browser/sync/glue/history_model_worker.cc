// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/history_model_worker.h"

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "content/public/browser/browser_thread.h"

using base::WaitableEvent;
using content::BrowserThread;

namespace browser_sync {

class WorkerTask : public history::HistoryDBTask {
 public:
  WorkerTask(
      const syncer::WorkCallback& work,
      WaitableEvent* done,
      syncer::SyncerError* error)
    : work_(work), done_(done), error_(error) {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    *error_ = work_.Run();
    done_->Signal();
    return true;
  }

  // Since the DoWorkAndWaitUntilDone() is synchronous, we don't need to run
  // any code asynchronously on the main thread after completion.
  virtual void DoneRunOnMainThread() OVERRIDE {}

 protected:
  virtual ~WorkerTask() {}

  syncer::WorkCallback work_;
  WaitableEvent* done_;
  syncer::SyncerError* error_;
};

class AddDBThreadObserverTask : public history::HistoryDBTask {
 public:
  explicit AddDBThreadObserverTask(base::Closure register_callback)
     : register_callback_(register_callback) {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    register_callback_.Run();
    return true;
  }

  virtual void DoneRunOnMainThread() OVERRIDE {}

 private:
  virtual ~AddDBThreadObserverTask() {}

  base::Closure register_callback_;
};

namespace {

// Post the work task on |history_service|'s DB thread from the UI
// thread.
void PostWorkerTask(const base::WeakPtr<HistoryService>& history_service,
                    const syncer::WorkCallback& work,
                    base::CancelableTaskTracker* cancelable_tracker,
                    WaitableEvent* done,
                    syncer::SyncerError* error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (history_service.get()) {
    scoped_ptr<history::HistoryDBTask> task(new WorkerTask(work, done, error));
    history_service->ScheduleDBTask(task.Pass(), cancelable_tracker);
  } else {
    *error = syncer::CANNOT_DO_WORK;
    done->Signal();
  }
}

}  // namespace

HistoryModelWorker::HistoryModelWorker(
    const base::WeakPtr<HistoryService>& history_service,
    syncer::WorkerLoopDestructionObserver* observer)
  : syncer::ModelSafeWorker(observer),
    history_service_(history_service) {
  CHECK(history_service.get());
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  cancelable_tracker_.reset(new base::CancelableTaskTracker);
}

void HistoryModelWorker::RegisterForLoopDestruction() {
  CHECK(history_service_.get());
  history_service_->ScheduleDBTask(
      scoped_ptr<history::HistoryDBTask>(new AddDBThreadObserverTask(
          base::Bind(&HistoryModelWorker::RegisterOnDBThread, this))),
      cancelable_tracker_.get());
}

void HistoryModelWorker::RegisterOnDBThread() {
  base::MessageLoop::current()->AddDestructionObserver(this);
  SetWorkingLoopToCurrent();
}

syncer::SyncerError HistoryModelWorker::DoWorkAndWaitUntilDoneImpl(
    const syncer::WorkCallback& work) {
  syncer::SyncerError error = syncer::UNSET;
  if (BrowserThread::PostTask(BrowserThread::UI,
                              FROM_HERE,
                              base::Bind(&PostWorkerTask,
                                         history_service_,
                                         work,
                                         cancelable_tracker_.get(),
                                         work_done_or_stopped(),
                                         &error))) {
    work_done_or_stopped()->Wait();
  } else {
    error = syncer::CANNOT_DO_WORK;
  }
  return error;
}

syncer::ModelSafeGroup HistoryModelWorker::GetModelSafeGroup() {
  return syncer::GROUP_HISTORY;
}

HistoryModelWorker::~HistoryModelWorker() {
  // The base::CancelableTaskTracker class is not thread-safe and must only be
  // used from a single thread but the current object may not be destroyed from
  // the UI thread, so delete it from the UI thread.
  BrowserThread::DeleteOnUIThread::Destruct(cancelable_tracker_.release());
}

}  // namespace browser_sync
