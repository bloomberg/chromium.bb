// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/gpu_channel_host.h"

#include <algorithm>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/common/gpu_param_traits_macros.h"
#include "ipc/ipc_sync_message.h"
#include "url/gurl.h"

using base::AutoLock;

namespace gpu {
namespace {

// Global atomic to generate unique transfer buffer IDs.
base::AtomicSequenceNumber g_next_transfer_buffer_id;

}  // namespace

// static
scoped_refptr<GpuChannelHost> GpuChannelHost::Create(
    GpuChannelHostFactory* factory,
    int channel_id,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const IPC::ChannelHandle& channel_handle,
    base::WaitableEvent* shutdown_event,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager) {
  DCHECK(factory->IsMainThread());
  scoped_refptr<GpuChannelHost> host =
      new GpuChannelHost(factory, channel_id, gpu_info, gpu_feature_info,
                         gpu_memory_buffer_manager);
  host->Connect(channel_handle, shutdown_event);
  return host;
}

GpuChannelHost::GpuChannelHost(
    GpuChannelHostFactory* factory,
    int channel_id,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager)
    : factory_(factory),
      channel_id_(channel_id),
      gpu_info_(gpu_info),
      gpu_feature_info_(gpu_feature_info),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager) {
  next_image_id_.GetNext();
  next_route_id_.GetNext();
}

void GpuChannelHost::Connect(const IPC::ChannelHandle& channel_handle,
                             base::WaitableEvent* shutdown_event) {
  DCHECK(factory_->IsMainThread());
  // Open a channel to the GPU process. We pass nullptr as the main listener
  // here since we need to filter everything to route it to the right thread.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      factory_->GetIOThreadTaskRunner();
  channel_ = IPC::SyncChannel::Create(channel_handle, IPC::Channel::MODE_CLIENT,
                                      nullptr, io_task_runner.get(), true,
                                      shutdown_event);

  channel_filter_ = new MessageFilter();

  // Install the filter last, because we intercept all leftover
  // messages.
  channel_->AddFilter(channel_filter_.get());
}

bool GpuChannelHost::Send(IPC::Message* msg) {
  TRACE_EVENT2("ipc", "GpuChannelHost::Send", "class",
               IPC_MESSAGE_ID_CLASS(msg->type()), "line",
               IPC_MESSAGE_ID_LINE(msg->type()));

  auto message = base::WrapUnique(msg);

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      factory_->GetIOThreadTaskRunner();
  DCHECK(!io_task_runner->BelongsToCurrentThread());

  // The GPU process never sends synchronous IPCs so clear the unblock flag to
  // preserve order.
  message->set_unblock(false);

  if (!message->is_sync()) {
    io_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&MessageFilter::SendMessage, channel_filter_,
                                  std::move(message), nullptr));
    return true;
  }

  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto deserializer = base::WrapUnique(
      static_cast<IPC::SyncMessage*>(message.get())->GetReplyDeserializer());

  IPC::PendingSyncMsg pending_sync(IPC::SyncMessage::GetMessageId(*message),
                                   deserializer.get(), &done_event);
  io_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&MessageFilter::SendMessage, channel_filter_,
                                std::move(message), &pending_sync));

  // http://crbug.com/125264
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  pending_sync.done_event->Wait();

  return pending_sync.send_result;
}

uint32_t GpuChannelHost::OrderingBarrier(
    int32_t route_id,
    int32_t put_offset,
    std::vector<ui::LatencyInfo> latency_info,
    std::vector<SyncToken> sync_token_fences) {
  AutoLock lock(context_lock_);

  if (flush_list_.empty() || flush_list_.back().route_id != route_id)
    flush_list_.push_back(FlushParams());

  FlushParams& flush_params = flush_list_.back();
  flush_params.flush_id = next_flush_id_++;
  flush_params.route_id = route_id;
  flush_params.put_offset = put_offset;
  flush_params.latency_info.insert(
      flush_params.latency_info.end(),
      std::make_move_iterator(latency_info.begin()),
      std::make_move_iterator(latency_info.end()));
  flush_params.sync_token_fences.insert(
      flush_params.sync_token_fences.end(),
      std::make_move_iterator(sync_token_fences.begin()),
      std::make_move_iterator(sync_token_fences.end()));
  return flush_params.flush_id;
}

void GpuChannelHost::EnsureFlush(uint32_t flush_id) {
  AutoLock lock(context_lock_);
  InternalFlush(flush_id);
}

void GpuChannelHost::VerifyFlush(uint32_t flush_id) {
  AutoLock lock(context_lock_);

  InternalFlush(flush_id);

  if (flush_id > verified_flush_id_) {
    Send(new GpuChannelMsg_Nop());
    verified_flush_id_ = next_flush_id_ - 1;
  }
}

void GpuChannelHost::InternalFlush(uint32_t flush_id) {
  context_lock_.AssertAcquired();

  if (!flush_list_.empty() && flush_id > flushed_flush_id_) {
    DCHECK_EQ(flush_list_.back().flush_id, next_flush_id_ - 1);

    Send(new GpuChannelMsg_FlushCommandBuffers(std::move(flush_list_)));

    flush_list_.clear();
    flushed_flush_id_ = next_flush_id_ - 1;
  }
}

void GpuChannelHost::DestroyChannel() {
  DCHECK(factory_->IsMainThread());
  AutoLock lock(context_lock_);
  channel_.reset();
}

