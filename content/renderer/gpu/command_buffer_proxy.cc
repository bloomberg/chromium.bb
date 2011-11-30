// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/command_buffer_proxy.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "base/stl_util.h"
#include "base/task.h"
#include "content/common/child_process_messages.h"
#include "content/common/child_thread.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/plugin_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/gpu/gpu_channel_host.h"
#include "content/renderer/plugin_channel_host.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "ui/gfx/size.h"

using gpu::Buffer;

CommandBufferProxy::CommandBufferProxy(
    GpuChannelHost* channel,
    int route_id)
    : num_entries_(0),
      channel_(channel),
      route_id_(route_id),
      flush_count_(0) {
}

CommandBufferProxy::~CommandBufferProxy() {
  // Delete all the locally cached shared memory objects, closing the handle
  // in this process.
  for (TransferBufferMap::iterator it = transfer_buffers_.begin();
       it != transfer_buffers_.end();
       ++it) {
    delete it->second.shared_memory;
    it->second.shared_memory = NULL;
  }
}

bool CommandBufferProxy::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CommandBufferProxy, message)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_UpdateState, OnUpdateState);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_Destroyed, OnDestroyed);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_NotifyRepaint,
                        OnNotifyRepaint);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_EchoAck, OnEchoAck);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  DCHECK(handled);
  return handled;
}

void CommandBufferProxy::OnChannelError() {
  for (Decoders::iterator it = video_decoder_hosts_.begin();
       it != video_decoder_hosts_.end(); ++it) {
    it->second->OnChannelError();
  }
  OnDestroyed(gpu::error::kUnknown);
}

void CommandBufferProxy::OnDestroyed(gpu::error::ContextLostReason reason) {
  // Prevent any further messages from being sent.
  channel_ = NULL;

  // When the client sees that the context is lost, they should delete this
  // CommandBufferProxy and create a new one.
  last_state_.error = gpu::error::kLostContext;
  last_state_.context_lost_reason = reason;

  if (!channel_error_callback_.is_null()) {
    channel_error_callback_.Run();
    // Avoid calling the error callback more than once.
    channel_error_callback_.Reset();
  }
}

void CommandBufferProxy::OnEchoAck() {
  DCHECK(!echo_tasks_.empty());
  base::Closure callback = echo_tasks_.front();
  echo_tasks_.pop();
  callback.Run();
}

void CommandBufferProxy::SetChannelErrorCallback(
    const base::Closure& callback) {
  channel_error_callback_ = callback;
}

bool CommandBufferProxy::Initialize(int32 size) {
  DCHECK(!ring_buffer_.get());

  ChildThread* child_thread = ChildThread::current();
  if (!child_thread)
    return false;

  base::SharedMemoryHandle handle;
  if (!child_thread->Send(new ChildProcessHostMsg_SyncAllocateSharedMemory(
      size,
      &handle))) {
    return false;
  }

  if (!base::SharedMemory::IsHandleValid(handle))
    return false;

#if defined(OS_POSIX)
  handle.auto_close = false;
#endif

  // Take ownership of shared memory. This will close the handle if Send below
  // fails. Otherwise, callee takes ownership before this variable
  // goes out of scope.
  base::SharedMemory shared_memory(handle, false);

  return Initialize(&shared_memory, size);
}

bool CommandBufferProxy::Initialize(base::SharedMemory* buffer, int32 size) {
  bool result;
  if (!Send(new GpuCommandBufferMsg_Initialize(route_id_,
                                               buffer->handle(),
                                               size,
                                               &result))) {
    LOG(ERROR) << "Could not send GpuCommandBufferMsg_Initialize.";
    return false;
  }

  if (!result) {
    LOG(ERROR) << "Failed to initialize command buffer service.";
    return false;
  }

  base::SharedMemoryHandle handle;
  if (!buffer->GiveToProcess(base::GetCurrentProcessHandle(), &handle)) {
    LOG(ERROR) << "Failed to duplicate command buffer handle.";
    return false;
  }

  ring_buffer_.reset(new base::SharedMemory(handle, false));
  if (!ring_buffer_->Map(size)) {
    LOG(ERROR) << "Failed to map shared memory for command buffer.";
    ring_buffer_.reset();
    return false;
  }

  num_entries_ = size / sizeof(gpu::CommandBufferEntry);
  return true;
}

Buffer CommandBufferProxy::GetRingBuffer() {
  DCHECK(ring_buffer_.get());
  // Return locally cached ring buffer.
  Buffer buffer;
  if (ring_buffer_.get()) {
    buffer.ptr = ring_buffer_->memory();
    buffer.size = num_entries_ * sizeof(gpu::CommandBufferEntry);
    buffer.shared_memory = ring_buffer_.get();
  } else {
    buffer.ptr = NULL;
    buffer.size = 0;
    buffer.shared_memory = NULL;
  }
  return buffer;
}

