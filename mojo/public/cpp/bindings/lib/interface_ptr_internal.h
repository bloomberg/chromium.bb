// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_INTERNAL_H_

#include <stdio.h>

#include "mojo/public/cpp/bindings/lib/filter_chain.h"
#include "mojo/public/cpp/bindings/lib/message_header_validator.h"
#include "mojo/public/cpp/bindings/lib/router.h"

namespace mojo {
namespace internal {

template <typename Interface>
class InterfacePtrState {
 public:
  InterfacePtrState() : instance_(NULL), router_(NULL) {}

  ~InterfacePtrState() {
    // Destruction order matters here. We delete |instance_| first, even though
    // |router_| may have a reference to it, so that |~Interface| may have a
    // shot at generating new outbound messages (ie, invoking client methods).
    delete instance_;
    delete router_;
  }

  Interface* instance() const { return instance_; }

  Router* router() const { return router_; }

  void Swap(InterfacePtrState* other) {
    std::swap(other->instance_, instance_);
    std::swap(other->router_, router_);
  }

  void ConfigureProxy(ScopedMessagePipeHandle handle,
                      MojoAsyncWaiter* waiter = GetDefaultAsyncWaiter()) {
    assert(!instance_);
    assert(!router_);

    FilterChain filters;
    filters.Append(new MessageHeaderValidator)
           .Append(new typename Interface::Client::RequestValidator_)
           .Append(new typename Interface::ResponseValidator_);

    router_ = new Router(handle.Pass(), filters.Pass(), waiter);
    ProxyWithStub* proxy = new ProxyWithStub(router_);
    router_->set_incoming_receiver(&proxy->stub);

    instance_ = proxy;
  }

 private:
  class ProxyWithStub : public Interface::Proxy_ {
   public:
    explicit ProxyWithStub(MessageReceiver* receiver)
        : Interface::Proxy_(receiver) {
    }
    virtual void SetClient(typename Interface::Client* client) MOJO_OVERRIDE {
      stub.set_sink(client);
    }
    typename Interface::Client::Stub_ stub;
   private:
    MOJO_DISALLOW_COPY_AND_ASSIGN(ProxyWithStub);
  };

  Interface* instance_;
  Router* router_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(InterfacePtrState);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_INTERNAL_H_
