// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/synchronous_compositor_filter.h"

#include "base/callback.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "content/common/android/sync_compositor_messages.h"
#include "content/common/input_messages.h"
#include "content/renderer/android/synchronous_compositor_proxy.h"
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
      base::Bind(&SynchronousCompositorFilter::FilterReadyyOnCompositorThread,
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

void SynchronousCompositorFilter::FilterReadyyOnCompositorThread() {
  DCHECK(!filter_ready_);
  filter_ready_ = true;
  for (const auto& entry_pair : entry_map_) {
    CheckIsReady(entry_pair.first);
  }
}

void SynchronousCompositorFilter::RegisterOutputSurface(
    int routing_id,
    SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(output_surface);
  Entry& entry = entry_map_[routing_id];
  DCHECK(!entry.output_surface);
  entry.output_surface = output_surface;
  CheckIsReady(routing_id);
}

void SynchronousCompositorFilter::UnregisterOutputSurface(
    int routing_id,
    SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(output_surface);
  DCHECK(ContainsKey(entry_map_, routing_id));
  Entry& entry = entry_map_[routing_id];
  DCHECK_EQ(output_surface, entry.output_surface);

  if (entry.IsReady())
    UnregisterObjects(routing_id);
  entry.output_surface = nullptr;
  RemoveEntryIfNeeded(routing_id);
}

void SynchronousCompositorFilter::RegisterBeginFrameSource(
    int routing_id,
    SynchronousCompositorExternalBeginFrameSource* begin_frame_source) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(begin_frame_source);
  Entry& entry = entry_map_[routing_id];
  DCHECK(!entry.begin_frame_source);
  entry.begin_frame_source = begin_frame_source;
  CheckIsReady(routing_id);
}

void SynchronousCompositorFilter::UnregisterBeginFrameSource(
    int routing_id,
    SynchronousCompositorExternalBeginFrameSource* begin_frame_source) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(begin_frame_source);
  DCHECK(ContainsKey(entry_map_, routing_id));
  Entry& entry = entry_map_[routing_id];
  DCHECK_EQ(begin_frame_source, entry.begin_frame_source);

  if (entry.IsReady())
    UnregisterObjects(routing_id);
  entry.begin_frame_source = nullptr;
  RemoveEntryIfNeeded(routing_id);
}

void SynchronousCompositorFilter::CheckIsReady(int routing_id) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(ContainsKey(entry_map_, routing_id));
  Entry& entry = entry_map_[routing_id];
  if (filter_ready_ && entry.IsReady()) {
    DCHECK(!sync_compositor_map_.contains(routing_id));
    sync_compositor_map_.add(
        routing_id,
        make_scoped_ptr(new SynchronousCompositorProxy(
            routing_id, this, entry.output_surface, entry.begin_frame_source,
            entry.synchronous_input_handler_proxy, &input_handler_)));
  }
}

void SynchronousCompositorFilter::UnregisterObjects(int routing_id) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(sync_compositor_map_.contains(routing_id));
  sync_compositor_map_.erase(routing_id);
}

void SynchronousCompositorFilter::RemoveEntryIfNeeded(int routing_id) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(ContainsKey(entry_map_, routing_id));
  Entry& entry = entry_map_[routing_id];
  if (!entry.begin_frame_source && !entry.output_surface &&
      !entry.synchronous_input_handler_proxy) {
    entry_map_.erase(routing_id);
  }
}

void SynchronousCompositorFilter::SetBoundHandler(const Handler& handler) {
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &SynchronousCompositorFilter::SetBoundHandlerOnCompositorThread, this,
          handler));
}

void SynchronousCompositorFilter::SetBoundHandlerOnCompositorThread(
    const Handler& handler) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  input_handler_ = handler;
}

void SynchronousCompositorFilter::DidAddInputHandler(
    int routing_id,
    ui::SynchronousInputHandlerProxy* synchronous_input_handler_proxy) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(synchronous_input_handler_proxy);
  Entry& entry = entry_map_[routing_id];
  DCHECK(!entry.synchronous_input_handler_proxy);
  entry.synchronous_input_handler_proxy = synchronous_input_handler_proxy;
  CheckIsReady(routing_id);
}

void SynchronousCompositorFilter::DidRemoveInputHandler(int routing_id) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(ContainsKey(entry_map_, routing_id));
  Entry& entry = entry_map_[routing_id];

  if (entry.IsReady())
    UnregisterObjects(routing_id);
  entry.synchronous_input_handler_proxy = nullptr;
  RemoveEntryIfNeeded(routing_id);
}

void SynchronousCompositorFilter::DidOverscroll(
    int routing_id,
    const DidOverscrollParams& params) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  SynchronousCompositorProxy* proxy = FindProxy(routing_id);
  if (!proxy) {
    DLOG(WARNING) << "No matching proxy in DidOverScroll " << routing_id;
    return;
  }
  proxy->DidOverscroll(params);
}

void SynchronousCompositorFilter::DidStopFlinging(int routing_id) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  Send(new InputHostMsg_DidStopFlinging(routing_id));
}

SynchronousCompositorFilter::Entry::Entry()
    : begin_frame_source(nullptr),
      output_surface(nullptr),
      synchronous_input_handler_proxy(nullptr) {}

bool SynchronousCompositorFilter::Entry::IsReady() {
  return begin_frame_source && output_surface &&
         synchronous_input_handler_proxy;
}

}  // namespace content
