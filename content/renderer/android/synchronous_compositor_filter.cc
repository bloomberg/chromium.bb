// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/synchronous_compositor_filter.h"

#include <utility>

#include "base/callback.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/android/sync_compositor_messages.h"
#include "content/common/input_messages.h"
#include "content/renderer/android/synchronous_compositor_proxy.h"
#include "ipc/ipc_message_macros.h"
#include "ui/events/blink/synchronous_input_handler_proxy.h"

namespace content {

SynchronousCompositorFilter::SynchronousCompositorFilter(
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner)
    : compositor_task_runner_(compositor_task_runner), filter_ready_(false) {
  DCHECK(compositor_task_runner_);
}

SynchronousCompositorFilter::~SynchronousCompositorFilter() {}

void SynchronousCompositorFilter::OnFilterAdded(IPC::Sender* sender) {
  io_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  sender_ = sender;
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SynchronousCompositorFilter::FilterReadyOnCompositorThread,
                 this));
}

void SynchronousCompositorFilter::OnFilterRemoved() {
  sender_ = nullptr;
}

void SynchronousCompositorFilter::OnChannelClosing() {
  sender_ = nullptr;
}

bool SynchronousCompositorFilter::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK_EQ(SyncCompositorMsgStart, IPC_MESSAGE_ID_CLASS(message.type()));
  bool result = compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &SynchronousCompositorFilter::OnMessageReceivedOnCompositorThread,
          this, message));
  if (!result && message.is_sync()) {
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
    reply->set_reply_error();
    SendOnIOThread(reply);
  }
  return result;
}

SynchronousCompositorProxy* SynchronousCompositorFilter::FindProxy(
    int routing_id) {
  auto itr = sync_compositor_map_.find(routing_id);
  if (itr == sync_compositor_map_.end()) {
    return nullptr;
  }
  return itr->second;
}

bool SynchronousCompositorFilter::GetSupportedMessageClasses(
    std::vector<uint32_t>* supported_message_classes) const {
  supported_message_classes->push_back(SyncCompositorMsgStart);
  return true;
}

void SynchronousCompositorFilter::OnMessageReceivedOnCompositorThread(
    const IPC::Message& message) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());

  SynchronousCompositorProxy* proxy = FindProxy(message.routing_id());
  if (proxy) {
    proxy->OnMessageReceived(message);
    return;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SynchronousCompositorFilter, message)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_SynchronizeRendererState,
                        OnSynchronizeRendererState)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  if (handled)
    return;

  if (!message.is_sync())
    return;
  IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
  reply->set_reply_error();
  Send(reply);
}

bool SynchronousCompositorFilter::Send(IPC::Message* message) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(filter_ready_);
  if (!io_task_runner_->PostTask(
          FROM_HERE, base::Bind(&SynchronousCompositorFilter::SendOnIOThread,
                                this, message))) {
    delete message;
    DLOG(WARNING) << "IO PostTask failed";
    return false;
  }
  return true;
}

void SynchronousCompositorFilter::SendOnIOThread(IPC::Message* message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(sender_);
  if (!sender_) {
    delete message;
    return;
  }
  bool result = sender_->Send(message);
  if (!result)
    DLOG(WARNING) << "Failed to send message";
}

void SynchronousCompositorFilter::FilterReadyOnCompositorThread() {
  DCHECK(!filter_ready_);
  filter_ready_ = true;
  for (const auto& entry_pair : synchronous_input_handler_proxy_map_) {
    DCHECK(entry_pair.second);
    int routing_id = entry_pair.first;
    CreateSynchronousCompositorProxy(routing_id, entry_pair.second);
    auto output_surface_entry = output_surface_map_.find(routing_id);
    if (output_surface_entry != output_surface_map_.end()) {
      SetProxyOutputSurface(routing_id, output_surface_entry->second);
    }
  }
}

void SynchronousCompositorFilter::RegisterOutputSurface(
    int routing_id,
    SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(output_surface);
  SynchronousCompositorProxy* proxy = FindProxy(routing_id);
  if (proxy) {
    proxy->SetOutputSurface(output_surface);
  } else {
    DCHECK(output_surface_map_.find(routing_id) == output_surface_map_.end());
    output_surface_map_[routing_id] = output_surface;
  }
}

void SynchronousCompositorFilter::OnSynchronizeRendererState(
    const std::vector<int>& routing_ids,
    std::vector<SyncCompositorCommonRendererParams>* out) {
  for (int routing_id : routing_ids) {
    SynchronousCompositorProxy* proxy = FindProxy(routing_id);
    SyncCompositorCommonRendererParams param;
    if (proxy)
      proxy->PopulateCommonParams(&param);
    out->push_back(param);
  }
}

void SynchronousCompositorFilter::UnregisterOutputSurface(
    int routing_id,
    SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(output_surface);
  SynchronousCompositorProxy* proxy = FindProxy(routing_id);
  if (proxy) {
    proxy->SetOutputSurface(nullptr);
  }
  auto entry = output_surface_map_.find(routing_id);
  if (entry != output_surface_map_.end())
    output_surface_map_.erase(entry);
}

void SynchronousCompositorFilter::CreateSynchronousCompositorProxy(
    int routing_id,
    ui::SynchronousInputHandlerProxy* synchronous_input_handler_proxy) {
  DCHECK(!sync_compositor_map_.contains(routing_id));
  std::unique_ptr<SynchronousCompositorProxy> proxy(
      new SynchronousCompositorProxy(routing_id, this,
                                     synchronous_input_handler_proxy));
  sync_compositor_map_.add(routing_id, std::move(proxy));
}

void SynchronousCompositorFilter::SetProxyOutputSurface(
    int routing_id,
    SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(output_surface);
  SynchronousCompositorProxy* proxy = FindProxy(routing_id);
  DCHECK(proxy);
  proxy->SetOutputSurface(output_surface);
}

void SynchronousCompositorFilter::DidAddSynchronousHandlerProxy(
    int routing_id,
    ui::SynchronousInputHandlerProxy* synchronous_input_handler_proxy) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(synchronous_input_handler_proxy);
  if (filter_ready_) {
    CreateSynchronousCompositorProxy(routing_id,
                                     synchronous_input_handler_proxy);
    auto entry = output_surface_map_.find(routing_id);
    if (entry != output_surface_map_.end())
      SetProxyOutputSurface(routing_id, entry->second);
  } else {
    auto*& mapped_synchronous_input_handler_proxy =
        synchronous_input_handler_proxy_map_[routing_id];
    DCHECK(!mapped_synchronous_input_handler_proxy);
    mapped_synchronous_input_handler_proxy = synchronous_input_handler_proxy;
  }
}

void SynchronousCompositorFilter::DidRemoveSynchronousHandlerProxy(
    int routing_id) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  if (ContainsKey(sync_compositor_map_, routing_id)) {
    DCHECK(compositor_task_runner_->BelongsToCurrentThread());
    DCHECK(sync_compositor_map_.contains(routing_id));
    sync_compositor_map_.erase(routing_id);
  }
  if (ContainsKey(synchronous_input_handler_proxy_map_, routing_id))
    synchronous_input_handler_proxy_map_.erase(routing_id);
}

}  // namespace content
