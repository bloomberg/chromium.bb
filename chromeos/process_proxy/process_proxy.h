// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PROCESS_PROXY_PROCESS_PROXY_H_
#define CHROMEOS_PROCESS_PROXY_PROCESS_PROXY_H_

#include <fcntl.h>
#include <signal.h>

#include <cstdio>
#include <string>

#include "base/memory/ref_counted.h"
#include "chromeos/process_proxy/process_output_watcher.h"

namespace base {
class TaskRunner;
class Thread;
}  // namespace base

namespace chromeos {

// Proxy to a single ChromeOS process.
// This is refcounted. Note that output watcher, when it gets triggered owns a
// a callback with ref to this, so in order for this to be freed, the watcher
// must be destroyed. This is done in Close.
class ProcessProxy : public base::RefCountedThreadSafe<ProcessProxy> {
 public:
  ProcessProxy();

  // Opens a process using command |command|. |pid| is set to new process' pid.
  bool Open(const std::string& command, pid_t* pid);

  // Triggers watcher object on |watch_thread|. |watch_thread| gets blocked, so
  // it should not be one of commonly used threads. It should be thread created
  // specifically for running process output watcher.
  bool StartWatchingOnThread(base::Thread* watch_thread,
                             const ProcessOutputCallback& callback);

  // Sends some data to the process.
  bool Write(const std::string& text);

  // Closes the process.
  // Must be called if we want this to be eventually deleted.
  void Close();

  // Notifies underlaying process of terminal size change.
  bool OnTerminalResize(int width, int height);

 private:
  friend class base::RefCountedThreadSafe<ProcessProxy>;
  // We want this be used as ref counted object only.
  ~ProcessProxy();

  // Create master and slave end of pseudo terminal that will be used to
  // communicate with process.
  // pt_pair[0] -> master, pt_pair[1] -> slave.
  // pt_pair must be allocated (to size at least 2).
  bool CreatePseudoTerminalPair(int *pt_pair);

  bool LaunchProcess(const std::string& command, int slave_fd, pid_t* pid);

  // Gets called by output watcher when the process writes something to its
  // output streams.
  void OnProcessOutput(ProcessOutputType type, const std::string& output);
  void CallOnProcessOutputCallback(ProcessOutputType type,
                                   const std::string& output);

  bool StopWatching();

  // Methods for cleaning up pipes.
  void CloseAllFdPairs();
  // Expects array of 2 file descripters.
  void CloseFdPair(int* pipe);
  // Expects pointer to single file descriptor.
  void CloseFd(int* fd);
  void ClearAllFdPairs();
  // Expects array of 2 file descripters.
  void ClearFdPair(int* pipe);

  bool process_launched_;
  pid_t pid_;

  bool callback_set_;
  ProcessOutputCallback callback_;
  scoped_refptr<base::TaskRunner> callback_runner_;

  bool watcher_started_;

  int pt_pair_[2];
  int shutdown_pipe_[2];

  DISALLOW_COPY_AND_ASSIGN(ProcessProxy);
};

}  // namespace chromeos

#endif  // CHROMEOS_PROCESS_PROXY_PROCESS_PROXY_H_
