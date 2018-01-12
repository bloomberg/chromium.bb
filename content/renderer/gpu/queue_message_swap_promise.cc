// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/queue_message_swap_promise.h"

#include "base/command_line.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/gpu/frame_swap_message_queue.h"
#include "ipc/ipc_sync_message_filter.h"

namespace content {

QueueMessageSwapPromise::QueueMessageSwapPromise(
    scoped_refptr<IPC::SyncMessageFilter> message_sender,
    scoped_refptr<content::FrameSwapMessageQueue> message_queue,
    int source_frame_number)
    : message_sender_(message_sender),
      message_queue_(message_queue),
      source_frame_number_(source_frame_number)
#if DCHECK_IS_ON()
      ,
      completed_(false)
#endif
{
  DCHECK(message_sender_.get());
  DCHECK(message_queue_.get());
}

QueueMessageSwapPromise::~QueueMessageSwapPromise() {
  // The promise should have either been kept or broken before it's deleted.
#if DCHECK_IS_ON()
  DCHECK(completed_);
#endif
}

void QueueMessageSwapPromise::DidActivate() {
#if DCHECK_IS_ON()
  DCHECK(!completed_);
#endif
  message_queue_->DidActivate(source_frame_number_);
  // The OutputSurface will take care of the Drain+Send.
}

void QueueMessageSwapPromise::WillSwap(
    viz::CompositorFrameMetadata* compositor_frame_metadata,
    cc::RenderFrameMetadata* render_frame_metadata) {
#if DCHECK_IS_ON()
  DCHECK(!completed_);
#endif
  message_queue_->DidSwap(source_frame_number_);

  if (!message_queue_->AreFramesDiscarded()) {
    std::unique_ptr<FrameSwapMessageQueue::SendMessageScope>
        send_message_scope = message_queue_->AcquireSendMessageScope();
    std::vector<std::unique_ptr<IPC::Message>> messages;
    message_queue_->DrainMessages(&messages);

    messages.push_back(base::MakeUnique<ViewHostMsg_OnRenderFrameSubmitted>(
        message_queue_->routing_id(), *render_frame_metadata));

    std::vector<IPC::Message> messages_to_send;
    FrameSwapMessageQueue::TransferMessages(&messages, &messages_to_send);
    if (!messages_to_send.empty()) {
      compositor_frame_metadata->frame_token =
          message_queue_->AllocateFrameToken();
      message_sender_->Send(new ViewHostMsg_FrameSwapMessages(
          message_queue_->routing_id(), compositor_frame_metadata->frame_token,
          messages_to_send));
    }
  }

  PromiseCompleted();
}

void QueueMessageSwapPromise::DidSwap() {}

cc::SwapPromise::DidNotSwapAction QueueMessageSwapPromise::DidNotSwap(
    DidNotSwapReason reason) {
#if DCHECK_IS_ON()
  DCHECK(!completed_);
#endif
  // TODO(eseckler): Deliver messages with the next swap instead of sending
  // them here directly.
  std::vector<std::unique_ptr<IPC::Message>> messages;
  message_queue_->DidNotSwap(source_frame_number_, reason, &messages);
  for (auto& msg : messages) {
    message_sender_->Send(msg.release());
  }
  PromiseCompleted();
  return DidNotSwapAction::BREAK_PROMISE;
}

void QueueMessageSwapPromise::PromiseCompleted() {
#if DCHECK_IS_ON()
  completed_ = true;
#endif
}

int64_t QueueMessageSwapPromise::TraceId() const {
  return 0;
}

}  // namespace content
