// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_SYNC_MSG_REPLY_DISPATCHER_H_
#define CHROME_FRAME_SYNC_MSG_REPLY_DISPATCHER_H_

#include <deque>

#include "base/callback.h"
#include "base/synchronization/lock.h"
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
// Define a class which inherits from SyncMessageCallContext which specifies
// the output_type tuple and has a Completed member function.
// class SampleContext
//     : public SyncMessageReplyDispatcher::SyncMessageCallContext {
// public:
//  typedef Tuple1<int> output_type;
//  void Completed(int arg) {}
// };
//
// // Add handling for desired message types.
// class SyncMessageReplyDispatcherImpl : public SyncMessageReplyDispatcher {
//   virtual bool HandleMessageType(const IPC::Message& msg,
//                                  SyncMessageReplyDispatcher* context) {
//    switch (context->message_type()) {
//      case AutomationMsg_CreateExternalTab::ID:
//        InvokeCallback<CreateExternalTabContext>(msg, context);
//        break;
//     [HANDLING FOR OTHER EXPECTED MESSAGE TYPES]
//   }
// }
//
// // Add the filter
// IPC::SyncChannel channel_;
// channel_.AddFilter(new SyncMessageReplyDispatcherImpl());
//
// sync_->Push(msg, new SampleContext, this);
// channel_->ChannelProxy::Send(msg);
//
class SyncMessageReplyDispatcher : public IPC::ChannelProxy::MessageFilter {
 public:
  class SyncMessageCallContext {
   public:
    SyncMessageCallContext()
        : id_(0),
          message_type_(0),
          key_(NULL) {}

    virtual ~SyncMessageCallContext() {}

    uint32 message_type() const {
      return message_type_;
    }

   private:
    int id_;
    uint32 message_type_;
    void* key_;

    friend class SyncMessageReplyDispatcher;
  };

  SyncMessageReplyDispatcher() {}
  void Push(IPC::SyncMessage* msg, SyncMessageCallContext* context,
            void* key);
  void Cancel(void* key);

 protected:
  typedef std::deque<SyncMessageCallContext*> PendingSyncMessageQueue;

  SyncMessageCallContext* GetContext(const IPC::Message& msg);

  virtual bool OnMessageReceived(const IPC::Message& msg);

  // Child classes must implement a handler for the message types they are
  // interested in handling responses for. If you don't care about the replies
  // to any of the sync messages you are handling, then you don't have to
  // implement this.
  virtual bool HandleMessageType(const IPC::Message& msg,
                                 SyncMessageCallContext* context);

  template <typename T>
  void InvokeCallback(const IPC::Message& msg,
                      SyncMessageCallContext* call_context) {
    if (!call_context || !call_context->key_) {
      NOTREACHED() << "Invalid context parameter";
      return;
    }

    T* context = static_cast<T*>(call_context);
    T::output_type tmp;  // Acts as "initializer" for output parameters.
    IPC::ParamDeserializer<T::output_type> deserializer(tmp);
    if (deserializer.MessageReplyDeserializer::SerializeOutputParameters(msg)) {
      DispatchToMethod(context, &T::Completed, deserializer.out_);
      delete context;
    } else {
      // TODO(stoyan): How to handle errors?
    }
  }

  PendingSyncMessageQueue message_queue_;
  base::Lock message_queue_lock_;
};

#endif  // CHROME_FRAME_SYNC_MSG_REPLY_DISPATCHER_H_
