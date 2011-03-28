// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/history_model_worker.h"

#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/history/history.h"

using base::WaitableEvent;

namespace browser_sync {

class WorkerTask : public HistoryDBTask {
 public:
  WorkerTask(Callback0::Type* work, WaitableEvent* done)
    : work_(work), done_(done) {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) {
    work_->Run();
    done_->Signal();
    return true;
  }

  // Since the DoWorkAndWaitUntilDone() is syncronous, we don't need to run any
  // code asynchronously on the main thread after completion.
  virtual void DoneRunOnMainThread() {}

 protected:
  Callback0::Type* work_;
  WaitableEvent* done_;
};


HistoryModelWorker::HistoryModelWorker(HistoryService* history_service)
  : history_service_(history_service) {
}

HistoryModelWorker::~HistoryModelWorker() {
}

void HistoryModelWorker::DoWorkAndWaitUntilDone(Callback0::Type* work) {
  WaitableEvent done(false, false);
  scoped_refptr<WorkerTask> task(new WorkerTask(work, &done));
  history_service_->ScheduleDBTask(task.get(), this);
  done.Wait();
}

ModelSafeGroup HistoryModelWorker::GetModelSafeGroup() {
  return GROUP_HISTORY;
}

bool HistoryModelWorker::CurrentThreadIsWorkThread() {
  // TODO(ncarter): How to determine this?
  return true;
}

}  // namespace browser_sync
