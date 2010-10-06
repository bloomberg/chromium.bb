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
const int64 kTimeoutPeriod = 2000;
}

GpuWatchdogThread::GpuWatchdogThread(MessageLoop* watched_message_loop)
    : base::Thread("Watchdog"),
      watched_message_loop_(watched_message_loop) {
  DCHECK(watched_message_loop);
}

GpuWatchdogThread::~GpuWatchdogThread() {
  // Verify that the thread was explicitly stopped. If the thread is stopped
  // implicitly by the destructor, CleanUp() will not be called.
  DCHECK(!method_factory_.get());
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

void GpuWatchdogThread::OnAcknowledge() {
  // Revoke any pending OnExit.
  method_factory_->RevokeAll();

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
    // Post a task to the monitored thread that simply responds with a task that
    // calls OnAcknowldge.
    watched_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &GpuWatchdogThread::PostAcknowledge));

    // Post a task to the watchdog thread to exit if the nmonitored thread does
    // not respond in time.
    message_loop()->PostDelayedTask(
        FROM_HERE,
        method_factory_->NewRunnableMethod(&GpuWatchdogThread::OnExit),
        kTimeoutPeriod);
  }
}

void GpuWatchdogThread::PostAcknowledge() {
  // Called on the monitored thread. Responds with OnAcknowledge. Cannot use
  // the method factory. Rely on reference counting instead.
  message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &GpuWatchdogThread::OnAcknowledge));
}

// Use the --disable-gpu-watchdog command line switch to disable this.
void GpuWatchdogThread::OnExit() {
  // For minimal developer annoyance, don't keep crashing.
  static bool crashed = false;
  if (crashed)
    return;

#if defined(OS_WIN)
  if (IsDebuggerPresent())
    return;
#endif

  LOG(ERROR) << "The GPU process hung. Restarting.";

  volatile int* null_pointer = NULL;
  *null_pointer = 0xb0a710ad;  // Boatload (of cycles)

  crashed = true;
}
