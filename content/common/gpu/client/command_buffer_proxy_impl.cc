// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/command_buffer_proxy_impl.h"

#include "base/callback.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "base/stl_util.h"
#include "content/common/child_process_messages.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/plugin_messages.h"
#include "content/common/view_messages.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "ui/gfx/size.h"

namespace content {

CommandBufferProxyImpl::CommandBufferProxyImpl(
    GpuChannelHost* channel,
    int route_id)
    : channel_(channel),
      route_id_(route_id),
      flush_count_(0),
      last_put_offset_(-1),
      next_signal_id_(0) {
}

CommandBufferProxyImpl::~CommandBufferProxyImpl() {
  // Delete all the locally cached shared memory objects, closing the handle
  // in this process.
  for (TransferBufferMap::iterator it = transfer_buffers_.begin();
       it != transfer_buffers_.end();
       ++it) {
    delete it->second.shared_memory;
    it->second.shared_memory = NULL;
  }
  for (Decoders::iterator it = video_decoder_hosts_.begin();
      it != video_decoder_hosts_.end(); ++it) {
    if (it->second)
      it->second->OnChannelError();
  }
}

bool CommandBufferProxyImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CommandBufferProxyImpl, message)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_Destroyed, OnDestroyed);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_EchoAck, OnEchoAck);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_ConsoleMsg, OnConsoleMessage);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SetMemoryAllocation,
                        OnSetMemoryAllocation);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SignalSyncPointAck,
                        OnSignalSyncPointAck);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  DCHECK(handled);
  return handled;
}

void CommandBufferProxyImpl::OnChannelError() {
  OnDestroyed(gpu::error::kUnknown);
}

void CommandBufferProxyImpl::OnDestroyed(gpu::error::ContextLostReason reason) {
  // Prevent any further messages from being sent.
  channel_ = NULL;

  // When the client sees that the context is lost, they should delete this
  // CommandBufferProxyImpl and create a new one.
  last_state_.error = gpu::error::kLostContext;
  last_state_.context_lost_reason = reason;

  if (!channel_error_callback_.is_null()) {
    channel_error_callback_.Run();
    // Avoid calling the error callback more than once.
    channel_error_callback_.Reset();
  }
}

void CommandBufferProxyImpl::OnEchoAck() {
  DCHECK(!echo_tasks_.empty());
  base::Closure callback = echo_tasks_.front();
  echo_tasks_.pop();
  callback.Run();
}

void CommandBufferProxyImpl::OnConsoleMessage(
    const GPUCommandBufferConsoleMessage& message) {
  if (!console_message_callback_.is_null()) {
    console_message_callback_.Run(message.message, message.id);
  }
}

void CommandBufferProxyImpl::SetMemoryAllocationChangedCallback(
    const base::Callback<void(const GpuMemoryAllocationForRenderer&)>&
        callback) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  memory_allocation_changed_callback_ = callback;
  Send(new GpuCommandBufferMsg_SetClientHasMemoryAllocationChangedCallback(
      route_id_, !memory_allocation_changed_callback_.is_null()));
}

void CommandBufferProxyImpl::OnSetMemoryAllocation(
    const GpuMemoryAllocationForRenderer& allocation) {
  if (!memory_allocation_changed_callback_.is_null())
    memory_allocation_changed_callback_.Run(allocation);
}

void CommandBufferProxyImpl::OnSignalSyncPointAck(uint32 id) {
  SignalTaskMap::iterator it = signal_tasks_.find(id);
  DCHECK(it != signal_tasks_.end());
  base::Closure callback = it->second;
  signal_tasks_.erase(it);
  callback.Run();
}

void CommandBufferProxyImpl::SetChannelErrorCallback(
    const base::Closure& callback) {
  channel_error_callback_ = callback;
}

bool CommandBufferProxyImpl::Initialize() {
  shared_state_shm_.reset(channel_->factory()->AllocateSharedMemory(
      sizeof(*shared_state())).release());
  if (!shared_state_shm_.get())
    return false;

  if (!shared_state_shm_->Map(sizeof(*shared_state())))
    return false;

  shared_state()->Initialize();

  // This handle is owned by the GPU process and must be passed to it or it
  // will leak. In otherwords, do not early out on error between here and the
  // sending of the Initialize IPC below.
  base::SharedMemoryHandle handle =
      channel_->ShareToGpuProcess(shared_state_shm_.get());
  if (!base::SharedMemory::IsHandleValid(handle))
    return false;

  bool result;
  if (!Send(new GpuCommandBufferMsg_Initialize(route_id_, handle, &result))) {
    LOG(ERROR) << "Could not send GpuCommandBufferMsg_Initialize.";
    return false;
  }

  if (!result) {
    LOG(ERROR) << "Failed to initialize command buffer service.";
    return false;
  }

  return true;
}

gpu::CommandBuffer::State CommandBufferProxyImpl::GetState() {
  // Send will flag state with lost context if IPC fails.
  if (last_state_.error == gpu::error::kNoError) {
    gpu::CommandBuffer::State state;
    if (Send(new GpuCommandBufferMsg_GetState(route_id_, &state)))
      OnUpdateState(state);
  }

  TryUpdateState();
  return last_state_;
}

