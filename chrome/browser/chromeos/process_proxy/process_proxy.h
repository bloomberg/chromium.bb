// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROCESS_PROXY_PROCESS_PROXY_H_
#define CHROME_BROWSER_CHROMEOS_PROCESS_PROXY_PROCESS_PROXY_H_
#pragma once

#include <cstdio>
#include <fcntl.h>
#include <signal.h>

#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/process_proxy/process_output_watcher.h"

namespace base {
class Thread;
}  // namespace base


// Proxy to a single ChromeOS process.
// This is refcounted. Note that output watcher, when it gets triggered owns a
// a callback with ref to this, so in order for this to be freed, the watcher
// must be destroyed. This is done in Close.
class ProcessProxy : public base::RefCountedThreadSafe<ProcessProxy> {
 public:
  ProcessProxy();

  // Open a process using command |command|. |pid| is set to new process' pid.
  bool Open(const std::string& command, pid_t* pid);

  // Triggers watcher object on |watch_thread|. |watch_thread| gets blocked, so
  // it should not be one of commonly used threads. It should be thread created
  // specifically for running process output watcher.
  bool StartWatchingOnThread(base::Thread* watch_thread,
                             const ProcessOutputCallback& callback);

  // Send some data to the process.
  bool Write(const std::string& text);

  // Close. Must be called if we want this to be eventually deleted.
  void Close();

 private:
  friend class base::RefCountedThreadSafe<ProcessProxy>;
  // We want this be used as ref counted object only.
  ~ProcessProxy();

  bool LaunchProcess(const std::string& command,
                     int in_fd, int out_fd, int err_fd,
                     pid_t* pid);

  // Gets called by output watcher when the process writes something to its
  // output streams.
  void OnProcessOutput(ProcessOutputType type, const std::string& output);

  bool StopWatching();

  // Methods for cleaning up pipes.
  void CloseAllPipes();
  // Expects array of 2 file descripters.
  void ClosePipe(int* pipe);
  void CloseUsedWriteFds();
  // Expects pointer to single file descriptor.
  void CloseFd(int* fd);
  void ClearAllPipes();
  // Expects array of 2 file descripters.
  void ClearPipe(int* pipe);

  bool process_launched_;
  pid_t pid_;

  bool callback_set_;
  ProcessOutputCallback callback_;

  bool watcher_started_;

  int out_pipe_[2];
  int err_pipe_[2];
  int in_pipe_[2];
  int shutdown_pipe_[2];

  DISALLOW_COPY_AND_ASSIGN(ProcessProxy);
};

#endif  // CHROME_BROWSER_CHROMEOS_PROCESS_PROXY_PROCESS_PROXY_H_
