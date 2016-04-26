// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRONG_BINDING_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRONG_BINDING_H_

#include <utility>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {

// This connects an interface implementation strongly to a pipe. When a
// connection error is detected or the current message loop is destructed the
// implementation is deleted.
//
// Example of an implementation that is always bound strongly to a pipe
//
//   class StronglyBound : public Foo {
//    public:
//     explicit StronglyBound(InterfaceRequest<Foo> request)
//         : binding_(this, std::move(request)) {}
//
//     // Foo implementation here
//
//    private:
//     StrongBinding<Foo> binding_;
//   };
//
//   class MyFooFactory : public InterfaceFactory<Foo> {
//    public:
//     void Create(..., InterfaceRequest<Foo> request) override {
//       new StronglyBound(std::move(request));  // The binding now owns the
//                                               // instance of StronglyBound.
//     }
//   };
//
// This class is thread hostile once it is bound to a message pipe. Until it is
// bound, it may be bound or destroyed on any thread.
template <typename Interface>
class StrongBinding : public base::MessageLoop::DestructionObserver {
  MOVE_ONLY_TYPE_FOR_CPP_03(StrongBinding);

 public:
  explicit StrongBinding(Interface* impl) : binding_(impl), observing_(true) {
    base::MessageLoop::current()->AddDestructionObserver(this);
  }

  StrongBinding(Interface* impl, ScopedMessagePipeHandle handle)
      : StrongBinding(impl) {
    Bind(std::move(handle));
  }

  StrongBinding(Interface* impl, InterfacePtr<Interface>* ptr)
      : StrongBinding(impl) {
    Bind(ptr);
  }

  StrongBinding(Interface* impl, InterfaceRequest<Interface> request)
      : StrongBinding(impl) {
    Bind(std::move(request));
  }

  ~StrongBinding() override { StopObservingIfNecessary(); }

  void Bind(ScopedMessagePipeHandle handle) {
    DCHECK(!binding_.is_bound());
    binding_.Bind(std::move(handle));
    binding_.set_connection_error_handler([this]() { OnConnectionError(); });
  }

  void Bind(InterfacePtr<Interface>* ptr) {
    DCHECK(!binding_.is_bound());
    binding_.Bind(ptr);
    binding_.set_connection_error_handler([this]() { OnConnectionError(); });
  }

  void Bind(InterfaceRequest<Interface> request) {
    DCHECK(!binding_.is_bound());
    binding_.Bind(std::move(request));
    binding_.set_connection_error_handler([this]() { OnConnectionError(); });
  }

  bool WaitForIncomingMethodCall() {
    return binding_.WaitForIncomingMethodCall();
  }

  // Note: The error handler must not delete the interface implementation.
  //
  // This method may only be called after this StrongBinding has been bound to a
  // message pipe.
  void set_connection_error_handler(const Closure& error_handler) {
    DCHECK(binding_.is_bound());
    connection_error_handler_ = error_handler;
  }

  void OnConnectionError() {
    StopObservingIfNecessary();
    connection_error_handler_.Run();
    delete binding_.impl();
  }

  // base::MessageLoop::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    StopObservingIfNecessary();
    binding_.Close();
    delete binding_.impl();
  }

 private:
  void StopObservingIfNecessary() {
    if (observing_) {
      observing_ = false;
      base::MessageLoop::current()->RemoveDestructionObserver(this);
    }
  }

  Closure connection_error_handler_;
  Binding<Interface> binding_;
  // Whether the object is observing message loop destruction.
  bool observing_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRONG_BINDING_H_
