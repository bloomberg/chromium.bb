// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_INTERNAL_H_

#include <algorithm>  // For |std::swap()|.

#include "mojo/public/cpp/bindings/lib/filter_chain.h"
#include "mojo/public/cpp/bindings/lib/message_header_validator.h"
#include "mojo/public/cpp/bindings/lib/router.h"
#include "mojo/public/cpp/environment/logging.h"

struct MojoAsyncWaiter;

namespace mojo {
namespace internal {

template <typename Interface>
class InterfacePtrState {
 public:
  InterfacePtrState() : proxy_(nullptr), router_(nullptr), waiter_(nullptr) {}

  ~InterfacePtrState() {
    // Destruction order matters here. We delete |proxy_| first, even though
    // |router_| may have a reference to it, so that |~Interface| may have a
    // shot at generating new outbound messages (ie, invoking client methods).
    delete proxy_;
    delete router_;
  }

  Interface* instance() {
    ConfigureProxyIfNecessary();

    // This will be null if the object is not bound.
    return proxy_;
  }

  void Swap(InterfacePtrState* other) {
    std::swap(other->proxy_, proxy_);
    std::swap(other->router_, router_);
    handle_.swap(other->handle_);
    std::swap(other->waiter_, waiter_);
  }

  void Bind(ScopedMessagePipeHandle handle, const MojoAsyncWaiter* waiter) {
    MOJO_DCHECK(!proxy_);
    MOJO_DCHECK(!router_);
    MOJO_DCHECK(!handle_.is_valid());
    MOJO_DCHECK(!waiter_);

    handle_ = handle.Pass();
    waiter_ = waiter;
  }

  bool WaitForIncomingMethodCall() {
    ConfigureProxyIfNecessary();

    MOJO_DCHECK(router_);
    return router_->WaitForIncomingMessage();
  }

  ScopedMessagePipeHandle PassMessagePipe() {
    if (router_)
      return router_->PassMessagePipe();

    waiter_ = nullptr;
    return handle_.Pass();
  }

  bool is_bound() const { return handle_.is_valid() || router_; }

  void set_client(typename Interface::Client* client) {
    ConfigureProxyIfNecessary();

    MOJO_DCHECK(proxy_);
    proxy_->stub.set_sink(client);
  }

  bool encountered_error() const {
    return router_ ? router_->encountered_error() : false;
  }

  void set_error_handler(ErrorHandler* error_handler) {
    ConfigureProxyIfNecessary();

    MOJO_DCHECK(router_);
    router_->set_error_handler(error_handler);
  }

  Router* router_for_testing() {
    ConfigureProxyIfNecessary();
    return router_;
  }

 private:
  class ProxyWithStub : public Interface::Proxy_ {
   public:
    explicit ProxyWithStub(MessageReceiverWithResponder* receiver)
        : Interface::Proxy_(receiver) {}
    typename Interface::Client::Stub_ stub;

   private:
    MOJO_DISALLOW_COPY_AND_ASSIGN(ProxyWithStub);
  };

  void ConfigureProxyIfNecessary() {
    // The proxy has been configured.
    if (proxy_) {
      MOJO_DCHECK(router_);
      return;
    }
    // The object hasn't been bound.
    if (!waiter_) {
      MOJO_DCHECK(!handle_.is_valid());
      return;
    }

    FilterChain filters;
    filters.Append<MessageHeaderValidator>();
    filters.Append<typename Interface::Client::RequestValidator_>();
    filters.Append<typename Interface::ResponseValidator_>();

    router_ = new Router(handle_.Pass(), filters.Pass(), waiter_);
    waiter_ = nullptr;

    ProxyWithStub* proxy = new ProxyWithStub(router_);
    router_->set_incoming_receiver(&proxy->stub);

    proxy_ = proxy;
  }

  ProxyWithStub* proxy_;
  Router* router_;

  // |proxy_| and |router_| are not initialized until read/write with the
  // message pipe handle is needed. Before that, |handle_| and |waiter_| store
  // the arguments of Bind().
  ScopedMessagePipeHandle handle_;
  const MojoAsyncWaiter* waiter_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(InterfacePtrState);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_INTERNAL_H_
