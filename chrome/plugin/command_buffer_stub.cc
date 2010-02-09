// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"
#include "base/shared_memory.h"
#include "chrome/common/command_buffer_messages.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/plugin/command_buffer_stub.h"
#include "chrome/plugin/plugin_channel.h"

using gpu::Buffer;

CommandBufferStub::CommandBufferStub(PluginChannel* channel,
                                     int plugin_host_route_id,
                                     gfx::PluginWindowHandle window)
    : channel_(channel),
      plugin_host_route_id_(plugin_host_route_id),
      window_(window) {
  route_id_ = channel->GenerateRouteID();
  channel->AddRoute(route_id_, this, false);
}

CommandBufferStub::~CommandBufferStub() {
  channel_->RemoveRoute(route_id_);
}

void CommandBufferStub::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(CommandBufferStub, message)
    IPC_MESSAGE_HANDLER(CommandBufferMsg_Initialize, OnInitialize);
    IPC_MESSAGE_HANDLER(CommandBufferMsg_GetState, OnGetState);
    IPC_MESSAGE_HANDLER(CommandBufferMsg_AsyncGetState, OnAsyncGetState);
    IPC_MESSAGE_HANDLER(CommandBufferMsg_Flush, OnFlush);
    IPC_MESSAGE_HANDLER(CommandBufferMsg_AsyncFlush, OnAsyncFlush);
    IPC_MESSAGE_HANDLER(CommandBufferMsg_CreateTransferBuffer,
                        OnCreateTransferBuffer);
    IPC_MESSAGE_HANDLER(CommandBufferMsg_DestroyTransferBuffer,
                        OnDestroyTransferBuffer);
    IPC_MESSAGE_HANDLER(CommandBufferMsg_GetTransferBuffer,
                        OnGetTransferBuffer);
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(CommandBufferMsg_SetWindowSize, OnSetWindowSize);
#endif
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

void CommandBufferStub::OnChannelError() {
  NOTREACHED() << "CommandBufferService::OnChannelError called";
}

bool CommandBufferStub::Send(IPC::Message* message) {
  if (!channel_) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

void CommandBufferStub::OnInitialize(int32 size,
                                     base::SharedMemoryHandle* ring_buffer) {
  DCHECK(!command_buffer_.get());

  *ring_buffer = base::SharedMemory::NULLHandle();

  // Assume service is responsible for duplicating the handle from the calling
  // process.
  base::ProcessHandle peer_handle;
  if (!base::OpenProcessHandle(channel_->peer_pid(), &peer_handle))
    return;

  command_buffer_.reset(new gpu::CommandBufferService);

  // Initialize the CommandBufferService and GPUProcessor.
  if (command_buffer_->Initialize(size)) {
    Buffer buffer = command_buffer_->GetRingBuffer();
    if (buffer.shared_memory) {
      processor_ = new gpu::GPUProcessor(command_buffer_.get());
      if (processor_->Initialize(window_)) {
        command_buffer_->SetPutOffsetChangeCallback(
            NewCallback(processor_.get(),
                        &gpu::GPUProcessor::ProcessCommands));
#if defined(OS_MACOSX)
        processor_->SetSwapBuffersCallback(
            NewCallback(this,
                        &CommandBufferStub::SwapBuffersCallback));
#endif
        buffer.shared_memory->ShareToProcess(peer_handle, ring_buffer);
      } else {
        processor_ = NULL;
        command_buffer_.reset();
      }
    }
  }

  base::CloseProcessHandle(peer_handle);
}

void CommandBufferStub::OnGetState(gpu::CommandBuffer::State* state) {
  *state = command_buffer_->GetState();
}

void CommandBufferStub::OnAsyncGetState() {
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  Send(new CommandBufferMsg_UpdateState(route_id_, state));
}

void CommandBufferStub::OnFlush(int32 put_offset,
                                gpu::CommandBuffer::State* state) {
  *state = command_buffer_->Flush(put_offset);
}

void CommandBufferStub::OnAsyncFlush(int32 put_offset) {
  gpu::CommandBuffer::State state = command_buffer_->Flush(put_offset);
  Send(new CommandBufferMsg_UpdateState(route_id_, state));
}

void CommandBufferStub::OnCreateTransferBuffer(int32 size, int32* id) {
  *id = command_buffer_->CreateTransferBuffer(size);
}

void CommandBufferStub::OnDestroyTransferBuffer(int32 id) {
  command_buffer_->DestroyTransferBuffer(id);
}

void CommandBufferStub::OnGetTransferBuffer(
    int32 id,
    base::SharedMemoryHandle* transfer_buffer,
    size_t* size) {
  *transfer_buffer = base::SharedMemoryHandle();
  *size = 0;

  // Assume service is responsible for duplicating the handle to the calling
  // process.
  base::ProcessHandle peer_handle;
  if (!base::OpenProcessHandle(channel_->peer_pid(), &peer_handle))
    return;

  Buffer buffer = command_buffer_->GetTransferBuffer(id);
  if (buffer.shared_memory) {
    buffer.shared_memory->ShareToProcess(peer_handle, transfer_buffer);
    *size = buffer.shared_memory->max_size();
  }

  base::CloseProcessHandle(peer_handle);
}

#if defined(OS_MACOSX)
void CommandBufferStub::OnSetWindowSize(int32 width, int32 height) {
  uint64 new_backing_store = processor_->SetWindowSize(width, height);
  if (new_backing_store) {
    Send(new PluginHostMsg_GPUPluginSetIOSurface(plugin_host_route_id_,
                                                 window_,
                                                 width,
                                                 height,
                                                 new_backing_store));
  }
}

void CommandBufferStub::SwapBuffersCallback() {
  Send(new PluginHostMsg_GPUPluginBuffersSwapped(plugin_host_route_id_,
                                                 window_));
}
#endif
