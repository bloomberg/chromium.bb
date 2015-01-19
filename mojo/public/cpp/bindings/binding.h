// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_BINDING_H_
#define MOJO_PUBLIC_CPP_BINDINGS_BINDING_H_

#include "mojo/public/c/environment/async_waiter.h"
#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/lib/filter_chain.h"
#include "mojo/public/cpp/bindings/lib/message_header_validator.h"
#include "mojo/public/cpp/bindings/lib/router.h"
#include "mojo/public/cpp/environment/logging.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {

// This binds an interface implementation a pipe. Deleting the binding closes
// the pipe.
//
// Example:
//
//   #include "foo.mojom.h"
//
//   class FooImpl : public Foo {
//    public:
//     explicit FooImpl(InterfaceRequest<Foo> request)
//         : binding_(this, request.Pass()) {}
//
//     // Foo implementation here.
//
//    private:
//     Binding<Foo> binding_;
//   };
//
//   class MyFooFactory : public InterfaceFactory<Foo> {
//    public:
//     void Create(..., InterfaceRequest<Foo> request) override {
//       auto f = new FooImpl(request.Pass());
//       // Do something to manage the lifetime of |f|. Use StrongBinding<> to
//       // delete FooImpl on connection errors.
//     }
//   };
template <typename Interface>
class Binding : public ErrorHandler {
 public:
  using Client = typename Interface::Client;

  explicit Binding(Interface* impl) : impl_(impl) { stub_.set_sink(impl_); }

  Binding(Interface* impl,
          ScopedMessagePipeHandle handle,
          const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter())
      : Binding(impl) {
    Bind(handle.Pass(), waiter);
  }

  Binding(Interface* impl,
          InterfacePtr<Interface>* ptr,
          const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter())
      : Binding(impl) {
    Bind(ptr, waiter);
  }

  Binding(Interface* impl,
          InterfaceRequest<Interface> request,
          const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter())
      : Binding(impl) {
    Bind(request.PassMessagePipe(), waiter);
  }

  ~Binding() override {
    delete proxy_;
    if (internal_router_) {
      internal_router_->set_error_handler(nullptr);
      delete internal_router_;
    }
  }

  void Bind(
      ScopedMessagePipeHandle handle,
      const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter()) {
    internal::FilterChain filters;
    filters.Append<internal::MessageHeaderValidator>();
    filters.Append<typename Interface::RequestValidator_>();
    filters.Append<typename Client::ResponseValidator_>();

    internal_router_ =
        new internal::Router(handle.Pass(), filters.Pass(), waiter);
    internal_router_->set_incoming_receiver(&stub_);
    internal_router_->set_error_handler(this);

    proxy_ = new typename Client::Proxy_(internal_router_);
  }

  void Bind(
      InterfacePtr<Interface>* ptr,
      const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter()) {
    MessagePipe pipe;
    ptr->Bind(pipe.handle0.Pass(), waiter);
    Bind(pipe.handle1.Pass(), waiter);
  }

  void Bind(
      InterfaceRequest<Interface> request,
      const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter()) {
    Bind(request.PassMessagePipe(), waiter);
  }

  bool WaitForIncomingMethodCall() {
    MOJO_DCHECK(internal_router_);
    return internal_router_->WaitForIncomingMessage();
  }

  void Close() {
    MOJO_DCHECK(internal_router_);
    internal_router_->CloseMessagePipe();
  }

  void set_error_handler(ErrorHandler* error_handler) {
    error_handler_ = error_handler;
  }

  // ErrorHandler implementation
  void OnConnectionError() override {
    if (error_handler_)
      error_handler_->OnConnectionError();
  }

  Interface* impl() { return impl_; }
  Client* client() { return proxy_; }

  bool is_bound() const { return !!internal_router_; }

  // Exposed for testing, should not generally be used.
  internal::Router* internal_router() { return internal_router_; }

 private:
  internal::Router* internal_router_ = nullptr;
  typename Client::Proxy_* proxy_ = nullptr;
  typename Interface::Stub_ stub_;
  Interface* impl_;
  ErrorHandler* error_handler_ = nullptr;

  MOJO_DISALLOW_COPY_AND_ASSIGN(Binding);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_BINDING_H_
