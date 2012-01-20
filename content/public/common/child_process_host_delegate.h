// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PULIC_COMMON_CHILD_PROCESS_HOST_DELEGATE_H_
#define CONTENT_PULIC_COMMON_CHILD_PROCESS_HOST_DELEGATE_H_
#pragma once

#include <string>

#include "ipc/ipc_channel.h"

namespace content {

// Interface that all users of ChildProcessHost need to provide.
class ChildProcessHostDelegate : public IPC::Channel::Listener {
 public:
  virtual ~ChildProcessHostDelegate() {}

  // Derived classes return true if it's ok to shut down the child process.
  // Normally they would return true. The exception is if the host is in the
  // middle of sending a request to the process, in which case the other side
  // might think it's ok to shutdown, when really it's not.
  virtual bool CanShutdown() = 0;

  // Notifies the derived class that we told the child process to kill itself.
  virtual void ShutdownStarted() = 0;

  // Called when the child process unexpected closes the IPC channel. Delegates
  // would normally delete the object in this case.
  virtual void OnChildDisconnected() = 0;
};

};  // namespace content

#endif  // CONTENT_PULIC_COMMON_CHILD_PROCESS_HOST_DELEGATE_H_
