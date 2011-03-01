// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_HISTORY_MODEL_WORKER_H_
#define CHROME_BROWSER_SYNC_GLUE_HISTORY_MODEL_WORKER_H_
#pragma once

#include "chrome/browser/sync/engine/model_safe_worker.h"

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/ref_counted.h"
#include "content/browser/cancelable_request.h"

class HistoryService;

namespace base {
class WaitableEvent;
}

namespace browser_sync {

// A ModelSafeWorker for history models that accepts requests
// from the syncapi that need to be fulfilled on the history thread.
class HistoryModelWorker : public browser_sync::ModelSafeWorker,
                           public CancelableRequestConsumerBase {
 public:
  explicit HistoryModelWorker(HistoryService* history_service);
  virtual ~HistoryModelWorker();

  // ModelSafeWorker implementation. Called on syncapi SyncerThread.
  virtual void DoWorkAndWaitUntilDone(Callback0::Type* work);
  virtual ModelSafeGroup GetModelSafeGroup();
  virtual bool CurrentThreadIsWorkThread();

  // CancelableRequestConsumerBase implementation.
  virtual void OnRequestAdded(CancelableRequestProvider* provider,
                              CancelableRequestProvider::Handle handle) {}

  virtual void OnRequestRemoved(CancelableRequestProvider* provider,
                                CancelableRequestProvider::Handle handle) {}

  virtual void WillExecute(CancelableRequestProvider* provider,
                           CancelableRequestProvider::Handle handle) {}

  virtual void DidExecute(CancelableRequestProvider* provider,
                          CancelableRequestProvider::Handle handle) {}

 private:
  scoped_refptr<HistoryService> history_service_;
  DISALLOW_COPY_AND_ASSIGN(HistoryModelWorker);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_HISTORY_MODEL_WORKER_H_
