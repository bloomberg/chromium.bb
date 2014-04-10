// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/rasterizer_delegate.h"

#include "base/debug/trace_event.h"

namespace cc {

RasterizerDelegate::RasterizerDelegate(RasterizerClient* client,
                                       Rasterizer** rasterizers,
                                       size_t num_rasterizers)
    : client_(client),
      rasterizers_(rasterizers, rasterizers + num_rasterizers),
      did_finish_running_tasks_pending_count_(0u),
      did_finish_running_tasks_required_for_activation_pending_count_(0u) {
  DCHECK(client_);
  for (RasterizerVector::iterator it = rasterizers_.begin();
       it != rasterizers_.end();
       ++it)
    (*it)->SetClient(this);
}

RasterizerDelegate::~RasterizerDelegate() {}

// static
scoped_ptr<RasterizerDelegate> RasterizerDelegate::Create(
    RasterizerClient* client,
    Rasterizer** rasterizers,
    size_t num_rasterizers) {
  return make_scoped_ptr(
      new RasterizerDelegate(client, rasterizers, num_rasterizers));
}

void RasterizerDelegate::Shutdown() {
  for (RasterizerVector::iterator it = rasterizers_.begin();
       it != rasterizers_.end();
       ++it)
    (*it)->Shutdown();
}

void RasterizerDelegate::ScheduleTasks(RasterTaskQueue* queue) {
  for (size_t i = 0; i < rasterizers_.size(); ++i)
    rasterizers_[i]->ScheduleTasks(&queue[i]);

  did_finish_running_tasks_pending_count_ = rasterizers_.size();
  did_finish_running_tasks_required_for_activation_pending_count_ =
      rasterizers_.size();
}

void RasterizerDelegate::CheckForCompletedTasks() {
  for (RasterizerVector::iterator it = rasterizers_.begin();
       it != rasterizers_.end();
       ++it)
    (*it)->CheckForCompletedTasks();
}

bool RasterizerDelegate::ShouldForceTasksRequiredForActivationToComplete()
    const {
  return client_->ShouldForceTasksRequiredForActivationToComplete();
}

void RasterizerDelegate::DidFinishRunningTasks() {
  TRACE_EVENT1("cc",
               "RasterizerDelegate::DidFinishRunningTasks",
               "pending_count",
               did_finish_running_tasks_pending_count_);

  DCHECK_LT(0u, did_finish_running_tasks_pending_count_);
  if (--did_finish_running_tasks_pending_count_)
    return;
  client_->DidFinishRunningTasks();
}

void RasterizerDelegate::DidFinishRunningTasksRequiredForActivation() {
  TRACE_EVENT1("cc",
               "RasterizerDelegate::DidFinishRunningTasksRequiredForActivation",
               "pending_count",
               did_finish_running_tasks_required_for_activation_pending_count_);

  DCHECK_LT(0u,
            did_finish_running_tasks_required_for_activation_pending_count_);
  if (--did_finish_running_tasks_required_for_activation_pending_count_)
    return;
  client_->DidFinishRunningTasksRequiredForActivation();
}

}  // namespace cc
