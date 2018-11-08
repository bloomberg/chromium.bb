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
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/common/gpu_param_traits_macros.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_sync_message.h"
#include "url/gurl.h"

using base::AutoLock;

namespace gpu {
namespace {

// Global atomic to generate unique transfer buffer IDs.
base::AtomicSequenceNumber g_next_transfer_buffer_id;

}  // namespace

GpuChannelHost::GpuChannelHost(int channel_id,
                               const gpu::GPUInfo& gpu_info,
                               const gpu::GpuFeatureInfo& gpu_feature_info,
                               mojo::ScopedMessagePipeHandle handle)
    : io_thread_(base::ThreadTaskRunnerHandle::Get()),
      channel_id_(channel_id),
      gpu_info_(gpu_info),
      gpu_feature_info_(gpu_feature_info),
      listener_(new Listener(std::move(handle), io_thread_),
                base::OnTaskRunnerDeleter(io_thread_)),
      shared_image_interface_(
          this,
          static_cast<int32_t>(
              GpuChannelReservedRoutes::kSharedImageInterface)),
      image_decode_accelerator_proxy_(
          this,
          static_cast<int32_t>(
              GpuChannelReservedRoutes::kImageDecodeAccelerator)) {
  next_image_id_.GetNext();
  for (int32_t i = 0;
       i <= static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue); ++i)
    next_route_id_.GetNext();
}

bool GpuChannelHost::Send(IPC::Message* msg) {
  TRACE_EVENT2("ipc", "GpuChannelHost::Send", "class",
               IPC_MESSAGE_ID_CLASS(msg->type()), "line",
               IPC_MESSAGE_ID_LINE(msg->type()));

  auto message = base::WrapUnique(msg);

  DCHECK(!io_thread_->BelongsToCurrentThread());

  // The GPU process never sends synchronous IPCs so clear the unblock flag to
  // preserve order.
  message->set_unblock(false);

  if (!message->is_sync()) {
    io_thread_->PostTask(FROM_HERE,
                         base::BindOnce(&Listener::SendMessage,
                                        base::Unretained(listener_.get()),
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
  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&Listener::SendMessage, base::Unretained(listener_.get()),
                     std::move(message), &pending_sync));

  // http://crbug.com/125264
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  pending_sync.done_event->Wait();

  return pending_sync.send_result;
}

uint32_t GpuChannelHost::OrderingBarrier(
    int32_t route_id,
    int32_t put_offset,
    std::vector<SyncToken> sync_token_fences) {
  AutoLock lock(context_lock_);

  if (pending_ordering_barrier_ &&
      pending_ordering_barrier_->route_id != route_id)
    EnqueuePendingOrderingBarrier();
  if (!pending_ordering_barrier_)
    pending_ordering_barrier_.emplace();

  pending_ordering_barrier_->deferred_message_id = next_deferred_message_id_++;
  pending_ordering_barrier_->route_id = route_id;
  pending_ordering_barrier_->put_offset = put_offset;
  pending_ordering_barrier_->sync_token_fences.insert(
      pending_ordering_barrier_->sync_token_fences.end(),
      std::make_move_iterator(sync_token_fences.begin()),
      std::make_move_iterator(sync_token_fences.end()));
  return pending_ordering_barrier_->deferred_message_id;
}

uint32_t GpuChannelHost::EnqueueDeferredMessage(
    const IPC::Message& message,
    std::vector<SyncToken> sync_token_fences) {
  AutoLock lock(context_lock_);

  EnqueuePendingOrderingBarrier();
  enqueued_deferred_message_id_ = next_deferred_message_id_++;
  GpuDeferredMessage deferred_message;
  deferred_message.message = message;
  deferred_message.sync_token_fences = std::move(sync_token_fences);
  deferred_messages_.push_back(std::move(deferred_message));
  return enqueued_deferred_message_id_;
}

void GpuChannelHost::EnsureFlush(uint32_t deferred_message_id) {
  AutoLock lock(context_lock_);
  InternalFlush(deferred_message_id);
}

void GpuChannelHost::VerifyFlush(uint32_t deferred_message_id) {
  AutoLock lock(context_lock_);

  InternalFlush(deferred_message_id);

  if (deferred_message_id > verified_deferred_message_id_) {
    Send(new GpuChannelMsg_Nop());
    verified_deferred_message_id_ = flushed_deferred_message_id_;
  }
}

void GpuChannelHost::EnqueuePendingOrderingBarrier() {
  context_lock_.AssertAcquired();
  if (!pending_ordering_barrier_)
    return;
  DCHECK_LT(enqueued_deferred_message_id_,
            pending_ordering_barrier_->deferred_message_id);
  enqueued_deferred_message_id_ =
      pending_ordering_barrier_->deferred_message_id;
  GpuDeferredMessage deferred_message;
  deferred_message.message = GpuCommandBufferMsg_AsyncFlush(
      pending_ordering_barrier_->route_id,
      pending_ordering_barrier_->put_offset,
      pending_ordering_barrier_->deferred_message_id);
  deferred_message.sync_token_fences =
      std::move(pending_ordering_barrier_->sync_token_fences);
  deferred_messages_.push_back(std::move(deferred_message));
  pending_ordering_barrier_.reset();
}

void GpuChannelHost::InternalFlush(uint32_t deferred_message_id) {
  context_lock_.AssertAcquired();

  EnqueuePendingOrderingBarrier();
  if (!deferred_messages_.empty() &&
      deferred_message_id > flushed_deferred_message_id_) {
    DCHECK_EQ(enqueued_deferred_message_id_, next_deferred_message_id_ - 1);

    Send(
        new GpuChannelMsg_FlushDeferredMessages(std::move(deferred_messages_)));

    deferred_messages_.clear();
    flushed_deferred_message_id_ = next_deferred_message_id_ - 1;
  }
}

void GpuChannelHost::DestroyChannel() {
  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&Listener::Close, base::Unretained(listener_.get())));
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
  io_thread_->PostTask(FROM_HERE,
                       base::Bind(&GpuChannelHost::Listener::AddRoute,
                                  base::Unretained(listener_.get()), route_id,
                                  listener, task_runner));
}

