// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_IMPL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_IMPL_H_

#include "mojo/public/cpp/bindings/lib/interface_impl_internal.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {

// InterfaceImpl<..> is designed to be the base class of an interface
// implementation. It may be bound to a pipe or a proxy, see BindToPipe and
// BindToProxy.
//
// NOTE: A base class of WithErrorHandler<Interface> is used to avoid multiple
// inheritance. This base class inserts the signature of ErrorHandler into the
// inheritance chain.
template <typename Interface>
class InterfaceImpl : public WithErrorHandler<Interface> {
 public:
  InterfaceImpl() : internal_state_(this) {}
  virtual ~InterfaceImpl() {}

  // Subclasses must handle connection errors.
  virtual void OnConnectionError() = 0;

  // DO NOT USE. Exposed only for internal use and for testing.
  internal::InterfaceImplState<Interface>* internal_state() {
    return &internal_state_;
  }

 private:
  internal::InterfaceImplState<Interface> internal_state_;
  MOJO_DISALLOW_COPY_AND_ASSIGN(InterfaceImpl);
};

// Takes an instance of an InterfaceImpl<..> subclass and binds it to the given
// MessagePipe. The instance is returned for convenience in member initializer
// lists, etc. If the pipe is closed, the instance's OnConnectionError method
// will be called.
//
// The instance is also bound to the current thread. Its methods will only be
// called on the current thread, and if the current thread exits, then it will
// also be deleted, and along with it, its end point of the pipe will be closed.
//
// Before returning, the instance will receive a SetClient call, providing it
// with a proxy to the client on the other end of the pipe.
template <typename Impl>
Impl* BindToPipe(Impl* instance,
                 ScopedMessagePipeHandle handle,
                 MojoAsyncWaiter* waiter = GetDefaultAsyncWaiter()) {
  instance->internal_state()->Bind(handle.Pass(), waiter);
  return instance;
}

// Takes an instance of an InterfaceImpl<..> subclass and binds it to the given
// InterfacePtr<..>. The instance is returned for convenience in member
// initializer lists, etc. If the pipe is closed, the instance's
// OnConnectionError method will be called.
//
// The instance is also bound to the current thread. Its methods will only be
// called on the current thread, and if the current thread exits, then it will
// also be deleted, and along with it, its end point of the pipe will be closed.
//
// Before returning, the instance will receive a SetClient call, providing it
// with a proxy to the client on the other end of the pipe.
template <typename Impl, typename Interface>
Impl* BindToProxy(Impl* instance,
                  InterfacePtr<Interface>* ptr,
                  MojoAsyncWaiter* waiter = GetDefaultAsyncWaiter()) {
  instance->internal_state()->BindProxy(ptr, waiter);
  return instance;
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_IMPL_H_
