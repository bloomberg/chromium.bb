// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_sync_message_filter.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "ipc/ipc_sync_message.h"

namespace IPC {

SyncMessageFilter::SyncMessageFilter(base::WaitableEvent* shutdown_event)
    : channel_(NULL),
      listener_loop_(MessageLoop::current()),
      io_loop_(NULL),
      shutdown_event_(shutdown_event) {
}

SyncMessageFilter::~SyncMessageFilter() {
}

bool SyncMessageFilter::Send(Message* message) {
  {
    base::AutoLock auto_lock(lock_);
    if (!io_loop_) {
      delete message;
      return false;
    }
  }

  if (!message->is_sync()) {
    io_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &SyncMessageFilter::SendOnIOThread, message));
    return true;
  }

  base::WaitableEvent done_event(true, false);
  PendingSyncMsg pending_message(
      SyncMessage::GetMessageId(*message),
      reinterpret_cast<SyncMessage*>(message)->GetReplyDeserializer(),
      &done_event);

  {
    base::AutoLock auto_lock(lock_);
    // Can't use this class on the main thread or else it can lead to deadlocks.
    // Also by definition, can't use this on IO thread since we're blocking it.
    DCHECK(MessageLoop::current() != listener_loop_);
    DCHECK(MessageLoop::current() != io_loop_);
    pending_sync_messages_.insert(&pending_message);
  }

  io_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &SyncMessageFilter::SendOnIOThread, message));

  base::WaitableEvent* events[2] = { shutdown_event_, &done_event };
  base::WaitableEvent::WaitMany(events, 2);

  {
    base::AutoLock auto_lock(lock_);
    delete pending_message.deserializer;
    pending_sync_messages_.erase(&pending_message);
  }

  return pending_message.send_result;
}

void SyncMessageFilter::SendOnIOThread(Message* message) {
  if (channel_) {
    channel_->Send(message);
    return;
  }

  if (message->is_sync()) {
    // We don't know which thread sent it, but it doesn't matter, just signal
    // them all.
    SignalAllEvents();
  }

  delete message;
}

void SyncMessageFilter::SignalAllEvents() {
  base::AutoLock auto_lock(lock_);
  for (PendingSyncMessages::iterator iter = pending_sync_messages_.begin();
       iter != pending_sync_messages_.end(); ++iter) {
    (*iter)->done_event->Signal();
  }
}

void SyncMessageFilter::OnFilterAdded(Channel* channel) {
  channel_ = channel;
  base::AutoLock auto_lock(lock_);
  io_loop_ = MessageLoop::current();
}

void SyncMessageFilter::OnChannelError() {
  channel_ = NULL;
  SignalAllEvents();
}

void SyncMessageFilter::OnChannelClosing() {
  channel_ = NULL;
  SignalAllEvents();
}

bool SyncMessageFilter::OnMessageReceived(const Message& message) {
  base::AutoLock auto_lock(lock_);
  for (PendingSyncMessages::iterator iter = pending_sync_messages_.begin();
       iter != pending_sync_messages_.end(); ++iter) {
    if (SyncMessage::IsMessageReplyTo(message, (*iter)->id)) {
      if (!message.is_reply_error()) {
        (*iter)->send_result =
            (*iter)->deserializer->SerializeOutputParameters(message);
      }
      (*iter)->done_event->Signal();
      return true;
    }
  }

  return false;
}

}  // namespace IPC
