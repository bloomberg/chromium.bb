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

#if defined(OS_WIN)
#include "content/public/common/sandbox_init.h"
#endif

using gpu::Buffer;

CommandBufferProxyImpl::CommandBufferProxyImpl(
    GpuChannelHost* channel,
    int route_id)
    : channel_(channel),
      route_id_(route_id),
      flush_count_(0),
      last_put_offset_(-1) {
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
}

bool CommandBufferProxyImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CommandBufferProxyImpl, message)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_Destroyed, OnDestroyed);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_NotifyRepaint,
                        OnNotifyRepaint);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_EchoAck, OnEchoAck);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_ConsoleMsg, OnConsoleMessage);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SetMemoryAllocation,
                        OnSetMemoryAllocation);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  DCHECK(handled);
  return handled;
}

void CommandBufferProxyImpl::OnChannelError() {
  for (Decoders::iterator it = video_decoder_hosts_.begin();
       it != video_decoder_hosts_.end(); ++it) {
    it->second->OnChannelError();
  }
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

void CommandBufferProxyImpl::SetChannelErrorCallback(
    const base::Closure& callback) {
  channel_error_callback_ = callback;
}

bool CommandBufferProxyImpl::Initialize() {
  bool result;
  if (!Send(new GpuCommandBufferMsg_Initialize(route_id_, &result))) {
    LOG(ERROR) << "Could not send GpuCommandBufferMsg_Initialize.";
    return false;
  }

  if (!result) {
    LOG(ERROR) << "Failed to initialize command buffer service.";
    return false;
  }

  int32 state_buffer = CreateTransferBuffer(sizeof *shared_state_, -1);

  if (state_buffer == -1) {
    LOG(ERROR) << "Failed to create shared state transfer buffer.";
    return false;
  }

  gpu::Buffer buffer = GetTransferBuffer(state_buffer);
  if (!buffer.ptr) {
    LOG(ERROR) << "Failed to get shared state transfer buffer";
    return false;
  }

  shared_state_ = reinterpret_cast<gpu::CommandBufferSharedState*>(buffer.ptr);
  shared_state_->Initialize();

  if (!Send(new GpuCommandBufferMsg_SetSharedStateBuffer(route_id_,
                                                         state_buffer))) {
    LOG(ERROR) << "Failed to initialize shared command buffer state.";
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

int32 CommandBufferProxyImpl::CreateTransferBuffer(
    size_t size, int32 id_request) {
  if (last_state_.error != gpu::error::kNoError)
    return -1;

  // Take ownership of shared memory. This will close the handle if Send below
  // fails. Otherwise, callee takes ownership before this variable
  // goes out of scope by duping the handle.
  scoped_ptr<base::SharedMemory> shm(
      channel_->factory()->AllocateSharedMemory(size));
  if (!shm.get())
    return -1;

  base::SharedMemoryHandle handle = shm->handle();
#if defined(OS_WIN)
  // Windows needs to explicitly duplicate the handle out to another process.
  if (!content::BrokerDuplicateHandle(handle, channel_->gpu_pid(),
                                      &handle, FILE_MAP_WRITE, 0)) {
    return -1;
  }
#elif defined(OS_POSIX)
  DCHECK(!handle.auto_close);
#endif

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

int32 CommandBufferProxyImpl::RegisterTransferBuffer(
    base::SharedMemory* shared_memory,
    size_t size,
    int32 id_request) {
  if (last_state_.error != gpu::error::kNoError)
    return -1;

  // Returns FileDescriptor with auto_close off.
  base::SharedMemoryHandle handle = shared_memory->handle();
#if defined(OS_WIN)
  // Windows needs to explicitly duplicate the handle out to another process.
  if (!content::BrokerDuplicateHandle(handle, channel_->gpu_pid(),
                                      &handle, FILE_MAP_WRITE, 0)) {
    return -1;
  }
#endif

  int32 id;
  if (!Send(new GpuCommandBufferMsg_RegisterTransferBuffer(
      route_id_,
      handle,
      size,
      id_request,
      &id))) {
    return -1;
  }

  return id;
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

Buffer CommandBufferProxyImpl::GetTransferBuffer(int32 id) {
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

void CommandBufferProxyImpl::SetToken(int32 token) {
  // Not implemented in proxy.
  NOTREACHED();
}

void CommandBufferProxyImpl::OnNotifyRepaint() {
  if (!notify_repaint_task_.is_null())
    MessageLoop::current()->PostNonNestableTask(
        FROM_HERE, notify_repaint_task_);
  notify_repaint_task_.Reset();
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

void CommandBufferProxyImpl::SetNotifyRepaintTask(const base::Closure& task) {
  notify_repaint_task_ = task;
}

scoped_refptr<GpuVideoDecodeAcceleratorHost>
CommandBufferProxyImpl::CreateVideoDecoder(
    media::VideoCodecProfile profile,
    media::VideoDecodeAccelerator::Client* client) {
  int decoder_route_id;
  if (!Send(new GpuCommandBufferMsg_CreateVideoDecoder(route_id_, profile,
                                                       &decoder_route_id))) {
    LOG(ERROR) << "Send(GpuCommandBufferMsg_CreateVideoDecoder) failed";
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
  // successful. See IPC::Message::Sender.
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
    shared_state_->Read(&last_state_);
}