gpu::CommandBuffer::State CommandBufferProxyImpl::GetLastState() {
  return last_state_;
}

int32 CommandBufferProxyImpl::GetLastToken() {
  TryUpdateState();
  return last_state_.token;
}

void CommandBufferProxyImpl::Flush(int32 put_offset) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  TRACE_EVENT1("gpu",
               "CommandBufferProxyImpl::Flush",
               "put_offset",
               put_offset);

  if (last_put_offset_ == put_offset)
    return;

  last_put_offset_ = put_offset;

  Send(new GpuCommandBufferMsg_AsyncFlush(route_id_,
                                          put_offset,
                                          ++flush_count_));
}

gpu::CommandBuffer::State CommandBufferProxyImpl::FlushSync(
    int32 put_offset,
    int32 last_known_get) {
  TRACE_EVENT1("gpu", "CommandBufferProxyImpl::FlushSync", "put_offset",
               put_offset);
  Flush(put_offset);
  TryUpdateState();
  if (last_known_get == last_state_.get_offset) {
    // Send will flag state with lost context if IPC fails.
    if (last_state_.error == gpu::error::kNoError) {
      gpu::CommandBuffer::State state;
      if (Send(new GpuCommandBufferMsg_GetStateFast(route_id_,
                                                    &state)))
        OnUpdateState(state);
    }
    TryUpdateState();
  }

  return last_state_;
}

void CommandBufferProxyImpl::SetGetBuffer(int32 shm_id) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new GpuCommandBufferMsg_SetGetBuffer(route_id_, shm_id));
  last_put_offset_ = -1;
}

void CommandBufferProxyImpl::SetGetOffset(int32 get_offset) {
  // Not implemented in proxy.
  NOTREACHED();
}

gpu::Buffer CommandBufferProxyImpl::CreateTransferBuffer(size_t size,
                                                         int32* id) {
  *id = -1;

  if (last_state_.error != gpu::error::kNoError)
    return gpu::Buffer();

  int32 new_id = channel_->ReserveTransferBufferId();
  DCHECK(transfer_buffers_.find(new_id) == transfer_buffers_.end());

  scoped_ptr<base::SharedMemory> shared_memory(
      channel_->factory()->AllocateSharedMemory(size));
  if (!shared_memory.get())
    return gpu::Buffer();

  DCHECK(!shared_memory->memory());
  if (!shared_memory->Map(size))
    return gpu::Buffer();

  // This handle is owned by the GPU process and must be passed to it or it
  // will leak. In otherwords, do not early out on error between here and the
  // sending of the RegisterTransferBuffer IPC below.
  base::SharedMemoryHandle handle =
      channel_->ShareToGpuProcess(shared_memory.get());
  if (!base::SharedMemory::IsHandleValid(handle))
    return gpu::Buffer();

  if (!Send(new GpuCommandBufferMsg_RegisterTransferBuffer(route_id_,
                                                           new_id,
                                                           handle,
                                                           size))) {
    return gpu::Buffer();
  }

  *id = new_id;
  gpu::Buffer buffer;
  buffer.ptr = shared_memory->memory();
  buffer.size = size;
  buffer.shared_memory = shared_memory.release();
  transfer_buffers_[new_id] = buffer;

  return buffer;
}

void CommandBufferProxyImpl::DestroyTransferBuffer(int32 id) {
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

gpu::Buffer CommandBufferProxyImpl::GetTransferBuffer(int32 id) {
  if (last_state_.error != gpu::error::kNoError)
    return gpu::Buffer();

  // Check local cache to see if there is already a client side shared memory
  // object for this id.
  TransferBufferMap::iterator it = transfer_buffers_.find(id);
  if (it != transfer_buffers_.end()) {
    return it->second;
  }

  // Assuming we are in the renderer process, the service is responsible for
  // duplicating the handle. This might not be true for NaCl.
  base::SharedMemoryHandle handle = base::SharedMemoryHandle();
  uint32 size;
  if (!Send(new GpuCommandBufferMsg_GetTransferBuffer(route_id_,
                                                      id,
                                                      &handle,
                                                      &size))) {
    return gpu::Buffer();
  }

  // Cache the transfer buffer shared memory object client side.
  scoped_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(handle, false));

  // Map the shared memory on demand.
  if (!shared_memory->memory()) {
    if (!shared_memory->Map(size))
      return gpu::Buffer();
  }

  gpu::Buffer buffer;
  buffer.ptr = shared_memory->memory();
  buffer.size = size;
  buffer.shared_memory = shared_memory.release();
  transfer_buffers_[id] = buffer;

  return buffer;
}

void CommandBufferProxyImpl::SetToken(int32 token) {
  // Not implemented in proxy.
  NOTREACHED();
}

void CommandBufferProxyImpl::SetParseError(
    gpu::error::Error error) {
  // Not implemented in proxy.
  NOTREACHED();
}

