// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/renderer_scheduler.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/scheduler/null_renderer_scheduler.h"
#include "content/renderer/scheduler/renderer_scheduler_impl.h"
#include "content/renderer/scheduler/renderer_scheduler_message_loop_delegate.h"

namespace content {

RendererScheduler::RendererScheduler() {
}

RendererScheduler::~RendererScheduler() {
}

// static
scoped_ptr<RendererScheduler> RendererScheduler::Create() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableBlinkScheduler)) {
    return make_scoped_ptr(new NullRendererScheduler());
  } else {
    base::MessageLoop* message_loop = base::MessageLoop::current();
    return make_scoped_ptr(new RendererSchedulerImpl(
        RendererSchedulerMessageLoopDelegate::Create(message_loop)));
  }
}

}  // namespace content
