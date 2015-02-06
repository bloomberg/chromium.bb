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
  DCHECK(!pending_operations_.empty());
  operation_running_ = false;
  pending_operations_.pop_front();
  RunOperationIfIdle();
}

bool ServiceWorkerCacheScheduler::Empty() const {
  return pending_operations_.empty();
}

void ServiceWorkerCacheScheduler::RunOperationIfIdle() {
  DCHECK(!operation_running_ || !pending_operations_.empty());

  if (!operation_running_ && !pending_operations_.empty()) {
    operation_running_ = true;
    // TODO(jkarlin): Run multiple operations in parallel where allowed.
    pending_operations_.front().Run();
  }
}

}  // namespace content
