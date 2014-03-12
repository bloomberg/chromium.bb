// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_REMOTE_PTR_H_
#define MOJO_PUBLIC_BINDINGS_REMOTE_PTR_H_

#include <assert.h>

#include "mojo/public/bindings/interface.h"
#include "mojo/public/bindings/lib/router.h"
#include "mojo/public/system/macros.h"

namespace mojo {

// A RemotePtr is a smart-pointer for managing the connection of a message pipe
// to an interface proxy.
//
// EXAMPLE:
//
// Given foo.mojom containing the following interfaces:
//
//   [Peer=FooClient]
//   interface Foo {
//     void Ping();
//   };
//
//   [Peer=Foo]
//   interface FooClient {
//     void Pong();
//   };
//
// On the client side of a service, RemotePtr might be used like so:
//
//   class FooClientImpl : public FooClient {
//    public:
//     explicit FooClientImpl(ScopedFooHandle handle)
//         : foo_(handle.Pass(), this) {
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
//   class FooImpl : public Foo {
//    public:
//     explicit FooImpl(ScopedFooClientHandle handle)
//         : client_(handle.Pass(), this) {
//     }
//     virtual void Ping() {
//       client_->Pong();
//     }
//    private:
//     mojo::RemotePtr<FooClient> client_;
//   };
//
// NOTE:
//
// 1- It is valid to pass NULL for the peer if you are not interested in
//    receiving incoming messages. Those messages will still be consumed.
//
// 2- You may optionally register an ErrorHandler on the RemotePtr to be
//    notified if the peer has gone away. Alternatively, you may poll the
//    |encountered_error()| method to check if the peer has gone away.
//
template <typename S>
class RemotePtr {
  struct State;
  MOJO_MOVE_ONLY_TYPE_FOR_CPP_03(RemotePtr, RValue)

 public:
  RemotePtr() : state_(NULL) {}
  explicit RemotePtr(typename Interface<S>::ScopedHandle interface_handle,
                     typename S::_Peer* peer = NULL,
                     ErrorHandler* error_handler = NULL,
                     MojoAsyncWaiter* waiter = GetDefaultAsyncWaiter())
      : state_(new State(ScopedMessagePipeHandle(interface_handle.Pass()), peer,
                         error_handler, waiter)) {
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

  void reset(typename Interface<S>::ScopedHandle interface_handle,
             typename S::_Peer* peer = NULL,
             ErrorHandler* error_handler = NULL,
             MojoAsyncWaiter* waiter = GetDefaultAsyncWaiter()) {
    delete state_;
    state_ = new State(ScopedMessagePipeHandle(interface_handle.Pass()), peer,
                       error_handler, waiter);
  }

  bool encountered_error() const {
    assert(state_);
    return state_->router.encountered_error();
  }

 private:
  struct State {
    State(ScopedMessagePipeHandle message_pipe, typename S::_Peer* peer,
          ErrorHandler* error_handler, MojoAsyncWaiter* waiter)
        : router(message_pipe.Pass(), waiter),
          proxy(&router),
          stub(peer) {
      router.set_error_handler(error_handler);
      if (peer)
        router.set_incoming_receiver(&stub);
    }
    internal::Router router;
    typename S::_Proxy proxy;
    typename S::_Peer::_Stub stub;
  };

  State* release() {
    State* state = state_;
    state_ = NULL;
    return state;
  }

  State* state_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_REMOTE_PTR_H_