gpu::CommandBuffer::State CommandBufferProxy::GetState() {
  // Send will flag state with lost context if IPC fails.
  if (last_state_.error == gpu::error::kNoError) {
    gpu::CommandBuffer::State state;
    if (Send(new GpuCommandBufferMsg_GetState(route_id_, &state)))
      OnUpdateState(state);
  }

  return last_state_;
}

gpu::CommandBuffer::State CommandBufferProxy::GetLastState() {
  return last_state_;
}

void CommandBufferProxy::Flush(int32 put_offset) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  TRACE_EVENT1("gpu", "CommandBufferProxy::Flush", "put_offset", put_offset);

  Send(new GpuCommandBufferMsg_AsyncFlush(route_id_,
                                          put_offset,
                                          ++flush_count_));
}

gpu::CommandBuffer::State CommandBufferProxy::FlushSync(int32 put_offset,
                                                        int32 last_known_get) {
  TRACE_EVENT1("gpu", "CommandBufferProxy::FlushSync", "put_offset",
               put_offset);
  Flush(put_offset);
  if (last_known_get == last_state_.get_offset) {
    // Send will flag state with lost context if IPC fails.
    if (last_state_.error == gpu::error::kNoError) {
      gpu::CommandBuffer::State state;
      if (Send(new GpuCommandBufferMsg_GetStateFast(route_id_,
                                                    &state)))
        OnUpdateState(state);
    }
  }

  return last_state_;
}

void CommandBufferProxy::SetGetOffset(int32 get_offset) {
  // Not implemented in proxy.
  NOTREACHED();
}

int32 CommandBufferProxy::CreateTransferBuffer(size_t size, int32 id_request) {
  if (last_state_.error != gpu::error::kNoError)
    return -1;

  ChildThread* child_thread = ChildThread::current();
  if (!child_thread)
    return -1;

  base::SharedMemoryHandle handle;
  if (!child_thread->Send(new ChildProcessHostMsg_SyncAllocateSharedMemory(
      size,
      &handle))) {
    return -1;
  }

  if (!base::SharedMemory::IsHandleValid(handle))
    return -1;

  // Handle is closed by the SharedMemory object below. This stops
  // base::FileDescriptor from closing it as well.
#if defined(OS_POSIX)
  handle.auto_close = false;
#endif

  // Take ownership of shared memory. This will close the handle if Send below
  // fails. Otherwise, callee takes ownership before this variable
  // goes out of scope by duping the handle.
  base::SharedMemory shared_memory(handle, false);

  int32 id;
  if (!Send(new GpuCommandBufferMsg_RegisterTransferBuffer(route_id_,
                                                           handle,
                                                           size,
                                                           id_request,
                                                           &id))) {
    return -1;
  }

  return id;
}

int32 CommandBufferProxy::RegisterTransferBuffer(
    base::SharedMemory* shared_memory,
    size_t size,
    int32 id_request) {
  if (last_state_.error != gpu::error::kNoError)
    return -1;

  int32 id;
  if (!Send(new GpuCommandBufferMsg_RegisterTransferBuffer(
      route_id_,
      shared_memory->handle(),  // Returns FileDescriptor with auto_close off.
      size,
      id_request,
      &id))) {
    return -1;
  }

  return id;
}

void CommandBufferProxy::DestroyTransferBuffer(int32 id) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  // Remove the transfer buffer from the client side cache.
  TransferBufferMap::iterator it = transfer_buffers_.find(id);
  if (it != transfer_buffers_.end()) {
    delete it->second.shared_memory;
    transfer_buffers_.erase(it);
  }

  Send(new GpuCommandBufferMsg_DestroyTransferBuffer(route_id_, id));
}

Buffer CommandBufferProxy::GetTransferBuffer(int32 id) {
  if (last_state_.error != gpu::error::kNoError)
    return Buffer();

  // Check local cache to see if there is already a client side shared memory
  // object for this id.
  TransferBufferMap::iterator it = transfer_buffers_.find(id);
  if (it != transfer_buffers_.end()) {
    return it->second;
  }

  // Assuming we are in the renderer process, the service is responsible for
  // duplicating the handle. This might not be true for NaCl.
  base::SharedMemoryHandle handle;
  uint32 size;
  if (!Send(new GpuCommandBufferMsg_GetTransferBuffer(route_id_,
                                                      id,
                                                      &handle,
                                                      &size))) {
    return Buffer();
  }

  // Cache the transfer buffer shared memory object client side.
  base::SharedMemory* shared_memory = new base::SharedMemory(handle, false);

  // Map the shared memory on demand.
  if (!shared_memory->memory()) {
    if (!shared_memory->Map(size)) {
      delete shared_memory;
      return Buffer();
    }
  }

  Buffer buffer;
  buffer.ptr = shared_memory->memory();
  buffer.size = size;
  buffer.shared_memory = shared_memory;
  transfer_buffers_[id] = buffer;

  return buffer;
}

