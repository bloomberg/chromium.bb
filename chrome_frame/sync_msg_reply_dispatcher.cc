// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/sync_msg_reply_dispatcher.h"

#include "ipc/ipc_sync_message.h"

void SyncMessageReplyDispatcher::Push(IPC::SyncMessage* msg,
                                      SyncMessageCallContext* context,
                                      void* key) {
  if (context) {
    context->message_type_ = msg->type();
    context->id_ = IPC::SyncMessage::GetMessageId(*msg);
    context->key_ = key;

    base::AutoLock lock(message_queue_lock_);
    message_queue_.push_back(context);
  }
}

bool SyncMessageReplyDispatcher::HandleMessageType(
    const IPC::Message& msg, SyncMessageCallContext* context) {
  return false;
}

bool SyncMessageReplyDispatcher::OnMessageReceived(const IPC::Message& msg) {
  SyncMessageCallContext* context = GetContext(msg);
  // No context e.g. no return values and/or don't care
  if (!context) {
    return false;
  }

  return HandleMessageType(msg, context);
}

void SyncMessageReplyDispatcher::Cancel(void* key) {
  DCHECK(key != NULL);
  base::AutoLock lock(message_queue_lock_);
  PendingSyncMessageQueue::iterator it = message_queue_.begin();
  while (it != message_queue_.end()) {
    SyncMessageCallContext* context = *it;
    if (context->key_ == key) {
      it = message_queue_.erase(it);
      delete context;
    } else {
      ++it;
    }
  }
}

SyncMessageReplyDispatcher::SyncMessageCallContext*
    SyncMessageReplyDispatcher::GetContext(const IPC::Message& msg) {
  if (!msg.is_reply())
    return NULL;

  int id = IPC::SyncMessage::GetMessageId(msg);
  base::AutoLock lock(message_queue_lock_);
  PendingSyncMessageQueue::iterator it;
  for (it = message_queue_.begin(); it != message_queue_.end(); ++it) {
    SyncMessageCallContext* context = *it;
    if (context->id_ == id) {
      message_queue_.erase(it);
      return context;
    }
  }
  return NULL;
}
