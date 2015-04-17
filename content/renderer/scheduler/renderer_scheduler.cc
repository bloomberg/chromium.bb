// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/renderer_scheduler.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_impl.h"
#include "content/child/scheduler/scheduler_message_loop_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/scheduler/null_renderer_scheduler.h"
#include "content/renderer/scheduler/renderer_scheduler_impl.h"

namespace content {

RendererScheduler::RendererScheduler() {
}

RendererScheduler::~RendererScheduler() {
}

// static
scoped_ptr<RendererScheduler> RendererScheduler::Create() {
  // Ensure worker.scheduler appears as an option in about://tracing
  base::trace_event::TraceLog::GetCategoryGroupEnabled(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler"));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableBlinkScheduler)) {
    return make_scoped_ptr(new NullRendererScheduler());
  } else {
    base::MessageLoop* message_loop = base::MessageLoop::current();
    return make_scoped_ptr(new RendererSchedulerImpl(
        SchedulerMessageLoopDelegate::Create(message_loop)));
  }
}

}  // namespace content
