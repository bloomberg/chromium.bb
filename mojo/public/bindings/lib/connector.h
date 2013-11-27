// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_CONNECTOR_H_
#define MOJO_PUBLIC_BINDINGS_LIB_CONNECTOR_H_

#include "mojo/public/bindings/lib/bindings_support.h"
#include "mojo/public/bindings/lib/message.h"
#include "mojo/public/bindings/lib/message_queue.h"
#include "mojo/public/system/core_cpp.h"

namespace mojo {

// The Connector class is responsible for performing read/write operations on a
// MessagePipe. It writes messages it receives through the MessageReceiver
// interface that it subclasses, and it forwards messages it reads through the
// MessageReceiver interface assigned as its incoming receiver.
//
// NOTE: MessagePipe I/O is non-blocking.
//
class Connector : public MessageReceiver {
 public:
  // The Connector takes ownership of |message_pipe|.
  explicit Connector(ScopedMessagePipeHandle message_pipe);
  virtual ~Connector();

  // Sets the receiver to handle messages read from the message pipe.  The
  // Connector will only read messages from the pipe if an incoming receiver
  // has been set.
  void SetIncomingReceiver(MessageReceiver* receiver);

  // Returns true if an error was encountered while reading from or writing to
  // the message pipe.
  bool EncounteredError() const { return error_; }

  // MessageReceiver implementation:
  virtual bool Accept(Message* message) MOJO_OVERRIDE;

 private:
  class Callback : public BindingsSupport::AsyncWaitCallback {
   public:
    Callback();
    virtual ~Callback();

    void SetOwnerToNotify(Connector* owner);
    void SetAsyncWaitID(BindingsSupport::AsyncWaitID async_wait_id);

    virtual void OnHandleReady(MojoResult result) MOJO_OVERRIDE;

   private:
    Connector* owner_;
    BindingsSupport::AsyncWaitID async_wait_id_;
  };
  friend class Callback;

  void OnHandleReady(Callback* callback, MojoResult result);
  void WaitToReadMore();
  void WaitToWriteMore();
  void ReadMore();
  void WriteMore();
  void WriteOne(Message* message, bool* wait_to_write);

  ScopedMessagePipeHandle message_pipe_;
  MessageReceiver* incoming_receiver_;
  MessageQueue write_queue_;

  Callback read_callback_;
  Callback write_callback_;

  bool error_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(Connector);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_CONNECTOR_H_