void GpuChannelHost::RemoveRoute(int route_id) {
  io_thread_->PostTask(FROM_HERE,
                       base::Bind(&GpuChannelHost::Listener::RemoveRoute,
                                  base::Unretained(listener_.get()), route_id));
}

base::SharedMemoryHandle GpuChannelHost::ShareToGpuProcess(
    const base::SharedMemoryHandle& source_handle) {
  if (IsLost())
    return base::SharedMemoryHandle();

  return base::SharedMemory::DuplicateHandle(source_handle);
}

base::UnsafeSharedMemoryRegion GpuChannelHost::ShareToGpuProcess(
    const base::UnsafeSharedMemoryRegion& source_region) {
  if (IsLost())
    return base::UnsafeSharedMemoryRegion();

  return source_region.Duplicate();
}

int32_t GpuChannelHost::ReserveTransferBufferId() {
  // 0 is a reserved value.
  int32_t id = g_next_transfer_buffer_id.GetNext();
  if (id)
    return id;
  return g_next_transfer_buffer_id.GetNext();
}

int32_t GpuChannelHost::ReserveImageId() {
  return next_image_id_.GetNext();
}

int32_t GpuChannelHost::GenerateRouteID() {
  return next_route_id_.GetNext();
}

void GpuChannelHost::CrashGpuProcessForTesting() {
  Send(new GpuChannelMsg_CrashForTesting());
}

GpuChannelHost::~GpuChannelHost() = default;

GpuChannelHost::Listener::RouteInfo::RouteInfo() = default;

GpuChannelHost::Listener::RouteInfo::RouteInfo(const RouteInfo& other) =
    default;
GpuChannelHost::Listener::RouteInfo::RouteInfo(RouteInfo&& other) = default;
GpuChannelHost::Listener::RouteInfo::~RouteInfo() = default;

GpuChannelHost::Listener::RouteInfo& GpuChannelHost::Listener::RouteInfo::
operator=(const RouteInfo& other) = default;

GpuChannelHost::Listener::RouteInfo& GpuChannelHost::Listener::RouteInfo::
operator=(RouteInfo&& other) = default;

GpuChannelHost::OrderingBarrierInfo::OrderingBarrierInfo() = default;

GpuChannelHost::OrderingBarrierInfo::~OrderingBarrierInfo() = default;

GpuChannelHost::OrderingBarrierInfo::OrderingBarrierInfo(
    OrderingBarrierInfo&&) = default;

GpuChannelHost::OrderingBarrierInfo& GpuChannelHost::OrderingBarrierInfo::
operator=(OrderingBarrierInfo&&) = default;

GpuChannelHost::Listener::Listener(
    mojo::ScopedMessagePipeHandle handle,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : channel_(IPC::ChannelMojo::Create(std::move(handle),
                                        IPC::Channel::MODE_CLIENT,
                                        this,
                                        io_task_runner,
                                        base::ThreadTaskRunnerHandle::Get())) {
  DCHECK(channel_);
  DCHECK(io_task_runner->BelongsToCurrentThread());
  bool result = channel_->Connect();
  DCHECK(result);
}

GpuChannelHost::Listener::~Listener() {
  DCHECK(pending_syncs_.empty());
}

void GpuChannelHost::Listener::Close() {
  OnChannelError();
}

void GpuChannelHost::Listener::AddRoute(
    int32_t route_id,
    base::WeakPtr<IPC::Listener> listener,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(routes_.find(route_id) == routes_.end());
  DCHECK(task_runner);
  RouteInfo info;
  info.listener = listener;
  info.task_runner = std::move(task_runner);
  routes_[route_id] = info;

  if (lost_) {
    info.task_runner->PostTask(
        FROM_HERE, base::Bind(&IPC::Listener::OnChannelError, info.listener));
  }
}

void GpuChannelHost::Listener::RemoveRoute(int32_t route_id) {
  routes_.erase(route_id);
}

bool GpuChannelHost::Listener::OnMessageReceived(const IPC::Message& message) {
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

  auto it = routes_.find(message.routing_id());
  if (it == routes_.end())
    return false;

  const RouteInfo& info = it->second;
  info.task_runner->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&IPC::Listener::OnMessageReceived),
                 info.listener, message));
  return true;
}

void GpuChannelHost::Listener::OnChannelError() {
  channel_ = nullptr;
  // Set the lost state before signalling the proxies. That way, if they
  // themselves post a task to recreate the context, they will not try to re-use
  // this channel host.
  {
    AutoLock lock(lock_);
    lost_ = true;
  }

  for (auto& kv : pending_syncs_) {
    IPC::PendingSyncMsg* pending_sync = kv.second;
    pending_sync->done_event->Signal();
  }
  pending_syncs_.clear();

  // Inform all the proxies that an error has occurred. This will be reported
  // via OpenGL as a lost context.
  for (const auto& kv : routes_) {
    const RouteInfo& info = kv.second;
    info.task_runner->PostTask(
        FROM_HERE, base::Bind(&IPC::Listener::OnChannelError, info.listener));
  }

  routes_.clear();
}

void GpuChannelHost::Listener::SendMessage(std::unique_ptr<IPC::Message> msg,
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
  channel_->Send(msg.release());
}

bool GpuChannelHost::Listener::IsLost() const {
  AutoLock lock(lock_);
  return lost_;
}

}  // namespace gpu
