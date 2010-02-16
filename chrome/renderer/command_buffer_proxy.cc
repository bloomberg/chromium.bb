// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/process_util.h"
#include "chrome/common/command_buffer_messages.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/renderer/command_buffer_proxy.h"
#include "chrome/renderer/plugin_channel_host.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"

using gpu::Buffer;

CommandBufferProxy::CommandBufferProxy(
    PluginChannelHost* channel,
    int route_id)
    : size_(0),
      channel_(channel),
      route_id_(route_id) {
  channel->AddRoute(route_id_, this, false);
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

  channel_->RemoveRoute(route_id_);
}

void CommandBufferProxy::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(CommandBufferProxy, message)
    IPC_MESSAGE_HANDLER(CommandBufferMsg_UpdateState, OnUpdateState);
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

void CommandBufferProxy::OnChannelError() {
}

bool CommandBufferProxy::Send(IPC::Message* msg) {
  if (channel_)
    return channel_->Send(msg);

  // Callee takes ownership of message, regardless of whether Send is
  // successful. See IPC::Message::Sender.
  delete msg;
  return false;
}

bool CommandBufferProxy::Initialize(int32 size) {
  DCHECK(!ring_buffer_.get());

  // Initialize the service. Assuming we are sandboxed, the GPU
  // process is responsible for duplicating the handle. This might not be true
  // for NaCl.
  base::SharedMemoryHandle handle;
  if (Send(new CommandBufferMsg_Initialize(route_id_, size, &handle)) &&
      base::SharedMemory::IsHandleValid(handle)) {
    ring_buffer_.reset(new base::SharedMemory(handle, false));
    if (ring_buffer_->Map(size * sizeof(int32))) {
      size_ = size;
      return true;
    }

    ring_buffer_.reset();
  }

  return false;
}

Buffer CommandBufferProxy::GetRingBuffer() {
  // Return locally cached ring buffer.
  Buffer buffer;
  buffer.ptr = ring_buffer_->memory();
  buffer.size = size_ * sizeof(gpu::CommandBufferEntry);
  buffer.shared_memory = ring_buffer_.get();
  return buffer;
}

gpu::CommandBuffer::State CommandBufferProxy::GetState() {
  Send(new CommandBufferMsg_GetState(route_id_, &last_state_));
  return last_state_;
}

gpu::CommandBuffer::State CommandBufferProxy::Flush(int32 put_offset) {
  Send(new CommandBufferMsg_Flush(route_id_,
                                  put_offset,
                                  &last_state_));
  return last_state_;
}

void CommandBufferProxy::SetGetOffset(int32 get_offset) {
  // Not implemented in proxy.
  NOTREACHED();
}

int32 CommandBufferProxy::CreateTransferBuffer(size_t size) {
  int32 id;
  if (Send(new CommandBufferMsg_CreateTransferBuffer(route_id_, size, &id)))
    return id;

  return -1;
}

void CommandBufferProxy::DestroyTransferBuffer(int32 id) {
  // Remove the transfer buffer from the client side4 cache.
  TransferBufferMap::iterator it = transfer_buffers_.find(id);
  DCHECK(it != transfer_buffers_.end());

  // Delete the shared memory object, closing the handle in this process.
  delete it->second.shared_memory;

  transfer_buffers_.erase(it);

  Send(new CommandBufferMsg_DestroyTransferBuffer(route_id_, id));
}

Buffer CommandBufferProxy::GetTransferBuffer(int32 id) {
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
  if (!Send(new CommandBufferMsg_GetTransferBuffer(route_id_,
                                                   id,
                                                   &handle,
                                                   &size))) {
    return Buffer();
  }

  // Cache the transfer buffer shared memory object client side.
#if defined(OS_WIN)
  // TODO(piman): Does Windows needs this version of the constructor ? It
  // duplicates the handle, but I'm not sure why it is necessary - it was
  // already duped by the CommandBufferStub.
  base::SharedMemory* shared_memory =
      new base::SharedMemory(handle, false, base::GetCurrentProcessHandle());
#else
  base::SharedMemory* shared_memory =
      new base::SharedMemory(handle, false);
#endif

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

void CommandBufferProxy::SetParseError(
    gpu::error::Error error) {
  // Not implemented in proxy.
  NOTREACHED();
}

#if defined(OS_MACOSX)
void CommandBufferProxy::SetWindowSize(int32 width, int32 height) {
  Send(new CommandBufferMsg_SetWindowSize(route_id_, width, height));
}
#endif

void CommandBufferProxy::AsyncGetState(Task* completion_task) {
  IPC::Message* message = new CommandBufferMsg_AsyncGetState(route_id_);

  // Do not let a synchronous flush hold up this message. If this handler is
  // deferred until after the synchronous flush completes, it will overwrite the
  // cached last_state_ with out-of-date data.
  message->set_unblock(true);

  if (Send(message))
    pending_async_flush_tasks_.push(linked_ptr<Task>(completion_task));
}

void CommandBufferProxy::AsyncFlush(int32 put_offset, Task* completion_task) {
  IPC::Message* message = new CommandBufferMsg_AsyncFlush(route_id_,
                                                          put_offset);

  // Do not let a synchronous flush hold up this message. If this handler is
  // deferred until after the synchronous flush completes, it will overwrite the
  // cached last_state_ with out-of-date data.
  message->set_unblock(true);

  if (Send(message))
    pending_async_flush_tasks_.push(linked_ptr<Task>(completion_task));
}

void CommandBufferProxy::OnUpdateState(gpu::CommandBuffer::State state) {
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
