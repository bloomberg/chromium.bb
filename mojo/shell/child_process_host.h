// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_CHILD_PROCESS_HOST_H_
#define MOJO_SHELL_CHILD_PROCESS_HOST_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "mojo/shell/child_process.h"  // For |ChildProcess::Type|.
#include "mojo/system/embedder/scoped_platform_handle.h"

namespace base {
class TaskRunner;
}

namespace mojo {
namespace shell {

class Context;

// (Base) class for a "child process host". Handles launching and connecting a
// platform-specific "pipe" to the child, and supports joining the child
// process. Intended for use as a base class, but may be used on its own in
// simple cases.
//
// This class is not thread-safe. It should be created/used/destroyed on a
// single thread.
//
// Note: Does not currently work on Windows before Vista.
class ChildProcessHost {
 public:
  ChildProcessHost(Context* context, ChildProcess::Type type);
  ~ChildProcessHost();

  // |Start()|s a
  typedef base::Callback<void(bool success)> DidStartCallback;
  void Start(DidStartCallback callback);

  // Waits for the child process to terminate, and returns its exit code.
  // Note: If |Start()| has been called, this must not be called until the
  // callback has been called.
  int Join();

  embedder::ScopedPlatformHandle* platform_channel() {
    return &platform_channel_;
  }

 private:
  bool DoLaunch();

  scoped_refptr<base::TaskRunner> process_launch_task_runner_;
  const ChildProcess::Type type_;

  base::ProcessHandle child_process_handle_;

  // Platform-specific "pipe" to the child process.
  // Valid after the |Start()| callback has been completed (successfully).
  embedder::ScopedPlatformHandle platform_channel_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessHost);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_CHILD_PROCESS_HOST_H_
