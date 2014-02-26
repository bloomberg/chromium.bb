// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NACL_TRUSTED_LISTENER_H_
#define COMPONENTS_NACL_LOADER_NACL_TRUSTED_LISTENER_H_

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/synchronization/waitable_event.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sync_channel.h"

class NaClTrustedListener : public base::RefCounted<NaClTrustedListener>,
                            public IPC::Listener {
 public:
  NaClTrustedListener(const IPC::ChannelHandle& handle,
                      base::MessageLoopProxy* message_loop_proxy,
                      base::WaitableEvent* shutdown_event);

#if defined(OS_POSIX)
  int TakeClientFileDescriptor();
#endif

  // Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  bool Send(IPC::Message* msg);

 private:
  friend class base::RefCounted<NaClTrustedListener>;
  virtual ~NaClTrustedListener();
  scoped_ptr<IPC::SyncChannel> channel_;

  DISALLOW_COPY_AND_ASSIGN(NaClTrustedListener);
};

#endif  // COMPONENTS_NACL_LOADER_NACL_TRUSTED_LISTENER_H_
