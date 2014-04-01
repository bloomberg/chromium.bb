// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SYNC_DISPATCHER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SYNC_DISPATCHER_H_

#include "mojo/public/cpp/system/core.h"

namespace mojo {

class MessageReceiver;

// Waits for one message to arrive on the message pipe, and dispatch it to the
// receiver. Returns true on success, false on failure.
bool WaitForMessageAndDispatch(MessagePipeHandle handle,
                               mojo::MessageReceiver* receiver);

template<typename S> class SyncDispatcher {
 public:
  SyncDispatcher(ScopedMessagePipeHandle message_pipe, S* sink)
      : message_pipe_(message_pipe.Pass()),
        stub_(sink) {
  }

  bool WaitAndDispatchOneMessage() {
    return WaitForMessageAndDispatch(message_pipe_.get(), &stub_);
  }

 private:
  ScopedMessagePipeHandle message_pipe_;
  typename S::_Stub stub_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SYNC_DISPATCHER_H_