void CommandBufferProxy::SetToken(int32 token) {
  // Not implemented in proxy.
  NOTREACHED();
}

void CommandBufferProxy::OnNotifyRepaint() {
  if (!notify_repaint_task_.is_null())
    MessageLoop::current()->PostNonNestableTask(
        FROM_HERE, notify_repaint_task_);
  notify_repaint_task_.Reset();
}

void CommandBufferProxy::SetParseError(
    gpu::error::Error error) {
  // Not implemented in proxy.
  NOTREACHED();
}

void CommandBufferProxy::SetContextLostReason(
    gpu::error::ContextLostReason reason) {
  // Not implemented in proxy.
  NOTREACHED();
}

bool CommandBufferProxy::Echo(const base::Closure& callback) {
  if (last_state_.error != gpu::error::kNoError) {
    return false;
  }

  if (!Send(new GpuChannelMsg_Echo(GpuCommandBufferMsg_EchoAck(route_id_)))) {
    return false;
  }

  echo_tasks_.push(callback);

  return true;
}

bool CommandBufferProxy::SetSurfaceVisible(bool visible) {
  if (last_state_.error != gpu::error::kNoError) {
    return false;
  }

  return Send(new GpuCommandBufferMsg_SetSurfaceVisible(route_id_, visible));
}


bool CommandBufferProxy::SetParent(CommandBufferProxy* parent_command_buffer,
                                   uint32 parent_texture_id) {
  if (last_state_.error != gpu::error::kNoError)
    return false;

  bool result;
  if (parent_command_buffer) {
    if (!Send(new GpuCommandBufferMsg_SetParent(
        route_id_,
        parent_command_buffer->route_id_,
        parent_texture_id,
        &result))) {
      return false;
    }
  } else {
    if (!Send(new GpuCommandBufferMsg_SetParent(
        route_id_,
        MSG_ROUTING_NONE,
        0,
        &result))) {
      return false;
    }
  }

  return result;
}

void CommandBufferProxy::SetNotifyRepaintTask(const base::Closure& task) {
  notify_repaint_task_ = task;
}

scoped_refptr<GpuVideoDecodeAcceleratorHost>
CommandBufferProxy::CreateVideoDecoder(
    media::VideoDecodeAccelerator::Profile profile,
    media::VideoDecodeAccelerator::Client* client) {
  int decoder_route_id;
  if (!Send(new GpuCommandBufferMsg_CreateVideoDecoder(route_id_, profile,
                                                       &decoder_route_id))) {
    LOG(ERROR) << "Send(GpuChannelMsg_CreateVideoDecoder) failed";
    return NULL;
  }

  scoped_refptr<GpuVideoDecodeAcceleratorHost> decoder_host =
      new GpuVideoDecodeAcceleratorHost(channel_, decoder_route_id, client);
  bool inserted = video_decoder_hosts_.insert(std::make_pair(
      decoder_route_id, decoder_host)).second;
  DCHECK(inserted);

  channel_->AddRoute(decoder_route_id, decoder_host->AsWeakPtr());

  return decoder_host;
}

bool CommandBufferProxy::Send(IPC::Message* msg) {
  // Caller should not intentionally send a message if the context is lost.
  DCHECK(last_state_.error == gpu::error::kNoError);

  if (channel_) {
    if (channel_->Send(msg)) {
      return true;
    } else {
      // Flag the command buffer as lost. Defer deleting the channel until
      // OnChannelError is called after returning to the message loop in case
      // it is referenced elsewhere.
      last_state_.error = gpu::error::kLostContext;
      return false;
    }
  }

  // Callee takes ownership of message, regardless of whether Send is
  // successful. See IPC::Message::Sender.
  delete msg;
  return false;
}

void CommandBufferProxy::OnUpdateState(const gpu::CommandBuffer::State& state) {
  // Handle wraparound. It works as long as we don't have more than 2B state
  // updates in flight across which reordering occurs.
  if (state.generation - last_state_.generation < 0x80000000U)
    last_state_ = state;
}
