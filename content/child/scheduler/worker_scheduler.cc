// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/scheduler/worker_scheduler.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "content/child/scheduler/null_worker_scheduler.h"
#include "content/child/scheduler/scheduler_message_loop_delegate.h"
#include "content/child/scheduler/worker_scheduler_impl.h"
#include "content/public/common/content_switches.h"

namespace content {

WorkerScheduler::WorkerScheduler() {
}

WorkerScheduler::~WorkerScheduler() {
}

// static
scoped_ptr<WorkerScheduler> WorkerScheduler::Create(
    base::MessageLoop* message_loop) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableBlinkScheduler)) {
    return make_scoped_ptr(new NullWorkerScheduler());
  } else {
    return make_scoped_ptr(new WorkerSchedulerImpl(
        SchedulerMessageLoopDelegate::Create(message_loop)));
  }
}

}  // namespace content
