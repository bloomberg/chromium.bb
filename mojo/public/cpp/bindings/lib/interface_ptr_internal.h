// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_INTERNAL_H_

#include <stdio.h>

#include "mojo/public/cpp/bindings/lib/router.h"

namespace mojo {
namespace internal {

template <typename Interface>
class InterfacePtrState {
 public:
  InterfacePtrState() : instance_(NULL), client_(NULL), router_(NULL) {}

  ~InterfacePtrState() {
    // Destruction order matters here. We delete |instance_| first, even though
    // |router_| may have a reference to it, so that |~Interface| may have a
    // shot at generating new outbound messages (ie, invoking client methods).
    delete instance_;
    delete router_;
    delete client_;
  }

  Interface* instance() const { return instance_; }
  void set_instance(Interface* instance) { instance_ = instance; }

  Router* router() const { return router_; }

  bool is_configured_as_proxy() const {
    // This question only makes sense if we have a bound pipe.
    return router_ && !client_;
  }

  void Swap(InterfacePtrState* other) {
    std::swap(other->instance_, instance_);
    std::swap(other->client_, client_);
    std::swap(other->router_, router_);
  }

  void ConfigureProxy(ScopedMessagePipeHandle handle,
                      MojoAsyncWaiter* waiter = GetDefaultAsyncWaiter()) {
    assert(!instance_);
    assert(!router_);

    router_ = new Router(handle.Pass(), waiter);
    ProxyWithStub* proxy = new ProxyWithStub(router_);
    router_->set_incoming_receiver(&proxy->stub);

    instance_ = proxy;
  }

  void ConfigureStub(ScopedMessagePipeHandle handle,
                     MojoAsyncWaiter* waiter = GetDefaultAsyncWaiter()) {
    assert(instance_);  // Should have already been set!
    assert(!router_);

    // Stub for binding to state_.instance
    // Proxy for communicating to the client on the other end of the pipe.

    router_ = new Router(handle.Pass(), waiter);
    ClientProxyWithStub* proxy = new ClientProxyWithStub(router_);
    proxy->stub.set_sink(instance_);
    router_->set_incoming_receiver(&proxy->stub);

    instance_->SetClient(proxy);
    client_ = proxy;
  }

 private:
  class ProxyWithStub : public Interface::Proxy_ {
   public:
    explicit ProxyWithStub(MessageReceiver* receiver)
        : Interface::Proxy_(receiver) {
    }
    virtual void SetClient(typename Interface::Client_* client) MOJO_OVERRIDE {
      stub.set_sink(client);
    }
    typename Interface::Client_::Stub_ stub;
   private:
    MOJO_DISALLOW_COPY_AND_ASSIGN(ProxyWithStub);
  };

  class ClientProxyWithStub : public Interface::Client_::Proxy_ {
   public:
    explicit ClientProxyWithStub(MessageReceiver* receiver)
        : Interface::Client_::Proxy_(receiver) {
    }
    typename Interface::Stub_ stub;
   private:
    MOJO_DISALLOW_COPY_AND_ASSIGN(ClientProxyWithStub);
  };

  Interface* instance_;
  typename Interface::Client_* client_;
  Router* router_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(InterfacePtrState);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_INTERNAL_H_
