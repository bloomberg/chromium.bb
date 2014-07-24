// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_APPLICATION_INTERFACE_FACTORY_WITH_CONTEXT_H_
#define MOJO_PUBLIC_CPP_APPLICATION_INTERFACE_FACTORY_WITH_CONTEXT_H_

#include "mojo/public/cpp/application/interface_factory.h"

namespace mojo {

// Use this class to allocate and bind instances of Impl constructed with a
// context parameter to interface requests. The lifetime of the constructed
// Impls is bound to the pipe.
template <typename Impl,
          typename Context,
          typename Interface = typename Impl::ImplementedInterface>
class InterfaceFactoryWithContext : public InterfaceFactory<Interface> {
 public:
  explicit InterfaceFactoryWithContext(Context* context) : context_(context) {}
  virtual ~InterfaceFactoryWithContext() {}

  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<Interface> request) MOJO_OVERRIDE {
    BindToRequest(new Impl(context_), &request);
  }

 private:
  Context* context_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_APPLICATION_INTERFACE_FACTORY_WITH_CONTEXT_H_
