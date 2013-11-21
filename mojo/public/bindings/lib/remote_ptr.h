// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_REMOTE_PTR_H_
#define MOJO_PUBLIC_BINDINGS_LIB_REMOTE_PTR_H_

#include "mojo/public/bindings/lib/connector.h"

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
 public:
  explicit RemotePtr(const MessagePipeHandle& message_pipe)
      : connector_(message_pipe),
        proxy_(&connector_) {
  }

  S* get() {
    return &proxy_;
  }

  S* operator->() {
    return get();
  }

  void SetPeer(typename S::_Peer::_Stub* peer) {
    connector_.SetIncomingReceiver(peer);
  }

 private:
  Connector connector_;
  typename S::_Proxy proxy_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(RemotePtr);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_REMOTE_PTR_H_
