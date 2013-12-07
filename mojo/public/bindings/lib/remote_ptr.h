// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_REMOTE_PTR_H_
#define MOJO_PUBLIC_BINDINGS_LIB_REMOTE_PTR_H_

#include <assert.h>

#include "mojo/public/bindings/lib/connector.h"
#include "mojo/public/system/macros.h"

namespace mojo {

// A RemotePtr is a smart-pointer for managing the connection of a message pipe
// to an interface proxy.
//
// EXAMPLE
//
// On the client side of a service, RemotePtr might be used like so:
//
//   class FooClientImpl : public FooClientStub {
//    public:
//     explicit FooClientImpl(const mojo::MessagePipeHandle& message_pipe)
//         : foo_(message_pipe) {
//       foo_.SetPeer(this);
//       foo_.Ping();
//     }
//     virtual void Pong() {
//       ...
//     }
//    private:
//     mojo::RemotePtr<Foo> foo_;
//   };
//
// On the implementation side of a service, RemotePtr might be used like so:
//
//   class FooImpl : public FooStub {
//    public:
//     explicit FooImpl(const mojo::MessagePipeHandle& message_pipe)
//         : client_(message_pipe) {
//       client_.SetPeer(this);
//     }
//     virtual void Ping() {
//       client_->Pong();
//     }
//    private:
//     mojo::RemotePtr<FooClient> client_;
//   };
//
template <typename S>
class RemotePtr {
  MOJO_MOVE_ONLY_TYPE_FOR_CPP_03(RemotePtr, RValue);

 public:
  RemotePtr() : state_(NULL) {}
  explicit RemotePtr(ScopedMessagePipeHandle message_pipe)
      : state_(new State(message_pipe.Pass())) {
  }

  // Move-only constructor and operator=.
  RemotePtr(RValue other) : state_(other.object->release()) {}
  RemotePtr& operator=(RValue other) {
    state_ = other.object->release();
    return *this;
  }

  ~RemotePtr() {
    delete state_;
  }

  bool is_null() const {
    return !state_;
  }

  S* get() {
    assert(state_);
    return &state_->proxy;
  }

  S* operator->() {
    return get();
  }

  void reset() {
    delete state_;
    state_ = NULL;
  }

  void reset(ScopedMessagePipeHandle message_pipe) {
    delete state_;
    state_ = new State(message_pipe.Pass());
  }

  bool encountered_error() const {
    assert(state_);
    return state_->connector.encountered_error();
  }

  void SetPeer(typename S::_Peer::_Stub* peer) {
    assert(state_);
    state_->connector.SetIncomingReceiver(peer);
  }

 private:
  struct State {
    State(ScopedMessagePipeHandle message_pipe)
        : connector(message_pipe.Pass()),
          proxy(&connector) {
    }
    internal::Connector connector;
    typename S::_Proxy proxy;
  };

  State* release() {
    State* state = state_;
    state_ = NULL;
    return state;
  }

  State* state_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_REMOTE_PTR_H_
