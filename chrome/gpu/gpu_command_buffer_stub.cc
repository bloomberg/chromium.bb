// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "base/process_util.h"
#include "base/shared_memory.h"
#include "build/build_config.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_channel.h"
#include "chrome/gpu/gpu_command_buffer_stub.h"

using gpu::Buffer;

GpuCommandBufferStub::GpuCommandBufferStub(GpuChannel* channel,
                                           gfx::PluginWindowHandle handle,
                                           GpuCommandBufferStub* parent,
                                           const gfx::Size& size,
                                           uint32 parent_texture_id,
                                           int32 route_id)
    : channel_(channel),
      handle_(handle),
      parent_(parent),
      initial_size_(size),
      parent_texture_id_(parent_texture_id),
      route_id_(route_id) {
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
          parent_processor,
          parent_texture_id_)) {
        command_buffer_->SetPutOffsetChangeCallback(
            NewCallback(processor_.get(),
                        &gpu::GPUProcessor::ProcessCommands));

        // Assume service is responsible for duplicating the handle from the
        // calling process.
        buffer.shared_memory->ShareToProcess(channel_->renderer_handle(),
                                             ring_buffer);
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

#endif  // ENABLE_GPU
