// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_SCHEDULER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_SCHEDULER_H_

#include <list>

#include "base/callback.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT ServiceWorkerCacheScheduler {
 public:
  ServiceWorkerCacheScheduler();
  virtual ~ServiceWorkerCacheScheduler();

  // Adds the operation to the tail of the queue and starts it if the scheduler
  // is idle.
  void ScheduleOperation(const base::Closure& closure);

  // Call this after each operation completes. It cleans up the current
  // operation and starts the next.
  void CompleteOperationAndRunNext();

  // Returns true if there are any running or pending operations.
  bool ScheduledOperations() const;

 private:
  void RunOperationIfIdle();

  // The list of operations waiting on initialization.
  std::list<base::Closure> pending_operations_;
  bool operation_running_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerCacheScheduler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_SCHEDULER_H_
