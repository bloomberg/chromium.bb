// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "chrome/gpu/gpu_watchdog_thread.h"

#include "base/compiler_specific.h"
#include "build/build_config.h"

namespace {
const int64 kCheckPeriod = 2000;

void DoNothing() {
}
}

GpuWatchdogThread::GpuWatchdogThread(MessageLoop* watched_message_loop,
                                     int timeout)
    : base::Thread("Watchdog"),
      watched_message_loop_(watched_message_loop),
      timeout_(timeout),
      armed_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_observer_(this)) {
  DCHECK(watched_message_loop);
  DCHECK(timeout >= 0);

  watched_message_loop_->AddTaskObserver(&task_observer_);
}

GpuWatchdogThread::~GpuWatchdogThread() {
  // Verify that the thread was explicitly stopped. If the thread is stopped
  // implicitly by the destructor, CleanUp() will not be called.
  DCHECK(!method_factory_.get());

  watched_message_loop_->RemoveTaskObserver(&task_observer_);
}

void GpuWatchdogThread::PostAcknowledge() {
  // Called on the monitored thread. Responds with OnAcknowledge. Cannot use
  // the method factory. Rely on reference counting instead.
  message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &GpuWatchdogThread::OnAcknowledge));
}

void GpuWatchdogThread::Init() {
  // The method factory must be created on the watchdog thread.
  method_factory_.reset(new MethodFactory(this));

  // Schedule the first check.
  OnCheck();
}

void GpuWatchdogThread::CleanUp() {
  // The method factory must be destroyed on the watchdog thread.
  method_factory_->RevokeAll();
  method_factory_.reset();

  // Prevent any more delayed tasks from being posted.
  watched_message_loop_ = NULL;
}

GpuWatchdogThread::GpuWatchdogTaskObserver::GpuWatchdogTaskObserver(
    GpuWatchdogThread* watchdog)
  : watchdog_(watchdog) {
}

GpuWatchdogThread::GpuWatchdogTaskObserver::~GpuWatchdogTaskObserver() {
}

void GpuWatchdogThread::GpuWatchdogTaskObserver::WillProcessTask(
    const Task* task)
{
  CheckArmed();
}

void GpuWatchdogThread::GpuWatchdogTaskObserver::DidProcessTask(
    const Task* task)
{
  CheckArmed();
}

void GpuWatchdogThread::GpuWatchdogTaskObserver::CheckArmed()
{
  // Acknowledge the watchdog if it has armed itself. The watchdog will not
  // change its armed state until it is acknowledged.
  if (watchdog_->armed()) {
    watchdog_->PostAcknowledge();
  }
}

void GpuWatchdogThread::OnAcknowledge() {
  // The check has already been acknowledged and another has already been
  // scheduled by a previous call to OnAcknowledge. It is normal for a
  // watched thread to see armed_ being true multiple times before
  // the OnAcknowledge task is run on the watchdog thread.
  if (!armed_)
    return;

  // Revoke any pending OnExit.
  method_factory_->RevokeAll();
  armed_ = false;

  // The monitored thread has responded. Post a task to check it again.
  if (watched_message_loop_) {
    message_loop()->PostDelayedTask(
        FROM_HERE,
        method_factory_->NewRunnableMethod(&GpuWatchdogThread::OnCheck),
        kCheckPeriod);
  }
}

void GpuWatchdogThread::OnCheck() {
  if (watched_message_loop_) {
    // Must set armed before posting the task. This task might be the only task
    // that will activate the TaskObserver on the watched thread and it must not
    // miss the false -> true transition.
    armed_ = true;

    // Post a task to the monitored thread that does nothing but wake up the
    // TaskObserver. Any other tasks that are pending on the watched thread will
    // also wake up the observer. This simply ensures there is at least one.
    watched_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableFunction(DoNothing));

    // Post a task to the watchdog thread to exit if the monitored thread does
    // not respond in time.
    message_loop()->PostDelayedTask(
        FROM_HERE,
        method_factory_->NewRunnableMethod(&GpuWatchdogThread::OnExit),
        timeout_);
  }
}

// Use the --disable-gpu-watchdog command line switch to disable this.
void GpuWatchdogThread::OnExit() {
  // Make sure the timeout period is on the stack before crashing.
  volatile int timeout = timeout_;

  // For minimal developer annoyance, don't keep crashing.
  static bool crashed = false;
  if (crashed)
    return;

#if defined(OS_WIN)
  if (IsDebuggerPresent())
    return;
#endif

  LOG(ERROR) << "The GPU process hung. Restarting after "
             << timeout_ << " seconds.";

  volatile int* null_pointer = NULL;
  *null_pointer = timeout;

  crashed = true;
}
