// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DATABASE_MODEL_WORKER_H_
#define CHROME_BROWSER_SYNC_GLUE_DATABASE_MODEL_WORKER_H_
#pragma once

#include "base/callback.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"

namespace base {
class WaitableEvent;
}

namespace browser_sync {

// A ModelSafeWorker for database models (eg. autofill) that accepts requests
// from the syncapi that need to be fulfilled on the database thread.
class DatabaseModelWorker : public browser_sync::ModelSafeWorker {
 public:
  explicit DatabaseModelWorker() {}

  // ModelSafeWorker implementation. Called on syncapi SyncerThread.
  virtual void DoWorkAndWaitUntilDone(Callback0::Type* work);
  virtual ModelSafeGroup GetModelSafeGroup();
  virtual bool CurrentThreadIsWorkThread();

 private:
  void CallDoWorkAndSignalTask(Callback0::Type* work,
                               base::WaitableEvent* done);

  DISALLOW_COPY_AND_ASSIGN(DatabaseModelWorker);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DATABASE_MODEL_WORKER_H_
