// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_CHANNEL_H_
#define CHROME_GPU_GPU_CHANNEL_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/id_map.h"
#include "base/scoped_open_process.h"
#include "base/scoped_ptr.h"
#include "build/build_config.h"
#include "chrome/common/gpu_create_command_buffer_config.h"
#include "chrome/common/gpu_video_common.h"
#include "chrome/common/message_router.h"
#include "chrome/gpu/gpu_command_buffer_stub.h"
#include "gfx/native_widget_types.h"
#include "gfx/size.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sync_channel.h"

// Encapsulates an IPC channel between the GPU process and one renderer
// process. On the renderer side there's a corresponding GpuChannelHost.
class GpuChannel : public IPC::Channel::Listener,
                   public IPC::Message::Sender,
                   public base::RefCountedThreadSafe<GpuChannel> {
 public:
  explicit GpuChannel(int renderer_id);
  virtual ~GpuChannel();

  bool Init();

  std::string GetChannelName();

#if defined(OS_POSIX)
  int GetRendererFileDescriptor();
#endif  // defined(OS_POSIX)

  base::ProcessHandle renderer_handle() const {
    return renderer_process_.handle();
  }

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

#if defined(OS_MACOSX)
  virtual void AcceleratedSurfaceBuffersSwapped(
      int32 route_id, uint64 swap_buffers_count);
  void DidDestroySurface(int32 renderer_route_id);

  bool IsRenderViewGone(int32 renderer_route_id);
#endif

 private:
  void OnControlMessageReceived(const IPC::Message& msg);

  int GenerateRouteID();

  // Message handlers.
  void OnCreateViewCommandBuffer(
      gfx::NativeViewId view,
      int32 render_view_id,
      const GPUCreateCommandBufferConfig& init_params,
      int32* route_id);
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

  scoped_ptr<IPC::SyncChannel> channel_;

  // Handle to the renderer process who is on the other side of the channel.
  base::ScopedOpenProcess renderer_process_;

  // The id of the renderer who is on the other side of the channel.
  int renderer_id_;

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

  DISALLOW_COPY_AND_ASSIGN(GpuChannel);
};

#endif  // CHROME_GPU_GPU_CHANNEL_H_
