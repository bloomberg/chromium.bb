// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_HISTORY_MODEL_WORKER_H_
#define CHROME_BROWSER_SYNC_GLUE_HISTORY_MODEL_WORKER_H_
#pragma once

#include "sync/internal_api/public/engine/model_safe_worker.h"

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/cancelable_request.h"

class HistoryService;

namespace browser_sync {

// A csync::ModelSafeWorker for history models that accepts requests
// from the syncapi that need to be fulfilled on the history thread.
class HistoryModelWorker : public csync::ModelSafeWorker {
 public:
  explicit HistoryModelWorker(HistoryService* history_service);

  // csync::ModelSafeWorker implementation. Called on syncapi SyncerThread.
  virtual csync::SyncerError DoWorkAndWaitUntilDone(
      const csync::WorkCallback& work) OVERRIDE;
  virtual csync::ModelSafeGroup GetModelSafeGroup() OVERRIDE;

 private:
  virtual ~HistoryModelWorker();

  scoped_refptr<HistoryService> history_service_;
  // Helper object to make sure we don't leave tasks running on the history
  // thread.
  CancelableRequestConsumerT<int, 0> cancelable_consumer_;
  DISALLOW_COPY_AND_ASSIGN(HistoryModelWorker);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_HISTORY_MODEL_WORKER_H_