void GpuChannelHost::AddRoute(int route_id,
                              base::WeakPtr<IPC::Listener> listener) {
  AddRouteWithTaskRunner(route_id, listener,
                         base::ThreadTaskRunnerHandle::Get());
}

void GpuChannelHost::AddRouteWithTaskRunner(
    int route_id,
    base::WeakPtr<IPC::Listener> listener,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      factory_->GetIOThreadTaskRunner();
  io_task_runner->PostTask(
      FROM_HERE, base::Bind(&GpuChannelHost::MessageFilter::AddRoute,
                            channel_filter_, route_id, listener, task_runner));
}

void GpuChannelHost::RemoveRoute(int route_id) {
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      factory_->GetIOThreadTaskRunner();
  io_task_runner->PostTask(
      FROM_HERE, base::Bind(&GpuChannelHost::MessageFilter::RemoveRoute,
                            channel_filter_, route_id));
}

base::SharedMemoryHandle GpuChannelHost::ShareToGpuProcess(
    const base::SharedMemoryHandle& source_handle) {
  if (IsLost())
    return base::SharedMemoryHandle();

  return base::SharedMemory::DuplicateHandle(source_handle);
}

int32_t GpuChannelHost::ReserveTransferBufferId() {
  // 0 is a reserved value.
  return g_next_transfer_buffer_id.GetNext() + 1;
}

int32_t GpuChannelHost::ReserveImageId() {
  return next_image_id_.GetNext();
}

int32_t GpuChannelHost::GenerateRouteID() {
  return next_route_id_.GetNext();
}

GpuChannelHost::~GpuChannelHost() {
#if DCHECK_IS_ON()
  AutoLock lock(context_lock_);
  DCHECK(!channel_)
      << "GpuChannelHost::DestroyChannel must be called before destruction.";
#endif
}

GpuChannelHost::MessageFilter::ListenerInfo::ListenerInfo() {}

GpuChannelHost::MessageFilter::ListenerInfo::ListenerInfo(
    const ListenerInfo& other) = default;

GpuChannelHost::MessageFilter::ListenerInfo::~ListenerInfo() {}

GpuChannelHost::MessageFilter::MessageFilter() = default;

GpuChannelHost::MessageFilter::~MessageFilter() = default;

void GpuChannelHost::MessageFilter::AddRoute(
    int32_t route_id,
    base::WeakPtr<IPC::Listener> listener,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(listeners_.find(route_id) == listeners_.end());
  DCHECK(task_runner);
  ListenerInfo info;
  info.listener = listener;
  info.task_runner = task_runner;
  listeners_[route_id] = info;
}

void GpuChannelHost::MessageFilter::RemoveRoute(int32_t route_id) {
  listeners_.erase(route_id);
}

void GpuChannelHost::MessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
  for (auto& message : pending_messages_)
    channel_->Send(message.release());
  pending_messages_.clear();
}

bool GpuChannelHost::MessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  if (message.is_reply()) {
    int id = IPC::SyncMessage::GetMessageId(message);
    auto it = pending_syncs_.find(id);
    if (it == pending_syncs_.end())
      return false;
    auto* pending_sync = it->second;
    pending_syncs_.erase(it);
    if (!message.is_reply_error()) {
      pending_sync->send_result =
          pending_sync->deserializer->SerializeOutputParameters(message);
    }
    pending_sync->done_event->Signal();
    return true;
  }

  auto it = listeners_.find(message.routing_id());
  if (it == listeners_.end())
    return false;

  const ListenerInfo& info = it->second;
  info.task_runner->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&IPC::Listener::OnMessageReceived),
                 info.listener, message));
  return true;
}

void GpuChannelHost::MessageFilter::OnChannelError() {
  channel_ = nullptr;
  // Set the lost state before signalling the proxies. That way, if they
  // themselves post a task to recreate the context, they will not try to re-use
  // this channel host.
  {
    AutoLock lock(lock_);
    lost_ = true;
  }

  pending_messages_.clear();

  for (auto& kv : pending_syncs_) {
    IPC::PendingSyncMsg* pending_sync = kv.second;
    pending_sync->done_event->Signal();
  }
  pending_syncs_.clear();

  // Inform all the proxies that an error has occurred. This will be reported
  // via OpenGL as a lost context.
  for (const auto& kv : listeners_) {
    const ListenerInfo& info = kv.second;
    info.task_runner->PostTask(
        FROM_HERE, base::Bind(&IPC::Listener::OnChannelError, info.listener));
  }

  listeners_.clear();
}

void GpuChannelHost::MessageFilter::OnChannelClosing() {
  OnChannelError();
}

void GpuChannelHost::MessageFilter::SendMessage(
    std::unique_ptr<IPC::Message> msg,
    IPC::PendingSyncMsg* pending_sync) {
  // Note: lost_ is only written on this thread, so it is safe to read here
  // without lock.
  if (pending_sync) {
    DCHECK(msg->is_sync());
    if (lost_) {
      pending_sync->done_event->Signal();
      return;
    }
    pending_syncs_.emplace(pending_sync->id, pending_sync);
  } else {
    if (lost_)
      return;
    DCHECK(!msg->is_sync());
  }
  DCHECK(!lost_);
  if (channel_)
    channel_->Send(msg.release());
  else
    pending_messages_.push_back(std::move(msg));
}

bool GpuChannelHost::MessageFilter::IsLost() const {
  AutoLock lock(lock_);
  return lost_;
}

}  // namespace gpu
