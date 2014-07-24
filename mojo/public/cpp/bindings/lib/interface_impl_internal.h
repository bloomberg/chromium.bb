// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_IMPL_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_IMPL_INTERNAL_H_

#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/lib/filter_chain.h"
#include "mojo/public/cpp/bindings/lib/message_header_validator.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/environment/logging.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace internal {

template <typename Interface>
class InterfaceImplBase : public Interface {
 public:
  virtual ~InterfaceImplBase() {}
  virtual void OnConnectionEstablished() = 0;
  virtual void OnConnectionError() = 0;
};

template <typename Interface>
class InterfaceImplState : public ErrorHandler {
 public:
  typedef typename Interface::Client Client;

  explicit InterfaceImplState(InterfaceImplBase<Interface>* instance)
      : router_(NULL),
        proxy_(NULL),
        instance_bound_to_pipe_(false)
#ifndef NDEBUG
        ,
        deleting_instance_due_to_error_(false)
#endif
  {
    MOJO_DCHECK(instance);
    stub_.set_sink(instance);
  }

  virtual ~InterfaceImplState() {
#ifndef NDEBUG
    MOJO_DCHECK(!instance_bound_to_pipe_ || deleting_instance_due_to_error_);
#endif
    delete proxy_;
    if (router_) {
      router_->set_error_handler(NULL);
      delete router_;
    }
  }

  void BindProxy(
      InterfacePtr<Interface>* ptr,
      bool instance_bound_to_pipe,
      const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter()) {
    MessagePipe pipe;
    ptr->Bind(pipe.handle0.Pass(), waiter);
    Bind(pipe.handle1.Pass(), instance_bound_to_pipe, waiter);
  }

  void Bind(ScopedMessagePipeHandle handle,
            bool instance_bound_to_pipe,
            const MojoAsyncWaiter* waiter) {
    MOJO_DCHECK(!router_);

    FilterChain filters;
    filters.Append<MessageHeaderValidator>();
    filters.Append<typename Interface::RequestValidator_>();
    filters.Append<typename Interface::Client::ResponseValidator_>();

    router_ = new Router(handle.Pass(), filters.Pass(), waiter);
    router_->set_incoming_receiver(&stub_);
    router_->set_error_handler(this);

    proxy_ = new typename Client::Proxy_(router_);

    instance_bound_to_pipe_ = instance_bound_to_pipe;

    instance()->OnConnectionEstablished();
  }

  bool WaitForIncomingMethodCall() {
    MOJO_DCHECK(router_);
    return router_->WaitForIncomingMessage();
  }

  Router* router() { return router_; }
  Client* client() { return proxy_; }

 private:
  InterfaceImplBase<Interface>* instance() {
    return static_cast<InterfaceImplBase<Interface>*>(stub_.sink());
  }

  virtual void OnConnectionError() MOJO_OVERRIDE {
    // If the the instance is not bound to the pipe, the instance might choose
    // to delete itself in the OnConnectionError handler, which would in turn
    // delete this.  Save the error behavior before invoking the error handler
    // so we can correctly decide what to do.
    bool bound = instance_bound_to_pipe_;
    instance()->OnConnectionError();
    if (!bound)
      return;
#ifndef NDEBUG
    deleting_instance_due_to_error_ = true;
#endif
    delete instance();
  }

  Router* router_;
  typename Client::Proxy_* proxy_;
  typename Interface::Stub_ stub_;
  bool instance_bound_to_pipe_;
#ifndef NDEBUG
  bool deleting_instance_due_to_error_;
#endif

  MOJO_DISALLOW_COPY_AND_ASSIGN(InterfaceImplState);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_IMPL_INTERNAL_H_
