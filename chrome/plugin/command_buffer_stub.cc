// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/scoped_open_process.h"
#include "base/shared_memory.h"
#include "chrome/common/gpu_messages.h"
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
  channel->AddRoute(route_id_, this, NULL);
}

CommandBufferStub::~CommandBufferStub() {
  Destroy();
  channel_->RemoveRoute(route_id_);
}

void CommandBufferStub::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(CommandBufferStub, message)
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
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SetWindowSize, OnSetWindowSize);
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

void CommandBufferStub::NotifyRepaint() {
  Send(new GpuCommandBufferMsg_NotifyRepaint(route_id_));
}

void CommandBufferStub::OnInitialize(int32 size,
                                     base::SharedMemoryHandle* ring_buffer) {
  DCHECK(!command_buffer_.get());

  *ring_buffer = base::SharedMemory::NULLHandle();

  // Assume service is responsible for duplicating the handle from the calling
  // process.
  base::ScopedOpenProcess peer_process;
  if (!peer_process.Open(channel_->peer_pid()))
    return;

  command_buffer_.reset(new gpu::CommandBufferService);

  // Initialize the CommandBufferService.
  if (!command_buffer_->Initialize(size)) {
    Destroy();
    return;
  }

  // Get the ring buffer.
  Buffer buffer = command_buffer_->GetRingBuffer();
  if (!buffer.shared_memory) {
    Destroy();
    return;
  }

  // Initialize the GPUProcessor.
  processor_.reset(new gpu::GPUProcessor(command_buffer_.get()));
  if (!processor_->Initialize(window_, gfx::Size(), std::vector<int32>(),
                              NULL, 0)) {
    Destroy();
    return;
  }

  // Perform platform specific initialization.
  if (!InitializePlatformSpecific()) {
    Destroy();
    return;
  }

  // Share the ring buffer to the client process.
  if (!buffer.shared_memory->ShareToProcess(peer_process.handle(),
                                            ring_buffer)) {
    Destroy();
    return;
  }

  // Setup callbacks for events.
  command_buffer_->SetPutOffsetChangeCallback(
      NewCallback(processor_.get(),
                  &gpu::GPUProcessor::ProcessCommands));
#if defined(OS_MACOSX)
  processor_->SetSwapBuffersCallback(
      NewCallback(this,
                  &CommandBufferStub::SwapBuffersCallback));
  processor_->SetTransportDIBAllocAndFree(
      NewCallback(this, &CommandBufferStub::AllocTransportDIB),
      NewCallback(this, &CommandBufferStub::FreeTransportDIB));
#endif
}

void CommandBufferStub::OnGetState(gpu::CommandBuffer::State* state) {
  *state = command_buffer_->GetState();
}

void CommandBufferStub::OnAsyncGetState() {
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  Send(new GpuCommandBufferMsg_UpdateState(route_id_, state));
}

void CommandBufferStub::OnFlush(int32 put_offset,
                                gpu::CommandBuffer::State* state) {
  *state = command_buffer_->Flush(put_offset);
}

void CommandBufferStub::OnAsyncFlush(int32 put_offset) {
  gpu::CommandBuffer::State state = command_buffer_->Flush(put_offset);
  Send(new GpuCommandBufferMsg_UpdateState(route_id_, state));
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
    uint32* size) {
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

void CommandBufferStub::Destroy() {
  processor_.reset();
  command_buffer_.reset();

  DestroyPlatformSpecific();
}

#if !defined(OS_WIN)
bool CommandBufferStub::InitializePlatformSpecific() {
  return true;
}

void CommandBufferStub::DestroyPlatformSpecific() {
}
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
void CommandBufferStub::OnSetWindowSize(const gfx::Size& size) {
  // Try using the IOSurface version first.
  bool notify_repaint = false;
  uint64 new_backing_store = processor_->SetWindowSizeForIOSurface(size);
  if (new_backing_store) {
    Send(new PluginHostMsg_AcceleratedSurfaceSetIOSurface(
        plugin_host_route_id_,
        window_,
        size.width(),
        size.height(),
        new_backing_store));
    notify_repaint = true;
  } else {
    // If |new_backing_store| is 0, it might mean that the IOSurface APIs are
    // not available.  In this case, see if TransportDIBs are supported.
    TransportDIB::Handle transport_dib =
        processor_->SetWindowSizeForTransportDIB(size);
    if (TransportDIB::is_valid(transport_dib)) {
      Send(new PluginHostMsg_AcceleratedSurfaceSetTransportDIB(
          plugin_host_route_id_,
          window_,
          size.width(),
          size.height(),
          transport_dib));
      notify_repaint = true;
    }
  }
  if (notify_repaint) {
    // Indicate to the client that at least one repaint is needed.
    NotifyRepaint();
  }
}

void CommandBufferStub::SwapBuffersCallback() {
  Send(new PluginHostMsg_AcceleratedSurfaceBuffersSwapped(plugin_host_route_id_,
                                                          window_));
}

void CommandBufferStub::AllocTransportDIB(const size_t size,
                                          TransportDIB::Handle* dib_handle) {
  Send(new PluginHostMsg_AllocTransportDIB(plugin_host_route_id_,
                                           size,
                                           dib_handle));
}

void CommandBufferStub::FreeTransportDIB(TransportDIB::Id dib_id) {
  Send(new PluginHostMsg_FreeTransportDIB(plugin_host_route_id_,
                                          dib_id));
}
#endif
