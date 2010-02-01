// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/database_model_worker.h"

#include "chrome/browser/chrome_thread.h"

using base::WaitableEvent;

namespace browser_sync {

void DatabaseModelWorker::DoWorkAndWaitUntilDone(Closure* work) {
  if (ChromeThread::CurrentlyOn(ChromeThread::DB)) {
    DLOG(WARNING) << "DoWorkAndWaitUntilDone called from the DB thread.";
    work->Run();
    return;
  }
  WaitableEvent done(false, false);
  if (!ChromeThread::PostTask(ChromeThread::DB, FROM_HERE,
      NewRunnableMethod(this, &DatabaseModelWorker::CallDoWorkAndSignalTask,
                        work, &done))) {
    NOTREACHED() << "Failed to post task to the db thread.";
    return;
  }
  done.Wait();
}

void DatabaseModelWorker::CallDoWorkAndSignalTask(Closure* work,
                                                  WaitableEvent* done) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  work->Run();
  done->Signal();
}

}  // namespace browser_sync
