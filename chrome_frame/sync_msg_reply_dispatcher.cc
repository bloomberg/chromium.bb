// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/sync_msg_reply_dispatcher.h"

#include "ipc/ipc_sync_message.h"

void SyncMessageReplyDispatcher::Push(IPC::SyncMessage* msg, void* callback,
                                      void* key) {
  MessageSent pending(IPC::SyncMessage::GetMessageId(*msg),
                      msg->type(), callback, key);
  AutoLock lock(message_queue_lock_);
  message_queue_.push_back(pending);
}

bool SyncMessageReplyDispatcher::HandleMessageType(const IPC::Message& msg,
                                                   const MessageSent& origin) {
  return false;
}

bool SyncMessageReplyDispatcher::OnMessageReceived(const IPC::Message& msg) {
  MessageSent origin;
  if (!Pop(msg, &origin)) {
    return false;
  }

  // No callback e.g. no return values and/or don't care
  if (origin.callback == NULL)
    return true;

  return HandleMessageType(msg, origin);
}

void SyncMessageReplyDispatcher::Cancel(void* key) {
  DCHECK(key != NULL);
  AutoLock lock(message_queue_lock_);
  PendingSyncMessageQueue::iterator it;
  for (it = message_queue_.begin(); it != message_queue_.end(); ++it) {
    if (it->key == key) {
      it->key = NULL;
    }
  }
}

bool SyncMessageReplyDispatcher::Pop(const IPC::Message& msg, MessageSent* t) {
  if (!msg.is_reply())
    return false;

  int id = IPC::SyncMessage::GetMessageId(msg);
  AutoLock lock(message_queue_lock_);
  PendingSyncMessageQueue::iterator it;
  for (it = message_queue_.begin(); it != message_queue_.end(); ++it) {
    if (it->id == id) {
      *t = *it;
      message_queue_.erase(it);
      return true;
    }
  }
  return false;
}
