// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_process.h"

#include <string.h>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/statistics_recorder.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/threading/thread.h"
#include "base/threading/thread_local.h"
#include "build/build_config.h"
#include "content/child/child_thread_impl.h"

namespace content {

namespace {
base::LazyInstance<base::ThreadLocalPointer<ChildProcess>>::DestructorAtExit
    g_lazy_tls = LAZY_INSTANCE_INITIALIZER;
}

ChildProcess::ChildProcess(
    base::ThreadPriority io_thread_priority,
    const std::string& task_scheduler_name,
    std::unique_ptr<base::TaskScheduler::InitParams> task_scheduler_init_params)
    : ref_count_(0),
      shutdown_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
      io_thread_("Chrome_ChildIOThread") {
  DCHECK(!g_lazy_tls.Pointer()->Get());
  g_lazy_tls.Pointer()->Set(this);

  base::StatisticsRecorder::Initialize();

  // Initialize TaskScheduler if not already done. A TaskScheduler may already
  // exist when ChildProcess is instantiated in the browser process or in a
  // test process.
  if (!base::TaskScheduler::GetInstance()) {
    if (task_scheduler_init_params) {
      base::TaskScheduler::Create(task_scheduler_name);
      base::TaskScheduler::GetInstance()->Start(
          *task_scheduler_init_params.get());
    } else {
      base::TaskScheduler::CreateAndStartWithDefaultParams(task_scheduler_name);
    }

    DCHECK(base::TaskScheduler::GetInstance());
    initialized_task_scheduler_ = true;
  }

  // We can't recover from failing to start the IO thread.
  base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
  thread_options.priority = io_thread_priority;
#if defined(OS_ANDROID)
  // TODO(reveman): Remove this in favor of setting it explicitly for each type
  // of process.
  thread_options.priority = base::ThreadPriority::DISPLAY;
#endif
  CHECK(io_thread_.StartWithOptions(thread_options));
}

ChildProcess::~ChildProcess() {
  DCHECK(g_lazy_tls.Pointer()->Get() == this);

  // Signal this event before destroying the child process.  That way all
  // background threads can cleanup.
  // For example, in the renderer the RenderThread instances will be able to
  // notice shutdown before the render process begins waiting for them to exit.
  shutdown_event_.Signal();

  if (main_thread_) {  // null in unittests.
    main_thread_->Shutdown();
    if (main_thread_->ShouldBeDestroyed()) {
      main_thread_.reset();
    } else {
      // Leak the main_thread_. See a comment in
      // RenderThreadImpl::ShouldBeDestroyed.
      main_thread_.release();
    }
  }

  g_lazy_tls.Pointer()->Set(NULL);
  io_thread_.Stop();

  if (initialized_task_scheduler_) {
    DCHECK(base::TaskScheduler::GetInstance());
    base::TaskScheduler::GetInstance()->Shutdown();
  }
}

ChildThreadImpl* ChildProcess::main_thread() {
  return main_thread_.get();
}

void ChildProcess::set_main_thread(ChildThreadImpl* thread) {
  main_thread_.reset(thread);
}

void ChildProcess::AddRefProcess() {
  DCHECK(!main_thread_.get() ||  // null in unittests.
         main_thread_->message_loop()->task_runner()->BelongsToCurrentThread());
  ref_count_++;
}

void ChildProcess::ReleaseProcess() {
  DCHECK(!main_thread_.get() ||  // null in unittests.
         main_thread_->message_loop()->task_runner()->BelongsToCurrentThread());
  DCHECK(ref_count_);
  if (--ref_count_)
    return;

  if (main_thread_)  // null in unittests.
    main_thread_->OnProcessFinalRelease();
}

#if defined(OS_LINUX)
void ChildProcess::SetIOThreadPriority(
    base::ThreadPriority io_thread_priority) {
  main_thread_->SetThreadPriority(io_thread_.GetThreadId(), io_thread_priority);
}
#endif

ChildProcess* ChildProcess::current() {
  return g_lazy_tls.Pointer()->Get();
}

base::WaitableEvent* ChildProcess::GetShutDownEvent() {
  return &shutdown_event_;
}

}  // namespace content
