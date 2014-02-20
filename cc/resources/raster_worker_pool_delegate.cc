// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool_delegate.h"

#include "base/message_loop/message_loop_proxy.h"

namespace cc {

RasterWorkerPoolDelegate::RasterWorkerPoolDelegate(
    RasterWorkerPoolClient* client,
    RasterWorkerPool** raster_worker_pools,
    size_t num_raster_worker_pools)
    : client_(client),
      raster_worker_pools_(raster_worker_pools,
                           raster_worker_pools + num_raster_worker_pools),
      did_finish_running_tasks_pending_(num_raster_worker_pools, false),
      did_finish_running_tasks_pending_count_(0u),
      did_finish_running_tasks_required_for_activation_pending_count_(0u),
      run_did_finish_running_tasks_pending_(false),
      weak_ptr_factory_(this) {
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

void RasterWorkerPoolDelegate::ScheduleTasks(RasterTaskQueue* raster_queue) {
  size_t did_finish_running_tasks_pending_count = 0u;
  for (size_t i = 0u; i < raster_worker_pools_.size(); ++i) {
    // Empty queue doesn't have to be scheduled unless raster worker pool has
    // pending DidFinishRunningTasks notifications.
    if (raster_queue[i].items.empty() && !did_finish_running_tasks_pending_[i])
      continue;

    did_finish_running_tasks_pending_[i] = true;
    ++did_finish_running_tasks_pending_count;
    raster_worker_pools_[i]->ScheduleTasks(&raster_queue[i]);
  }

  did_finish_running_tasks_pending_count_ =
      did_finish_running_tasks_pending_count;
  did_finish_running_tasks_required_for_activation_pending_count_ =
      did_finish_running_tasks_pending_count;

  // Need to schedule a call to RunDidFinishRunningTasks() when no
  // DidFinishRunningTasks notifications are expected from raster worker pool
  // instances.
  if (!did_finish_running_tasks_pending_count)
    ScheduleRunDidFinishRunningTasks();
}

void RasterWorkerPoolDelegate::CheckForCompletedTasks() {
  for (RasterWorkerPoolVector::iterator it = raster_worker_pools_.begin();
       it != raster_worker_pools_.end();
       ++it)
    (*it)->CheckForCompletedTasks();
}

bool RasterWorkerPoolDelegate::ShouldForceTasksRequiredForActivationToComplete()
    const {
  return client_->ShouldForceTasksRequiredForActivationToComplete();
}

void RasterWorkerPoolDelegate::DidFinishRunningTasks() {
  DCHECK_LT(0u, did_finish_running_tasks_pending_count_);
  --did_finish_running_tasks_pending_count_;
  RunDidFinishRunningTasks();
}

void RasterWorkerPoolDelegate::DidFinishRunningTasksRequiredForActivation() {
  DCHECK_LT(0u,
            did_finish_running_tasks_required_for_activation_pending_count_);
  --did_finish_running_tasks_required_for_activation_pending_count_;
  RunDidFinishRunningTasks();
}

void RasterWorkerPoolDelegate::ScheduleRunDidFinishRunningTasks() {
  if (run_did_finish_running_tasks_pending_)
    return;

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&RasterWorkerPoolDelegate::OnRunDidFinishRunningTasks,
                 weak_ptr_factory_.GetWeakPtr()));
  run_did_finish_running_tasks_pending_ = true;
}

void RasterWorkerPoolDelegate::OnRunDidFinishRunningTasks() {
  DCHECK(run_did_finish_running_tasks_pending_);
  run_did_finish_running_tasks_pending_ = false;
  RunDidFinishRunningTasks();
}

void RasterWorkerPoolDelegate::RunDidFinishRunningTasks() {
  if (!did_finish_running_tasks_required_for_activation_pending_count_)
    client_->DidFinishRunningTasksRequiredForActivation();

  if (!did_finish_running_tasks_pending_count_)
    client_->DidFinishRunningTasks();

  // Reset |run_did_finish_running_tasks_pending_| vector if no notifications
  // are pending.
  if (!did_finish_running_tasks_required_for_activation_pending_count_ &&
      !did_finish_running_tasks_pending_count_) {
    did_finish_running_tasks_pending_.assign(
        did_finish_running_tasks_pending_.size(), false);
  }
}

}  // namespace cc
