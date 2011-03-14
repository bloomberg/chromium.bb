// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_CHANNEL_H_
#define CONTENT_GPU_GPU_CHANNEL_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/id_map.h"
#include "base/process.h"
#include "base/scoped_ptr.h"
#include "build/build_config.h"
#include "content/common/message_router.h"
#include "content/gpu/gpu_command_buffer_stub.h"
#include "ipc/ipc_sync_channel.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

class GpuThread;
struct GPUCreateCommandBufferConfig;

// Encapsulates an IPC channel between the GPU process and one renderer
// process. On the renderer side there's a corresponding GpuChannelHost.
class GpuChannel : public IPC::Channel::Listener,
                   public IPC::Message::Sender,
                   public base::RefCountedThreadSafe<GpuChannel> {
 public:
  // Takes ownership of the renderer process handle.
  GpuChannel(GpuThread* gpu_thread,
             int renderer_id);
  virtual ~GpuChannel();

  bool Init();

  // Get the GpuThread that owns this channel.
  GpuThread* gpu_thread() const { return gpu_thread_; }

  // Returns the name of the associated IPC channel.
  std::string GetChannelName();

#if defined(OS_POSIX)
  int GetRendererFileDescriptor();
#endif  // defined(OS_POSIX)

  base::ProcessHandle renderer_process() const {
    return renderer_process_;
  }

  // IPC::Channel::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();
  virtual void OnChannelConnected(int32 peer_pid);

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  void CreateViewCommandBuffer(
      gfx::PluginWindowHandle window,
      int32 render_view_id,
      const GPUCreateCommandBufferConfig& init_params,
      int32* route_id);

#if defined(OS_MACOSX)
  virtual void AcceleratedSurfaceBuffersSwapped(
      int32 route_id, uint64 swap_buffers_count);
  void DidDestroySurface(int32 renderer_route_id);

  bool IsRenderViewGone(int32 renderer_route_id);
#endif

 private:
  bool OnControlMessageReceived(const IPC::Message& msg);

  int GenerateRouteID();

  // Message handlers.
  void OnInitialize(base::ProcessHandle renderer_process);
  void OnCreateOffscreenCommandBuffer(
      int32 parent_route_id,
      const gfx::Size& size,
      const GPUCreateCommandBufferConfig& init_params,
      uint32 parent_texture_id,
      int32* route_id);
  void OnDestroyCommandBuffer(int32 route_id);

  void OnCreateVideoDecoder(int32 context_route_id,
                            int32 decoder_host_id);
  void OnDestroyVideoDecoder(int32 decoder_id);

  // The lifetime of objects of this class is managed by a GpuThread. The
  // GpuThreadss destroy all the GpuChannels that they own when they
  // are destroyed. So a raw pointer is safe.
  GpuThread* gpu_thread_;

  scoped_ptr<IPC::SyncChannel> channel_;

  // The id of the renderer who is on the other side of the channel.
  int renderer_id_;

  // Handle to the renderer process that is on the other side of the channel.
  base::ProcessHandle renderer_process_;

  // The process id of the renderer process.
  base::ProcessId renderer_pid_;

  // Used to implement message routing functionality to CommandBuffer objects
  MessageRouter router_;

#if defined(ENABLE_GPU)
  typedef IDMap<GpuCommandBufferStub, IDMapOwnPointer> StubMap;
  StubMap stubs_;

#if defined(OS_MACOSX)
  std::set<int32> destroyed_renderer_routes_;
#endif  // defined (OS_MACOSX)
#endif  // defined (ENABLE_GPU)

  bool log_messages_;  // True if we should log sent and received messages.
  gpu::gles2::DisallowedExtensions disallowed_extensions_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannel);
};

#endif  // CONTENT_GPU_GPU_CHANNEL_H_
