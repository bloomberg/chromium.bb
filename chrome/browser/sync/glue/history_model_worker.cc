// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
      const WorkCallback& work,
      WaitableEvent* done,
      UnrecoverableErrorInfo* error_info)
    : work_(work), done_(done), error_info_(error_info) {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) {
    *error_info_ = work_.Run();
    done_->Signal();
    return true;
  }

  // Since the DoWorkAndWaitUntilDone() is syncronous, we don't need to run any
  // code asynchronously on the main thread after completion.
  virtual void DoneRunOnMainThread() {}

 protected:
  WorkCallback work_;
  WaitableEvent* done_;
  UnrecoverableErrorInfo* error_info_;
};


HistoryModelWorker::HistoryModelWorker(HistoryService* history_service)
  : history_service_(history_service) {
  CHECK(history_service);
}

HistoryModelWorker::~HistoryModelWorker() {
}

UnrecoverableErrorInfo HistoryModelWorker::DoWorkAndWaitUntilDone(
    const WorkCallback& work) {
  WaitableEvent done(false, false);
  UnrecoverableErrorInfo error_info;
  scoped_refptr<WorkerTask> task(new WorkerTask(work, &done, &error_info));
  history_service_->ScheduleDBTask(task.get(), &cancelable_consumer_);
  done.Wait();
  return error_info;
}

ModelSafeGroup HistoryModelWorker::GetModelSafeGroup() {
  return GROUP_HISTORY;
}

}  // namespace browser_sync
