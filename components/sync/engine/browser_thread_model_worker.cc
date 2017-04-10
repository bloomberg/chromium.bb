// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/browser_thread_model_worker.h"

#include <utility>

namespace syncer {

BrowserThreadModelWorker::BrowserThreadModelWorker(
    const scoped_refptr<base::SingleThreadTaskRunner>& runner,
    ModelSafeGroup group)
    : runner_(runner), group_(group) {}

void BrowserThreadModelWorker::ScheduleWork(base::OnceClosure work) {
  if (runner_->BelongsToCurrentThread()) {
    DLOG(WARNING) << "Already on thread " << runner_;
    std::move(work).Run();
  } else {
    runner_->PostTask(FROM_HERE, std::move(work));
  }
}

ModelSafeGroup BrowserThreadModelWorker::GetModelSafeGroup() {
  return group_;
}

bool BrowserThreadModelWorker::IsOnModelThread() {
  return runner_->BelongsToCurrentThread();
}

BrowserThreadModelWorker::~BrowserThreadModelWorker() {}

}  // namespace syncer
