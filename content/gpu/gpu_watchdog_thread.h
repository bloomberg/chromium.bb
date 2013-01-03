// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_WATCHDOG_THREAD_H_
#define CONTENT_GPU_GPU_WATCHDOG_THREAD_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "content/common/gpu/gpu_watchdog.h"

namespace content {

// A thread that intermitently sends tasks to a group of watched message loops
// and deliberately crashes if one of them does not respond after a timeout.
class GpuWatchdogThread : public base::Thread,
                          public GpuWatchdog,
                          public base::RefCountedThreadSafe<GpuWatchdogThread> {
 public:
  explicit GpuWatchdogThread(int timeout);

  // Accessible on watched thread but only modified by watchdog thread.
  bool armed() const { return armed_; }
  void PostAcknowledge();

  // Implement GpuWatchdog.
  virtual void CheckArmed() OVERRIDE;

 protected:
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<GpuWatchdogThread>;

  // An object of this type intercepts the reception and completion of all tasks
  // on the watched thread and checks whether the watchdog is armed.
  class GpuWatchdogTaskObserver : public MessageLoop::TaskObserver {
   public:
    explicit GpuWatchdogTaskObserver(GpuWatchdogThread* watchdog);
    virtual ~GpuWatchdogTaskObserver();

    // Implements MessageLoop::TaskObserver.
    virtual void WillProcessTask(base::TimeTicks time_posted) OVERRIDE;
    virtual void DidProcessTask(base::TimeTicks time_posted) OVERRIDE;

   private:
    GpuWatchdogThread* watchdog_;
  };

  virtual ~GpuWatchdogThread();

  void OnAcknowledge();
  void OnCheck(bool after_suspend);
  void DeliberatelyTerminateToRecoverFromHang();

#if defined(OS_WIN)
  base::TimeDelta GetWatchedThreadTime();
#endif

  MessageLoop* watched_message_loop_;
  base::TimeDelta timeout_;
  volatile bool armed_;
  GpuWatchdogTaskObserver task_observer_;

#if defined(OS_WIN)
  void* watched_thread_handle_;
  base::TimeDelta arm_cpu_time_;
#endif

  // Time after which it's assumed that the computer has been suspended since
  // the task was posted.
  base::Time suspension_timeout_;

  base::WeakPtrFactory<GpuWatchdogThread> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuWatchdogThread);
};

}  // namespace content

#endif  // CONTENT_GPU_GPU_WATCHDOG_THREAD_H_
