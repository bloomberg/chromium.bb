// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_TRUSTED_PLUGIN_CHANNEL_H_
#define COMPONENTS_NACL_RENDERER_TRUSTED_PLUGIN_CHANNEL_H_

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sync_channel.h"
#include "ppapi/c/pp_completion_callback.h"

namespace nacl {

class TrustedPluginChannel : public IPC::Listener {
 public:
  TrustedPluginChannel(const IPC::ChannelHandle& handle,
                       PP_CompletionCallback connected_callback,
                       base::WaitableEvent* waitable_event);
  virtual ~TrustedPluginChannel();

  bool Send(IPC::Message* message);

  // Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

 private:
  scoped_ptr<IPC::SyncChannel> channel_;
  PP_CompletionCallback connected_callback_;

  DISALLOW_COPY_AND_ASSIGN(TrustedPluginChannel);
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_TRUSTED_PLUGIN_CHANNEL_H_
