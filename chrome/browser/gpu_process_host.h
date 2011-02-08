// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_PROCESS_HOST_H_
#define CHROME_BROWSER_GPU_PROCESS_HOST_H_
#pragma once

#include "base/threading/non_thread_safe.h"
#include "chrome/browser/browser_child_process_host.h"

namespace IPC {
class Message;
}

class GpuProcessHost : public BrowserChildProcessHost,
                       public base::NonThreadSafe {
 public:
  // Getter for the singleton. This will return NULL on failure.
  static GpuProcessHost* Get();

  virtual bool Send(IPC::Message* msg);

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

 private:
  GpuProcessHost();
  virtual ~GpuProcessHost();

  bool EnsureInitialized();
  bool Init();

  virtual bool CanShutdown();
  virtual void OnChildDied();
  virtual void OnProcessCrashed(int exit_code);

  bool CanLaunchGpuProcess() const;
  bool LaunchGpuProcess();

  bool initialized_;
  bool initialized_successfully_;

  DISALLOW_COPY_AND_ASSIGN(GpuProcessHost);
};

#endif  // CHROME_BROWSER_GPU_PROCESS_HOST_H_
