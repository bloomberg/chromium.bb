// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/database_model_worker.h"

#include "base/synchronization/waitable_event.h"
#include "content/browser/browser_thread.h"

using base::WaitableEvent;

namespace browser_sync {

void DatabaseModelWorker::DoWorkAndWaitUntilDone(Callback0::Type* work) {
  if (BrowserThread::CurrentlyOn(BrowserThread::DB)) {
    DLOG(WARNING) << "DoWorkAndWaitUntilDone called from the DB thread.";
    work->Run();
    return;
  }
  WaitableEvent done(false, false);
  if (!BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      NewRunnableMethod(this, &DatabaseModelWorker::CallDoWorkAndSignalTask,
                        work, &done))) {
    NOTREACHED() << "Failed to post task to the db thread.";
    return;
  }
  done.Wait();
}

void DatabaseModelWorker::CallDoWorkAndSignalTask(Callback0::Type* work,
                                                  WaitableEvent* done) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  work->Run();
  done->Signal();
}

ModelSafeGroup DatabaseModelWorker::GetModelSafeGroup() {
  return GROUP_DB;
}

bool DatabaseModelWorker::CurrentThreadIsWorkThread() {
  return BrowserThread::CurrentlyOn(BrowserThread::DB);
}

}  // namespace browser_sync
