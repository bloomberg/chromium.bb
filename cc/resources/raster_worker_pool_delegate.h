// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RASTER_WORKER_POOL_DELEGATE_H_
#define CC_RESOURCES_RASTER_WORKER_POOL_DELEGATE_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "cc/resources/raster_worker_pool.h"

namespace cc {

class RasterWorkerPoolDelegate : public RasterWorkerPoolClient {
 public:
  virtual ~RasterWorkerPoolDelegate();

  static scoped_ptr<RasterWorkerPoolDelegate> Create(
      RasterWorkerPoolClient* client,
      base::SequencedTaskRunner* task_runner,
      RasterWorkerPool** raster_worker_pools,
      size_t num_raster_worker_pools);

  void SetClient(RasterWorkerPoolClient* client);
  void Shutdown();
  void ScheduleTasks(RasterTaskQueue* raster_queue);
  void CheckForCompletedTasks();

  // Overriden from RasterWorkerPoolClient:
  virtual bool ShouldForceTasksRequiredForActivationToComplete() const OVERRIDE;
  virtual void DidFinishRunningTasks() OVERRIDE;
  virtual void DidFinishRunningTasksRequiredForActivation() OVERRIDE;

 private:
  RasterWorkerPoolDelegate(RasterWorkerPoolClient* client,
                           base::SequencedTaskRunner* task_runner,
                           RasterWorkerPool** raster_worker_pools,
                           size_t num_raster_worker_pools);

  void ScheduleRunDidFinishRunningTasks();
  void OnRunDidFinishRunningTasks();
  void RunDidFinishRunningTasks();

  RasterWorkerPoolClient* client_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  typedef std::vector<RasterWorkerPool*> RasterWorkerPoolVector;
  RasterWorkerPoolVector raster_worker_pools_;
  std::vector<bool> has_scheduled_tasks_;
  size_t did_finish_running_tasks_pending_count_;
  size_t did_finish_running_tasks_required_for_activation_pending_count_;
  bool run_did_finish_running_tasks_pending_;
  base::WeakPtrFactory<RasterWorkerPoolDelegate> weak_ptr_factory_;
};

}  // namespace cc

#endif  // CC_RESOURCES_RASTER_WORKER_POOL_DELEGATE_H_
