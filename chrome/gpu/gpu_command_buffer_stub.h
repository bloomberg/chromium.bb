// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_COMMAND_BUFFER_STUB_H_
#define CHROME_GPU_GPU_COMMAND_BUFFER_STUB_H_
#pragma once

#if defined(ENABLE_GPU)

#include <vector>

#include "base/process.h"
#include "base/weak_ptr.h"
#include "gfx/native_widget_types.h"
#include "gfx/size.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/gpu_processor.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"

class GpuChannel;

class GpuCommandBufferStub
    : public IPC::Channel::Listener,
      public IPC::Message::Sender,
      public base::SupportsWeakPtr<GpuCommandBufferStub> {
 public:
  GpuCommandBufferStub(GpuChannel* channel,
                       gfx::PluginWindowHandle handle,
                       GpuCommandBufferStub* parent,
                       const gfx::Size& size,
                       const std::vector<int32>& attribs,
                       uint32 parent_texture_id,
                       int32 route_id,
                       int32 renderer_id,
                       int32 render_view_id);

  virtual ~GpuCommandBufferStub();

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& message);

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  // Get the GLContext associated with this object.
  gpu::GPUProcessor* processor() const { return processor_.get(); }

  int32 route_id() const { return route_id_; }

 private:
  // Message handlers:
  void OnInitialize(int32 size, base::SharedMemoryHandle* ring_buffer);
  void OnGetState(gpu::CommandBuffer::State* state);
  void OnAsyncGetState();
  void OnFlush(int32 put_offset, gpu::CommandBuffer::State* state);
  void OnAsyncFlush(int32 put_offset);
  void OnCreateTransferBuffer(int32 size, int32* id);
  void OnDestroyTransferBuffer(int32 id);
  void OnGetTransferBuffer(int32 id,
                           base::SharedMemoryHandle* transfer_buffer,
                           uint32* size);
  void OnResizeOffscreenFrameBuffer(const gfx::Size& size);

  void OnSwapBuffers();

#if defined(OS_MACOSX)
  void OnSetWindowSize(const gfx::Size& size);
  void SwapBuffersCallback();
#endif

  // The lifetime of objects of this class is managed by a GpuChannel. The
  // GpuChannels destroy all the GpuCommandBufferStubs that they own when they
  // are destroyed. So a raw pointer is safe.
  GpuChannel* channel_;

  gfx::PluginWindowHandle handle_;
  base::WeakPtr<GpuCommandBufferStub> parent_;
  gfx::Size initial_size_;
  std::vector<int32> requested_attribs_;
  uint32 parent_texture_id_;
  int32 route_id_;

  // The following two fields are used on Mac OS X to identify the window
  // for the rendering results on the browser side.
  int32 renderer_id_;
  int32 render_view_id_;

  scoped_ptr<gpu::CommandBufferService> command_buffer_;
  scoped_ptr<gpu::GPUProcessor> processor_;

  DISALLOW_COPY_AND_ASSIGN(GpuCommandBufferStub);
};

#endif  // ENABLE_GPU

#endif  // CHROME_GPU_GPU_COMMAND_BUFFER_STUB_H_
