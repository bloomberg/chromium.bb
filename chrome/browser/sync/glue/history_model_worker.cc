// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/history_model_worker.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "base/waitable_event.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/sync/util/closure.h"

using base::WaitableEvent;

namespace browser_sync {

class WorkerTask : public HistoryDBTask {
 public:
  WorkerTask(Closure* work, WaitableEvent* done)
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
  Closure* work_;
  WaitableEvent* done_;
};


HistoryModelWorker::HistoryModelWorker(HistoryService* history_service)
  : history_service_(history_service) {
}

void HistoryModelWorker::DoWorkAndWaitUntilDone(Closure* work) {
  WaitableEvent done(false, false);
  scoped_refptr<WorkerTask> task = new WorkerTask(work, &done);
  history_service_->ScheduleDBTask(task.get(), this);
  done.Wait();
}

}  // namespace browser_sync
