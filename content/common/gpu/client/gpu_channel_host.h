// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_CHANNEL_HOST_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_CHANNEL_HOST_H_
#pragma once

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/gpu_video_decode_accelerator_host.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/message_router.h"
#include "content/public/common/gpu_info.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_sync_channel.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/gl/gpu_preference.h"

class CommandBufferProxy;
class CommandBufferProxyImpl;
struct GPUCreateCommandBufferConfig;
class GURL;
class TransportTextureService;
class MessageLoop;

namespace base {
class MessageLoopProxy;
}

namespace IPC {
class SyncMessageFilter;
}

struct GpuListenerInfo {
  GpuListenerInfo();
  ~GpuListenerInfo();

  base::WeakPtr<IPC::Channel::Listener> listener;
  scoped_refptr<base::MessageLoopProxy> loop;
};

class CONTENT_EXPORT GpuChannelHostFactory {
 public:
  virtual ~GpuChannelHostFactory() {}

  virtual bool IsMainThread() = 0;
  virtual bool IsIOThread() = 0;
  virtual MessageLoop* GetMainLoop() = 0;
  virtual scoped_refptr<base::MessageLoopProxy> GetIOLoopProxy() = 0;
  virtual base::WaitableEvent* GetShutDownEvent() = 0;
  virtual scoped_ptr<base::SharedMemory> AllocateSharedMemory(uint32 size) = 0;
  virtual int32 CreateViewCommandBuffer(
      int32 surface_id, const GPUCreateCommandBufferConfig& init_params) = 0;
  virtual GpuChannelHost* EstablishGpuChannelSync(
      content::CauseForGpuLaunch) = 0;
};

// Encapsulates an IPC channel between the client and one GPU process.
// On the GPU process side there's a corresponding GpuChannel.
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
  GpuChannelHost(GpuChannelHostFactory* factory,
                 int gpu_host_id,
                 int client_id);

  // Connect to GPU process channel.
  void Connect(const IPC::ChannelHandle& channel_handle);

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
      int32 surface_id,
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
      media::VideoCodecProfile profile,
      media::VideoDecodeAccelerator::Client* client);

  // Destroy a command buffer created by this channel.
  void DestroyCommandBuffer(CommandBufferProxy* command_buffer);

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

  GpuChannelHostFactory* factory() const { return factory_; }
  int gpu_host_id() const { return gpu_host_id_; }
  base::ProcessId gpu_pid() const { return channel_->peer_pid(); }
  int client_id() const { return client_id_; }

 private:
  friend class base::RefCountedThreadSafe<GpuChannelHost>;
  virtual ~GpuChannelHost();

  // A filter used internally to route incoming messages from the IO thread
  // to the correct message loop.
  class MessageFilter : public IPC::ChannelProxy::MessageFilter {
   public:
    explicit MessageFilter(GpuChannelHost* parent);

    void AddRoute(int route_id,
                  base::WeakPtr<IPC::Channel::Listener> listener,
                  scoped_refptr<base::MessageLoopProxy> loop);
    void RemoveRoute(int route_id);

    // IPC::ChannelProxy::MessageFilter implementation:
    virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
    virtual void OnChannelError() OVERRIDE;

   private:
    virtual ~MessageFilter();

    GpuChannelHost* parent_;

    typedef base::hash_map<int, GpuListenerInfo> ListenerMap;
    ListenerMap listeners_;
  };

  GpuChannelHostFactory* factory_;
  int client_id_;
  int gpu_host_id_;

  State state_;

  content::GPUInfo gpu_info_;

  scoped_ptr<IPC::SyncChannel> channel_;
  scoped_refptr<MessageFilter> channel_filter_;

  // Used to look up a proxy from its routing id.
  typedef base::hash_map<int, CommandBufferProxyImpl*> ProxyMap;
  ProxyMap proxies_;

  // A lock to guard against concurrent access to members like the proxies map
  // for calls from contexts that may live on the compositor or main thread.
  mutable base::Lock context_lock_;

  // A filter for sending messages from thread other than the main thread.
  scoped_refptr<IPC::SyncMessageFilter> sync_filter_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelHost);
};

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_CHANNEL_HOST_H_
