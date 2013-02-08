// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/history_model_worker.h"

#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/history/history_db_task.h"
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

  // Since the DoWorkAndWaitUntilDone() is syncronous, we don't need to run any
  // code asynchronously on the main thread after completion.
  virtual void DoneRunOnMainThread() OVERRIDE {}

 protected:
  virtual ~WorkerTask() {}

  syncer::WorkCallback work_;
  WaitableEvent* done_;
  syncer::SyncerError* error_;
};

namespace {

// Post the work task on |history_service|'s DB thread from the UI
// thread.
void PostWorkerTask(const base::WeakPtr<HistoryService>& history_service,
                    const syncer::WorkCallback& work,
                    CancelableRequestConsumerT<int, 0>* cancelable_consumer,
                    WaitableEvent* done,
                    syncer::SyncerError* error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (history_service.get()) {
    scoped_refptr<WorkerTask> task(new WorkerTask(work, done, error));
    history_service->ScheduleDBTask(task.get(), cancelable_consumer);
  } else {
    *error = syncer::CANNOT_DO_WORK;
    done->Signal();
  }
}

}  // namespace

HistoryModelWorker::HistoryModelWorker(
    const base::WeakPtr<HistoryService>& history_service)
  : history_service_(history_service) {
  CHECK(history_service.get());
}

syncer::SyncerError HistoryModelWorker::DoWorkAndWaitUntilDone(
    const syncer::WorkCallback& work) {
  syncer::SyncerError error = syncer::UNSET;
  WaitableEvent done(false, false);
  if (BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::Bind(&PostWorkerTask,
                                         history_service_, work,
                                         &cancelable_consumer_,
                                         &done, &error))) {
    done.Wait();
  } else {
    error = syncer::CANNOT_DO_WORK;
  }
  return error;
}

syncer::ModelSafeGroup HistoryModelWorker::GetModelSafeGroup() {
  return syncer::GROUP_HISTORY;
}

HistoryModelWorker::~HistoryModelWorker() {}

}  // namespace browser_sync
