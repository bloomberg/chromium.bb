// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_MOJO_IPC_MOJO_BOOTSTRAP_H_
#define IPC_MOJO_IPC_MOJO_BOOTSTRAP_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_listener.h"
#include "third_party/mojo/src/mojo/edk/embedder/scoped_platform_handle.h"

namespace IPC {

// MojoBootstrap establishes a bootstrap pipe between two processes in
// Chrome. It creates a native IPC::Channel first, then sends one
// side of a newly created pipe to peer process. The pipe is intended
// to be wrapped by Mojo MessagePipe.
//
// Clients should implement MojoBootstrapDelegate to get the pipe
// from MojoBootstrap object.
//
// This lives on IO thread other than Create(), which can be called from
// UI thread as Channel::Create() can be.
class IPC_MOJO_EXPORT MojoBootstrap : public Listener {
 public:
  class Delegate {
   public:
    virtual void OnPipeAvailable(mojo::embedder::ScopedPlatformHandle handle,
                                 int32_t peer_pid) = 0;
    virtual void OnBootstrapError() = 0;
  };

  // Create the MojoBootstrap instance.
  // Instead of creating IPC::Channel, passs its ChannelHandle as |handle|,
  // mode as |mode|. The result is notified to passed |delegate|.
  static scoped_ptr<MojoBootstrap> Create(ChannelHandle handle,
                                          Channel::Mode mode,
                                          Delegate* delegate);

  MojoBootstrap();
  ~MojoBootstrap() override;

  // Start the handshake over the underlying platform channel.
  bool Connect();

  // GetSelfPID returns the PID associated with |channel_|.
  base::ProcessId GetSelfPID() const;

#if defined(OS_POSIX) && !defined(OS_NACL)
  int GetClientFileDescriptor() const;
  base::ScopedFD TakeClientFileDescriptor();
#endif  // defined(OS_POSIX) && !defined(OS_NACL)

 protected:
  // On MojoServerBootstrap: INITIALIZED -> WAITING_ACK -> READY
  // On MojoClientBootstrap: INITIALIZED -> READY
  // STATE_ERROR is a catch-all state that captures any observed error.
  enum State { STATE_INITIALIZED, STATE_WAITING_ACK, STATE_READY, STATE_ERROR };

  Delegate* delegate() const { return delegate_; }
  bool Send(Message* message);
  void Fail();
  bool HasFailed() const;

  State state() const { return state_; }
  void set_state(State state) { state_ = state; }

 private:
  void Init(scoped_ptr<Channel> channel, Delegate* delegate);

  // Listener implementations
  void OnBadMessageReceived(const Message& message) override;
  void OnChannelError() override;

  scoped_ptr<Channel> channel_;
  Delegate* delegate_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(MojoBootstrap);
};

}  // namespace IPC

#endif  // IPC_MOJO_IPC_MOJO_BOOTSTRAP_H_
