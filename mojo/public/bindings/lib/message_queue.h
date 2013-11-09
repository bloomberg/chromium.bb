// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_MESSAGE_QUEUE_H_
#define MOJO_PUBLIC_BINDINGS_LIB_MESSAGE_QUEUE_H_

#include <queue>

#include "mojo/public/system/macros.h"

namespace mojo {

class Message;

// A queue for Message objects.
class MessageQueue {
 public:
  MessageQueue();
  ~MessageQueue();

  bool IsEmpty() const;
  Message* Peek();

  // This method transfers ownership of |message->data| and |message->handles|
  // to the message queue, resetting |message| in the process.
  void Push(Message* message);

  // Removes the next message from the queue, transferring ownership of its
  // data and handles to the given |message|.
  void Pop(Message* message);

  // Removes the next message from the queue, discarding its data and handles.
  // This is meant to be used in conjunction with |Peek|.
  void Pop();

 private:
  std::queue<Message*> queue_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(MessageQueue);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_MESSAGE_QUEUE_H_
