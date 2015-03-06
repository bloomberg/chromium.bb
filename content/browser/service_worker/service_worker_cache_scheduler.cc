// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache_scheduler.h"

#include <string>

#include "base/logging.h"

namespace content {

ServiceWorkerCacheScheduler::ServiceWorkerCacheScheduler()
    : operation_running_(false) {
}

ServiceWorkerCacheScheduler::~ServiceWorkerCacheScheduler() {
}

void ServiceWorkerCacheScheduler::ScheduleOperation(
    const base::Closure& closure) {
  pending_operations_.push_back(closure);
  RunOperationIfIdle();
}

void ServiceWorkerCacheScheduler::CompleteOperationAndRunNext() {
  DCHECK(operation_running_);
  operation_running_ = false;
  RunOperationIfIdle();
}

bool ServiceWorkerCacheScheduler::ScheduledOperations() const {
  return operation_running_ || !pending_operations_.empty();
}

void ServiceWorkerCacheScheduler::RunOperationIfIdle() {
  if (!operation_running_ && !pending_operations_.empty()) {
    operation_running_ = true;
    // TODO(jkarlin): Run multiple operations in parallel where allowed.
    base::Closure closure = pending_operations_.front();
    pending_operations_.pop_front();
    closure.Run();
  }
}

}  // namespace content
