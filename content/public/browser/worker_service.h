// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WORKER_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_WORKER_SERVICE_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class WorkerServiceObserver;

// A singleton for managing HTML5 shared web workers. These may be run in a
// separate process, since multiple renderer processes can be talking to a
// single shared worker. All the methods below can only be called on the UI
// thread.
class CONTENT_EXPORT WorkerService {
 public:
  virtual ~WorkerService() {}

  // Returns the WorkerService singleton.
  static WorkerService* GetInstance();

  // Terminates the given worker. Returns true if the process was found.
  virtual bool TerminateWorker(int process_id, int route_id) = 0;

  // Terminates all workers and notifies when complete. This is used for
  // testing when it is important to make sure that all shared worker activity
  // has stopped.
  virtual void TerminateAllWorkersForTesting(base::OnceClosure callback) = 0;

  struct WorkerInfo {
    GURL url;
    std::string name;
    int process_id;
    int route_id;
  };

  // Return information about all the currently running workers.
  virtual std::vector<WorkerInfo> GetWorkers() = 0;

  virtual void AddObserver(WorkerServiceObserver* observer) = 0;
  virtual void RemoveObserver(WorkerServiceObserver* observer) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WORKER_SERVICE_H_
