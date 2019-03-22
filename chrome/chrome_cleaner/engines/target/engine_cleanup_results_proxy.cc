// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/engine_cleanup_results_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"

namespace chrome_cleaner {

EngineCleanupResultsProxy::EngineCleanupResultsProxy(
    mojom::EngineCleanupResultsAssociatedPtr cleanup_results_ptr,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : cleanup_results_ptr_(std::move(cleanup_results_ptr)),
      task_runner_(task_runner) {}

void EngineCleanupResultsProxy::UnbindCleanupResultsPtr() {
  cleanup_results_ptr_.reset();
}

void EngineCleanupResultsProxy::CleanupDone(uint32_t result) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&EngineCleanupResultsProxy::OnDone, this, result));
}

EngineCleanupResultsProxy::~EngineCleanupResultsProxy() = default;

void EngineCleanupResultsProxy::OnDone(uint32_t result) {
  if (!cleanup_results_ptr_.is_bound()) {
    LOG(ERROR) << "Cleanup result reported after the engine was shut down";
    return;
  }
  cleanup_results_ptr_->Done(result);
}

}  // namespace chrome_cleaner
