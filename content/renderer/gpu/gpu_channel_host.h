// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_GPU_CHANNEL_HOST_H_
#define CONTENT_RENDERER_GPU_GPU_CHANNEL_HOST_H_
#pragma once

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process_util.h"
#include "base/synchronization/lock.h"
#include "content/common/message_router.h"
#include "content/public/common/gpu_info.h"
#include "content/renderer/gpu/gpu_video_decode_accelerator_host.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_sync_channel.h"
#include "ui/gfx/gl/gpu_preference.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

class CommandBufferProxy;
class GURL;
class TransportTextureService;

namespace base {
class MessageLoopProxy;
}

namespace IPC {
class SyncMessageFilter;
}

// Encapsulates an IPC channel between the renderer and one plugin process.
// On the plugin side there's a corresponding GpuChannel.
class GpuChannelHost : public IPC::Message::Sender,
                       public base::RefCountedThreadSafe<GpuChannelHost> {
 public:
  enum State {
    // Not yet connected.
    kUnconnected,
    // Ready to use.
    kConnected,
    // An error caused the host to become disconnected. Recreate channel to
    // reestablish connection.
    kLost
  };

  // Called on the render thread
  GpuChannelHost();
  virtual ~GpuChannelHost();

  // Connect to GPU process channel.
  void Connect(const IPC::ChannelHandle& channel_handle,
               base::ProcessHandle renderer_process_for_gpu);

  State state() const { return state_; }

  // Change state to kLost.
  void SetStateLost();

  // The GPU stats reported by the GPU process.
  void set_gpu_info(const content::GPUInfo& gpu_info);
  const content::GPUInfo& gpu_info() const;

  void OnChannelError();

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // Create and connect to a command buffer in the GPU process.
  CommandBufferProxy* CreateViewCommandBuffer(
      int render_view_id,
      CommandBufferProxy* share_group,
      const std::string& allowed_extensions,
      const std::vector<int32>& attribs,
      const GURL& active_url,
      gfx::GpuPreference gpu_preference);

  // Create and connect to a command buffer in the GPU process.
  CommandBufferProxy* CreateOffscreenCommandBuffer(
      const gfx::Size& size,
      CommandBufferProxy* share_group,
      const std::string& allowed_extensions,
      const std::vector<int32>& attribs,
      const GURL& active_url,
      gfx::GpuPreference gpu_preference);

  // Creates a video decoder in the GPU process.
  // Returned pointer is owned by the CommandBufferProxy for |route_id|.
  GpuVideoDecodeAcceleratorHost* CreateVideoDecoder(
      int command_buffer_route_id,
      media::VideoDecodeAccelerator::Profile profile,
      media::VideoDecodeAccelerator::Client* client);

  // Destroy a command buffer created by this channel.
  void DestroyCommandBuffer(CommandBufferProxy* command_buffer);

  TransportTextureService* transport_texture_service() {
    return transport_texture_service_.get();
  }

  // Add a route for the current message loop.
  void AddRoute(int route_id, base::WeakPtr<IPC::Channel::Listener> listener);
  void RemoveRoute(int route_id);

  // Asks the GPU process whether the creation or destruction of the
  // given command buffer on the given GPU will provoke a switch of
  // the GPU from integrated to discrete or vice versa. This requires
  // all of the GL contexts in the same share group in the GPU process
  // to be dropped.
  bool WillGpuSwitchOccur(bool is_creating_context,
                          gfx::GpuPreference gpu_preference);

  // Forcibly close the channel on the GPU process side. This will
  // cause all command buffers on this side to soon afterward start
  // registering lost contexts. It also has the side effect of setting
  // the state on this side to lost.
  void ForciblyCloseChannel();

 private:
  // An shim class for working with listeners between threads.
  // It is used to post a task to the thread that owns the listener,
  // and where it's safe to dereference the weak pointer.
  class Listener :
    public base::RefCountedThreadSafe<Listener> {
   public:
    Listener(base::WeakPtr<IPC::Channel::Listener> listener,
                       scoped_refptr<base::MessageLoopProxy> loop);
    virtual ~Listener();

    void DispatchMessage(const IPC::Message& msg);
    void DispatchError();

    scoped_refptr<base::MessageLoopProxy> loop() { return loop_; }

   private:
    base::WeakPtr<IPC::Channel::Listener> listener_;
    scoped_refptr<base::MessageLoopProxy> loop_;
  };

  // A filter used internally to route incoming messages from the IO thread
  // to the correct message loop.
  class MessageFilter : public IPC::ChannelProxy::MessageFilter {
   public:
    explicit MessageFilter(GpuChannelHost* parent);
    virtual ~MessageFilter();

    void AddRoute(int route_id,
                  base::WeakPtr<IPC::Channel::Listener> listener,
                  scoped_refptr<base::MessageLoopProxy> loop);
    void RemoveRoute(int route_id);

    // IPC::ChannelProxy::MessageFilter implementation:
    virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
    virtual void OnChannelError() OVERRIDE;

   private:
    GpuChannelHost* parent_;

    typedef base::hash_map<int,
        scoped_refptr<GpuChannelHost::Listener> > ListenerMap;
    ListenerMap listeners_;
  };

  State state_;

  content::GPUInfo gpu_info_;

  scoped_ptr<IPC::SyncChannel> channel_;
  scoped_refptr<MessageFilter> channel_filter_;

  // Used to look up a proxy from its routing id.
  typedef base::hash_map<int, CommandBufferProxy*> ProxyMap;
  ProxyMap proxies_;

  // A lock to guard against concurrent access to members like the proxies map
  // for calls from contexts that may live on the compositor or main thread.
  mutable base::Lock context_lock_;

  // This is a MessageFilter to intercept IPC messages related to transport
  // textures. These messages are routed to TransportTextureHost.
  scoped_refptr<TransportTextureService> transport_texture_service_;

  // A filter for sending messages from thread other than the main thread.
  scoped_refptr<IPC::SyncMessageFilter> sync_filter_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelHost);
};

#endif  // CONTENT_RENDERER_GPU_GPU_CHANNEL_HOST_H_
