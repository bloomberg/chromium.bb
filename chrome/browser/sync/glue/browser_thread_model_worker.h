// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_BROWSER_THREAD_MODEL_WORKER_H_
#define CHROME_BROWSER_SYNC_GLUE_BROWSER_THREAD_MODEL_WORKER_H_
#pragma once

#include "base/callback.h"
#include "base/basictypes.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "content/browser/browser_thread.h"

namespace base {
class WaitableEvent;
}

namespace browser_sync {

// A ModelSafeWorker for models that accept requests from the syncapi that need
// to be fulfilled on a browser thread, for example autofill on the DB thread.
// TODO(sync): Try to generalize other ModelWorkers (e.g. history, etc).
class BrowserThreadModelWorker : public browser_sync::ModelSafeWorker {
 public:
  BrowserThreadModelWorker(BrowserThread::ID thread, ModelSafeGroup group);
  virtual ~BrowserThreadModelWorker();

  // ModelSafeWorker implementation. Called on the sync thread.
  virtual void DoWorkAndWaitUntilDone(Callback0::Type* work);
  virtual ModelSafeGroup GetModelSafeGroup();

 private:
  void CallDoWorkAndSignalTask(
      Callback0::Type* work, base::WaitableEvent* done);

  BrowserThread::ID thread_;
  ModelSafeGroup group_;

  DISALLOW_COPY_AND_ASSIGN(BrowserThreadModelWorker);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_BROWSER_THREAD_MODEL_WORKER_H_