void CommandBufferProxyImpl::SetContextLostReason(
    gpu::error::ContextLostReason reason) {
  // Not implemented in proxy.
  NOTREACHED();
}

int CommandBufferProxyImpl::GetRouteID() const {
  return route_id_;
}

bool CommandBufferProxyImpl::Echo(const base::Closure& callback) {
  if (last_state_.error != gpu::error::kNoError) {
    return false;
  }

  if (!Send(new GpuCommandBufferMsg_Echo(route_id_,
                    GpuCommandBufferMsg_EchoAck(route_id_)))) {
    return false;
  }

  echo_tasks_.push(callback);

  return true;
}

bool CommandBufferProxyImpl::SetSurfaceVisible(bool visible) {
  if (last_state_.error != gpu::error::kNoError)
    return false;

  return Send(new GpuCommandBufferMsg_SetSurfaceVisible(route_id_, visible));
}

bool CommandBufferProxyImpl::DiscardBackbuffer() {
  if (last_state_.error != gpu::error::kNoError)
    return false;

  return Send(new GpuCommandBufferMsg_DiscardBackbuffer(route_id_));
}

bool CommandBufferProxyImpl::EnsureBackbuffer() {
  if (last_state_.error != gpu::error::kNoError)
    return false;

  return Send(new GpuCommandBufferMsg_EnsureBackbuffer(route_id_));
}

uint32 CommandBufferProxyImpl::InsertSyncPoint() {
  uint32 sync_point = 0;
  Send(new GpuCommandBufferMsg_InsertSyncPoint(route_id_, &sync_point));
  return sync_point;
}

bool CommandBufferProxyImpl::SignalSyncPoint(uint32 sync_point,
                                             const base::Closure& callback) {
  if (last_state_.error != gpu::error::kNoError) {
    return false;
  }

  uint32 signal_id = next_signal_id_++;
  if (!Send(new GpuCommandBufferMsg_SignalSyncPoint(route_id_,
                                                    sync_point,
                                                    signal_id))) {
    return false;
  }

  signal_tasks_.insert(std::make_pair(signal_id, callback));

  return true;
}


bool CommandBufferProxyImpl::GenerateMailboxNames(
    unsigned num,
    std::vector<std::string>* names) {
  return channel_->GenerateMailboxNames(num, names);
}

bool CommandBufferProxyImpl::SetParent(
    CommandBufferProxy* parent_command_buffer,
    uint32 parent_texture_id) {
  if (last_state_.error != gpu::error::kNoError)
    return false;

  bool result;
  if (parent_command_buffer) {
    if (!Send(new GpuCommandBufferMsg_SetParent(
        route_id_,
        parent_command_buffer->GetRouteID(),
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

GpuVideoDecodeAcceleratorHost*
CommandBufferProxyImpl::CreateVideoDecoder(
    media::VideoCodecProfile profile,
    media::VideoDecodeAccelerator::Client* client) {
  int decoder_route_id;
  if (!Send(new GpuCommandBufferMsg_CreateVideoDecoder(route_id_, profile,
                                                       &decoder_route_id))) {
    LOG(ERROR) << "Send(GpuCommandBufferMsg_CreateVideoDecoder) failed";
    return NULL;
  }

  if (decoder_route_id < 0) {
    DLOG(ERROR) << "Failed to Initialize GPU decoder on profile: " << profile;
    return NULL;
  }

  GpuVideoDecodeAcceleratorHost* decoder_host =
      new GpuVideoDecodeAcceleratorHost(channel_, decoder_route_id, client);
  bool inserted = video_decoder_hosts_.insert(std::make_pair(
      decoder_route_id, base::AsWeakPtr(decoder_host))).second;
  DCHECK(inserted);

  channel_->AddRoute(decoder_route_id, base::AsWeakPtr(decoder_host));

  return decoder_host;
}

gpu::error::Error CommandBufferProxyImpl::GetLastError() {
  return last_state_.error;
}

bool CommandBufferProxyImpl::Send(IPC::Message* msg) {
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
  // successful. See IPC::Sender.
  delete msg;
  return false;
}

void CommandBufferProxyImpl::OnUpdateState(
    const gpu::CommandBuffer::State& state) {
  // Handle wraparound. It works as long as we don't have more than 2B state
  // updates in flight across which reordering occurs.
  if (state.generation - last_state_.generation < 0x80000000U)
    last_state_ = state;
}

void CommandBufferProxyImpl::SetOnConsoleMessageCallback(
    const GpuConsoleMessageCallback& callback) {
  console_message_callback_ = callback;
}

void CommandBufferProxyImpl::TryUpdateState() {
  if (last_state_.error == gpu::error::kNoError)
    shared_state()->Read(&last_state_);
}

void CommandBufferProxyImpl::SendManagedMemoryStats(
    const GpuManagedMemoryStats& stats) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new GpuCommandBufferMsg_SendClientManagedMemoryStats(route_id_,
                                                            stats));
}

}  // namespace content
