// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_IMPL_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_IMPL_INTERNAL_H_

#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace internal {

template <typename Interface>
class InterfaceImplState : public ErrorHandler {
 public:
  explicit InterfaceImplState(WithErrorHandler<Interface>* instance)
      : router_(NULL),
        proxy_(NULL) {
    assert(instance);
    stub_.set_sink(instance);
  }

  virtual ~InterfaceImplState() {
    delete proxy_;
    if (router_) {
      router_->set_error_handler(NULL);
      delete router_;
    }
  }

  void BindProxy(
      InterfacePtr<Interface>* ptr,
      MojoAsyncWaiter* waiter = GetDefaultAsyncWaiter()) {
    MessagePipe pipe;
    ptr->Bind(pipe.handle0.Pass(), waiter);
    Bind(pipe.handle1.Pass(), waiter);
  }

  void Bind(ScopedMessagePipeHandle handle,
            MojoAsyncWaiter* waiter) {
    assert(!router_);

    router_ = new Router(handle.Pass(), waiter);
    router_->set_incoming_receiver(&stub_);
    router_->set_error_handler(this);

    proxy_ = new typename Interface::Client_::Proxy_(router_);

    stub_.sink()->SetClient(proxy_);
  }

  Router* router() { return router_; }

 private:
  virtual void OnConnectionError() MOJO_OVERRIDE {
    static_cast<WithErrorHandler<Interface>*>(stub_.sink())->
        OnConnectionError();
  }

  internal::Router* router_;
  typename Interface::Client_::Proxy_* proxy_;
  typename Interface::Stub_ stub_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(InterfaceImplState);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_IMPL_INTERNAL_H_
