// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_WATCHDOG_THREAD_H_
#define CHROME_GPU_GPU_WATCHDOG_THREAD_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/thread.h"

// A thread that intermitently sends tasks to a group of watched message loops
// and deliberately crashes if one of them does not respond after a timeout.
class GpuWatchdogThread : public base::Thread,
                          public base::RefCountedThreadSafe<GpuWatchdogThread> {
 public:
  GpuWatchdogThread(MessageLoop* watched_message_loop, int timeout);
  virtual ~GpuWatchdogThread();

 protected:
  virtual void Init();
  virtual void CleanUp();

 private:
  void OnAcknowledge();
  void OnCheck();
  void PostAcknowledge();
  void OnExit();
  void Disable();

  MessageLoop* watched_message_loop_;
  int timeout_;

  typedef ScopedRunnableMethodFactory<GpuWatchdogThread> MethodFactory;
  scoped_ptr<MethodFactory> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuWatchdogThread);
};

#endif  // CHROME_GPU_GPU_WATCHDOG_THREAD_H_
