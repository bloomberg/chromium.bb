// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_MOJO_HOST_H_
#define IPC_IPC_CHANNEL_MOJO_HOST_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "ipc/ipc_export.h"
#include "ipc/mojo/ipc_channel_mojo.h"

namespace base {
class TaskRunner;
}

namespace IPC {

// Through ChannelMojoHost, ChannelMojo gets extra information that
// its client provides, including the child process's process handle. Every
// server process that uses ChannelMojo must have a ChannelMojoHost
// instance and call OnClientLaunched().
class IPC_MOJO_EXPORT ChannelMojoHost {
 public:
  explicit ChannelMojoHost(scoped_refptr<base::TaskRunner> io_task_runner);
  ~ChannelMojoHost();

  void OnClientLaunched(base::ProcessHandle process);
  ChannelMojo::Delegate* channel_delegate() const;

 private:
  class ChannelDelegate;

  // Delegate talks to ChannelMojo, whch lives in IO thread, thus
  // the Delegate should also live and dies in the IO thread as well.
  class DelegateDeleter {
   public:
    void operator()(ChannelDelegate* ptr) const;
  };

  base::WeakPtrFactory<ChannelMojoHost> weak_factory_;
  const scoped_refptr<base::TaskRunner> io_task_runner_;
  scoped_ptr<ChannelDelegate, DelegateDeleter> channel_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ChannelMojoHost);
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_MOJO_HOST_H_
