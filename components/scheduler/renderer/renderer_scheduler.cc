// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/renderer_scheduler.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/message_loop/message_loop.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_impl.h"
#include "components/scheduler/child/scheduler_tqm_delegate_impl.h"
#include "components/scheduler/child/virtual_time_tqm_delegate.h"
#include "components/scheduler/common/scheduler_switches.h"
#include "components/scheduler/renderer/renderer_scheduler_impl.h"

namespace scheduler {
namespace {
const base::Feature kExpensiveTaskBlockingPolicyFeature{
    "SchedulerExpensiveTaskBlocking", base::FEATURE_DISABLED_BY_DEFAULT};
}

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

  scoped_ptr<RendererSchedulerImpl> scheduler;
  base::MessageLoop* message_loop = base::MessageLoop::current();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableVirtualizedTime)) {
    scheduler.reset(new RendererSchedulerImpl(
        VirtualTimeTqmDelegate::Create(message_loop, base::TimeTicks::Now())));
  } else {
    scheduler.reset(new RendererSchedulerImpl(SchedulerTqmDelegateImpl::Create(
        message_loop, make_scoped_ptr(new base::DefaultTickClock()))));
  }

  // Runtime features are not currently available in html_viewer.
  if (base::FeatureList::GetInstance()) {
    scheduler->SetExpensiveTaskBlockingAllowed(
        base::FeatureList::IsEnabled(kExpensiveTaskBlockingPolicyFeature));
  }
  return make_scoped_ptr<RendererScheduler>(scheduler.release());
}

// static
const char* RendererScheduler::UseCaseToString(UseCase use_case) {
  switch (use_case) {
    case UseCase::NONE:
      return "none";
    case UseCase::COMPOSITOR_GESTURE:
      return "compositor_gesture";
    case UseCase::MAIN_THREAD_GESTURE:
      return "main_thread_gesture";
    case UseCase::SYNCHRONIZED_GESTURE:
      return "synchronized_gesture";
    case UseCase::TOUCHSTART:
      return "touchstart";
    case UseCase::LOADING:
      return "loading";
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
const char* RendererScheduler::InputEventStateToString(
    InputEventState input_event_state) {
  switch (input_event_state) {
    case InputEventState::EVENT_CONSUMED_BY_COMPOSITOR:
      return "event_consumed_by_compositor";
    case InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD:
      return "event_forwarded_to_main_thread";
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace scheduler
