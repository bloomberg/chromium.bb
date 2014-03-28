// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_INTERFACE_H_
#define MOJO_PUBLIC_BINDINGS_INTERFACE_H_

#include <assert.h>

#include "mojo/public/bindings/message.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {


// NoInterface is for use in cases when a non-existent or empty interface is
// needed (e.g., when the Mojom "Peer" attribute is not present).

class NoInterface;

class NoInterfaceStub : public MessageReceiver {
 public:
  NoInterfaceStub(NoInterface* unused) {}
  virtual bool Accept(Message* message) MOJO_OVERRIDE;
  virtual bool AcceptWithResponder(Message* message, MessageReceiver* responder)
      MOJO_OVERRIDE;
};

class NoInterface {
 public:
  typedef NoInterfaceStub _Stub;
  typedef NoInterface _Peer;
};


// AnyInterface is for use in cases where any interface would do (e.g., see the
// Shell::Connect method).

typedef NoInterface AnyInterface;


// InterfaceHandle<S>

template <typename S>
class InterfaceHandle : public MessagePipeHandle {
 public:
  InterfaceHandle() {}
  explicit InterfaceHandle(MojoHandle value) : MessagePipeHandle(value) {}
};


// Interface<S>

template <typename S>
struct Interface {
  typedef InterfaceHandle<S> Handle;
  typedef ScopedHandleBase<InterfaceHandle<S> > ScopedHandle;
};

template <>
struct Interface<mojo::NoInterface> {
  typedef MessagePipeHandle Handle;
  typedef ScopedMessagePipeHandle ScopedHandle;
};


// InterfacePipe<S,P> is used to construct a MessagePipe with typed interfaces
// on either end.

template <typename S, typename P = typename S::_Peer>
class InterfacePipe {
 public:
  InterfacePipe() {
    typename Interface<S>::Handle h0;
    typename Interface<P>::Handle h1;
    MojoResult result MOJO_ALLOW_UNUSED =
        MojoCreateMessagePipe(h0.mutable_value(), h1.mutable_value());
    assert(result == MOJO_RESULT_OK);
    handle_to_self.reset(h0);
    handle_to_peer.reset(h1);
  }

  typename Interface<S>::ScopedHandle handle_to_self;
  typename Interface<P>::ScopedHandle handle_to_peer;
};

// TODO(darin): Once we have the ability to use C++11 features, consider
// defining a template alias for ScopedInterfaceHandle<S>.

}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_SCOPED_INTERFACE_H_
