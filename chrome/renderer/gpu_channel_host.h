// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_GPU_CHANNEL_HOST_H_
#define CHROME_RENDERER_GPU_CHANNEL_HOST_H_

#include <string>

#include "base/hash_tables.h"
#include "chrome/common/message_router.h"
#include "gfx/native_widget_types.h"
#include "gfx/size.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sync_channel.h"

class CommandBufferProxy;

// Encapsulates an IPC channel between the renderer and one plugin process.
// On the plugin side there's a corresponding GpuChannel.
class GpuChannelHost : public IPC::Channel::Listener,
                       public IPC::Message::Sender,
                       public base::RefCountedThreadSafe<GpuChannelHost> {
 public:
  enum State {
    // Not yet connected.
    UNCONNECTED,
    // Ready to use.
    CONNECTED,
    // An error caused the host to become disconnected. Recreate channel to
    // reestablish connection.
    LOST
  };

  // Called on the render thread
  GpuChannelHost();
  ~GpuChannelHost();

  // Connect to GPU process channel.
  void Connect(const std::string& channel_name);

  State state() const { return state_; }

  // Returns whether the channel to the GPU process is ready.
  bool ready() const { return channel_.get() != NULL; }

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  // Create and connect to a command buffer in the GPU process.
  CommandBufferProxy* CreateViewCommandBuffer(gfx::NativeViewId view);

  // Create and connect to a command buffer in the GPU process.
  CommandBufferProxy* CreateOffscreenCommandBuffer(CommandBufferProxy* parent,
                                                   const gfx::Size& size,
                                                   uint32 parent_texture_id);


  // Destroy a command buffer created by this channel.
  void DestroyCommandBuffer(CommandBufferProxy* command_buffer);

 private:
  State state_;

  scoped_ptr<IPC::SyncChannel> channel_;

  // Used to implement message routing functionality to CommandBufferProxy
  // objects
  MessageRouter router_;

  // Keep track of all the registered CommandBufferProxies to
  // inform about OnChannelError
  typedef base::hash_map<int, IPC::Channel::Listener*> ProxyMap;
  ProxyMap proxies_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelHost);
};

#endif  // CHROME_RENDERER_GPU_CHANNEL_HOST_H_
