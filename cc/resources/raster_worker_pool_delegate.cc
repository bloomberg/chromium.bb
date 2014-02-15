// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool_delegate.h"

namespace cc {

RasterWorkerPoolDelegate::RasterWorkerPoolDelegate(
    RasterWorkerPoolClient* client,
    RasterWorkerPool** raster_worker_pools,
    size_t num_raster_worker_pools)
    : client_(client),
      raster_worker_pools_(raster_worker_pools,
                           raster_worker_pools + num_raster_worker_pools),
      did_finish_running_tasks_pending_count_(0u),
      did_finish_running_tasks_required_for_activation_pending_count_(0u) {
  DCHECK(client_);
  for (RasterWorkerPoolVector::iterator it = raster_worker_pools_.begin();
       it != raster_worker_pools_.end();
       ++it)
    (*it)->SetClient(this);
}

RasterWorkerPoolDelegate::~RasterWorkerPoolDelegate() {}

// static
scoped_ptr<RasterWorkerPoolDelegate> RasterWorkerPoolDelegate::Create(
    RasterWorkerPoolClient* client,
    RasterWorkerPool** raster_worker_pools,
    size_t num_raster_worker_pools) {
  return make_scoped_ptr(new RasterWorkerPoolDelegate(
      client, raster_worker_pools, num_raster_worker_pools));
}

void RasterWorkerPoolDelegate::Shutdown() {
  for (RasterWorkerPoolVector::iterator it = raster_worker_pools_.begin();
       it != raster_worker_pools_.end();
       ++it)
    (*it)->Shutdown();
}

void RasterWorkerPoolDelegate::ScheduleTasks(
    RasterWorkerPool::RasterTask::Queue* raster_queue) {
  for (size_t i = 0; i < raster_worker_pools_.size(); ++i)
    raster_worker_pools_[i]->ScheduleTasks(&raster_queue[i]);

  did_finish_running_tasks_pending_count_ = raster_worker_pools_.size();
  did_finish_running_tasks_required_for_activation_pending_count_ =
      raster_worker_pools_.size();
}

void RasterWorkerPoolDelegate::CheckForCompletedTasks() {
  for (RasterWorkerPoolVector::iterator it = raster_worker_pools_.begin();
       it != raster_worker_pools_.end();
       ++it)
    (*it)->CheckForCompletedTasks();
}

bool RasterWorkerPoolDelegate::ShouldForceTasksRequiredForActivationToComplete()
    const {
  DCHECK(client_);
  return client_->ShouldForceTasksRequiredForActivationToComplete();
}

void RasterWorkerPoolDelegate::DidFinishRunningTasks() {
  DCHECK_LT(0u, did_finish_running_tasks_pending_count_);
  if (--did_finish_running_tasks_pending_count_)
    return;
  client_->DidFinishRunningTasks();
}

void RasterWorkerPoolDelegate::DidFinishRunningTasksRequiredForActivation() {
  DCHECK_LT(0u,
            did_finish_running_tasks_required_for_activation_pending_count_);
  if (--did_finish_running_tasks_required_for_activation_pending_count_)
    return;
  client_->DidFinishRunningTasksRequiredForActivation();
}

}  // namespace cc
