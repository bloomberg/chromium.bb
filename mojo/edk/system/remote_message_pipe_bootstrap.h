// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_REMOTE_MESSAGE_PIPE_BOOTSTRAP_H_
#define MOJO_EDK_SYSTEM_REMOTE_MESSAGE_PIPE_BOOTSTRAP_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/task_runner.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/ports/port_ref.h"

namespace mojo {
namespace edk {

class NodeController;

// This is used by Core to negotiate a new cross-process message pipe given
// an arbitrary platform channel. One end of the channel must be passed to
// CreateForParent() in the parent process, and the other end must be passed to
// CreateForChild() in the child process.
class RemoteMessagePipeBootstrap
    : public Channel::Delegate,
      public base::MessageLoop::DestructionObserver {
 public:
  ~RemoteMessagePipeBootstrap() override;

  // The NodeController should already have reserved a local port for |token|
  // before calling this method.
  static void CreateForParent(NodeController* node_controller,
                              ScopedPlatformHandle platform_handle,
                              const std::string& token);

  // |port| must be a reference to an uninitialized local port.
  static void CreateForChild(NodeController* node_controller,
                             ScopedPlatformHandle platform_handle,
                             const ports::PortRef& port,
                             const base::Closure& callback);

  // Handle a received token.
  virtual void OnTokenReceived(const std::string& token) = 0;

 protected:
  explicit RemoteMessagePipeBootstrap(ScopedPlatformHandle platform_handle);

  void ShutDown();

  bool shutting_down_ = false;
  scoped_refptr<base::TaskRunner> io_task_runner_;
  scoped_refptr<Channel> channel_;

 private:
  // base::MessageLoop::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override;

  // Channel::Delegate:
  void OnChannelMessage(const void* payload,
                        size_t payload_size,
                        ScopedPlatformHandleVectorPtr handles) override;
  void OnChannelError() override;

  void ShutDownNow();

  DISALLOW_COPY_AND_ASSIGN(RemoteMessagePipeBootstrap);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_REMOTE_MESSAGE_PIPE_BOOTSTRAP_H_
