// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "content/gpu/gpu_watchdog_thread.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/process_util.h"
#include "base/process.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"

namespace content {
namespace {
const int64 kCheckPeriodMs = 2000;
}  // namespace

GpuWatchdogThread::GpuWatchdogThread(int timeout)
    : base::Thread("Watchdog"),
      watched_message_loop_(MessageLoop::current()),
      timeout_(base::TimeDelta::FromMilliseconds(timeout)),
      armed_(false),
#if defined(OS_WIN)
      watched_thread_handle_(0),
      arm_cpu_time_(),
#endif
      ALLOW_THIS_IN_INITIALIZER_LIST(task_observer_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
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

void GpuWatchdogThread::PostAcknowledge() {
  // Called on the monitored thread. Responds with OnAcknowledge. Cannot use
  // the method factory. Rely on reference counting instead.
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&GpuWatchdogThread::OnAcknowledge, this));
}

void GpuWatchdogThread::CheckArmed() {
  // Acknowledge the watchdog if it has armed itself. The watchdog will not
  // change its armed state until it is acknowledged.
  if (armed()) {
    PostAcknowledge();
  }
}

void GpuWatchdogThread::Init() {
  // Schedule the first check.
  OnCheck(false);
}

void GpuWatchdogThread::CleanUp() {
  weak_factory_.InvalidateWeakPtrs();
}

GpuWatchdogThread::GpuWatchdogTaskObserver::GpuWatchdogTaskObserver(
    GpuWatchdogThread* watchdog)
    : watchdog_(watchdog) {
}

GpuWatchdogThread::GpuWatchdogTaskObserver::~GpuWatchdogTaskObserver() {
}

void GpuWatchdogThread::GpuWatchdogTaskObserver::WillProcessTask(
    base::TimeTicks time_posted) {
  watchdog_->CheckArmed();
}

void GpuWatchdogThread::GpuWatchdogTaskObserver::DidProcessTask(
    base::TimeTicks time_posted) {
  watchdog_->CheckArmed();
}

GpuWatchdogThread::~GpuWatchdogThread() {
  // Verify that the thread was explicitly stopped. If the thread is stopped
  // implicitly by the destructor, CleanUp() will not be called.
  DCHECK(!weak_factory_.HasWeakPtrs());

#if defined(OS_WIN)
  CloseHandle(watched_thread_handle_);
#endif

  watched_message_loop_->RemoveTaskObserver(&task_observer_);
}

void GpuWatchdogThread::OnAcknowledge() {
  // The check has already been acknowledged and another has already been
  // scheduled by a previous call to OnAcknowledge. It is normal for a
  // watched thread to see armed_ being true multiple times before
  // the OnAcknowledge task is run on the watchdog thread.
  if (!armed_)
    return;

  // Revoke any pending hang termination.
  weak_factory_.InvalidateWeakPtrs();
  armed_ = false;

  // If it took a long time for the acknowledgement, assume the computer was
  // recently suspended.
  bool was_suspended = (base::Time::Now() > suspension_timeout_);

  // The monitored thread has responded. Post a task to check it again.
  message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&GpuWatchdogThread::OnCheck, weak_factory_.GetWeakPtr(),
          was_suspended),
      base::TimeDelta::FromMilliseconds(kCheckPeriodMs));
}

void GpuWatchdogThread::OnCheck(bool after_suspend) {
  if (armed_)
    return;

  // Must set armed before posting the task. This task might be the only task
  // that will activate the TaskObserver on the watched thread and it must not
  // miss the false -> true transition.
  armed_ = true;

#if defined(OS_WIN)
  arm_cpu_time_ = GetWatchedThreadTime();
#endif

  // Immediately after the computer is woken up from being suspended it might
  // be pretty sluggish, so allow some extra time before the next timeout.
  base::TimeDelta timeout = timeout_ * (after_suspend ? 3 : 1);
  suspension_timeout_ = base::Time::Now() + timeout * 2;

  // Post a task to the monitored thread that does nothing but wake up the
  // TaskObserver. Any other tasks that are pending on the watched thread will
  // also wake up the observer. This simply ensures there is at least one.
  watched_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&base::DoNothing));

  // Post a task to the watchdog thread to exit if the monitored thread does
  // not respond in time.
  message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(
          &GpuWatchdogThread::DeliberatelyTerminateToRecoverFromHang,
          weak_factory_.GetWeakPtr()),
      timeout);
}

// Use the --disable-gpu-watchdog command line switch to disable this.
void GpuWatchdogThread::DeliberatelyTerminateToRecoverFromHang() {
#if defined(OS_WIN)
  // Defer termination until a certain amount of CPU time has elapsed on the
  // watched thread.
  base::TimeDelta time_since_arm = GetWatchedThreadTime() - arm_cpu_time_;
  if (time_since_arm < timeout_) {
    message_loop()->PostDelayedTask(
        FROM_HERE,
        base::Bind(
            &GpuWatchdogThread::DeliberatelyTerminateToRecoverFromHang,
            weak_factory_.GetWeakPtr()),
        timeout_ - time_since_arm);
    return;
  }
#endif

  // If the watchdog woke up significantly behind schedule, disarm and reset
  // the watchdog check. This is to prevent the watchdog thread from terminating
  // when a machine wakes up from sleep or hibernation, which would otherwise
  // appear to be a hang.
  if (base::Time::Now() > suspension_timeout_) {
    armed_ = false;
    OnCheck(true);
    return;
  }

  // For minimal developer annoyance, don't keep terminating. You need to skip
  // the call to base::Process::Terminate below in a debugger for this to be
  // useful.
  static bool terminated = false;
  if (terminated)
    return;

#if defined(OS_WIN)
  if (IsDebuggerPresent())
    return;
#endif

  LOG(ERROR) << "The GPU process hung. Terminating after "
             << timeout_.InMilliseconds() << " ms.";

  bool should_crash =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kCrashOnGpuHang);

#if defined(OS_WIN)
  should_crash = true;
#endif

  if (should_crash) {
    // Deliberately crash the process to create a crash dump.
    *((volatile int*)0) = 0x1337;
  } else {
    base::Process current_process(base::GetCurrentProcessHandle());
    current_process.Terminate(RESULT_CODE_HUNG);
  }

  terminated = true;
}

#if defined(OS_WIN)
base::TimeDelta GpuWatchdogThread::GetWatchedThreadTime() {
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
  return base::TimeDelta::FromMilliseconds(static_cast<int64>(
      (user_time64.QuadPart + kernel_time64.QuadPart) / 10000));
}
#endif

}  // namespace content
