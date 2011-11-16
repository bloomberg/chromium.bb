// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_CHANNEL_H_
#define CONTENT_COMMON_GPU_GPU_CHANNEL_H_
#pragma once

#include <deque>
#include <string>

#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "build/build_config.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/message_router.h"
#include "ipc/ipc_sync_channel.h"
#include "ui/gfx/gl/gl_share_group.h"
#include "ui/gfx/gl/gpu_preference.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

class GpuChannelManager;
struct GPUCreateCommandBufferConfig;
class GpuWatchdog;
class TransportTexture;

namespace base {
class MessageLoopProxy;
class WaitableEvent;
}

// Encapsulates an IPC channel between the GPU process and one renderer
// process. On the renderer side there's a corresponding GpuChannelHost.
class GpuChannel : public IPC::Channel::Listener,
                   public IPC::Message::Sender,
                   public base::RefCountedThreadSafe<GpuChannel> {
 public:
  // Takes ownership of the renderer process handle.
  GpuChannel(GpuChannelManager* gpu_channel_manager,
             GpuWatchdog* watchdog,
             int renderer_id,
             bool software);
  virtual ~GpuChannel();

  bool Init(base::MessageLoopProxy* io_message_loop,
            base::WaitableEvent* shutdown_event);

  // Get the GpuChannelManager that owns this channel.
  GpuChannelManager* gpu_channel_manager() const {
    return gpu_channel_manager_;
  }

  // Returns the name of the associated IPC channel.
  std::string GetChannelName();

#if defined(OS_POSIX)
  int TakeRendererFileDescriptor();
#endif  // defined(OS_POSIX)

  base::ProcessHandle renderer_process() const {
    return renderer_process_;
  }

  // IPC::Channel::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  // Whether this channel is able to handle IPC messages.
  bool IsScheduled();

  // This is called when a command buffer transitions from the unscheduled
  // state to the scheduled state, which potentially means the channel
  // transitions from the unscheduled to the scheduled state. When this occurs
  // deferred IPC messaged are handled.
  void OnScheduled();

  void CreateViewCommandBuffer(
      gfx::PluginWindowHandle window,
      int32 render_view_id,
      const GPUCreateCommandBufferConfig& init_params,
      int32* route_id);

  gfx::GLShareGroup* share_group() const { return share_group_.get(); }

  GpuCommandBufferStub* LookupCommandBuffer(int32 route_id);

  void LoseAllContexts();

  // Destroy channel and all contained contexts.
  void DestroySoon();

  // Get the TransportTexture by ID.
  TransportTexture* GetTransportTexture(int32 route_id);

  // Destroy the TransportTexture by ID. This method is only called by
  // TransportTexture to delete and detach itself.
  void DestroyTransportTexture(int32 route_id);

  // Generate a route ID guaranteed to be unique for this channel.
  int GenerateRouteID();

  // Called to add/remove a listener for a particular message routing ID.
  void AddRoute(int32 route_id, IPC::Channel::Listener* listener);
  void RemoveRoute(int32 route_id);

  // Indicates whether newly created contexts should prefer the
  // discrete GPU even if they would otherwise use the integrated GPU.
  bool ShouldPreferDiscreteGpu() const;

 private:
  void OnDestroy();

  bool OnControlMessageReceived(const IPC::Message& msg);

  void HandleMessage();

  // Message handlers.
  void OnInitialize(base::ProcessHandle renderer_process);
  void OnCreateOffscreenCommandBuffer(
      const gfx::Size& size,
      const GPUCreateCommandBufferConfig& init_params,
      IPC::Message* reply_message);
  void OnDestroyCommandBuffer(int32 route_id, IPC::Message* reply_message);

  void OnCreateTransportTexture(int32 context_route_id, int32 host_id);

  void OnEcho(const IPC::Message& message);

  void OnWillGpuSwitchOccur(bool is_creating_context,
                            gfx::GpuPreference gpu_preference,
                            IPC::Message* reply_message);
  void OnCloseChannel();

  void WillCreateCommandBuffer(gfx::GpuPreference gpu_preference);
  void DidDestroyCommandBuffer(gfx::GpuPreference gpu_preference);

  // The lifetime of objects of this class is managed by a GpuChannelManager.
  // The GpuChannelManager destroy all the GpuChannels that they own when they
  // are destroyed. So a raw pointer is safe.
  GpuChannelManager* gpu_channel_manager_;

  scoped_ptr<IPC::SyncChannel> channel_;

  std::deque<IPC::Message*> deferred_messages_;

  // The id of the renderer who is on the other side of the channel.
  int renderer_id_;

  // Handle to the renderer process that is on the other side of the channel.
  base::ProcessHandle renderer_process_;

  // The process id of the renderer process.
  base::ProcessId renderer_pid_;

  // Used to implement message routing functionality to CommandBuffer objects
  MessageRouter router_;

  // The share group that all contexts associated with a particular renderer
  // process use.
  scoped_refptr<gfx::GLShareGroup> share_group_;

#if defined(ENABLE_GPU)
  typedef IDMap<GpuCommandBufferStub, IDMapOwnPointer> StubMap;
  StubMap stubs_;
#endif  // defined (ENABLE_GPU)

  // A collection of transport textures created.
  typedef IDMap<TransportTexture, IDMapOwnPointer> TransportTextureMap;
  TransportTextureMap transport_textures_;

  bool log_messages_;  // True if we should log sent and received messages.
  gpu::gles2::DisallowedFeatures disallowed_features_;
  GpuWatchdog* watchdog_;
  bool software_;
  bool handle_messages_scheduled_;
  bool processed_get_state_fast_;
  int32 num_contexts_preferring_discrete_gpu_;

  ScopedRunnableMethodFactory<GpuChannel> task_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannel);
};

#endif  // CONTENT_COMMON_GPU_GPU_CHANNEL_H_
