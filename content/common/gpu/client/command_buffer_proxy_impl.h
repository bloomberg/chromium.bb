// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_
#define CONTENT_COMMON_GPU_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_
#pragma once

#if defined(ENABLE_GPU)

#include <map>
#include <queue>
#include <string>

#include "gpu/ipc/command_buffer_proxy.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/gpu/client/gpu_video_decode_accelerator_host.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"

class GpuChannelHost;
struct GPUCommandBufferConsoleMessage;
struct GpuMemoryAllocationForRenderer;

namespace base {
class SharedMemory;
}

// Client side proxy that forwards messages synchronously to a
// CommandBufferStub.
class CommandBufferProxyImpl
    : public CommandBufferProxy,
      public IPC::Channel::Listener,
      public base::SupportsWeakPtr<CommandBufferProxyImpl> {
 public:
  typedef base::Callback<void(
      const std::string& msg, int id)> GpuConsoleMessageCallback;

  CommandBufferProxyImpl(GpuChannelHost* channel, int route_id);
  virtual ~CommandBufferProxyImpl();

  // Sends an IPC message to create a GpuVideoDecodeAccelerator. Creates and
  // returns a pointer to a GpuVideoDecodeAcceleratorHost.
  // Returns NULL on failure to create the GpuVideoDecodeAcceleratorHost.
  // Note that the GpuVideoDecodeAccelerator may still fail to be created in
  // the GPU process, even if this returns non-NULL. In this case the client is
  // notified of an error later.
  scoped_refptr<GpuVideoDecodeAcceleratorHost> CreateVideoDecoder(
      media::VideoCodecProfile profile,
      media::VideoDecodeAccelerator::Client* client);

  // IPC::Channel::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // CommandBufferProxy implementation:
  virtual int GetRouteID() const OVERRIDE;
  virtual bool Echo(const base::Closure& callback) OVERRIDE;
  virtual bool SetSurfaceVisible(bool visible) OVERRIDE;
  virtual bool DiscardBackbuffer() OVERRIDE;
  virtual bool EnsureBackbuffer() OVERRIDE;
  virtual void SetMemoryAllocationChangedCallback(
      const base::Callback<void(const GpuMemoryAllocationForRenderer&)>&
          callback) OVERRIDE;
  virtual bool SetParent(CommandBufferProxy* parent_command_buffer,
                         uint32 parent_texture_id) OVERRIDE;
  virtual void SetChannelErrorCallback(const base::Closure& callback) OVERRIDE;
  virtual void SetNotifyRepaintTask(const base::Closure& callback) OVERRIDE;
  virtual void SetOnConsoleMessageCallback(
      const GpuConsoleMessageCallback& callback) OVERRIDE;

  // CommandBuffer implementation:
  virtual bool Initialize() OVERRIDE;
  virtual State GetState() OVERRIDE;
  virtual State GetLastState() OVERRIDE;
  virtual void Flush(int32 put_offset) OVERRIDE;
  virtual State FlushSync(int32 put_offset, int32 last_known_get) OVERRIDE;
  virtual void SetGetBuffer(int32 shm_id) OVERRIDE;
  virtual void SetGetOffset(int32 get_offset) OVERRIDE;
  virtual int32 CreateTransferBuffer(size_t size, int32 id_request) OVERRIDE;
  virtual int32 RegisterTransferBuffer(base::SharedMemory* shared_memory,
                                       size_t size,
                                       int32 id_request) OVERRIDE;
  virtual void DestroyTransferBuffer(int32 id) OVERRIDE;
  virtual gpu::Buffer GetTransferBuffer(int32 handle) OVERRIDE;
  virtual void SetToken(int32 token) OVERRIDE;
  virtual void SetParseError(gpu::error::Error error) OVERRIDE;
  virtual void SetContextLostReason(
      gpu::error::ContextLostReason reason) OVERRIDE;

  // TODO(apatrick): this is a temporary optimization while skia is calling
  // ContentGLContext::MakeCurrent prior to every GL call. It saves returning 6
  // ints redundantly when only the error is needed for the
  // CommandBufferProxyImpl implementation.
  virtual gpu::error::Error GetLastError() OVERRIDE;

  GpuChannelHost* channel() const { return channel_; }

 private:
  // Send an IPC message over the GPU channel. This is private to fully
  // encapsulate the channel; all callers of this function must explicitly
  // verify that the context has not been lost.
  bool Send(IPC::Message* msg);

  // Message handlers:
  void OnUpdateState(const gpu::CommandBuffer::State& state);
  void OnNotifyRepaint();
  void OnDestroyed(gpu::error::ContextLostReason reason);
  void OnEchoAck();
  void OnConsoleMessage(const GPUCommandBufferConsoleMessage& message);
  void OnSetMemoryAllocation(const GpuMemoryAllocationForRenderer& allocation);

  // Try to read an updated copy of the state from shared memory.
  void TryUpdateState();

  // Local cache of id to transfer buffer mapping.
  typedef std::map<int32, gpu::Buffer> TransferBufferMap;
  TransferBufferMap transfer_buffers_;

  // Zero or more video decoder hosts owned by this proxy, keyed by their
  // decoder_route_id.
  typedef std::map<int, scoped_refptr<GpuVideoDecodeAcceleratorHost> > Decoders;
  Decoders video_decoder_hosts_;

  // The last cached state received from the service.
  State last_state_;

  // The shared memory area used to update state.
  gpu::CommandBufferSharedState* shared_state_;

  // |*this| is owned by |*channel_| and so is always outlived by it, so using a
  // raw pointer is ok.
  GpuChannelHost* channel_;
  int route_id_;
  unsigned int flush_count_;
  int32 last_put_offset_;

  // Tasks to be invoked in echo responses.
  std::queue<base::Closure> echo_tasks_;

  base::Closure notify_repaint_task_;

  base::Closure channel_error_callback_;

  base::Callback<void(const GpuMemoryAllocationForRenderer&)>
      memory_allocation_changed_callback_;

  GpuConsoleMessageCallback console_message_callback_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferProxyImpl);
};

#endif  // ENABLE_GPU

#endif  // CONTENT_COMMON_GPU_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_
