// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "base/task.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/command_buffer_proxy.h"
#include "chrome/renderer/plugin_channel_host.h"
#include "chrome/renderer/render_thread.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "ui/gfx/size.h"

using gpu::Buffer;

CommandBufferProxy::CommandBufferProxy(
    IPC::Channel::Sender* channel,
    int route_id)
    : num_entries_(0),
      channel_(channel),
      route_id_(route_id) {
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
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SwapBuffers, OnSwapBuffers);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_NotifyRepaint,
                        OnNotifyRepaint);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void CommandBufferProxy::OnChannelError() {
  // Prevent any further messages from being sent.
  channel_ = NULL;

  // When the client sees that the context is lost, they should delete this
  // CommandBufferProxy and create a new one.
  last_state_.error = gpu::error::kLostContext;

  if (channel_error_callback_.get())
    channel_error_callback_->Run();
}

void CommandBufferProxy::SetChannelErrorCallback(Callback0::Type* callback) {
  channel_error_callback_.reset(callback);
}

bool CommandBufferProxy::Initialize(int32 size) {
  DCHECK(!ring_buffer_.get());

  // Initialize the service. Assuming we are sandboxed, the GPU
  // process is responsible for duplicating the handle. This might not be true
  // for NaCl.
  base::SharedMemoryHandle handle;
  if (Send(new GpuCommandBufferMsg_Initialize(route_id_, size, &handle)) &&
      base::SharedMemory::IsHandleValid(handle)) {
    ring_buffer_.reset(new base::SharedMemory(handle, false));
    if (ring_buffer_->Map(size)) {
      num_entries_ = size / sizeof(gpu::CommandBufferEntry);
      return true;
    }

    ring_buffer_.reset();
  }

  return false;
}

Buffer CommandBufferProxy::GetRingBuffer() {
  DCHECK(ring_buffer_.get());
  // Return locally cached ring buffer.
  Buffer buffer;
  buffer.ptr = ring_buffer_->memory();
  buffer.size = num_entries_ * sizeof(gpu::CommandBufferEntry);
  buffer.shared_memory = ring_buffer_.get();
  return buffer;
}

gpu::CommandBuffer::State CommandBufferProxy::GetState() {
  // Send will flag state with lost context if IPC fails.
  if (last_state_.error == gpu::error::kNoError)
    Send(new GpuCommandBufferMsg_GetState(route_id_, &last_state_));

  return last_state_;
}

void CommandBufferProxy::Flush(int32 put_offset) {
  AsyncFlush(put_offset, NULL);
}

gpu::CommandBuffer::State CommandBufferProxy::FlushSync(int32 put_offset) {
  // Send will flag state with lost context if IPC fails.
  if (last_state_.error == gpu::error::kNoError) {
    Send(new GpuCommandBufferMsg_Flush(route_id_,
                                       put_offset,
                                       &last_state_));
  }

  return last_state_;
}

void CommandBufferProxy::SetGetOffset(int32 get_offset) {
  // Not implemented in proxy.
  NOTREACHED();
}

int32 CommandBufferProxy::CreateTransferBuffer(size_t size) {
  if (last_state_.error != gpu::error::kNoError)
    return -1;

  RenderThread* render_thread = RenderThread::current();
  if (!render_thread)
    return -1;

  base::SharedMemoryHandle handle;
  if (!render_thread->Send(new ViewHostMsg_AllocateSharedMemoryBuffer(
      size,
      &handle))) {
    return -1;
  }

  // Take ownership of shared memory. This will close the handle if Send below
  // fails. Otherwise, callee takes ownership before this variable
  // goes out of scope by duping the handle.
  base::SharedMemory shared_memory(handle, false);

  int32 id;
  if (!Send(new GpuCommandBufferMsg_RegisterTransferBuffer(route_id_,
                                                           handle,
                                                           size,
                                                           &id))) {
    return -1;
  }

  return id;
}

int32 CommandBufferProxy::RegisterTransferBuffer(
    base::SharedMemory* shared_memory,
    size_t size) {
  if (last_state_.error != gpu::error::kNoError)
    return -1;

  int32 id;
  if (!Send(new GpuCommandBufferMsg_RegisterTransferBuffer(
      route_id_,
      shared_memory->handle(),
      size,
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
  if (notify_repaint_task_.get())
    MessageLoop::current()->PostNonNestableTask(
        FROM_HERE, notify_repaint_task_.release());
}

void CommandBufferProxy::SetParseError(
    gpu::error::Error error) {
  // Not implemented in proxy.
  NOTREACHED();
}

void CommandBufferProxy::OnSwapBuffers() {
  if (swap_buffers_callback_.get())
    swap_buffers_callback_->Run();
}

void CommandBufferProxy::SetSwapBuffersCallback(Callback0::Type* callback) {
  swap_buffers_callback_.reset(callback);
}

void CommandBufferProxy::ResizeOffscreenFrameBuffer(const gfx::Size& size) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  IPC::Message* message =
      new GpuCommandBufferMsg_ResizeOffscreenFrameBuffer(route_id_, size);

  // We need to set the unblock flag on this message to guarantee the
  // order in which it is processed in the GPU process. Ordinarily in
  // certain situations, namely if a synchronous message is being
  // processed, other synchronous messages may be processed before
  // asynchronous messages. During some page reloads WebGL seems to
  // send three messages (sync, async, sync) in rapid succession in
  // that order, and the sync message (GpuCommandBufferMsg_Flush, on
  // behalf of SwapBuffers) is sometimes processed before the async
  // message (GpuCommandBufferMsg_ResizeOffscreenFrameBuffer). This
  // causes the WebGL content to disappear because the back buffer is
  // not correctly resized.
  message->set_unblock(true);
  Send(message);
}

void CommandBufferProxy::SetNotifyRepaintTask(Task* task) {
  notify_repaint_task_.reset(task);
}

#if defined(OS_MACOSX)
void CommandBufferProxy::SetWindowSize(const gfx::Size& size) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new GpuCommandBufferMsg_SetWindowSize(route_id_, size));
}
#endif

void CommandBufferProxy::AsyncGetState(Task* completion_task) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  IPC::Message* message = new GpuCommandBufferMsg_AsyncGetState(route_id_);

  // Do not let a synchronous flush hold up this message. If this handler is
  // deferred until after the synchronous flush completes, it will overwrite the
  // cached last_state_ with out-of-date data.
  message->set_unblock(true);

  if (Send(message))
    pending_async_flush_tasks_.push(linked_ptr<Task>(completion_task));
}

void CommandBufferProxy::AsyncFlush(int32 put_offset, Task* completion_task) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  IPC::Message* message = new GpuCommandBufferMsg_AsyncFlush(route_id_,
                                                          put_offset);

  // Do not let a synchronous flush hold up this message. If this handler is
  // deferred until after the synchronous flush completes, it will overwrite the
  // cached last_state_ with out-of-date data.
  message->set_unblock(true);

  if (Send(message))
    pending_async_flush_tasks_.push(linked_ptr<Task>(completion_task));
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
  last_state_ = state;

  linked_ptr<Task> task = pending_async_flush_tasks_.front();
  pending_async_flush_tasks_.pop();

  if (task.get()) {
    // Although we need need to update last_state_ while potentially waiting
    // for a synchronous flush to complete, we do not need to invoke the
    // callback synchonously. Also, post it as a non nestable task so it is
    // always invoked by the outermost message loop.
    MessageLoop::current()->PostNonNestableTask(FROM_HERE, task.release());
  }
}
