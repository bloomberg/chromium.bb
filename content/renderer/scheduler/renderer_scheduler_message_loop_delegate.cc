// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/renderer_scheduler_message_loop_delegate.h"

namespace content {

// static
scoped_refptr<RendererSchedulerMessageLoopDelegate>
RendererSchedulerMessageLoopDelegate::Create(base::MessageLoop* message_loop) {
  return make_scoped_refptr(
      new RendererSchedulerMessageLoopDelegate(message_loop));
}

RendererSchedulerMessageLoopDelegate::RendererSchedulerMessageLoopDelegate(
    base::MessageLoop* message_loop)
    : message_loop_(message_loop) {
}

RendererSchedulerMessageLoopDelegate::~RendererSchedulerMessageLoopDelegate() {
}

bool RendererSchedulerMessageLoopDelegate::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return message_loop_->task_runner()->PostDelayedTask(from_here, task, delay);
}

bool RendererSchedulerMessageLoopDelegate::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return message_loop_->task_runner()->PostNonNestableDelayedTask(from_here,
                                                                  task, delay);
}

bool RendererSchedulerMessageLoopDelegate::RunsTasksOnCurrentThread() const {
  return message_loop_->task_runner()->RunsTasksOnCurrentThread();
}

bool RendererSchedulerMessageLoopDelegate::IsNested() const {
  return message_loop_->IsNested();
}

}  // namespace content
