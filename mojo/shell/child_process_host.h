// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHELL_CHILD_PROCESS_HOST_H_
#define SHELL_CHILD_PROCESS_HOST_H_

#include "base/macros.h"
#include "base/process/process.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/shell/child_process.h"  // For |ChildProcess::Type|.

namespace mojo {
namespace shell {

class Context;

// (Base) class for a "child process host". Handles launching and connecting a
// platform-specific "pipe" to the child, and supports joining the child
// process.
//
// This class is not thread-safe. It should be created/used/destroyed on a
// single thread.
//
// Note: Does not currently work on Windows before Vista.
class ChildProcessHost {
 public:
  explicit ChildProcessHost(Context* context);
  virtual ~ChildProcessHost();

  // |Start()|s the child process; calls |DidStart()| (on the thread on which
  // |Start()| was called) when the child has been started (or failed to start).
  // After calling |Start()|, this object must not be destroyed until
  // |DidStart()| has been called.
  // TODO(vtl): Consider using weak pointers and removing this requirement.
  void Start();

  // Waits for the child process to terminate, and returns its exit code.
  // Note: If |Start()| has been called, this must not be called until the
  // callback has been called.
  int Join();

  embedder::ScopedPlatformHandle* platform_channel() {
    return &platform_channel_;
  }

  virtual void WillStart() = 0;
  virtual void DidStart(bool success) = 0;

 protected:
  Context* context() const { return context_; }

 private:
  bool DoLaunch();

  Context* const context_;

  base::Process child_process_;

  embedder::PlatformChannelPair platform_channel_pair_;

  // Platform-specific "pipe" to the child process. Valid immediately after
  // creation.
  embedder::ScopedPlatformHandle platform_channel_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessHost);
};

}  // namespace shell
}  // namespace mojo

#endif  // SHELL_CHILD_PROCESS_HOST_H_
