// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "base/process_util.h"
#include "base/shared_memory.h"
#include "build/build_config.h"
#include "chrome/common/child_thread.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_channel.h"
#include "chrome/gpu/gpu_command_buffer_stub.h"

using gpu::Buffer;

GpuCommandBufferStub::GpuCommandBufferStub(GpuChannel* channel,
                                           gfx::PluginWindowHandle handle,
                                           GpuCommandBufferStub* parent,
                                           const gfx::Size& size,
                                           const std::vector<int32>& attribs,
                                           uint32 parent_texture_id,
                                           int32 route_id,
                                           int32 renderer_id,
                                           int32 render_view_id)
    : channel_(channel),
      handle_(handle),
      parent_(
          parent ? parent->AsWeakPtr() : base::WeakPtr<GpuCommandBufferStub>()),
      initial_size_(size),
      requested_attribs_(attribs),
      parent_texture_id_(parent_texture_id),
      route_id_(route_id),
      renderer_id_(renderer_id),
      render_view_id_(render_view_id) {
}

GpuCommandBufferStub::~GpuCommandBufferStub() {
  if (processor_.get()) {
    processor_->Destroy();
  }
}

void GpuCommandBufferStub::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(GpuCommandBufferStub, message)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_Initialize, OnInitialize);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_GetState, OnGetState);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_AsyncGetState, OnAsyncGetState);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_Flush, OnFlush);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_AsyncFlush, OnAsyncFlush);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_CreateTransferBuffer,
                        OnCreateTransferBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DestroyTransferBuffer,
                        OnDestroyTransferBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_GetTransferBuffer,
                        OnGetTransferBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_ResizeOffscreenFrameBuffer,
                        OnResizeOffscreenFrameBuffer);
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SetWindowSize, OnSetWindowSize);
#endif
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

bool GpuCommandBufferStub::Send(IPC::Message* message) {
  return channel_->Send(message);
}

void GpuCommandBufferStub::OnInitialize(
    int32 size,
    base::SharedMemoryHandle* ring_buffer) {
  DCHECK(!command_buffer_.get());

  *ring_buffer = base::SharedMemory::NULLHandle();

  command_buffer_.reset(new gpu::CommandBufferService);

  // Initialize the CommandBufferService and GPUProcessor.
  if (command_buffer_->Initialize(size)) {
    Buffer buffer = command_buffer_->GetRingBuffer();
    if (buffer.shared_memory) {
      gpu::GPUProcessor* parent_processor =
          parent_ ? parent_->processor_.get() : NULL;
      processor_.reset(new gpu::GPUProcessor(command_buffer_.get()));
      if (processor_->Initialize(
          handle_,
          initial_size_,
          requested_attribs_,
          parent_processor,
          parent_texture_id_)) {
        command_buffer_->SetPutOffsetChangeCallback(
            NewCallback(processor_.get(),
                        &gpu::GPUProcessor::ProcessCommands));
        processor_->SetSwapBuffersCallback(
            NewCallback(this, &GpuCommandBufferStub::OnSwapBuffers));

        // Assume service is responsible for duplicating the handle from the
        // calling process.
        buffer.shared_memory->ShareToProcess(channel_->renderer_handle(),
                                             ring_buffer);
#if defined(OS_MACOSX)
        if (handle_) {
          // This context conceptually puts its output directly on the
          // screen, rendered by the accelerated plugin layer in
          // RenderWidgetHostViewMac. Set up a pathway to notify the
          // browser process when its contents change.
          processor_->SetSwapBuffersCallback(
              NewCallback(this,
                          &GpuCommandBufferStub::SwapBuffersCallback));
        }
#endif
      } else {
        processor_.reset();
        command_buffer_.reset();
      }
    }
  }
}

void GpuCommandBufferStub::OnGetState(gpu::CommandBuffer::State* state) {
  *state = command_buffer_->GetState();
}

void GpuCommandBufferStub::OnAsyncGetState() {
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  Send(new GpuCommandBufferMsg_UpdateState(route_id_, state));
}

void GpuCommandBufferStub::OnFlush(int32 put_offset,
                                   gpu::CommandBuffer::State* state) {
  *state = command_buffer_->Flush(put_offset);
}

void GpuCommandBufferStub::OnAsyncFlush(int32 put_offset) {
  gpu::CommandBuffer::State state = command_buffer_->Flush(put_offset);
  Send(new GpuCommandBufferMsg_UpdateState(route_id_, state));
}

void GpuCommandBufferStub::OnCreateTransferBuffer(int32 size, int32* id) {
  *id = command_buffer_->CreateTransferBuffer(size);
}

void GpuCommandBufferStub::OnDestroyTransferBuffer(int32 id) {
  command_buffer_->DestroyTransferBuffer(id);
}

void GpuCommandBufferStub::OnGetTransferBuffer(
    int32 id,
    base::SharedMemoryHandle* transfer_buffer,
    uint32* size) {
  *transfer_buffer = base::SharedMemoryHandle();
  *size = 0;

  Buffer buffer = command_buffer_->GetTransferBuffer(id);
  if (buffer.shared_memory) {
    // Assume service is responsible for duplicating the handle to the calling
    // process.
    buffer.shared_memory->ShareToProcess(channel_->renderer_handle(),
                                         transfer_buffer);
    *size = buffer.shared_memory->max_size();
  }
}

void GpuCommandBufferStub::OnResizeOffscreenFrameBuffer(const gfx::Size& size) {
  processor_->ResizeOffscreenFrameBuffer(size);
}

void GpuCommandBufferStub::OnSwapBuffers() {
  Send(new GpuCommandBufferMsg_SwapBuffers(route_id_));
}

#if defined(OS_MACOSX)
void GpuCommandBufferStub::OnSetWindowSize(const gfx::Size& size) {
  ChildThread* gpu_thread = ChildThread::current();
  // Try using the IOSurface version first.
  uint64 new_backing_store = processor_->SetWindowSizeForIOSurface(size);
  if (new_backing_store) {
    GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params params;
    params.renderer_id = renderer_id_;
    params.render_view_id = render_view_id_;
    params.window = handle_;
    params.width = size.width();
    params.height = size.height();
    params.identifier = new_backing_store;
    gpu_thread->Send(new GpuHostMsg_AcceleratedSurfaceSetIOSurface(params));
  } else {
    // TODO(kbr): figure out what to do here. It wouldn't be difficult
    // to support the compositor on 10.5, but the performance would be
    // questionable.
    NOTREACHED();
  }
}

void GpuCommandBufferStub::SwapBuffersCallback() {
  ChildThread* gpu_thread = ChildThread::current();
  gpu_thread->Send(
      new GpuHostMsg_AcceleratedSurfaceBuffersSwapped(renderer_id_,
                                                      render_view_id_,
                                                      handle_));
}
#endif  // defined(OS_MACOSX)

#endif  // ENABLE_GPU
