// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMMAND_BUFFER_PROXY_H_
#define CONTENT_RENDERER_GPU_COMMAND_BUFFER_PROXY_H_
#pragma once

#if defined(ENABLE_GPU)

#include <map>
#include <queue>

#include "base/callback_old.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "content/renderer/gpu/gpu_video_decode_accelerator_host.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"

namespace base {
class SharedMemory;
}

namespace gfx {
class Size;
}

class GpuChannelHost;
class PluginChannelHost;
class Task;

// Client side proxy that forwards messages synchronously to a
// CommandBufferStub.
class CommandBufferProxy : public gpu::CommandBuffer,
                           public IPC::Channel::Listener {
 public:
  CommandBufferProxy(GpuChannelHost* channel, int route_id);
  virtual ~CommandBufferProxy();

  // IPC::Channel::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelError();

  int route_id() const { return route_id_; }

  // CommandBuffer implementation:
  virtual bool Initialize(int32 size);
  virtual bool Initialize(base::SharedMemory* buffer, int32 size);
  virtual gpu::Buffer GetRingBuffer();
  virtual State GetState();
  virtual State GetLastState();
  virtual void Flush(int32 put_offset);
  virtual State FlushSync(int32 put_offset, int32 last_known_get);
  virtual void SetGetOffset(int32 get_offset);
  virtual int32 CreateTransferBuffer(size_t size, int32 id_request);
  virtual int32 RegisterTransferBuffer(base::SharedMemory* shared_memory,
                                       size_t size,
                                       int32 id_request);
  virtual void DestroyTransferBuffer(int32 id);
  virtual gpu::Buffer GetTransferBuffer(int32 handle);
  virtual void SetToken(int32 token);
  virtual void SetParseError(gpu::error::Error error);
  virtual void SetContextLostReason(gpu::error::ContextLostReason reason);
  virtual void OnSwapBuffers();

  // Reparent a command buffer. TODO(apatrick): going forward, the notion of
  // the parent / child relationship between command buffers is going away in
  // favor of the notion of surfaces that can be drawn to in one command buffer
  // and bound as a texture in any other.
  virtual bool SetParent(CommandBufferProxy* parent_command_buffer,
                         uint32 parent_texture_id);

  // Set a callback that will be invoked when the SwapBuffers call has been
  // issued.
  void SetSwapBuffersCallback(Callback0::Type* callback);
  void SetChannelErrorCallback(Callback0::Type* callback);

  // Asynchronously resizes an offscreen frame buffer.
  void ResizeOffscreenFrameBuffer(const gfx::Size& size);

  // Set a task that will be invoked the next time the window becomes invalid
  // and needs to be repainted. Takes ownership of task.
  void SetNotifyRepaintTask(Task* task);

  // Sends an IPC message to create a GpuVideoDecodeAccelerator. Creates and
  // returns a pointer to a GpuVideoDecodeAcceleratorHost.
  // Returns NULL on failure to create the GpuVideoDecodeAcceleratorHost.
  // Note that the GpuVideoDecodeAccelerator may still fail to be created in
  // the GPU process, even if this returns non-NULL. In this case the client is
  // notified of an error later.
  scoped_refptr<GpuVideoDecodeAcceleratorHost> CreateVideoDecoder(
      const std::vector<int32>& configs,
      media::VideoDecodeAccelerator::Client* client);

#if defined(OS_MACOSX)
  virtual void SetWindowSize(const gfx::Size& size);
#endif

 private:

  // Send an IPC message over the GPU channel. This is private to fully
  // encapsulate the channel; all callers of this function must explicitly
  // verify that the context has not been lost.
  bool Send(IPC::Message* msg);

  // Message handlers:
  void OnUpdateState(const gpu::CommandBuffer::State& state);
  void OnNotifyRepaint();
  void OnDestroyed(gpu::error::ContextLostReason reason);

  // As with the service, the client takes ownership of the ring buffer.
  int32 num_entries_;
  scoped_ptr<base::SharedMemory> ring_buffer_;

  // Local cache of id to transfer buffer mapping.
  typedef std::map<int32, gpu::Buffer> TransferBufferMap;
  TransferBufferMap transfer_buffers_;

  // Zero or more video decoder hosts owned by this proxy, keyed by their
  // decoder_route_id.
  typedef std::map<int, scoped_refptr<GpuVideoDecodeAcceleratorHost> > Decoders;
  Decoders video_decoder_hosts_;

  // The last cached state received from the service.
  State last_state_;

  // |*this| is owned by |*channel_| and so is always outlived by it, so using a
  // raw pointer is ok.
  GpuChannelHost* channel_;
  int route_id_;
  unsigned int flush_count_;

  scoped_ptr<Task> notify_repaint_task_;

  scoped_ptr<Callback0::Type> swap_buffers_callback_;
  scoped_ptr<Callback0::Type> channel_error_callback_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferProxy);
};

#endif  // ENABLE_GPU

#endif  // CONTENT_RENDERER_GPU_COMMAND_BUFFER_PROXY_H_
