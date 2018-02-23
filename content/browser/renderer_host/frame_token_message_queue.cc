// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_token_message_queue.h"

#include "ipc/ipc_message.h"

namespace content {

FrameTokenMessageQueue::FrameTokenMessageQueue(Client* client)
    : client_(client) {}

FrameTokenMessageQueue::~FrameTokenMessageQueue() {}

void FrameTokenMessageQueue::DidProcessFrame(uint32_t frame_token) {
  // Frame tokens always increase.
  if (frame_token <= last_received_frame_token_) {
    client_->OnInvalidFrameToken(frame_token);
    return;
  }

  last_received_frame_token_ = frame_token;

  while (queued_messages_.size() &&
         queued_messages_.front().first <= frame_token) {
    ProcessSwapMessages(std::move(queued_messages_.front().second));
    queued_messages_.pop();
  }
}

void FrameTokenMessageQueue::OnFrameSwapMessagesReceived(
    uint32_t frame_token,
    std::vector<IPC::Message> messages) {
  // Zero token is invalid.
  if (!frame_token) {
    client_->OnInvalidFrameToken(frame_token);
    return;
  }

  // Frame tokens always increase.
  if (queued_messages_.size() && frame_token <= queued_messages_.back().first) {
    client_->OnInvalidFrameToken(frame_token);
    return;
  }

  if (frame_token <= last_received_frame_token_) {
    ProcessSwapMessages(std::move(messages));
    return;
  }

  queued_messages_.push(std::make_pair(frame_token, std::move(messages)));
}

void FrameTokenMessageQueue::Reset() {
  last_received_frame_token_ = 0;
  // base::queue does not contain a clear.
  auto doomed = std::move(queued_messages_);
}

void FrameTokenMessageQueue::ProcessSwapMessages(
    std::vector<IPC::Message> messages) {
  for (std::vector<IPC::Message>::const_iterator i = messages.begin();
       i != messages.end(); ++i) {
    client_->OnProcessSwapMessage(*i);
    if (i->dispatch_error())
      client_->OnMessageDispatchError(*i);
  }
}

}  // namespace content
