// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_SYNC_MSG_REPLY_DISPATCHER_H_
#define CHROME_FRAME_SYNC_MSG_REPLY_DISPATCHER_H_

#include <deque>

#include "base/lock.h"
#include "ipc/ipc_channel_proxy.h"

// Base class used to allow synchronous IPC messages to be sent and
// received in an asynchronous manner. To use this class add it as a filter to
// your IPC channel using ChannelProxy::AddFilter(). From then on, before
// sending a synchronous message, call SyncMessageReplyDispatcher::Push() with
// a callback and a key. This class will then handle the message response and
// will call the callback when it is received.
//
// This class is intended to be extended by classes implementing
// HandleMessageType with delegation for the messages they expect to receive in
// cases where you care about the return values of synchronous messages.
//
// Sample usage pattern:
//
// // Add handling for desired message types.
// class SyncMessageReplyDispatcherImpl : public SyncMessageReplyDispatcher {
//   virtual bool HandleMessageType(const IPC::Message& msg,
//                                  const MessageSent& origin) {
//    switch (origin.type) {
//      case AutomationMsg_CreateExternalTab::ID:
//        InvokeCallback<Tuple3<HWND, HWND, int> >(msg, origin);
//        break;
//     [HANDLING FOR OTHER EXPECTED MESSAGE TYPES]
//   }
// }
//
// // Add the filter
// IPC::SyncChannel channel_;
// channel_.AddFilter(new SyncMessageReplyDispatcherImpl());
//
// sync_->Push(msg, NewCallback(this, CallbackMethod, this);
// channel_->ChannelProxy::Send(msg);
//
class SyncMessageReplyDispatcher : public IPC::ChannelProxy::MessageFilter {
 public:
  SyncMessageReplyDispatcher() {}
  void Push(IPC::SyncMessage* msg, void* callback, void* key);
  void Cancel(void* key);

 protected:
  struct MessageSent {
    MessageSent() {}
    MessageSent(int id, uint32 type, void* callback, void* key)
      : id(id), callback(callback), type(type), key(key) {}
    int id;
    void* callback;
    void* key;
    uint32 type;
  };

  typedef std::deque<MessageSent> PendingSyncMessageQueue;

  bool Pop(const IPC::Message& msg, MessageSent* t);
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // Child classes must implement a handler for the message types they are
  // interested in handling responses for. If you don't care about the replies
  // to any of the sync messages you are handling, then you don't have to
  // implement this.
  virtual bool HandleMessageType(const IPC::Message& msg,
                                 const MessageSent& origin);

  template <typename T>
  void InvokeCallback(const IPC::Message& msg, const MessageSent& origin) {
    // Ensure we delete the callback
    scoped_ptr<CallbackRunner<T> > c(
        reinterpret_cast<CallbackRunner<T>*>(origin.callback));
    if (origin.key == NULL) {  // Canceled
      return;
    }

    T tmp;  // Acts as "initializer" for output parameters.
    IPC::ParamDeserializer<T> deserializer(tmp);
    if (deserializer.MessageReplyDeserializer::SerializeOutputParameters(msg)) {
      c->RunWithParams(deserializer.out_);
    } else {
      // TODO(stoyan): How to handle errors?
    }
  }

  PendingSyncMessageQueue message_queue_;
  Lock message_queue_lock_;
};

#endif  // CHROME_FRAME_SYNC_MSG_REPLY_DISPATCHER_H_
