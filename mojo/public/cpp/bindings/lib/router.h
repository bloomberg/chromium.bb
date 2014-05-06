// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_ROUTER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_ROUTER_H_

#include <map>

#include "mojo/public/cpp/bindings/lib/connector.h"
#include "mojo/public/cpp/bindings/lib/shared_data.h"

namespace mojo {
namespace internal {

class Router : public MessageReceiver {
 public:
  // The Router takes ownership of |message_pipe|.
  explicit Router(ScopedMessagePipeHandle message_pipe,
                  MojoAsyncWaiter* waiter = GetDefaultAsyncWaiter());
  virtual ~Router();

  // Sets the receiver to handle messages read from the message pipe that do
  // not have the kMessageIsResponse flag set.
  void set_incoming_receiver(MessageReceiver* receiver) {
    incoming_receiver_ = receiver;
  }

  // Sets the error handler to receive notifications when an error is
  // encountered while reading from the pipe or waiting to read from the pipe.
  void set_error_handler(ErrorHandler* error_handler) {
    connector_.set_error_handler(error_handler);
  }

  // Errors from incoming receivers will force the router's connector into an
  // error state, where no more messages will be processed. This method is used
  // during testing to prevent that from happening.
  void set_enforce_errors_from_incoming_receiver(bool enforce) {
    connector_.set_enforce_errors_from_incoming_receiver(enforce);
  }

  // Returns true if an error was encountered while reading from the pipe or
  // waiting to read from the pipe.
  bool encountered_error() const { return connector_.encountered_error(); }

  // MessageReceiver implementation:
  virtual bool Accept(Message* message) MOJO_OVERRIDE;
  virtual bool AcceptWithResponder(Message* message, MessageReceiver* responder)
      MOJO_OVERRIDE;

 private:
  typedef std::map<uint64_t, MessageReceiver*> ResponderMap;

  class HandleIncomingMessageThunk : public MessageReceiver {
   public:
    HandleIncomingMessageThunk(Router* router);
    virtual ~HandleIncomingMessageThunk();

    // MessageReceiver implementation:
    virtual bool Accept(Message* message) MOJO_OVERRIDE;
    virtual bool AcceptWithResponder(Message* message,
                                     MessageReceiver* responder) MOJO_OVERRIDE;
   private:
    Router* router_;
  };

  bool HandleIncomingMessage(Message* message);

  Connector connector_;
  SharedData<Router*> weak_self_;
  MessageReceiver* incoming_receiver_;
  HandleIncomingMessageThunk thunk_;
  ResponderMap responders_;
  uint64_t next_request_id_;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ROUTER_H_
