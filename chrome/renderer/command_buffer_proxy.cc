// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/process_util.h"
#include "chrome/common/command_buffer_messages.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/renderer/command_buffer_proxy.h"
#include "chrome/renderer/plugin_channel_host.h"

CommandBufferProxy::CommandBufferProxy(
    PluginChannelHost* channel,
    int route_id)
    : size_(0),
      channel_(channel),
      route_id_(route_id) {
}

CommandBufferProxy::~CommandBufferProxy() {
}

bool CommandBufferProxy::Send(IPC::Message* msg) {
  if (channel_)
    return channel_->Send(msg);

  // Callee takes ownership of message, regardless of whether Send is
  // successful. See IPC::Message::Sender.
  delete msg;
  return false;
}

base::SharedMemory* CommandBufferProxy::Initialize(int32 size) {
  DCHECK(!ring_buffer_.get());

  // Initialize the service. Assuming we are in the renderer process, the GPU
  // process is responsible for duplicating the handle. This might not be true
  // for NaCl.
  base::SharedMemoryHandle handle;
  if (Send(new CommandBufferMsg_Initialize(route_id_, size, &handle)) &&
      base::SharedMemory::IsHandleValid(handle)) {
    ring_buffer_.reset(new base::SharedMemory(handle, false));
    if (ring_buffer_->Map(size * sizeof(int32))) {
      size_ = size;
      return ring_buffer_.get();
    }

    ring_buffer_.reset();
  }

  return NULL;
}

base::SharedMemory* CommandBufferProxy::GetRingBuffer() {
  // Return locally cached ring buffer.
  return ring_buffer_.get();
}

int32 CommandBufferProxy::GetSize() {
  // Return locally cached size.
  return size_;
}

int32 CommandBufferProxy::SyncOffsets(int32 put_offset) {
  int32 get_offset;
  if (Send(new CommandBufferMsg_SyncOffsets(route_id_,
                                            put_offset,
                                            &get_offset)))
    return get_offset;

  return -1;
}

int32 CommandBufferProxy::GetGetOffset() {
  int32 get_offset;
  if (Send(new CommandBufferMsg_GetGetOffset(route_id_, &get_offset)))
    return get_offset;

  return -1;
}

void CommandBufferProxy::SetGetOffset(int32 get_offset) {
  // Not implemented in proxy.
  NOTREACHED();
}

int32 CommandBufferProxy::GetPutOffset() {
  int put_offset;
  if (Send(new CommandBufferMsg_GetPutOffset(route_id_, &put_offset)))
    return put_offset;

  return -1;
}

void CommandBufferProxy::SetPutOffsetChangeCallback(Callback0::Type* callback) {
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
  transfer_buffers_.erase(id);

  Send(new CommandBufferMsg_DestroyTransferBuffer(route_id_, id));
}

base::SharedMemory* CommandBufferProxy::GetTransferBuffer(int32 id) {
  // Check local cache to see if there is already a client side shared memory
  // object for this id.
  TransferBufferMap::iterator it = transfer_buffers_.find(id);
  if (it != transfer_buffers_.end())
    return it->second.get();

  // Assuming we are in the renderer process, the service is responsible for
  // duplicating the handle. This might not be true for NaCl.
  base::SharedMemoryHandle handle;
  if (!Send(new CommandBufferMsg_GetTransferBuffer(route_id_, id, &handle)))
    return NULL;

  // Cache the transfer buffer shared memory object client side.
  base::SharedMemory* transfer_buffer =
      new base::SharedMemory(handle, false, base::GetCurrentProcessHandle());
  transfer_buffers_[id].reset(transfer_buffer);

  return transfer_buffer;
}

int32 CommandBufferProxy::GetToken() {
  int32 token;
  if (Send(new CommandBufferMsg_GetToken(route_id_, &token)))
    return token;

  return -1;
}

void CommandBufferProxy::SetToken(int32 token) {
  // Not implemented in proxy.
  NOTREACHED();
}

int32 CommandBufferProxy::ResetParseError() {
  int32 parse_error;
  if (Send(new CommandBufferMsg_ResetParseError(route_id_, &parse_error)))
    return parse_error;

  return -1;
}

void CommandBufferProxy::SetParseError(int32 parse_error) {
  // Not implemented in proxy.
  NOTREACHED();
}

bool CommandBufferProxy::GetErrorStatus() {
  bool status;
  if (Send(new CommandBufferMsg_GetErrorStatus(route_id_, &status)))
    return status;

  return true;
}

void CommandBufferProxy::RaiseErrorStatus() {
  // Not implemented in proxy.
  NOTREACHED();
}
