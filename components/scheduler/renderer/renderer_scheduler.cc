// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/renderer_scheduler.h"

#include "base/message_loop/message_loop.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_impl.h"
#include "components/scheduler/child/scheduler_task_runner_delegate_impl.h"
#include "components/scheduler/renderer/renderer_scheduler_impl.h"

namespace scheduler {

RendererScheduler::RendererScheduler() {
}

RendererScheduler::~RendererScheduler() {
}

// static
scoped_ptr<RendererScheduler> RendererScheduler::Create() {
  // Ensure worker.scheduler, worker.scheduler.debug and
  // renderer.scheduler.debug appear as an option in about://tracing
  base::trace_event::TraceLog::GetCategoryGroupEnabled(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler"));
  base::trace_event::TraceLog::GetCategoryGroupEnabled(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler.debug"));
  base::trace_event::TraceLog::GetCategoryGroupEnabled(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler.debug"));

  base::MessageLoop* message_loop = base::MessageLoop::current();
  return make_scoped_ptr(new RendererSchedulerImpl(
      SchedulerTaskRunnerDelegateImpl::Create(message_loop)));
}

}  // namespace scheduler
