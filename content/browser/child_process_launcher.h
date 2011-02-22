// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_H_
#define CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_H_
#pragma once

#include "base/basictypes.h"
#include "base/process_util.h"
#include "base/ref_counted.h"

class CommandLine;

// Launches a process asynchronously and notifies the client of the process
// handle when it's available.  It's used to avoid blocking the calling thread
// on the OS since often it can take > 100 ms to create the process.
class ChildProcessLauncher {
 public:
  class Client {
   public:
    // Will be called on the thread that the ChildProcessLauncher was
    // constructed on.
    virtual void OnProcessLaunched() = 0;

   protected:
    virtual ~Client() {}
  };

  // Launches the process asynchronously, calling the client when the result is
  // ready.  Deleting this object before the process is created is safe, since
  // the callback won't be called.  If the process is still running by the time
  // this object destructs, it will be terminated.
  // Takes ownership of cmd_line.
  ChildProcessLauncher(
#if defined(OS_WIN)
      const FilePath& exposed_dir,
#elif defined(OS_POSIX)
      bool use_zygote,
      const base::environment_vector& environ,
      int ipcfd,
#endif
      CommandLine* cmd_line,
      Client* client);
  ~ChildProcessLauncher();

  // True if the process is being launched and so the handle isn't available.
  bool IsStarting();

  // Getter for the process handle.  Only call after the process has started.
  base::ProcessHandle GetHandle();

  // Call this when the child process exits to know what happened to
  // it.  |exit_code| is the exit code of the process if it exited
  // (e.g. status from waitpid if on posix, from GetExitCodeProcess on
  // Windows). |exit_code| may be NULL.
  base::TerminationStatus GetChildTerminationStatus(int* exit_code);

  // Changes whether the process runs in the background or not.  Only call
  // this after the process has started.
  void SetProcessBackgrounded(bool background);

 private:
  class Context;

  scoped_refptr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessLauncher);
};

#endif  // CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_H_
