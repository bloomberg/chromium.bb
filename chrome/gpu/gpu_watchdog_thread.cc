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

GpuWatchdogThread::GpuWatchdogThread(int timeout)
    : base::Thread("Watchdog"),
      watched_message_loop_(MessageLoop::current()),
      timeout_(timeout),
      armed_(false),
#if defined(OS_WIN)
      watched_thread_handle_(0),
      arm_cpu_time_(0),
#endif
      ALLOW_THIS_IN_INITIALIZER_LIST(task_observer_(this)) {
  DCHECK(timeout >= 0);

#if defined(OS_WIN)
  // GetCurrentThread returns a pseudo-handle that cannot be used by one thread
  // to identify another. DuplicateHandle creates a "real" handle that can be
  // used for this purpose.
  BOOL result = DuplicateHandle(GetCurrentProcess(),
                                GetCurrentThread(),
                                GetCurrentProcess(),
                                &watched_thread_handle_,
                                THREAD_QUERY_INFORMATION,
                                FALSE,
                                0);
  DCHECK(result);
#endif

  watched_message_loop_->AddTaskObserver(&task_observer_);
}

GpuWatchdogThread::~GpuWatchdogThread() {
  // Verify that the thread was explicitly stopped. If the thread is stopped
  // implicitly by the destructor, CleanUp() will not be called.
  DCHECK(!method_factory_.get());

#if defined(OS_WIN)
  CloseHandle(watched_thread_handle_);
#endif

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
  message_loop()->PostDelayedTask(
      FROM_HERE,
      method_factory_->NewRunnableMethod(&GpuWatchdogThread::OnCheck),
      kCheckPeriod);
}

#if defined(OS_WIN)
int64 GpuWatchdogThread::GetWatchedThreadTime() {
  FILETIME creation_time;
  FILETIME exit_time;
  FILETIME user_time;
  FILETIME kernel_time;
  BOOL result = GetThreadTimes(watched_thread_handle_,
                               &creation_time,
                               &exit_time,
                               &kernel_time,
                               &user_time);
  DCHECK(result);

  ULARGE_INTEGER user_time64;
  user_time64.HighPart = user_time.dwHighDateTime;
  user_time64.LowPart = user_time.dwLowDateTime;

  ULARGE_INTEGER kernel_time64;
  kernel_time64.HighPart = kernel_time.dwHighDateTime;
  kernel_time64.LowPart = kernel_time.dwLowDateTime;

  // Time is reported in units of 100 nanoseconds. Kernel and user time are
  // summed to deal with to kinds of hangs. One is where the GPU process is
  // stuck in user level, never calling into the kernel and kernel time is
  // not increasing. The other is where either the kernel hangs and never
  // returns to user level or where user level code
  // calls into kernel level repeatedly, giving up its quanta before it is
  // tracked, for example a loop that repeatedly Sleeps.
  return static_cast<int64>(
      (user_time64.QuadPart + kernel_time64.QuadPart) / 10000);
}
#endif

void GpuWatchdogThread::OnCheck() {
  if (armed_)
    return;

  // Must set armed before posting the task. This task might be the only task
  // that will activate the TaskObserver on the watched thread and it must not
  // miss the false -> true transition.
  armed_ = true;

#if defined(OS_WIN)
  arm_cpu_time_ = GetWatchedThreadTime();
#endif

  arm_absolute_time_ = base::Time::Now();

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

// Use the --disable-gpu-watchdog command line switch to disable this.
void GpuWatchdogThread::OnExit() {
#if defined(OS_WIN)
  // Defer termination until a certain amount of CPU time has elapsed on the
  // watched thread.
  int64 time_since_arm = GetWatchedThreadTime() - arm_cpu_time_;
  if (time_since_arm < timeout_) {
    message_loop()->PostDelayedTask(
        FROM_HERE,
        method_factory_->NewRunnableMethod(&GpuWatchdogThread::OnExit),
        timeout_ - time_since_arm);
    return;
  }
#endif

  // If the watchdog woke up significantly behind schedule, disarm and reset
  // the watchdog check. This is to prevent the watchdog thread from terminating
  // when a machine wakes up from sleep or hibernation, which would otherwise
  // appear to be a hang.
  if ((base::Time::Now() - arm_absolute_time_).InMilliseconds() >
      timeout_ * 2) {
    armed_ = false;
    OnCheck();
    return;
  }

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

  LOG(ERROR) << "The GPU process hung. Terminating after "
             << timeout_ << " ms.";

  volatile int* null_pointer = NULL;
  *null_pointer = timeout;

  crashed = true;
}
