// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_SERVICE_OBSERVER_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_SERVICE_OBSERVER_H_
#pragma once

#include "content/browser/worker_host/worker_process_host.h"

class WorkerServiceObserver {
 public:
  virtual void WorkerCreated(
      WorkerProcessHost* process,
      const WorkerProcessHost::WorkerInstance& instance) = 0;
  virtual void WorkerDestroyed(
      WorkerProcessHost* process,
      int worker_route_id) = 0;
  virtual void WorkerContextStarted(
      WorkerProcessHost* process,
      int worker_route_id) = 0;

 protected:
  virtual ~WorkerServiceObserver() {}
};

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_SERVICE_OBSERVER_H_
