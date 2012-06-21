// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/history_model_worker.h"

#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/history/history.h"

using base::WaitableEvent;

namespace browser_sync {

class WorkerTask : public HistoryDBTask {
 public:
  WorkerTask(
      const csync::WorkCallback& work,
      WaitableEvent* done,
      csync::SyncerError* error)
    : work_(work), done_(done), error_(error) {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) {
    *error_ = work_.Run();
    done_->Signal();
    return true;
  }

  // Since the DoWorkAndWaitUntilDone() is syncronous, we don't need to run any
  // code asynchronously on the main thread after completion.
  virtual void DoneRunOnMainThread() {}

 protected:
  virtual ~WorkerTask() {}

  csync::WorkCallback work_;
  WaitableEvent* done_;
  csync::SyncerError* error_;
};


HistoryModelWorker::HistoryModelWorker(HistoryService* history_service)
  : history_service_(history_service) {
  CHECK(history_service);
}

csync::SyncerError HistoryModelWorker::DoWorkAndWaitUntilDone(
    const csync::WorkCallback& work) {
  WaitableEvent done(false, false);
  csync::SyncerError error = csync::UNSET;
  scoped_refptr<WorkerTask> task(new WorkerTask(work, &done, &error));
  history_service_->ScheduleDBTask(task.get(), &cancelable_consumer_);
  done.Wait();
  return error;
}

csync::ModelSafeGroup HistoryModelWorker::GetModelSafeGroup() {
  return csync::GROUP_HISTORY;
}

HistoryModelWorker::~HistoryModelWorker() {}

}  // namespace browser_sync
